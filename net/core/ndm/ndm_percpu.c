#include <linux/percpu.h>

void ndm_free_percpu(void __percpu *ptr)
{
	return free_percpu(ptr);
}
EXPORT_SYMBOL(ndm_free_percpu);

const void __percpu *__ndm_alloc_percpu(size_t size, size_t align)
{
	return __alloc_percpu(size, align);
}
EXPORT_SYMBOL(__ndm_alloc_percpu);

int ndm_smp_call_any(const struct cpumask *mask,
		     smp_call_func_t func, void *info, int wait)
{
	return smp_call_function_any(mask, func, info, wait);
}
EXPORT_SYMBOL(ndm_smp_call_any);
