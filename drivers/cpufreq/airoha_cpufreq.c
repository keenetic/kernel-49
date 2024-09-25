// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2021-2023 Airoha Inc.
/*
 * Frequency Scaling implementaion based on Airoha SDK
 * Author: Ishant Kumar <Ishant.Kumar@airoha.com>
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include <linux/timex.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>

#define AIROHA_SIP_AVS_HANDLE		(0x82000301)
#define AVS_OP_FREQ_DYN_ADJ		(0xddddddd1)
#define AVS_OP_GET_FREQ			(0xddddddd2)

struct airoha_cpu_dvfs_info {
	struct cpumask cpus;
	struct device *cpu_dev;
	struct thermal_cooling_device *cdev;
	struct list_head list_head;
};

static LIST_HEAD(dvfs_info_list);

/* Check dvfs information for current cpu is already present or not */
static struct airoha_cpu_dvfs_info *airoha_cpu_dvfs_info_lookup(int cpu)
{
	struct airoha_cpu_dvfs_info *info;

	list_for_each_entry(info, &dvfs_info_list, list_head) {
		if (cpumask_test_cpu(cpu, &info->cpus))
			return info;
	}

	return NULL;
}

static unsigned int airoha_freq_get_cpu_frequency(unsigned int cpu)
{
	struct arm_smccc_res res;

	arm_smccc_smc(AIROHA_SIP_AVS_HANDLE, AVS_OP_GET_FREQ, 0, 0, 0, 0, 0, 0,
		      &res);

	return (int) (res.a0 * 1000);
}

static int airoha_freq_target(struct cpufreq_policy *policy, unsigned int state)
{
	struct cpufreq_frequency_table *freq_table = policy->freq_table;
	struct airoha_cpu_dvfs_info *info = policy->driver_data;
	struct device *cpu_dev = info->cpu_dev;
	struct arm_smccc_res res;
	struct dev_pm_opp *opp;
	long freq_hz;

	freq_hz = freq_table[state].frequency * 1000;
	opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_hz);
	if (!opp)
		pr_err("OPP Frequency from current value not found\n");

	/* Adjusting CPU Freq */
	arm_smccc_smc(AIROHA_SIP_AVS_HANDLE, AVS_OP_FREQ_DYN_ADJ, 0, state, 0, 0,
		      0, 0, &res);

	return 0;
}

static int airoha_cpu_dvfs_info_init(struct airoha_cpu_dvfs_info *info, int cpu)
{
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	/* Get OPP-sharing information from "operating-points-v2" bindings */
	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, &info->cpus);
	if (ret) {
		pr_err("failed to get OPP-sharing information for cpu%d\n", cpu);
		return ret;
	}

	ret = dev_pm_opp_of_cpumask_add_table(&info->cpus);
	if (ret) {
		pr_err("no OPP table for cpu%d\n", cpu);
		return ret;
	}

	info->cpu_dev = cpu_dev;

	return 0;
}

static void airoha_cpu_dvfs_info_release(struct airoha_cpu_dvfs_info *info)
{
	dev_pm_opp_of_cpumask_remove_table(&info->cpus);
}

static void airoha_cpufreq_ready(struct cpufreq_policy *policy)
{
	struct airoha_cpu_dvfs_info *info = policy->driver_data;
	struct device_node *np = of_node_get(info->cpu_dev->of_node);
	u32 capacitance = 0;

	if (!np)
		return;

	if (of_find_property(np, "#cooling-cells", NULL)) {
		of_property_read_u32(np, "dynamic-power-coefficient",
				     &capacitance);

		info->cdev = of_cpufreq_power_cooling_register(np,
						policy->related_cpus,
						capacitance,
						NULL);

		if (IS_ERR(info->cdev)) {
			dev_err(info->cpu_dev,
				"running cpufreq without cooling device: %ld\n",
				PTR_ERR(info->cdev));

			info->cdev = NULL;
		}
	}

	of_node_put(np);
}

