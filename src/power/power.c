/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Service PM locks and controls for Zephyr system PM.
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_POWEROFF
#include <zephyr/sys/poweroff.h>
#endif

#ifdef CONFIG_HWINFO
#include <zephyr/drivers/hwinfo.h>
#endif

#include <rzi/power/power.h>

/* Helpers ending in _locked require the caller to hold power_lock. */
K_MUTEX_DEFINE(power_lock);

/* One native all-state PM lock for the runtime switch, plus lock references. */
static bool is_pm_locked;
static uint8_t lock_counts[RZI_POWER_LOCK_COUNT];
static int64_t wake_deadline_ms = -1;

#ifdef CONFIG_PM
static struct pm_policy_event wake_event;
static bool is_wake_event_registered;
#endif

static uint32_t get_lock_mask_locked(void)
{
	uint32_t mask = 0U;

	for (size_t i = 0; i < ARRAY_SIZE(lock_counts); ++i) {
		if (lock_counts[i] != 0U) {
			mask |= BIT(i);
		}
	}
	return mask;
}

#ifdef CONFIG_PM
static void update_wake_event_locked(void)
{
	/* The owner clears or advances the event after handling the deadline. */
	if (wake_deadline_ms < 0) {
		if (is_wake_event_registered) {
			pm_policy_event_unregister(&wake_event);
			is_wake_event_registered = false;
		}
		return;
	}

	int64_t deadline_ticks = k_ms_to_ticks_ceil64((uint64_t)wake_deadline_ms);

	if (is_wake_event_registered) {
		pm_policy_event_update(&wake_event, deadline_ticks);
	} else {
		pm_policy_event_register(&wake_event, deadline_ticks);
		is_wake_event_registered = true;
	}
}
#else
static void update_wake_event_locked(void)
{
}
#endif

int rzi_power_set_pm_enabled(bool enabled)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (!IS_ENABLED(CONFIG_PM)) {
		return -RZI_ERR_NOT_SUPPORTED;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	if (!enabled && !is_pm_locked) {
		pm_policy_state_all_lock_get();
		is_pm_locked = true;
	} else if (enabled && is_pm_locked) {
		pm_policy_state_all_lock_put();
		is_pm_locked = false;
	}
	k_mutex_unlock(&power_lock);
	return 0;
}

int rzi_power_get_pm_enabled(bool *enabled)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	*enabled = IS_ENABLED(CONFIG_PM) && !is_pm_locked;
	k_mutex_unlock(&power_lock);
	return 0;
}

int rzi_power_lock_get(enum rzi_power_lock lock)
{
	int rc = 0;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if ((unsigned int)lock >= RZI_POWER_LOCK_COUNT) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	if (lock_counts[lock] == UINT8_MAX) {
		rc = -RZI_ERR_NO_RESOURCE;
	} else {
		pm_policy_state_all_lock_get();
		lock_counts[lock]++;
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_lock_put(enum rzi_power_lock lock)
{
	int rc = 0;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if ((unsigned int)lock >= RZI_POWER_LOCK_COUNT) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	if (lock_counts[lock] == 0U) {
		rc = -RZI_ERR_INVALID;
	} else {
		lock_counts[lock]--;
		pm_policy_state_all_lock_put();
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_get_lock_mask(uint32_t *lock_mask)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (lock_mask == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	*lock_mask = get_lock_mask_locked();
	k_mutex_unlock(&power_lock);
	return 0;
}

int rzi_power_set_wake_deadline(int64_t uptime_ms)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (!IS_ENABLED(CONFIG_PM)) {
		return -RZI_ERR_NOT_SUPPORTED;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	wake_deadline_ms = uptime_ms;
	update_wake_event_locked();
	k_mutex_unlock(&power_lock);
	return 0;
}

int rzi_power_get_wake_deadline(int64_t *uptime_ms)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (uptime_ms == NULL) {
		return -RZI_ERR_INVALID;
	}
	if (!IS_ENABLED(CONFIG_PM)) {
		return -RZI_ERR_NOT_SUPPORTED;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	*uptime_ms = wake_deadline_ms;
	k_mutex_unlock(&power_lock);
	return 0;
}

int rzi_power_get_reset_cause(uint32_t *cause)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
	if (cause == NULL) {
		return -RZI_ERR_INVALID;
	}
#ifdef CONFIG_HWINFO
	return rzi_err_from_errno(hwinfo_get_reset_cause(cause));
#else
	return -RZI_ERR_NOT_SUPPORTED;
#endif
}

int rzi_power_clear_reset_cause(void)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}
#ifdef CONFIG_HWINFO
	return rzi_err_from_errno(hwinfo_clear_reset_cause());
#else
	return -RZI_ERR_NOT_SUPPORTED;
#endif
}

int rzi_power_sleep(int64_t timeout_ms)
{
	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}

	k_sleep(timeout_ms < 0 ? K_FOREVER : K_MSEC(timeout_ms));
	return 0;
}

int rzi_power_shutdown(void)
{
	int rc = -RZI_ERR_NOT_SUPPORTED;

	if (k_is_in_isr()) {
		return -RZI_ERR_ISR;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	if (get_lock_mask_locked() != 0U) {
		rc = -RZI_ERR_BUSY;
	} else {
#ifdef CONFIG_POWEROFF
		/* Keep PM lock references stable until the non-returning power-off call. */
		sys_poweroff();
#endif
	}
	k_mutex_unlock(&power_lock);
	return rc;
}
