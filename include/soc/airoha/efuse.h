#ifndef _EFUSE_H_
#define _EFUSE_H_

#if defined(CONFIG_MACH_AN7583)

void *airoha_efuse_load(void);
u32 airoha_efuse_get(const void *mem, u32 start_bit, u32 len);
void airoha_efuse_free(void *mem);

#define EFUSE_INVALID_VAL			0xffffffff

/* AN7583 efuse buffer offsets */
#define SDS_PCIe_efuse_valid_LSB		 31
#define SDS_PCIe_efuse_valid_MSB		 33
#define SDS_USB_efuse_valid_LSB			 34
#define SDS_USB_efuse_valid_MSB			 36
#define PON_XFI_efuse_valid_LSB			 37
#define PON_XFI_efuse_valid_MSB			 37
#define GPHY_efuse_valid_LSB			 38
#define GPHY_efuse_valid_MSB			 38
#define AVS_thermal_sensor_efuse_valid_LSB	 39
#define AVS_thermal_sensor_efuse_valid_MSB	 39

#define DA_QP_SSUSB_TX_TERM_SEL_LSB		 47 /*[1:0]*/
#define DA_QP_SSUSB_TX_TERM_SEL_MSB		 48 /*[1:0]*/
#define DA_QP_SSUSB_RX_IMP_SEL_LSB		 49 /*[4:0]*/
#define DA_QP_SSUSB_RX_IMP_SEL_MSB		 53 /*[4:0]*/

#define DA_QP_PCIE_TX_TERM_SEL_LSB		 67 /*[1:0]*/
#define DA_QP_PCIE_TX_TERM_SEL_MSB		 68 /*[1:0]*/
#define DA_QP_PCIE_RX_IMP_SEL_LSB		 69 /*[4:0]*/
#define DA_QP_PCIE_RX_IMP_SEL_MSB		 73 /*[4:0]*/

#define RG_USB20_P0_INTR_CTRL_LSB		 89 /*[4:0]*/
#define RG_USB20_P0_INTR_CTRL_MSB		 93 /*[4:0]*/
#define RG_USB20_P1_INTR_CTRL_LSB		 94 /*[4:0]*/
#define RG_USB20_P1_INTR_CTRL_MSB		 98 /*[4:0]*/

#define RG_PXP_PCIE_CMN_TRIM_LSB		103 /*[4:0]*/
#define RG_PXP_PCIE_CMN_TRIM_MSB		107 /*[4:0]*/
#define RG_PXP_PCIE_TX0_TERM_SEL_LSB		108 /*[1:0]*/
#define RG_PXP_PCIE_TX0_TERM_SEL_MSB		109 /*[1:0]*/
#define RG_PXP_PCIE_RX0_FE_50OHMS_SEL_LSB	110 /*[1:0]*/
#define RG_PXP_PCIE_RX0_FE_50OHMS_SEL_MSB	111 /*[1:0]*/
#define RG_PXP_PCIE_TX1_TERM_SEL_LSB		112 /*[1:0]*/
#define RG_PXP_PCIE_TX1_TERM_SEL_MSB		113 /*[1:0]*/
#define RG_PXP_PCIE_RX1_FE_50OHMS_SEL_LSB	114 /*[1:0]*/
#define RG_PXP_PCIE_RX1_FE_50OHMS_SEL_MSB	115 /*[1:0]*/

#define RG_PXP_XPON_CMN_TRIM_LSB		120 /*[4:0]*/
#define RG_PXP_XPON_CMN_TRIM_MSB		124 /*[4:0]*/
#define RG_PXP_XPON_TX_TERM_SEL_LSB		125 /*[1:0]*/
#define RG_PXP_XPON_TX_TERM_SEL_MSB		126 /*[1:0]*/
#define RG_PXP_XPON_RX_FE_50OHMS_SEL_LSB	127 /*[1:0]*/
#define RG_PXP_XPON_RX_FE_50OHMS_SEL_MSB	128 /*[1:0]*/

#define RG_PXP_XFI_CMN_TRIM_LSB			133 /*[4:0]*/
#define RG_PXP_XFI_CMN_TRIM_MSB			137 /*[4:0]*/
#define RG_PXP_XFI_TX_TERM_SEL_LSB		138 /*[1:0]*/
#define RG_PXP_XFI_TX_TERM_SEL_MSB		139 /*[1:0]*/
#define RG_PXP_XFI_RX_FE_50OHMS_SEL_LSB		140 /*[1:0]*/
#define RG_PXP_XFI_RX_FE_50OHMS_SEL_MSB		141 /*[1:0]*/