static int airoha_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;
	struct airoha_cpu_dvfs_info *info;
	int ret;

	info = airoha_cpu_dvfs_info_lookup(policy->cpu);
	if (!info) {
		pr_err("dvfs info for cpu%d is not initialized.\n",
		       policy->cpu);
		return -EINVAL;
	}

	ret = dev_pm_opp_init_cpufreq_table(info->cpu_dev, &freq_table);
	if (ret) {
		pr_err("failed to init cpufreq table for cpu%d: %d\n",
		       policy->cpu, ret);
		return ret;
	}

	ret = cpufreq_table_validate_and_show(policy, freq_table);
	if (ret) {
		pr_err("%s: invalid frequency table: %d\n", __func__, ret);
		goto out_free_cpufreq_table;
	}

	cpumask_copy(policy->cpus, &info->cpus);
	policy->driver_data = info;

	return 0;

out_free_cpufreq_table:
	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &freq_table);
	return ret;
}

static int airoha_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct airoha_cpu_dvfs_info *info = policy->driver_data;

	cpufreq_cooling_unregister(info->cdev);
	dev_pm_opp_free_cpufreq_table(info->cpu_dev, &policy->freq_table);

	return 0;
}

static struct cpufreq_driver airoha_freq_driver = {
	.flags		 = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
			   CPUFREQ_POSTCHANGE,
	.get		 = airoha_freq_get_cpu_frequency,
	.verify		 = cpufreq_generic_frequency_table_verify,
	.target_index	 = airoha_freq_target,
	.init		 = airoha_cpufreq_init,
	.exit		 = airoha_cpufreq_exit,
	.ready		 = airoha_cpufreq_ready,
	.attr		 = cpufreq_generic_attr,
	.name		 = "airoha-cpufreq",
};

static int airoha_cpufreq_probe(struct platform_device *pdev)
{
	struct airoha_cpu_dvfs_info *info, *tmp;
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		info = airoha_cpu_dvfs_info_lookup(cpu);
		if (info)
			continue;

		info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
		if (!info) {
			ret = -ENOMEM;
			goto release_dvfs_info_list;
		}

		ret = airoha_cpu_dvfs_info_init(info, cpu);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to initialize dvfs info for cpu%d\n",
				cpu);
			goto release_dvfs_info_list;
		}

		/* add current cpumask details to the list */
		list_add(&info->list_head, &dvfs_info_list);
	}

	ret = cpufreq_register_driver(&airoha_freq_driver);
	if (ret) {
		dev_err(&pdev->dev, "failed to register cpufreq driver\n");
		goto release_dvfs_info_list;
	}

	dev_info(&pdev->dev, "CPU frequency scaling driver registered\n");

	return 0;

release_dvfs_info_list:
	list_for_each_entry_safe(info, tmp, &dvfs_info_list, list_head) {
		airoha_cpu_dvfs_info_release(info);
		list_del(&info->list_head);
	}

	return ret;
}

/* List of machines supported by this driver */
static const struct of_device_id airoha_cpufreq_machines[] __initconst = {
	{ .compatible = "airoha,an7581", },
	{ .compatible = "airoha,an7583", },
	{ }
};

static struct platform_driver airoha_cpufreq_platdrv = {
	.probe = airoha_cpufreq_probe,
	.driver = {
		.name = "airoha-cpufreq",
	},
};

static int __init airoha_cpufreq_driver_init(void)
{
	struct device_node *np;
	const struct of_device_id *match;
	struct platform_device *pdev;
	int err;

	np = of_find_node_by_path("/");
	if (!np)
		return -ENODEV;

	match = of_match_node(airoha_cpufreq_machines, np);
	of_node_put(np);
	if (!match) {
		pr_warn("Machine is not compatible with airoha-cpufreq\n");
		return -ENODEV;
	}

	/* Register the Platform driver */
	err = platform_driver_register(&airoha_cpufreq_platdrv);
	if (err)
		return err;

	/*
	 * Since there's no place to hold device registration code and no
	 * device tree based way to match cpufreq driver yet, both the driver
	 * and the device registration codes are put here to handle defer
	 * probing.
	 */
	pdev = platform_device_register_simple("airoha-cpufreq", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		pr_err("failed to register airoha-cpufreq platform device\n");
		platform_driver_unregister(&airoha_cpufreq_platdrv);
		return PTR_ERR(pdev);
	}

	return 0;
}
device_initcall(airoha_cpufreq_driver_init);

MODULE_DESCRIPTION("airoha cpufreq driver");
MODULE_AUTHOR("Ishant");
MODULE_LICENSE("GPL");
