/*
 * This file is part of spi_nand_flash.c
 */

/* SPI NAND Size Define */
#define _SPI_NAND_PAGE_SIZE_512				0x0200
#define _SPI_NAND_PAGE_SIZE_2KBYTE			0x0800
#define _SPI_NAND_PAGE_SIZE_4KBYTE			0x1000
#define _SPI_NAND_OOB_SIZE_64BYTE			0x40
#define _SPI_NAND_OOB_SIZE_120BYTE			0x78
#define _SPI_NAND_OOB_SIZE_128BYTE			0x80
#define _SPI_NAND_OOB_SIZE_256BYTE			0x100
#define _SPI_NAND_BLOCK_SIZE_128KBYTE			0x20000
#define _SPI_NAND_BLOCK_SIZE_256KBYTE			0x40000
#define _SPI_NAND_CHIP_SIZE_512MBIT			0x04000000
#define _SPI_NAND_CHIP_SIZE_1GBIT			0x08000000
#define _SPI_NAND_CHIP_SIZE_2GBIT			0x10000000
#define _SPI_NAND_CHIP_SIZE_4GBIT			0x20000000

/* SPI NAND Manufacturers ID */
#define _SPI_NAND_MANUFACTURER_ID_GIGADEVICE		0xC8
#define _SPI_NAND_MANUFACTURER_ID_WINBOND		0xEF
#define _SPI_NAND_MANUFACTURER_ID_ESMT			0xC8
#define _SPI_NAND_MANUFACTURER_ID_MXIC			0xC2
#define _SPI_NAND_MANUFACTURER_ID_ZENTEL		0xC8
#define _SPI_NAND_MANUFACTURER_ID_ETRON			0xD5
#define _SPI_NAND_MANUFACTURER_ID_TOSHIBA		0x98
#define _SPI_NAND_MANUFACTURER_ID_MICRON		0x2C
#define _SPI_NAND_MANUFACTURER_ID_HEYANG		0xC9
#define _SPI_NAND_MANUFACTURER_ID_PN			0xA1
#define _SPI_NAND_MANUFACTURER_ID_ATO			0x9B
#define _SPI_NAND_MANUFACTURER_ID_ATO_2			0xAD
#define _SPI_NAND_MANUFACTURER_ID_FM			0xA1
#define _SPI_NAND_MANUFACTURER_ID_XTX			0x0B
#define _SPI_NAND_MANUFACTURER_ID_MIRA			0xC8
#define _SPI_NAND_MANUFACTURER_ID_BIWIN			0xBC
#define _SPI_NAND_MANUFACTURER_ID_FORESEE		0xCD
#define _SPI_NAND_MANUFACTURER_ID_DS			0xE5
#define _SPI_NAND_MANUFACTURER_ID_FISON			0x6B
#define _SPI_NAND_MANUFACTURER_ID_TYM			0x19

