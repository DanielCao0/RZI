/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <cmsis_core.h>

void arch_busy_wait(uint32_t time_us)
{
	uint64_t remaining_cycles = (uint64_t)time_us * (SystemCoreClock / 1000000U);

	/*
	 * The LPTIM-based system cycle counter may not remain monotonic while
	 * its interrupt is pending. Use the Cortex-M4 cycle counter for PM
	 * resume and MCUboot warm-reset delays.
	 */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	while (remaining_cycles > 0U) {
		uint32_t chunk =
			remaining_cycles > UINT32_MAX ? UINT32_MAX : (uint32_t)remaining_cycles;
		uint32_t start = DWT->CYCCNT;

		while ((uint32_t)(DWT->CYCCNT - start) < chunk) {
			__NOP();
		}
		remaining_cycles -= chunk;
	}
}