#define DA_TX_I2MPB_A_gbe_P0_LSB		149 /*[5:0]*/
#define DA_TX_I2MPB_A_gbe_P0_MSB		154 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P0_LSB		155 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P0_MSB		160 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P0_LSB		161 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P0_MSB		166 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P0_LSB		167 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P0_MSB		172 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P0_LSB		173 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P0_MSB		178 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P0_LSB		179 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P0_MSB		184 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P0_LSB		185 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P0_MSB		190 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P0_LSB		191 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P0_MSB		196 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P0_LSB		197 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P0_MSB		202 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P0_LSB		203 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P0_MSB		208 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P0_LSB		209 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P0_MSB		214 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P0_LSB		215 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P0_MSB		220 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P0_LSB		221 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P0_MSB		226 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P0_LSB		227 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P0_MSB		232 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P0_LSB		233 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P0_MSB		238 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P0_LSB		239 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P0_MSB		244 /*[5:0]*/
#define DA_R50OHM_RSEL_TX_A_P0_LSB		245 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_A_P0_MSB		251 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P0_LSB		252 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P0_MSB		258 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P0_LSB		259 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P0_MSB		265 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P0_LSB		266 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P0_MSB		272 /*[6:0]*/

#define DA_TX_I2MPB_A_gbe_P1_LSB		276 /*[5:0]*/
#define DA_TX_I2MPB_A_gbe_P1_MSB		281 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P1_LSB		282 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P1_MSB		287 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P1_LSB		288 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P1_MSB		293 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P1_LSB		294 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P1_MSB		299 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P1_LSB		300 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P1_MSB		305 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P1_LSB		306 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P1_MSB		311 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P1_LSB		312 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P1_MSB		317 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P1_LSB		318 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P1_MSB		323 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P1_LSB		324 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P1_MSB		329 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P1_LSB		330 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P1_MSB		335 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P1_LSB		336 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P1_MSB		341 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P1_LSB		342 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P1_MSB 		347 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P1_LSB		348 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P1_MSB		353 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P1_LSB		354 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P1_MSB		359 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P1_LSB		360 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P1_MSB		365 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P1_LSB		366 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P1_MSB		371 /*[5:0]*/
#define DA_R50OHM_RSEL_TX_A_P1_LSB		372 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_A_P1_MSB		378 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P1_LSB		379 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P1_MSB		385 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P1_LSB		386 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P1_MSB		392 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P1_LSB		393 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P1_MSB		399 /*[6:0]*/

#define RG_BG_RASEL_LSB				400 /*[2:0]*/
#define RG_BG_RASEL_MSB				402 /*[2:0]*/
#define RG_REXT_TRIM_LSB			403 /*[5:0]*/
#define RG_REXT_TRIM_MSB			408 /*[5:0]*/
#define DA_TX_PS_DRIR0_hbt_tbt_LSB		409 /*[3:0]*/
#define DA_TX_PS_DRIR0_hbt_tbt_MSB		412 /*[3:0]*/
#define DA_TX_PS_DRIR0_B_hbt_tbt_LSB		413 /*[3:0]*/
#define DA_TX_PS_DRIR0_B_hbt_tbt_MSB		416 /*[3:0]*/
#define DA_TX_PS_OP_hbt_LSB			417 /*[4:0]*/
#define DA_TX_PS_OP_hbt_MSB			421 /*[4:0]*/
#define DA_TX_PS_OP_B_hbt_LSB			422 /*[4:0]*/
#define DA_TX_PS_OP_B_hbt_MSB			426 /*[4:0]*/
#define DA_TX_PS_OP_tbt_LSB			427 /*[4:0]*/
#define DA_TX_PS_OP_tbt_MSB			431 /*[4:0]*/
#define DA_TX_PS_OP_B_tbt_LSB			432 /*[4:0]*/
#define DA_TX_PS_OP_B_tbt_MSB			436 /*[4:0]*/
#define DA_TX_RM2P_OP_LSB			437 /*[2:0]*/
#define DA_TX_RM2P_OP_MSB			439 /*[2:0]*/
#define DA_TX_RM2P_OP_B_LSB			440 /*[2:0]*/
#define DA_TX_RM2P_OP_B_MSB			442 /*[2:0]*/
#define DA_TX_CM1_OP_LSB			443 /*[3:0]*/
#define DA_TX_CM1_OP_MSB			446 /*[3:0]*/
#define RG_HVGA_ECHO_GAIN_LSB			447 /*[2:0]*/
#define RG_HVGA_ECHO_GAIN_MSB			449 /*[2:0]*/
#define RG_HVGA_ECHO_RES_LSB			450 /*[0:0]*/
#define RG_HVGA_ECHO_RES_MSB			450 /*[0:0]*/
#define DA_ADBUF_BIAS_gbe_LSB			451 /*[1:0]*/
#define DA_ADBUF_BIAS_gbe_MSB			452 /*[1:0]*/
#define DA_ADBUF_BIAS_lp_LSB			453 /*[1:0]*/
#define DA_ADBUF_BIAS_lp_MSB			454 /*[1:0]*/