/* SPI NAND Device ID */
#define _SPI_NAND_DEVICE_ID_GD5F1GQ4UAYIG	0xF1
#define _SPI_NAND_DEVICE_ID_GD5F1GQ4UBYIG	0xD1
#define _SPI_NAND_DEVICE_ID_GD5F1GQ4UCYIG	0xB1
#define _SPI_NAND_DEVICE_ID_GD5F1GQ4UEYIS	0xD3
#define _SPI_NAND_DEVICE_ID_GD5F2GQ4UBYIG	0xD2
#define _SPI_NAND_DEVICE_ID_GD5F2GQ4UE9IS	0xD5
#define _SPI_NAND_DEVICE_ID_GD5F2GQ4UCYIG	0xB2
#define _SPI_NAND_DEVICE_ID_GD5F4GQ4UBYIG	0xD4
#define _SPI_NAND_DEVICE_ID_GD5F4GQ4UCYIG	0xB4
#define _SPI_NAND_DEVICE_ID_F50L512M41A		0x20
#define _SPI_NAND_DEVICE_ID_F50L1G41A0		0x21
#define _SPI_NAND_DEVICE_ID_F50L1G41LB		0x01
#define _SPI_NAND_DEVICE_ID_F50L2G41LB		0x0A
#define _SPI_NAND_DEVICE_ID_W25N01GV		0xAA
#define _SPI_NAND_DEVICE_ID_W25M02GV		0xAB
#define _SPI_NAND_DEVICE_ID_MXIC35LF1GE4AB	0x12
#define _SPI_NAND_DEVICE_ID_MXIC35LF2GE4AB	0x22
#define _SPI_NAND_DEVICE_ID_MXIC35LF4GE4AB	0x32
#define _SPI_NAND_DEVICE_ID_A5U12A21ASC		0x20
#define _SPI_NAND_DEVICE_ID_A5U1GA21BWS		0x21
#define _SPI_NAND_DEVICE_ID_EM73C044SNA		0x19
#define _SPI_NAND_DEVICE_ID_EM73C044SNB		0x11
#define _SPI_NAND_DEVICE_ID_EM73C044SND		0x1D
#define _SPI_NAND_DEVICE_ID_EM73C044SNF		0x09
#define _SPI_NAND_DEVICE_ID_EM73C044VCA		0x18
#define _SPI_NAND_DEVICE_ID_EM73C044VCD		0x1C
#define _SPI_NAND_DEVICE_ID_EM73D044SNA		0x12
#define _SPI_NAND_DEVICE_ID_EM73D044SNC		0x0A
#define _SPI_NAND_DEVICE_ID_EM73D044SND		0x1E
#define _SPI_NAND_DEVICE_ID_EM73D044SNF		0x10
#define _SPI_NAND_DEVICE_ID_EM73D044VCA		0x13
#define _SPI_NAND_DEVICE_ID_EM73D044VCB		0x14
#define _SPI_NAND_DEVICE_ID_EM73D044VCD		0x17
#define _SPI_NAND_DEVICE_ID_EM73D044VCG		0x1F
#define _SPI_NAND_DEVICE_ID_EM73D044VCH		0x1B
#define _SPI_NAND_DEVICE_ID_EM73E044SNA		0x03
#define _SPI_NAND_DEVICE_ID_TC58CVG0S3H		0xC2
#define _SPI_NAND_DEVICE_ID_TC58CVG1S3H		0xCB
#define _SPI_NAND_DEVICE_ID_TC58CVG2S0H		0xCD
#define _SPI_NAND_DEVICE_ID_MT29F1G01		0x14
#define _SPI_NAND_DEVICE_ID_MT29F2G01		0x24
#define _SPI_NAND_DEVICE_ID_MT29F4G01		0x36
#define _SPI_NAND_DEVICE_ID_HYF1GQ4UAACAE	0x51
#define _SPI_NAND_DEVICE_ID_HYF2GQ4UAACAE	0x52
#define _SPI_NAND_DEVICE_ID_HYF2GQ4UHCCAE	0x5A
#define _SPI_NAND_DEVICE_ID_HYF1GQ4UDACAE	0x21
#define _SPI_NAND_DEVICE_ID_HYF2GQ4UDACAE	0x22
#define _SPI_NAND_DEVICE_ID_PN26G01AWSIUG	0xE1
#define _SPI_NAND_DEVICE_ID_PN26G02AWSIUG	0xE2
#define _SPI_NAND_DEVICE_ID_ATO25D1GA		0x12
#define _SPI_NAND_DEVICE_ID_ATO25D2GA		0xF1
#define _SPI_NAND_DEVICE_ID_ATO25D2GB		0xDA
#define _SPI_NAND_DEVICE_ID_FM25S01			0xA1
#define _SPI_NAND_DEVICE_ID_FM25G01B		0xD1
#define _SPI_NAND_DEVICE_ID_FM25G02B		0xD2
#define _SPI_NAND_DEVICE_ID_FM25G02		0xF2
#define _SPI_NAND_DEVICE_ID_FM25G02C		0x92
#define _SPI_NAND_DEVICE_ID_XT26G02B		0xF2
#define _SPI_NAND_DEVICE_ID_XT26G01A		0xE1
#define _SPI_NAND_DEVICE_ID_XT26G02A		0xE2
#define _SPI_NAND_DEVICE_ID_PSU1GS20BN		0x21
#define _SPI_NAND_DEVICE_ID_BWJX08U			0xB1
#define _SPI_NAND_DEVICE_ID_BWET08U			0xB2
#define _SPI_NAND_DEVICE_ID_FS35ND01GD1F1	0xA1
#define _SPI_NAND_DEVICE_ID_FS35ND01GS1F1	0xB1
#define _SPI_NAND_DEVICE_ID_FS35ND02GS2F1	0xA2
#define _SPI_NAND_DEVICE_ID_FS35ND02GD1F1	0xB2
#define _SPI_NAND_DEVICE_ID_DS35Q2GA		0x72
#define _SPI_NAND_DEVICE_ID_DS35Q1GA		0x71
#define _SPI_NAND_DEVICE_ID_CS11G0T0A0AA	0x00
#define _SPI_NAND_DEVICE_ID_CS11G1T0A0AA	0x01
#define _SPI_NAND_DEVICE_ID_CS11G0G0A0AA	0x10
#define _SPI_NAND_DEVICE_ID_TYM25D2GA01		0x01
#define _SPI_NAND_DEVICE_ID_TYM25D2GA02		0x02
#define _SPI_NAND_DEVICE_ID_TYM25D1GA03		0x03

