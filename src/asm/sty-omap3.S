/**
 * @file
 * @brief Sty utility
 *
 * FileName: src/asm/sty-omap3.S
 *
 * sty is a small setup header for disabling watchdog,
 * setting up atag and starting off zImage
 * it sets up two magic numbers for the final app to fill
 * up
 */
/*
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 * Nishanth Menon <nm@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include "tagger.h"

/* This is where ATAG and related info will finally reside. Risky? */
#define INTERIM_DATA_ADDRESS		0x80000100

/* OMAP3 Watchdog timer base and registers */
#define WDT_BASE			0x48314000
#define WSPR				0x48
#define WWPS				0x34

#define CONTROL_PADCONF_UARTx_RX_VAL    0x00
#define CONTROL_PADCONF_UARTx_TX_VAL	0x00
#define CONTROL_PADCONF_MASK		0xFF

#define UART_MODE			0x00
/* Soft reset values */
#define LCR_VAL_EFR_SOFT_RESET		0xBF
#define EFR_VAL_SOFT_RESET		0x10
#define LCR_VAL_IER_MCR_SOFT_RESET	0x80
#define IER_VAL_SOFT_RESET		0x00
#define MCR_VAL_SOFT_RESET		0x00
#define LCR_VAL_BREAKS_REMOVED		0x40
#define RESET_MODE_VAL			0x07

/* FIFO config values */
#define MCR_VAL_FIFO_CONFIG		0x40
#define TLR_VAL_FIFO_CONFIG		0x00
#define FCR_VAL_FIFO_CONFIG		0x05

/* Baudrate, data and stop config */
#define LCR_VAL_DLL_ACCESS		0x80
#define DATA_BITS			0x03 /* 8 data bits */
#define PARITY_BITS			0x00 /* None */
#define STOP_BITS			0x00 /* 1 stop bit */
#define DLL_VAL_BAUD			0x1A /* Value for baudrate of 115200 */
#define DHL_VAL_BAUD			0x00

#define UART_REG_RHR			0x00
#define UART_REG_THR			0x00
#define UART_REG_DLL			0x00
#define UART_REG_DLH			0x04
#define UART_REG_IER			0x04
#define UART_REG_EFR			0x08
#define UART_REG_IIR			0x08
#define UART_REG_FCR			0x08
#define UART_REG_LCR			0x0C
#define UART_REG_MCR			0x10
#define UART_REG_XON1			0x10
#define UART_REG_XON2			0x14
#define UART_REG_LSR			0x14
#define UART_REG_MSR			0x18
#define UART_REG_TCR			0x18
#define UART_REG_XOFF1			0x18
#define UART_REG_SPR			0x1C
#define UART_REG_TLR			0x1C
#define UART_REG_XOFF2			0x1C
#define UART_REG_MDR1			0x20
#define UART_REG_MDR2			0x24
#define UART_REG_SFLSR			0x28
#define UART_REG_TXFLL			0x28
#define UART_REG_TXFLH			0x2C
#define UART_REG_RESUME			0x2C
#define UART_REG_SFREGL			0x30
#define UART_REG_RXFLL			0x30
#define UART_REG_SFREGH			0x34
#define UART_REG_RXFLH			0x34
#define UART_REG_BLR			0x38
#define UART_REG_ACREG			0x3C
#define UART_DIV_1_6			0x3C
#define UART_REG_SCR			0x40
#define UART_REG_SSR			0x44
#define UART_REG_EBLR			0x48

