/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Product sleep-policy coordinator.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_PM
#include <zephyr/pm/policy.h>
#endif

#ifdef CONFIG_POWEROFF
#include <zephyr/sys/poweroff.h>
#endif

#ifdef CONFIG_HWINFO
#include <zephyr/drivers/hwinfo.h>
#endif

#include <rzi/power/power.h>

enum power_constraint {
	POWER_CONSTRAINT_NONE = 0,
	POWER_CONSTRAINT_NO_DEEP,
	POWER_CONSTRAINT_ALL,
};

K_MUTEX_DEFINE(power_lock);

static bool initialized;
static enum rzi_power_policy policy;
static enum rzi_power_wake_reason wake_reason;
static enum power_constraint applied_constraint;
static uint8_t blocker_count[RZI_POWER_BLOCK_COUNT];
static int64_t wake_deadline_ms = -1;
static uint32_t wake_sources;

#ifdef CONFIG_PM
static struct pm_policy_event wake_event;
static bool wake_event_registered;
#endif

static uint32_t default_wake_sources(void)
{
#if defined(CONFIG_SOC_SERIES_NRF52X)
	return RZI_POWER_WAKE_TIMER | RZI_POWER_WAKE_GPIO | RZI_POWER_WAKE_RADIO |
	       RZI_POWER_WAKE_USB;
#elif defined(CONFIG_SOC_SERIES_STM32WLX)
	return RZI_POWER_WAKE_TIMER | RZI_POWER_WAKE_GPIO | RZI_POWER_WAKE_RADIO |
	       RZI_POWER_WAKE_UART;
#else
	return RZI_POWER_WAKE_TIMER | RZI_POWER_WAKE_GPIO;
#endif
}

static enum rzi_power_policy default_policy(void)
{
	if (IS_ENABLED(CONFIG_RZI_POWER_POLICY_DEFAULT_RUN)) {
		return RZI_POWER_POLICY_RUN;
	}
	if (IS_ENABLED(CONFIG_RZI_POWER_POLICY_DEFAULT_SUSPEND)) {
		return RZI_POWER_POLICY_SUSPEND;
	}
	if (IS_ENABLED(CONFIG_RZI_POWER_POLICY_DEFAULT_SHUTDOWN)) {
		return RZI_POWER_POLICY_SHUTDOWN;
	}
	return RZI_POWER_POLICY_IDLE;
}

static void capture_wake_reason(void)
{
#ifdef CONFIG_HWINFO
	uint32_t cause = 0U;

	if (hwinfo_get_reset_cause(&cause) == 0 && cause != 0U) {
		if ((cause & (RESET_POR | RESET_BROWNOUT)) != 0U) {
			wake_reason = RZI_POWER_WAKE_REASON_POWER_ON;
		} else if ((cause & RESET_LOW_POWER_WAKE) != 0U) {
			wake_reason = RZI_POWER_WAKE_REASON_LOW_POWER;
		} else if ((cause & RESET_PIN) != 0U) {
			wake_reason = RZI_POWER_WAKE_REASON_PIN;
		} else if ((cause & (RESET_SOFTWARE | RESET_BOOTLOADER | RESET_USER)) != 0U) {
			wake_reason = RZI_POWER_WAKE_REASON_SOFTWARE;
		} else if ((cause & RESET_WATCHDOG) != 0U) {
			wake_reason = RZI_POWER_WAKE_REASON_WATCHDOG;
		} else if ((cause & (RESET_DEBUG | RESET_CPU_LOCKUP)) != 0U) {
			wake_reason = RZI_POWER_WAKE_REASON_DEBUG;
		} else {
			wake_reason = RZI_POWER_WAKE_REASON_UNKNOWN;
		}
		(void)hwinfo_clear_reset_cause();
		return;
	}
#endif
	wake_reason = RZI_POWER_WAKE_REASON_UNKNOWN;
}

