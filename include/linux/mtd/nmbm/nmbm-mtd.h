#ifndef _NMBM_MTD_H_
#define _NMBM_MTD_H_

bool is_nmbm_mtd(struct mtd_info *mtd);

/* from mtdcore.c */
int mtd_check_oob_ops(struct mtd_info *mtd, loff_t offs,
		      struct mtd_oob_ops *ops);

#endif /* _NMBM_MTD_H_ */