sty_start:
	/*
	 * set the cpu to SVC32 mode
	 */
	mrs	r0,	cpsr
	bic	r0,	r0,	#0x1f
	orr	r0,	r0,	#0xd3
	msr	cpsr,	r0

	/* Do some initial cleanups */
	/* Disable watch dog timer */
	ldr	r0,	WD_BASE
	ldr	r1,	WD_AAAA
	str	r1,	[r0, #WSPR]
	nop

wdl1:
	ldr	r2,	[r0, #WWPS]
	cmp	r2,	#0
	bne	wdl1

	ldr	r1,	WD_5555
	str	r1,	[r0, #WSPR]
	nop

wdl2:
	ldr	r2,	[r0, #WWPS]
	cmp	r2,	#0
	bne	wdl2
	/*
	 * Invalidate L1 I/D
	 */
        mov	r0, #0                 /* set up for MCR */
        mcr	p15, 0, r0, c8, c7, 0  /* invalidate TLBs */
        mcr	p15, 0, r0, c7, c5, 1  /* invalidate icache */

	/* Invalide L2 cache (gp device call point)
	 * - warning, this may have issues on EMU/HS devices
	 * this call can corrupt r0-r5
	 */
	mov	r12,	#0x1	/* set up to invalide L2 */
smi: 	.word	0xE1600070	/* Call SMI monitor */

	/*
	 * disable MMU stuff and caches
	 */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x00002000	/* clear bits 13 (--V-) */
	bic	r0, r0, #0x00000007	/* clear bits 2:0 (-CAM) */
	orr	r0, r0, #0x00000002	/* set bit 1 (--A-) Align */
	mcr	p15, 0, r0, c1, c0, 0

	/* disable irq and fiq */
	mov	r0,	#0xC0
	mrs	r3,	cpsr
	orr	r2,	r3,	r0
	msr	cpsr_c,	r2


	/*
	 * UART Configuration
	 * XXX: Ideally, this should be handled by kernel, Not here!!!
	 */

	/* Enable the function clock for UART1 and UART2 */
	ldr	r0,	CM_FCLKEN1_CORE
	ldr	r1,	[r0]
	ldr	r2,	CM_FCLKEN1_CORE_VAL
	orr	r1,	r1,	r2
	str	r1,	[r0]
	ldr	r0,	CM_ICLKEN1_CORE
	ldr	r1,	[r0]
	ldr	r2,	CM_FCLKEN1_CORE_VAL
	orr	r1,	r1,	r2
	str	r1,	[r0]

	/* Enable uart 3 */
	ldr	r0,	CM_FCLKEN_PER
	ldr	r1,	[r0]
	ldr	r2,	CM_FCLKEN_PER_VAL
	orr	r1,	r1,	r2
	str	r1,	[r0]
	ldr	r0,	CM_ICLKEN_PER
	ldr	r1,	[r0]
	orr	r1,	r1,	r2
	str	r1,	[r0]

	/* MUX setup for UART Module */
	ldr	r0,	CONTROL_PADCONF_UARTx_RX
	ldrb	r1,	[r0]
	bic	r1,	r1,	#CONTROL_PADCONF_MASK
	mov	r2,	#CONTROL_PADCONF_UARTx_RX_VAL
	orr	r1,	r1,r2
	strb	r1,	[r0]

	ldr	r0,	CONTROL_PADCONF_UARTx_TX
	ldrb	r1,	[r0]
	bic	r1,	r1,	#CONTROL_PADCONF_MASK
	mov	r2,	#CONTROL_PADCONF_UARTx_TX_VAL
	orr	r1,	r1,r2
	strb	r1,	[r0]

	/* UART config - 1 and 3 - hoping all boards live */
	ldr	r2,	UART1_REG_BASE_ADDR
	bl	uart_config
	ldr	r2,	UART3_REG_BASE_ADDR
	bl	uart_config

	/* Finish off the rest of the configuration */
	b	start_linux

uart_config:
	/* Saving the LR value for return purpose */
	mov	r0,	lr
	/* UART soft reset */
	mov	r3,	#LCR_VAL_EFR_SOFT_RESET
	/* To access the EFR, writing the cmd to LCR */
	strb	r3,	[r2,	#UART_REG_LCR]
	mov	r3,	#EFR_VAL_SOFT_RESET
	strb	r3,	[r2,	#UART_REG_EFR]
	/* Load the contents of LCR into R4 */
	ldrb	r4,	[r2,	#UART_REG_LCR]
	bic	r4,	r4,	#LCR_VAL_IER_MCR_SOFT_RESET
	strb	r4,	[r2,	#UART_REG_LCR]
	mov	r3,	#IER_VAL_SOFT_RESET
	/* Disable the interrupts */
	strb	r3,	[r2,	#UART_REG_IER]

	mov	r3,	#MCR_VAL_SOFT_RESET
	/* Set force control signals inactive */
	strb	r3,	[r2,	#UART_REG_MCR]
	/* Getting the data from UART LCR register */
	ldrb	r4,	[r2,	#UART_REG_LCR]
	bic	r4,	r4,	#LCR_VAL_BREAKS_REMOVED
	strb	r4,	[r2,	#UART_REG_LCR]

	mov	r3,	#RESET_MODE_VAL
	/* Resetting the UART(by MDR1[2:0]=7) */
	strb	r3,	[r2,	#UART_REG_MDR1]

	/* UART FIFO config */
	ldrb	r3,	[r2,	#UART_REG_MCR]
	orr	r3,	r3,	#MCR_VAL_FIFO_CONFIG
	strb	r3,	[r2,	#UART_REG_MCR]

	mov	r3,	#TLR_VAL_FIFO_CONFIG
	strb	r3,	[r2,	#UART_REG_TLR]

	mov	r3,	#FCR_VAL_FIFO_CONFIG
	strb	r3,	[r2,	#UART_REG_FCR]

	/* Baud rate, data, stop config */
	/* Enable the DLL & DLH access*/
	mov	r3,	#LCR_VAL_DLL_ACCESS
	orr	r3,	r3,	#DATA_BITS
	orr	r3,	r3,	#PARITY_BITS
	orr	r3,	r3,	#STOP_BITS
	strb	r3,	[r2,	#UART_REG_LCR]

	mov	r3,	#DLL_VAL_BAUD
	strb	r3,	[r2,	#UART_REG_DLL]
	mov	r3,	#DHL_VAL_BAUD
	strb	r3,	[r2,	#UART_REG_DLH]

	ldrb	r4,	[r2,	#UART_REG_LCR]
	/* Disable the DLL & DLH access*/
	bic	r4,	r4,	#LCR_VAL_DLL_ACCESS
	strb	r4,	[r2,	#UART_REG_LCR]

	/* Set the UART MODE */
	mov	r3,	#UART_MODE
	/* Enable uart with 16x baudrate mode */
	strb	r3,	[r2,	#UART_REG_MDR1]
	/* return */
	mov	pc,	r0

CM_FCLKEN1_CORE:	.word	0x48004A00
CM_FCLKEN1_CORE_VAL:	.word	0x00006000
CM_ICLKEN1_CORE:	.word	0x48004A10
CM_FCLKEN_PER:		.word	0x48005000
CM_ICLKEN_PER:		.word	0x48005010
CM_FCLKEN_PER_VAL:	.word	0x00000800
/* UART specific baseaddress */
UART3_REG_BASE_ADDR:	.word	0x49020000
UART1_REG_BASE_ADDR:	.word	0x4806A000

/* UART1 */
CONTROL_PADCONF_UARTx_RX:	.word	0x48002182
CONTROL_PADCONF_UARTx_TX:	.word	0x4800217c

	/*********** end me config ****/
start_linux:
	/* atag is recommended in first 16Kb of memory- so relocate */
	adr	r0,	sty_end
	ldr	r1,	interim_data_size
	ldr	r3,	interim_data_store
	add	r0,	r0,	#4
	add	r1,	r1,	#8
copy_loop:
	ldr	r4,	[r0]
	str	r4,	[r3]
	sub	r1,	r1,	#4
	add	r0,	r0,	#4
	add	r3,	r3,	#4
	cmp	r1,	#8
	bgt	copy_loop


	/*
	 * Ref: http://www.arm.linux.org.uk/developer/booting.php
	 * before we jump to zImage, set
	 * r0 = 0.
	 * r1 = machine type number discovered in (3) above.
	 * r2 = physical address of tagged list in system RAM.
	 */
	ldr	r1, mach_id
	/*
	 * image is organized as follows:
	 *  sty- this code
	 *  atag
	 *  (optional)device tree data and other future stuff we want added
	 *  zImage
	 *
	 * interim_data_size =atag_size + additiona data like device_tree
	 * zImage location(or jump to location) = sty_end + interim_data_size
	 * atag data_location = sty_end + 4
	 */
	adr	r4,	sty_end
	ldr	r5,	interim_data_size

	/* r3 stores the zImage address to jump to when we are ready */
	add	r3,	r4,	r5
	add	r3,	r3,	#4

	/* atag is packed right behind this, so add a word */
	ldr	r2,	interim_data_store

	/* set r0 to 0 */
	mov	r0,	#0

	/*
	 * we are all set.. now jump and start praying ;)
	 * NOTE: we dont disable watchdog etc yet.. mebbe we should be doing
	 * it.. but if the kernel does not bootup, it is better that the
	 * watchdog kill us..
	 */
	bx	r3

interim_data_store:	.word	INTERIM_DATA_ADDRESS
WD_BASE:		.word	WDT_BASE
WD_AAAA:		.word	0xAAAA
WD_5555:		.word	0x5555

mach_id:		.word	MAGIC_MACHID
interim_data_size:	.word	MAGIC_INTERIMSIZE
sty_end:		.word	0x0
