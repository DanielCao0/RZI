/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RZI SDK service capability implementation.
 */

#include <zephyr/sys/util.h>

#include <rzi/capabilities.h>

uint32_t rzi_get_capabilities(void)
{
	uint32_t capabilities = 0U;

	if (IS_ENABLED(CONFIG_RZI_LORAWAN)) {
		capabilities |= RZI_CAP_LORAWAN;
	}
	if (IS_ENABLED(CONFIG_RZI_AT)) {
		capabilities |= RZI_CAP_AT;
	}
	if (IS_ENABLED(CONFIG_RZI_STORAGE)) {
		capabilities |= RZI_CAP_STORAGE;
	}
	return capabilities;
}