#define DA_TX_I2MPB_A_gbe_P2_LSB		458 /*[5:0]*/
#define DA_TX_I2MPB_A_gbe_P2_MSB		463 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P2_LSB		464 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P2_MSB		469 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P2_LSB		470 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P2_MSB		475 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P2_LSB		476 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P2_MSB		481 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P2_LSB		482 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P2_MSB		487 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P2_LSB		488 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P2_MSB		493 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P2_LSB		494 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P2_MSB		499 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P2_LSB		500 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P2_MSB		505 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P2_LSB		506 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P2_MSB		511 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P2_LSB		512 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P2_MSB		517 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P2_LSB		518 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P2_MSB		523 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P2_LSB		524 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P2_MSB		529 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P2_LSB		530 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P2_MSB		535 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P2_LSB		536 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P2_MSB		541 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P2_LSB		542 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P2_MSB		547 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P2_LSB		548 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P2_MSB		553 /*[5:0]*/
#define DA_R50OHM_RSEL_TX_A_P2_LSB		554 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_A_P2_MSB		560 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P2_LSB		561 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P2_MSB		567 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P2_LSB		568 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P2_MSB		574 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P2_LSB		575 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P2_MSB		581 /*[6:0]*/

#define DA_TX_I2MPB_A_gbe_P3_LSB		585 /*[5:0]*/
#define DA_TX_I2MPB_A_gbe_P3_MSB		590 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P3_LSB		591 /*[5:0]*/
#define DA_TX_I2MPB_B_gbe_P3_MSB		596 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P3_LSB		597 /*[5:0]*/
#define DA_TX_I2MPB_C_gbe_P3_MSB		602 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P3_LSB		603 /*[5:0]*/
#define DA_TX_I2MPB_D_gbe_P3_MSB		608 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P3_LSB		609 /*[5:0]*/
#define DA_TX_I2MPB_A_tst_P3_MSB		614 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P3_LSB		615 /*[5:0]*/
#define DA_TX_I2MPB_B_tst_P3_MSB		620 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P3_LSB		621 /*[5:0]*/
#define DA_TX_I2MPB_C_tst_P3_MSB		626 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P3_LSB		627 /*[5:0]*/
#define DA_TX_I2MPB_D_tst_P3_MSB		632 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P3_LSB		633 /*[5:0]*/
#define DA_TX_I2MPB_A_hbt_P3_MSB		638 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P3_LSB		639 /*[5:0]*/
#define DA_TX_I2MPB_B_hbt_P3_MSB		644 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P3_LSB		645 /*[5:0]*/
#define DA_TX_I2MPB_A_tbt_P3_MSB		650 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P3_LSB		651 /*[5:0]*/
#define DA_TX_I2MPB_B_tbt_P3_MSB		656 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P3_LSB		657 /*[5:0]*/
#define DA_TX_AMP_OFFSET_A_P3_MSB		662 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P3_LSB		663 /*[5:0]*/
#define DA_TX_AMP_OFFSET_B_P3_MSB		668 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P3_LSB		669 /*[5:0]*/
#define DA_TX_AMP_OFFSET_C_P3_MSB		674 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P3_LSB		675 /*[5:0]*/
#define DA_TX_AMP_OFFSET_D_P3_MSB		680 /*[5:0]*/
#define DA_R50OHM_RSEL_TX_A_P3_LSB		681 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_A_P3_MSB		687 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P3_LSB		688 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_B_P3_MSB		694 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P3_LSB		695 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_C_P3_MSB		701 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P3_LSB		702 /*[6:0]*/
#define DA_R50OHM_RSEL_TX_D_P3_MSB		708 /*[6:0]*/

#define AVS_TADC_AVS_DIO_CP_LSB			745 /*[15:0]*/
#define AVS_TADC_AVS_DIO_CP_MSB			760 /*[15:0]*/
#define AVS_TADC_EXT_BJT_CP_LSB			761 /*[15:0]*/
#define AVS_TADC_EXT_BJT_CP_MSB			776 /*[15:0]*/
#define STAL_TADC_STAL_DIO_CP_LSB		777 /*[15:0]*/
#define STAL_TADC_STAL_DIO_CP_MSB		792 /*[15:0]*/
#define AVS_TADC_AVS_RES_CP_LSB			793 /*[15:0]*/
#define AVS_TADC_AVS_RES_CP_MSB			808 /*[15:0]*/
#define STAL_TADC_STAL_RES_CP_LSB		809 /*[15:0]*/
#define STAL_TADC_STAL_RES_CP_MSB		824 /*[15:0]*/

#define AVS_Voltage_AVS_Voltage_Cal_1_ADC_LSB	841 /*[15:0]*/
#define AVS_Voltage_AVS_Voltage_Cal_1_ADC_MSB	856 /*[15:0]*/
#define AVS_Voltage_AVS_Voltage_Cal_2_ADC_LSB	857 /*[15:0]*/
#define AVS_Voltage_AVS_Voltage_Cal_2_ADC_MSB	872 /*[15:0]*/

#endif

#endif /* _EFUSE_H_ */
