#ifndef __PHR_PERICOM_H
#define __PHR_PERICOM_H

#include <linux/types.h>

struct mutex *phr_pericom_get_mutex(u8 index);

/* must protected by mutex from "phr_pericom_get_mutex" */
int phr_pericom_get(u8 index, u8 offset, bool *on);
int phr_pericom_set(u8 index, u8 offset, bool on);
int phr_pericom_set_dir(u8 index, u8 offset, bool out);

#endif /* __PHR_PERICOM_H */
