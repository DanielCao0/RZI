/*
 * RAK4631: nRF52840 runs from VDDH. After a full erase, UICR REGOUT0
 * defaults to 1.8 V. LEDs/USB may still work, but SX1262 GPIOs do not.
 * Write REGOUT0 to 3.3 V and reset so the new GPIO voltage takes effect.
 *
 * Official rak4631 board has no board.c; keep this in the app so west
 * update does not wipe it.
 */

#include <zephyr/init.h>
#include <hal/nrf_power.h>

void board_early_init_hook(void)
{
	if ((nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) &&
	    ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
	     (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))) {

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
			;
		}

		NRF_UICR->REGOUT0 =
			(NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
			(UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
			;
		}

		/* UICR writes take effect only after reset. */
		NVIC_SystemReset();
	}
}