static bool any_blocker_locked(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(blocker_count); ++i) {
		if (blocker_count[i] != 0U) {
			return true;
		}
	}
	return false;
}

static uint32_t blocker_mask_locked(void)
{
	uint32_t mask = 0U;

	for (size_t i = 0; i < ARRAY_SIZE(blocker_count); ++i) {
		if (blocker_count[i] != 0U) {
			mask |= BIT(i);
		}
	}
	return mask;
}

#ifdef CONFIG_PM
static void acquire_constraint(enum power_constraint constraint)
{
	switch (constraint) {
	case POWER_CONSTRAINT_NONE:
		break;
	case POWER_CONSTRAINT_NO_DEEP:
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_DISK, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_SOFT_OFF, PM_ALL_SUBSTATES);
		break;
	case POWER_CONSTRAINT_ALL:
		pm_policy_state_all_lock_get();
		break;
	}
}

static void release_constraint(enum power_constraint constraint)
{
	switch (constraint) {
	case POWER_CONSTRAINT_NONE:
		break;
	case POWER_CONSTRAINT_NO_DEEP:
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
		pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_DISK, PM_ALL_SUBSTATES);
		pm_policy_state_lock_put(PM_STATE_SOFT_OFF, PM_ALL_SUBSTATES);
		break;
	case POWER_CONSTRAINT_ALL:
		pm_policy_state_all_lock_put();
		break;
	}
}
#else
static void acquire_constraint(enum power_constraint constraint)
{
	ARG_UNUSED(constraint);
}

static void release_constraint(enum power_constraint constraint)
{
	ARG_UNUSED(constraint);
}
#endif

static enum power_constraint wanted_constraint_locked(void)
{
	if (policy == RZI_POWER_POLICY_RUN || policy == RZI_POWER_POLICY_SHUTDOWN) {
		return POWER_CONSTRAINT_ALL;
	}
	if (any_blocker_locked() || policy == RZI_POWER_POLICY_IDLE) {
		return POWER_CONSTRAINT_NO_DEEP;
	}
	return POWER_CONSTRAINT_NONE;
}

static void refresh_constraint_locked(void)
{
	enum power_constraint wanted = wanted_constraint_locked();

	if (wanted == applied_constraint) {
		return;
	}
	release_constraint(applied_constraint);
	acquire_constraint(wanted);
	applied_constraint = wanted;
}

#ifdef CONFIG_PM
static void refresh_wake_event_locked(void)
{
	if (wake_deadline_ms < 0) {
		if (wake_event_registered) {
			pm_policy_event_unregister(&wake_event);
			wake_event_registered = false;
		}
		return;
	}

	int64_t ticks = k_ms_to_ticks_ceil64((uint64_t)wake_deadline_ms);

	if (wake_event_registered) {
		pm_policy_event_update(&wake_event, ticks);
	} else {
		pm_policy_event_register(&wake_event, ticks);
		wake_event_registered = true;
	}
}
#else
static void refresh_wake_event_locked(void)
{
}
#endif

static bool shutdown_wake_supported_locked(void)
{
#if defined(CONFIG_SOC_SERIES_NRF52X)
	return (wake_sources & (RZI_POWER_WAKE_GPIO | RZI_POWER_WAKE_USB)) != 0U;
#elif defined(CONFIG_SOC_SERIES_STM32WLX)
	return (wake_sources &
		(RZI_POWER_WAKE_GPIO | RZI_POWER_WAKE_TIMER | RZI_POWER_WAKE_RADIO)) != 0U;
#else
	return (wake_sources & RZI_POWER_WAKE_GPIO) != 0U;
#endif
}

static int ensure_ready(void)
{
	if (initialized) {
		refresh_constraint_locked();
		return 0;
	}

	policy = default_policy();
	wake_sources = default_wake_sources();
	wake_deadline_ms = -1;
	applied_constraint = POWER_CONSTRAINT_NONE;
	capture_wake_reason();
	initialized = true;
	refresh_constraint_locked();
	return 0;
}

