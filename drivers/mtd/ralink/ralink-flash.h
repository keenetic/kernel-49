#ifndef __RALINK_FLASH_H__
#define __RALINK_FLASH_H__

#define BOOT_FROM_NOR	0
#define BOOT_FROM_NAND	2
#define BOOT_FROM_SPI	3

int ra_check_flash_type(void);

#endif
