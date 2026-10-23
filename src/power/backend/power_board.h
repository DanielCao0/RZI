/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Board-specific low-power entry.
 *
 * One backend is compiled. Callers in power.c already checked the policy.
 */

#ifndef POWER_BOARD_H
#define POWER_BOARD_H

/**
 * @brief Board steps that are safe while the kernel keeps running.
 *
 * @retval 0 Retention sleep may start.
 * @retval negative Preparation failed.
 */
int power_board_prepare_retention(void);

/**
 * @brief Quiesce this SoC and enter power-off.
 *
 * Does not return on success.
 *
 * @retval negative Power-off is not available.
 */
int power_board_enter_off(void);

#endif /* POWER_BOARD_H */
