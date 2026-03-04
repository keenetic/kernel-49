#ifndef __NDMPART_DI_PROC_H__
#define __NDMPART_DI_PROC_H__

#include <linux/types.h>
#include <linux/proc_fs.h>

#define NDMPART_DI_PROC_DECLARE_RO(entry)				 \
static const struct file_operations ndmpart_di_proc_fops_##entry = {	 \
	.open	 = __ndmpart_di_proc_open_##entry,			 \
	.read	 = seq_read,						 \
	.llseek	 = seq_lseek,						 \
	.release = single_release					 \
}

#define NDMPART_DI_PROC_DECLARE_WO(entry)				 \
static const struct file_operations ndmpart_di_proc_fops_##entry = {	 \
	.write	 = __ndmpart_di_proc_write_##entry			 \
}

#define NDMPART_DI_PROC_DECLARE_RW(entry)				 \
static const struct file_operations ndmpart_di_proc_fops_##entry = {	 \
	.open	 = __ndmpart_di_proc_open_##entry,			 \
	.read	 = seq_read,						 \
	.llseek	 = seq_lseek,						 \
	.release = single_release,					 \
	.write   = __ndmpart_di_proc_write_##entry			 \
}

#define NDMPART_DI_PROC_CREATE_DIR(name, root)				 \
	proc_mkdir(name, root)

#define NDMPART_DI_PROC_CREATE_RO(entry, root, data)			 \
	proc_create_data(#entry, 0444, root,				 \
			 &ndmpart_di_proc_fops_##entry, data)

#define NDMPART_DI_PROC_CREATE_WO(entry, root, data)			 \
	proc_create_data(#entry, 0220, root,				 \
			 &ndmpart_di_proc_fops_##entry, data)

#define NDMPART_DI_PROC_CREATE_RW(entry, root, data)			 \
	proc_create_data(#entry, 0664, root,				 \
			 &ndmpart_di_proc_fops_##entry, data)

#define NDMPART_DI_PROC_REMOVE(entry, root)				 \
	remove_proc_entry(#entry, root)

#define NDMPART_DI_PROC_REMOVE_DIR(name, root)				 \
	remove_proc_entry(name, root)

#define NDMPART_DI_PROC_ENTRY_DATA(fp)					 \
	PDE_DATA(file_inode(fp))

#endif /* __NDMPART_DI_PROC_H__ */
