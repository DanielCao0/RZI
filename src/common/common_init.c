/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Unified initialization for compiled-in RZI services.
 */

#include <zephyr/kernel.h>

#include <rzi/err.h>
#include <rzi/init.h>

#if defined(CONFIG_RZI_STORAGE)
#include <rzi/storage/storage.h>
#endif

int rzi_init(void)
{
	int rc = 0;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}

#if defined(CONFIG_RZI_STORAGE)
	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
#endif

	return rc;
}
