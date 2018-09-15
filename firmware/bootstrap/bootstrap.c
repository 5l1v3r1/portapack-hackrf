/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <lpc43xx_m4.h>
#include <nvic.h>

/* Bootstrap runs from SPIFI on the M4, immediately after the LPC43xx built-in
 * boot ROM runs.
 */

/* After boot ROM executes:
 * PLL1 is at 288MHz (IRC * 24)
 * IDIVB_CTRL = PLL1 / 9 = 32MHz
 * IDIVC_CTRL = PLL1 / 3 = 96MHz
 * BASE_SPIFI_CLK.CLK_SEL = IDIVB
 */

void configure_spifi(void) {
	/* Configure pins first, to enable SCK input buffer for feedback */

	/* Configure SPIFI pins for maximum I/O rate */
	const uint32_t scu_spifi_io =
		  (3 << 0)	/* Function 3 */
		| (0 << 3)	/* Disable pull-down */
		| (1 << 4)	/* Disable pull-up */
		| (1 << 5)	/* Fast slew rate */
		| (1 << 6)	/* Enable input buffer */
		| (1 << 7)	/* Disable input glitch filter */
		;
	LPC_SCU->SFSP3_3 = scu_spifi_io;
	LPC_SCU->SFSP3_4 = scu_spifi_io;
	LPC_SCU->SFSP3_5 = scu_spifi_io;
	LPC_SCU->SFSP3_6 = scu_spifi_io;
	LPC_SCU->SFSP3_7 = scu_spifi_io;
	LPC_SCU->SFSP3_8 = scu_spifi_io;

	/* Tweak SPIFI mode */
	LPC_SPIFI->CTRL =
		  (0xffff <<  0)	/* Timeout */
		| (0x1    << 16)	/* CS high time in "clocks - 1" */
		| (0      << 21)	/* 0: Attempt speculative prefetch on data accesses */
		| (0      << 22)	/* 0: No interrupt on command ended */
		| (0      << 23)	/* 0: SCK driven low after rising edge at which last bit of command is captured. Stays low while CS# is high. */
		| (0      << 27)	/* 0: Cache prefetching enabled */
		| (0      << 28)	/* 0: Quad protocol, IO3:0 */
		| (1      << 29)	/* 1: Read data sampled on falling edge of clock */
		| (1      << 30)	/* 1: Read data is sampled using feedback clock from SCK pin */
		| (0      << 31)	/* 0: DMA request disabled */
		;

	/* Throttle up the SPIFI interface to 102MHz (PLL1 / 2) */
	LPC_CGU->IDIVB_CTRL.word =
		  (0 <<  0)	/* PD */
		| (1 <<  2)	/* IDIV (/2) */
		| (1 << 11)	/* AUTOBLOCK */
		| (9 << 24)	/* PLL1 */
		;

	LPC_CGU->BASE_SPIFI_CLK.word =
		  ( 0 <<  0) /* PD */
		| ( 1 << 11) /* AUTOBLOCK */
		| (13 << 24) /* IDIVB */
		;
}

static void delay(const uint32_t count) {
	volatile uint32_t i = count;
	while(i--);
}

static void cpu_clock_to_max_speed() {
	LPC_CGU->BASE_M4_CLK.word =
		  (0 <<  0) /* PD */
		| (1 << 11) /* AUTOBLOCK */
		| (1 << 24) /* IRC */
		;

	LPC_CGU->XTAL_OSC_CTRL.word =
		  (0 <<  0) /* ENABLE */
		| (0 <<  1) /* BYPASS */
		| (0 <<  2) /* HF */
		;

	/* Delay >250us at 90-110MHz clock speed */
	delay(7000);

	LPC_CGU->PLL1_CTRL =
		  ( 0 <<  0) /* PD */
		| ( 0 <<  1) /* BYPASS */
		| ( 0 <<  6) /* FBSEL */
		| ( 0 <<  7) /* DIRECT */
		| ( 0 <<  8) /* PSEL */
		| ( 1 << 11) /* AUTOBLOCK */
		| ( 0 << 12) /* NSEL */
		| (16 << 16) /* MSEL: x 17 */
		| ( 6 << 24) /* CLK_SEL: Crystal */
		;

	while( (LPC_CGU->PLL1_STAT & (1 << 0)) == 0 );

	LPC_CGU->BASE_M4_CLK.word =
		  (0 <<  0) /* PD */
		| (1 << 11) /* AUTOBLOCK */
		| (9 << 24) /* PLL1 */
		;

	/* Delay >50us at 204MHz clock speed */
	delay(2600);

	LPC_CGU->PLL1_CTRL |= (1 << 7); /* DIRECT */
}

int main(void) {
	cpu_clock_to_max_speed();

#if 0
	/* Configure LEDs and make sure they're off to start */
	LPC_SCU->SFSP4_1  = (1 << 4) | 0;	/* GPIO2[1] */
	LPC_SCU->SFSP4_2  = (1 << 4) | 0;	/* GPIO2[2] */
	LPC_SCU->SFSP6_12 = (1 << 4) | 0;	/* GPIO2[8] */
	LPC_GPIO->CLR[2] = (1 << 8) | (1 << 2) | (1 << 1);
	LPC_GPIO->DIR[2] = (1 << 8) | (1 << 2) | (1 << 1);

	/* Indicate M4 is working */
	LPC_GPIO->SET[2] = (1 << 1);
#endif
	configure_spifi();

	/* NOTE: MEMMAP registers are ORed with the shadow address to create the
	 * actual address.
	 */
	LPC_CREG->M0APPMEMMAP = LPC_SPIFI_DATA_CACHED_BASE + 0x40000;

	/* Change M0APP_RST to 0 */
	LPC_RGU->RESET_CTRL[1] = 0;

	while(1) {
		__WFE();
	}
}

void SystemInit(void) {
}
