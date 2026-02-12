#ifdef CONFIG_NDM_PARTITION
int ndm_partition(struct parsed_partitions *state);
#else
static inline int ndm_partition(struct parsed_partitions *state)
{
	return 1;
}
#endif