int rzi_power_init(void)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_set_policy(enum rzi_power_policy next)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if ((unsigned int)next > RZI_POWER_POLICY_SHUTDOWN) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		policy = next;
		refresh_constraint_locked();
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_get_policy(enum rzi_power_policy *out_policy)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (out_policy == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		*out_policy = policy;
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_block(enum rzi_power_blocker blocker)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if ((unsigned int)blocker >= RZI_POWER_BLOCK_COUNT) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		if (blocker_count[blocker] == UINT8_MAX) {
			rc = -ENOMEM;
		} else {
			blocker_count[blocker]++;
			refresh_constraint_locked();
		}
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_unblock(enum rzi_power_blocker blocker)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if ((unsigned int)blocker >= RZI_POWER_BLOCK_COUNT) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		if (blocker_count[blocker] == 0U) {
			rc = -EINVAL;
		} else {
			blocker_count[blocker]--;
			refresh_constraint_locked();
		}
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_get_blockers(uint32_t *blockers)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (blockers == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		*blockers = blocker_mask_locked();
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

bool rzi_power_is_sleep_allowed(void)
{
	bool allowed = false;

	if (k_is_in_isr()) {
		return false;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	if (ensure_ready() == 0) {
		allowed = (policy == RZI_POWER_POLICY_IDLE || policy == RZI_POWER_POLICY_SUSPEND) &&
			  !any_blocker_locked();
	}
	k_mutex_unlock(&power_lock);
	return allowed;
}

int rzi_power_set_wake_deadline(int64_t uptime_ms)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		wake_deadline_ms = uptime_ms;
		refresh_wake_event_locked();
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_get_wake_deadline(int64_t *uptime_ms)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (uptime_ms == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		*uptime_ms = wake_deadline_ms;
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_set_wake_sources(uint32_t sources)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		wake_sources = sources == 0U ? default_wake_sources() : sources;
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_get_wake_sources(uint32_t *sources)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (sources == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		*sources = wake_sources;
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_get_wake_reason(enum rzi_power_wake_reason *reason)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}
	if (reason == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		*reason = wake_reason;
	}
	k_mutex_unlock(&power_lock);
	return rc;
}

int rzi_power_sleep(int32_t timeout_ms)
{
	int64_t deadline_ms;
	int64_t remaining_ms;
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	deadline_ms = wake_deadline_ms;
	k_mutex_unlock(&power_lock);
	if (rc != 0) {
		return rc;
	}

	remaining_ms = timeout_ms < 0 ? INT64_MAX : (int64_t)timeout_ms;

	if (deadline_ms >= 0) {
		int64_t until_deadline = deadline_ms - k_uptime_get();

		if (until_deadline <= 0) {
			return 0;
		}
		if (until_deadline < remaining_ms) {
			remaining_ms = until_deadline;
		}
	}

	if (remaining_ms == INT64_MAX) {
		k_sleep(K_FOREVER);
	} else if (remaining_ms > 0) {
		k_sleep(K_MSEC((int32_t)MIN(remaining_ms, INT32_MAX)));
	}
	return 0;
}

int rzi_power_shutdown(void)
{
	int rc;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&power_lock, K_FOREVER);
	rc = ensure_ready();
	if (rc == 0) {
		if (policy != RZI_POWER_POLICY_SHUTDOWN || any_blocker_locked()) {
			rc = -EBUSY;
		} else if (!IS_ENABLED(CONFIG_POWEROFF) || !shutdown_wake_supported_locked()) {
			rc = -ENOTSUP;
		}
	}
	k_mutex_unlock(&power_lock);
	if (rc != 0) {
		return rc;
	}

#ifdef CONFIG_POWEROFF
	sys_poweroff();
#endif
	return -ENOTSUP;
}
