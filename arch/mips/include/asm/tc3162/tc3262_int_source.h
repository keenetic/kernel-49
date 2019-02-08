/************************************************************************
 *
 *	Copyright (C) 2006 Trendchip Technologies, Corp.
 *	All Rights Reserved.
 *
 * Trendchip Confidential; Need to Know only.
 * Protected as an unpublished work.
 *
 * The computer program listings, specifications and documentation
 * herein are the property of Trendchip Technologies, Co. and shall
 * not be reproduced, copied, disclosed, or used in whole or in part
 * for any reason without the prior express written permission of
 * Trendchip Technologeis, Co.
 *
 *************************************************************************/

#ifndef _TC3262_INT_SOURCE_H_
#define _TC3262_INT_SOURCE_H_

enum interrupt_source
{
#if defined(CONFIG_MIPS_TC3262_1004K)
	/* intrName		    irqNum  Source  fullName        */
	DUMMY=0,		/*  n/a     n/a     dummy           */
	UART5_INT,		/*  1       46      UART5           */
	UART4_INT,		/*  2       45      UART4           */
	UART3_INT,		/*  3       38      UART3           */
	UART2_INT,		/*  4       16      UART2           */
	UART_INT,		/*  5       2       UART            */
	GPIO_INT,		/*  6       10      GPIO            */
	GDMA_INTR,		/*  7       14      GDMA            */
	CRYPTO_INT,		/*  8       28      Crypto engine   */
	USB_HOST_2,		/*  9       48      USB host 2 (port1) */
	IRQ_RT3XXX_USB,		/*  10      17      USB host (port0)*/
	HSDMA_INTR,		/*  11      47      High Speed DMA  */
	WDMA1_WOE_INTR,		/*  12      58      WIFI DMA 1 for WOE */
	WDMA1_P1_INTR,		/*  13      57      WIFI DMA 1 port 1 */
	WDMA1_P0_INTR,		/*  14      56      WIFI DMA 1 port 0 */
	WDMA0_WOE_INTR,		/*  15      55      WIFI DMA 1 for WOE */
	WDMA0_P1_INTR,		/*  16      54      WIFI DMA 1 port 1 */
	WDMA0_P0_INTR,		/*  17      53      WIFI DMA 1 port 0 */
	WOE1_INTR,		/*  18      52      WIFI Offload Engine 1 */
	WOE0_INTR,		/*  19      51      WIFI Offload Engine 0 */
	PCIE_A_INT,		/*  20      24      PCIE port 1     */
	PCIE_0_INT,		/*  21      23      PCIE port 0     */
	MAC1_INT,		/*  22      15      Giga Switch     */
	XSI_PHY_INTR,		/*  23      50      XFI/HGSMII PHY interface */
	XSI_MAC_INTR,		/*  24      49      XFI/HGSMII MAC interface */
	QDMA_LAN3_INTR,		/*  25      41      QDMA LAN 3      */
	QDMA_LAN2_INTR,		/*  26      40      QDMA LAN 2      */
	QDMA_LAN1_INTR,		/*  27      39      QDMA LAN 1      */
	QDMA_LAN0_INTR,		/*  28      21      QDMA LAN 0      */
	QDMA_WAN3_INTR,		/*  29      44      QDMA WAN 3      */
	QDMA_WAN2_INTR,		/*  30      43      QDMA WAN 2      */
	QDMA_WAN1_INTR,		/*  31      42      QDMA WAN 1      */
	QDMA_WAN0_INTR,		/*  32      22      QDMA WAN 0      */
	TIMER2_INT,		/*  33      6       timer 2         */
	TIMER1_INT,		/*  34      5       timer 1         */
	TIMER0_INT,		/*  35      4       timer 0         */
	PCM2_INT,		/*  36      32      PCM 2           */
	PCM1_INT,		/*  37      11      PCM 1           */
	XPON_PHY_INTR,		/*  38      27      XPON PHY        */
	XPON_MAC_INTR,		/*  39      26      XPON MAC        */
	DMT_INT,		/*  40      19      xDSL DMT        */
	DYINGGASP_INT,		/*  41      18      Dying gasp      */
	CPU_CM_PCINT,		/*  42      1       CPU CM Perf Cnt overflow */
	CPU_CM_ERR,		/*  43      0       CPU Coherence Manager Error */
	FE_ERR_INTR,		/*  44      33      Frame Engine Error */
	EFUSE_ERR1_INTR,	/*  45      60      efuse error for prev action not finished */
	EFUSE_ERR0_INTR,	/*  46      59      efuse error for not setting key */
	AUTO_MANUAL_INT,	/*  47      35      SPI Controller Error */
	PCIE_SERR_INT,		/*  48      25      PCIE error      */
	DRAM_PROTECTION,	/*  49      3       dram illegal access*/
	BUS_TOUT_INT,		/*  50      31      Pbus timeout    */
	TIMER5_INT,		/*  51      9       timer 3 (watchdog) */
	SI_TIMER_INT,		/*  52  30/29/37/36 external CPU timers 0/1/2/3 */
	GIC_EDGE_NMI,		/*  53      20      send NMI to CPU by gic edge register */
	RESVINT1,		/*  54      n/a     n/a             */
	RESVINT2,		/*  55      n/a     n/a             */
	IPI_RESCHED_INT0,	/*  56      7       ipi resched 0   */
	IPI_RESCHED_INT1,	/*  57      8       ipi resched 1   */
	IPI_RESCHED_INT2,	/*  58      12      ipi resched 2   */
	IPI_RESCHED_INT3,	/*  59      13      ipi resched 3   */
	IPI_CALL_INT0,		/*  60      34      ipi call 0      */
	IPI_CALL_INT1,		/*  61      61      ipi call 0      */
	IPI_CALL_INT2,		/*  62      62      ipi call 0      */
	IPI_CALL_INT3,		/*  63      63      ipi call 0      */
#elif defined(CONFIG_MIPS_TC3262_34K)
	DUMMY_INT,
	UART_INT,		//0 	IPL10
	PTM_B0_INT,		//1
	SI_SWINT1_INT0,		//2
	SI_SWINT1_INT1,		//3
	TIMER0_INT,		//4 	IPL1
	TIMER1_INT,		//5 	IPL5
	TIMER2_INT,		//6 	IPL6
	SI_SWINT_INT0, 		//7
	SI_SWINT_INT1,		//8
	TIMER5_INT, 		//9 	IPL9
	GPIO_INT,		//10	IPL11
	PCM_INT,		//11	IPL20
	SI_PC1_INT,		//12
	SI_PC_INT, 		//13
	APB_DMA0_INT,		//14	IPL12
	ESW_INT,		//15	IPL13
	HSUART_INT,		//16	IPL23
	IRQ_RT3XXX_USB,		//17	IPL24
	DYINGGASP_INT,		//18	IPL25
	DMT_INT,		//19	IPL26
	UNUSED0_INT,		//20
	FE_MAC_INT,		//21	IPL3
	SAR_INT,		//22	IPL2
	PCIE1_INT,		//23
	PCIE_INT,		//24
	NFC_INT,		//25
	UNUSED1_INT,		//26	IPL15
	SPI_MC_INT,		//27	IPL16
	CRYPTO_INT,		//28	IPL17
	SI_TIMER1_INT,		//29
	SI_TIMER_INT,		//30
	SWR_INT,		//31	IPL4
	BUS_TOUT_INT,		//32
	RESERVE_A_INT,		//33
	RESERVE_B_INT,		//34
	RESERVE_C_INT,		//35
	AUTO_MANUAL_INT		//36
#endif
};

#endif /* _TC3262_INT_SOURCE_H_ */