/* OOB layout */
struct spi_nand_flash_ooblayout ooblayout_esmt = {
	.oobsize = 36,
	.oobfree = {{0,1}, {8,8}, {16,1}, {24,8}, {32,1}, {40,8}, {48,1}, {56,8} }
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_esmt_41lb = {
	.oobsize = 20,
	.oobfree = {{0,4}, {4,4}, {20,4}, {36,4}, {52,4}}
};

struct spi_nand_flash_ooblayout ooblayout_mxic = {
	.oobsize = 64,
	.oobfree = {{0,64}}
};

struct spi_nand_flash_ooblayout ooblayout_winbond = {
	.oobsize = 32,
	.oobfree = {{0,8}, {16,8}, {32,8}, {48,8} }
};

struct spi_nand_flash_ooblayout ooblayout_gigadevice_a = {
	.oobsize = 48,
	.oobfree = {{0,12}, {16,12}, {32,12}, {48,12} }
};

struct spi_nand_flash_ooblayout ooblayout_gigadevice_128 = {
	.oobsize = 64,
	.oobfree = {{0,64}}
};

struct spi_nand_flash_ooblayout ooblayout_gigadevice_256 = {
	.oobsize = 128,
	.oobfree = {{0,128}}
};

struct spi_nand_flash_ooblayout ooblayout_gigadevice_GD5FXGQ4U = {
	.oobsize = 52,
	.oobfree = {{0,16}, {20,12}, {36,12}, {52,12}}
};

struct spi_nand_flash_ooblayout ooblayout_zentel = {
	.oobsize = 36,
	.oobfree = {{0,1}, {8,8}, {16,1}, {24,8}, {32,1}, {40,8}, {48,1}, {56,8} }
};

struct spi_nand_flash_ooblayout ooblayout_etron_73C044SNB = {
	.oobsize = 64,
	.oobfree = {{0,16}, {30,16}, {60,16}, {90,16}}
};

struct spi_nand_flash_ooblayout ooblayout_etron_73D044SNA = {
	.oobsize = 72,
	.oobfree = {{0,18}, {32,18}, {64,18}, {96,18}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_etron_73D044SNC = {
	.oobsize = 64,
	.oobfree = {{0,16}, {30,16}, {60,16}, {90,16}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_type1 = {
	.oobsize = 32,
	.oobfree = {{0,8}, {16,8}, {32,8}, {48,8}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_type6 = {
	.oobsize = 33,
	.oobfree = {{0,1}, {8,8}, {24,8}, {40,8}, {56,8}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_type10 = {
	.oobsize = 72,
	.oobfree = {{0,18}, {32,18}, {64,18}, {96,18}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_type14 = {
	.oobsize = 20,
	.oobfree = {{0,4}, {4,4}, {36,4}, {68,4}, {100,4}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_type15 = {
	.oobsize = 36,
	.oobfree = {{0,4}, {4,8}, {20,8}, {36,8}, {52,8}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_type18 = {
	.oobsize = 96,
	.oobfree = {{0,24}, {32,24},  {64,24}, {96,24}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_type19 = {
	.oobsize = 44,
	.oobfree = {{0,4}, {8,40}}
};

struct spi_nand_flash_ooblayout ooblayout_etron_73E044SNA = {
	.oobsize = 144,
	.oobfree = {{0,18}, {32,18}, {64,18}, {96,18}, {128,18}, {160,18}, {192,18}, {224,18}}
};

struct spi_nand_flash_ooblayout ooblayout_toshiba_128 = {
	.oobsize = 64,
	.oobfree = {{0,64}}
};

struct spi_nand_flash_ooblayout ooblayout_toshiba_256 = {
	.oobsize = 128,
	.oobfree = {{0,128}}
};

struct spi_nand_flash_ooblayout ooblayout_micron = {
	.oobsize = 64,
	.oobfree = {{0,64}}
};

struct spi_nand_flash_ooblayout ooblayout_heyang = {
	.oobsize = 32,
	.oobfree = {{0,8}, {32,8}, {64,8}, {96,8}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_pn = {
	.oobsize = 44,
	.oobfree = {{0,4}, {4,2}, {19,2}, {34,2}, {49,2}, {96,32}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_ato = {
	.oobsize = 64,
	.oobfree = {{0,64}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_ato_25D2GA = {
	.oobsize = 12,
	.oobfree = {{0,3}, {16,3}, {32,3}, {48,3}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_ato_25D2GB = {
	.oobsize = 48,
	.oobfree = {{0,12}, {16,12}, {32,12}, {48,12}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_fm = {
	.oobsize = 64,
	.oobfree = {{0,64}}
};

/* only use user meta data with ECC protected */
struct spi_nand_flash_ooblayout ooblayout_fm_32 = {
	.oobsize = 32,
	.oobfree = {{0,32}}
};

struct spi_nand_flash_ooblayout ooblayout_spi_controller_ecc_64 = {
	.oobsize = 32,
	.oobfree = {{0,8}, {16,8}, {32,8}, {48,8}}
};

struct spi_nand_flash_ooblayout ooblayout_spi_controller_ecc_128 = {
	.oobsize = 96,
	.oobfree = {{0,8}, {16,8}, {32,8}, {48,8}, {64,64}}
};

struct spi_nand_flash_ooblayout ooblayout_spi_controller_ecc_256 = {
	.oobsize = 224,
	.oobfree = {{0,8}, {16,8}, {32,8}, {48,8}, {64,192}}
};

struct spi_nand_flash_ooblayout ooblayout_ds = {
	.oobsize = 20,
	.oobfree = {{0,8}, {20,4}, {36,4}, {52,4}}
};

struct spi_nand_flash_ooblayout ooblayout_fison = {
	.oobsize = 64,
	.oobfree = {{0,64}}
};

struct spi_nand_flash_ooblayout ooblayout_tym = {
	.oobsize = 12,
	.oobfree = {{0,3}, {16,3}, {32,3}, {48,3}}
};

/*****************************[ Notice]******************************/
/* If new spi nand chip have page size more than 4KB,  or oob size more than 256 bytes, than */
/* it will need to adjust the #define of _SPI_NAND_PAGE_SIZE and _SPI_NAND_OOB_SIZE */
/*****************************[ Notice]******************************/

static const struct SPI_NAND_FLASH_INFO_T spi_nand_flash_tables[] = {
	/* GIGADEVICE */
	{
		ptr_name:		"GD5F1GQ4UA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F1GQ4UAYIG,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_a,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F1GQ4UB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F1GQ4UBYIG,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_128,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F1GQ4UC",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F1GQ4UCYIG,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size: 		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode: 		SPI_NAND_FLASH_READ_DUMMY_BYTE_PREPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_128,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F1GQ4UE",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F1GQ4UEYIS,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_GD5FXGQ4U,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F2GQ4UB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F2GQ4UBYIG,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_128,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F2GQ4UE9IS",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F2GQ4UE9IS,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_GD5FXGQ4U,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F2GQ4UC",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F2GQ4UCYIG,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_PREPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_128,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F4GQ4UB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F4GQ4UBYIG,
		device_size:		_SPI_NAND_CHIP_SIZE_4GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_4KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_256BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_256KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_256,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"GD5F4GQ4UC",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_GIGADEVICE,
		dev_id:			_SPI_NAND_DEVICE_ID_GD5F4GQ4UCYIG,
		device_size:		_SPI_NAND_CHIP_SIZE_4GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_4KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_256BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_256KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_PREPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_gigadevice_256,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* ESMT */
	{
		ptr_name:		"F50L512",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ESMT,
		dev_id:			_SPI_NAND_DEVICE_ID_F50L512M41A,
		device_size:		_SPI_NAND_CHIP_SIZE_512MBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_esmt,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"F50L1G",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ESMT,
		dev_id:			_SPI_NAND_DEVICE_ID_F50L1G41A0,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_esmt,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"F50L1G41LB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ESMT,
		dev_id:			_SPI_NAND_DEVICE_ID_F50L1G41LB,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_esmt_41lb,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"F50L2G41LB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ESMT,
		dev_id:			_SPI_NAND_DEVICE_ID_F50L2G41LB,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_esmt_41lb,
		feature:		SPI_NAND_FLASH_DIE_SELECT_1_HAVE,
	},
	/* WINBOND */
	{
		ptr_name:		"W25N01G",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_WINBOND,
		dev_id:			_SPI_NAND_DEVICE_ID_W25N01GV,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_winbond,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"W25M02G",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_WINBOND,
		dev_id:			_SPI_NAND_DEVICE_ID_W25M02GV,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_winbond,
		feature:		SPI_NAND_FLASH_DIE_SELECT_1_HAVE,
	},
	/* MXIC */
	{
		ptr_name:		"MXIC35LF1G",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_MXIC,
		dev_id:			_SPI_NAND_DEVICE_ID_MXIC35LF1GE4AB,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_mxic,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"MXIC35LF2G",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_MXIC,
		dev_id:			_SPI_NAND_DEVICE_ID_MXIC35LF2GE4AB,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_mxic,
		feature:		SPI_NAND_FLASH_PLANE_SELECT_HAVE,
	},
	/* ZENTEL */
	{
		ptr_name:		"A5U12A21ASC",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ZENTEL,
		dev_id:			_SPI_NAND_DEVICE_ID_A5U12A21ASC,
		device_size:		_SPI_NAND_CHIP_SIZE_512MBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_zentel,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"A5U1GA21BWS",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ZENTEL,
		dev_id:			_SPI_NAND_DEVICE_ID_A5U1GA21BWS,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_zentel,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* ETRON */
	{
		ptr_name:		"EM73C044SNB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73C044SNB,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_etron_73C044SNB,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73C044SND",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73C044SND,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73C044SNF",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73C044SNF,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type10,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73C044VCA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73C044VCA,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout: 	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73C044VCD",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73C044VCD,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044SNA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044SNA,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_etron_73D044SNA,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044SNC",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044SNC,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_etron_73D044SNC,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044SND",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044SND,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044SNF",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044SNF,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type10,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044VCA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044VCA,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type18,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044VCB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044VCB,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044VCD",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044VCD,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type10,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044VCG",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044VCG,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73D044VCH",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73D044VCH,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"EM73E044SNA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ETRON,
		dev_id:			_SPI_NAND_DEVICE_ID_EM73E044SNA,
		device_size:		_SPI_NAND_CHIP_SIZE_4GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_4KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_256BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_256KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_etron_73E044SNA,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* TOSHIBA */
	{
		ptr_name:		"TC58CVG0S3H",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_TOSHIBA,
		dev_id:			_SPI_NAND_DEVICE_ID_TC58CVG0S3H,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_toshiba_128,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"TC58CVG1S3H",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_TOSHIBA,
		dev_id:			_SPI_NAND_DEVICE_ID_TC58CVG1S3H,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_toshiba_128,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"TC58CVG2S0H",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_TOSHIBA,
		dev_id:			_SPI_NAND_DEVICE_ID_TC58CVG2S0H,
		device_size:		_SPI_NAND_CHIP_SIZE_4GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_4KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_256KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_toshiba_256,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* MICRON */
	{
		ptr_name:		"MT29F1G01",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_MICRON,
		dev_id:			_SPI_NAND_DEVICE_ID_MT29F1G01,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_micron,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"MT29F2G01",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_MICRON,
		dev_id:			_SPI_NAND_DEVICE_ID_MT29F2G01,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size: 		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode: 		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_micron,
		feature:		SPI_NAND_FLASH_PLANE_SELECT_HAVE,
	},
	{
		ptr_name:		"MT29F4G01",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_MICRON,
		dev_id:			_SPI_NAND_DEVICE_ID_MT29F4G01,
		device_size:		_SPI_NAND_CHIP_SIZE_4GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_micron,
		feature:		SPI_NAND_FLASH_PLANE_SELECT_HAVE | SPI_NAND_FLASH_DIE_SELECT_2_HAVE,
	},
	/* HEYANG */
	{
		ptr_name:		"HYF1GQ4UAACAE",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_HEYANG,
		dev_id:			_SPI_NAND_DEVICE_ID_HYF1GQ4UAACAE,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_heyang,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"HYF2GQ4UAACAE",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_HEYANG,
		dev_id:			_SPI_NAND_DEVICE_ID_HYF2GQ4UAACAE,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_heyang,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"HYF2GQ4UHCCAE",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_HEYANG,
		dev_id:			_SPI_NAND_DEVICE_ID_HYF2GQ4UHCCAE,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type14,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"HYF1GQ4UDACAE",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_HEYANG,
		dev_id:			_SPI_NAND_DEVICE_ID_HYF1GQ4UDACAE,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"HYF2GQ4UDACAE",
		mfr_id: 		_SPI_NAND_MANUFACTURER_ID_HEYANG,
		dev_id: 		_SPI_NAND_DEVICE_ID_HYF2GQ4UDACAE,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size: 		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode: 		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* PN (Paragon) */
	{
		ptr_name:		"PN26G01AWSIUG",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_PN,
		dev_id:			_SPI_NAND_DEVICE_ID_PN26G01AWSIUG,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_pn,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"PN26G02AWSIUG",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_PN,
		dev_id:			_SPI_NAND_DEVICE_ID_PN26G02AWSIUG,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_pn,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* ATO */
	{
		ptr_name:		"ATO25D1GA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ATO,
		dev_id:			_SPI_NAND_DEVICE_ID_ATO25D1GA,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_SINGLE,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_ato,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"ATO25D2GA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ATO,
		dev_id:			_SPI_NAND_DEVICE_ID_ATO25D2GA,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_ato_25D2GA,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"ATO25D2GB",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_ATO_2,
		dev_id:			_SPI_NAND_DEVICE_ID_ATO25D2GB,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_ato_25D2GB,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* FM */
	{
		ptr_name:		"FM25S01",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FM,
		dev_id:			_SPI_NAND_DEVICE_ID_FM25S01,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_fm,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"FM25G01B",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FM,
		dev_id:			_SPI_NAND_DEVICE_ID_FM25G01B,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_fm,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"FM25G02B",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FM,
		dev_id:			_SPI_NAND_DEVICE_ID_FM25G02B,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_fm,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"FM25G02C",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FM,
		dev_id:			_SPI_NAND_DEVICE_ID_FM25G02C,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_fm_32,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"FM25G02",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FM,
		dev_id:			_SPI_NAND_DEVICE_ID_FM25G02,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* XTX */
	{
		ptr_name:		"XT26G02B",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_XTX,
		dev_id:			_SPI_NAND_DEVICE_ID_XT26G02B,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"XT26G01A",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_XTX,
		dev_id:			_SPI_NAND_DEVICE_ID_XT26G01A,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type19,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"XT26G02A",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_XTX,
		dev_id:			_SPI_NAND_DEVICE_ID_XT26G02A,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type19,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* Mira */
	{
		ptr_name:		"PSU1GS20BN",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_MIRA,
		dev_id:			_SPI_NAND_DEVICE_ID_PSU1GS20BN,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type6,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* BIWIN */
	{
		ptr_name:		"BWJX08U",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_BIWIN,
		dev_id:			_SPI_NAND_DEVICE_ID_BWJX08U,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type15,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"BWET08U",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_BIWIN,
		dev_id:			_SPI_NAND_DEVICE_ID_BWET08U,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type10,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* FORESEE */
	{
		ptr_name:		"FS35ND01GD1F1",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FORESEE,
		dev_id:			_SPI_NAND_DEVICE_ID_FS35ND01GD1F1,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"FS35ND01GS1F1",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FORESEE,
		dev_id:			_SPI_NAND_DEVICE_ID_FS35ND01GS1F1,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"FS35ND02GS2F1",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FORESEE,
		dev_id:			_SPI_NAND_DEVICE_ID_FS35ND02GS2F1,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"FS35ND02GD1F1",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FORESEE,
		dev_id:			_SPI_NAND_DEVICE_ID_FS35ND02GD1F1,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_128BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_type1,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* DS */
	{
		ptr_name:		"DS35Q2GA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_DS,
		dev_id:			_SPI_NAND_DEVICE_ID_DS35Q2GA,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_ds,
		feature:		SPI_NAND_FLASH_PLANE_SELECT_HAVE,
	},
	{
		ptr_name:		"DS35Q1GA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_DS,
		dev_id:			_SPI_NAND_DEVICE_ID_DS35Q1GA,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_ds,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* FISON */
	{
		ptr_name:		"CS11G0T0A0AA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FISON,
		dev_id:			_SPI_NAND_DEVICE_ID_CS11G0T0A0AA,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_fison,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"CS11G1T0A0AA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FISON,
		dev_id:			_SPI_NAND_DEVICE_ID_CS11G1T0A0AA,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_fison,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"CS11G0G0A0AA",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_FISON,
		dev_id:			_SPI_NAND_DEVICE_ID_CS11G0G0A0AA,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_fison,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	/* TYM */
	{
		ptr_name:		"TYM25D2GA01",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_TYM,
		dev_id:			_SPI_NAND_DEVICE_ID_TYM25D2GA01,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_tym,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"TYM25D2GA02",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_TYM,
		dev_id:			_SPI_NAND_DEVICE_ID_TYM25D2GA02,
		device_size:		_SPI_NAND_CHIP_SIZE_2GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_tym,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
	{
		ptr_name:		"TYM25D1GA03",
		mfr_id:			_SPI_NAND_MANUFACTURER_ID_TYM,
		dev_id:			_SPI_NAND_DEVICE_ID_TYM25D1GA03,
		device_size:		_SPI_NAND_CHIP_SIZE_1GBIT,
		page_size:		_SPI_NAND_PAGE_SIZE_2KBYTE,
		oob_size:		_SPI_NAND_OOB_SIZE_64BYTE,
		erase_size:		_SPI_NAND_BLOCK_SIZE_128KBYTE,
		dummy_mode:		SPI_NAND_FLASH_READ_DUMMY_BYTE_APPEND,
		read_mode:		SPI_NAND_FLASH_READ_SPEED_MODE_DUAL,
		write_mode:		SPI_NAND_FLASH_WRITE_SPEED_MODE_SINGLE,
		oob_free_layout:	&ooblayout_tym,
		feature:		SPI_NAND_FLASH_FEATURE_NONE,
	},
};

