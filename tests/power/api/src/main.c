/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#ifdef CONFIG_PM
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#endif

#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <rzi/power/power.h>

ZTEST(rzi_power_api, test_default_native_pm)
{
	bool enabled;
	uint32_t lock_mask = 0xFFFFFFFFU;

	zassert_ok(rzi_power_get_pm_enabled(&enabled));
	zassert_equal(enabled, IS_ENABLED(CONFIG_PM));
	zassert_ok(rzi_power_get_lock_mask(&lock_mask));
	zassert_equal(lock_mask, 0U);
}

ZTEST(rzi_power_api, test_arguments_and_locks)
{
	uint32_t lock_mask = 0U;

	zassert_equal(rzi_power_get_pm_enabled(NULL), -RZI_ERR_INVALID);
	zassert_equal(rzi_power_get_lock_mask(NULL), -RZI_ERR_INVALID);
	zassert_equal(rzi_power_get_wake_deadline(NULL), -RZI_ERR_INVALID);
	zassert_equal(rzi_power_get_reset_cause(NULL), -RZI_ERR_INVALID);
	zassert_equal(rzi_power_lock_get(RZI_POWER_LOCK_COUNT), -RZI_ERR_INVALID);
	zassert_equal(rzi_power_lock_put(RZI_POWER_LOCK_APP), -RZI_ERR_INVALID);

	zassert_ok(rzi_power_lock_get(RZI_POWER_LOCK_APP));
	zassert_ok(rzi_power_lock_get(RZI_POWER_LOCK_TRANSPORT));
	zassert_ok(rzi_power_get_lock_mask(&lock_mask));
	zassert_equal(lock_mask, BIT(RZI_POWER_LOCK_APP) | BIT(RZI_POWER_LOCK_TRANSPORT));
	zassert_ok(rzi_power_lock_put(RZI_POWER_LOCK_TRANSPORT));
	zassert_ok(rzi_power_lock_put(RZI_POWER_LOCK_APP));
	zassert_ok(rzi_power_get_lock_mask(&lock_mask));
	zassert_equal(lock_mask, 0U);
}

ZTEST(rzi_power_api, test_nested_locks_and_overflow)
{
	uint32_t lock_mask;

	for (unsigned int i = 0; i < UINT8_MAX; ++i) {
		zassert_ok(rzi_power_lock_get(RZI_POWER_LOCK_APP));
	}
	zassert_equal(rzi_power_lock_get(RZI_POWER_LOCK_APP), -RZI_ERR_NO_RESOURCE);
	for (unsigned int i = 1; i < UINT8_MAX; ++i) {
		zassert_ok(rzi_power_lock_put(RZI_POWER_LOCK_APP));
	}
	zassert_ok(rzi_power_get_lock_mask(&lock_mask));
	zassert_equal(lock_mask, BIT(RZI_POWER_LOCK_APP));
	zassert_ok(rzi_power_lock_put(RZI_POWER_LOCK_APP));
	zassert_equal(rzi_power_lock_put(RZI_POWER_LOCK_APP), -RZI_ERR_INVALID);
	zassert_ok(rzi_power_get_lock_mask(&lock_mask));
	zassert_equal(lock_mask, 0U);
}

ZTEST(rzi_power_api, test_sleep_is_independent_of_pm_deadline)
{
	int64_t deadline_ms;

	if (IS_ENABLED(CONFIG_PM)) {
		zassert_ok(rzi_power_set_wake_deadline(0));
	} else {
		zassert_equal(rzi_power_set_wake_deadline(0), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_power_get_wake_deadline(&deadline_ms), -RZI_ERR_NOT_SUPPORTED);
	}
	int64_t start_ms = k_uptime_get();

	zassert_ok(rzi_power_sleep(20));
	zassert_true(k_uptime_get() - start_ms >= 20);
	if (IS_ENABLED(CONFIG_PM)) {
		zassert_ok(rzi_power_get_wake_deadline(&deadline_ms));
		zassert_equal(deadline_ms, 0);
		zassert_ok(rzi_power_set_wake_deadline(-1));
	}
}

static K_THREAD_STACK_DEFINE(sleep_stack, 2048);
static struct k_thread sleep_thread;
static int64_t requested_sleep_ms;
static int sleep_result;
K_SEM_DEFINE(sleep_done, 0, 1);

static void sleep_worker(void *unused1, void *unused2, void *unused3)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);
	sleep_result = rzi_power_sleep(requested_sleep_ms);
	k_sem_give(&sleep_done);
}

ZTEST(rzi_power_api, test_long_and_forever_waits_can_be_woken)
{
	const int64_t durations_ms[] = {(int64_t)INT32_MAX + 1000, -1};

	if (IS_ENABLED(CONFIG_PM)) {
		zassert_ok(rzi_power_set_wake_deadline(0));
	}
	for (size_t i = 0; i < ARRAY_SIZE(durations_ms); ++i) {
		requested_sleep_ms = durations_ms[i];
		k_sem_reset(&sleep_done);
		k_thread_create(&sleep_thread, sleep_stack, K_THREAD_STACK_SIZEOF(sleep_stack),
				sleep_worker, NULL, NULL, NULL, K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
		k_sleep(K_TICKS(1));
		zassert_equal(k_sem_take(&sleep_done, K_NO_WAIT), -EBUSY);
		if (requested_sleep_ms > 0) {
			k_ticks_t remaining_ticks = k_thread_timeout_remaining_ticks(&sleep_thread);

			zassert_true(remaining_ticks > k_ms_to_ticks_ceil64(INT32_MAX));
		}
		k_wakeup(&sleep_thread);
		zassert_ok(k_sem_take(&sleep_done, K_SECONDS(1)));
		zassert_ok(k_thread_join(&sleep_thread, K_SECONDS(1)));
		zassert_ok(sleep_result);
	}
	if (IS_ENABLED(CONFIG_PM)) {
		zassert_ok(rzi_power_set_wake_deadline(-1));
	}
}

ZTEST(rzi_power_api, test_reset_cause_read_and_explicit_clear)
{
	uint32_t cause;
#ifdef CONFIG_HWINFO
	uint32_t native_cause;

	zassert_ok(hwinfo_get_reset_cause(&native_cause));
	zassert_ok(rzi_power_get_reset_cause(&cause));
	zassert_equal(cause, native_cause);
	zassert_ok(hwinfo_get_reset_cause(&cause));
	zassert_equal(cause, native_cause);
	zassert_ok(rzi_power_clear_reset_cause());
	zassert_ok(rzi_power_get_reset_cause(&cause));
	zassert_equal(cause, 0U);
#else
	zassert_equal(rzi_power_get_reset_cause(&cause), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_power_clear_reset_cause(), -RZI_ERR_NOT_SUPPORTED);
#endif
}

#ifdef CONFIG_TEST_HWINFO_MOCK
extern uint32_t mock_reset_flags;
extern int mock_reset_error;

ZTEST(rzi_power_api, test_reset_flags_preserved_and_errors_propagated)
{
	uint32_t cause;
	bool enabled;

	mock_reset_flags = RESET_PIN | RESET_WATCHDOG | RESET_CLOCK;
	zassert_ok(rzi_power_get_reset_cause(&cause));
	zassert_equal(cause,
		      RZI_POWER_RESET_PIN | RZI_POWER_RESET_WATCHDOG | RZI_POWER_RESET_CLOCK);
	zassert_equal(mock_reset_flags, cause);
	zassert_ok(rzi_power_get_pm_enabled(&enabled));
	zassert_equal(mock_reset_flags, cause);
	mock_reset_error = -ENOSYS;
	zassert_equal(rzi_power_get_reset_cause(&cause), -RZI_ERR_NOT_SUPPORTED);
	zassert_equal(rzi_power_clear_reset_cause(), -RZI_ERR_NOT_SUPPORTED);
	mock_reset_error = 0;
	zassert_ok(rzi_power_clear_reset_cause());
	zassert_equal(mock_reset_flags, 0U);
}
#endif

ZTEST(rzi_power_api, test_runtime_pm_switch)
{
	bool enabled;

	if (IS_ENABLED(CONFIG_PM)) {
		zassert_ok(rzi_power_set_pm_enabled(false));
		zassert_ok(rzi_power_set_pm_enabled(false));
		zassert_ok(rzi_power_get_pm_enabled(&enabled));
		zassert_false(enabled);
		zassert_ok(rzi_power_set_pm_enabled(true));
		zassert_ok(rzi_power_set_pm_enabled(true));
		zassert_ok(rzi_power_get_pm_enabled(&enabled));
		zassert_true(enabled);
	} else {
		zassert_equal(rzi_power_set_pm_enabled(false), -RZI_ERR_NOT_SUPPORTED);
		zassert_equal(rzi_power_set_pm_enabled(true), -RZI_ERR_NOT_SUPPORTED);
		zassert_ok(rzi_power_get_pm_enabled(&enabled));
		zassert_false(enabled);
	}
}

ZTEST(rzi_power_api, test_shutdown_refused)
{
	/* native_sim has no power-off implementation. */
	BUILD_ASSERT(!IS_ENABLED(CONFIG_POWEROFF));
	zassert_ok(rzi_power_lock_get(RZI_POWER_LOCK_APP));
	zassert_equal(rzi_power_shutdown(), -RZI_ERR_BUSY);
	zassert_ok(rzi_power_lock_put(RZI_POWER_LOCK_APP));
	zassert_equal(rzi_power_shutdown(), -RZI_ERR_NOT_SUPPORTED);
}

#ifdef CONFIG_PM
/* The test DTS defines lockable states without assigning CPU sleep states. */
void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);
	zassert_unreachable("The test must not enter a hardware sleep state");
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);
	zassert_unreachable("The test must not exit a hardware sleep state");
}

ZTEST(rzi_power_api, test_native_pm_locks_are_independent)
{
	bool enabled;

	for (unsigned int i = 0; i < 3; ++i) {
		zassert_ok(rzi_power_set_pm_enabled(false));
		zassert_ok(rzi_power_set_pm_enabled(false));
		zassert_true(
			pm_policy_state_lock_is_active(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES));
		zassert_true(
			pm_policy_state_lock_is_active(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES));
		zassert_ok(rzi_power_set_pm_enabled(true));
		zassert_ok(rzi_power_set_pm_enabled(true));
		zassert_false(
			pm_policy_state_lock_is_active(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES));
	}

	/* Enabling PM releases only the runtime switch's reference. */
	zassert_ok(rzi_power_lock_get(RZI_POWER_LOCK_APP));
	zassert_ok(rzi_power_lock_get(RZI_POWER_LOCK_APP));
	zassert_ok(rzi_power_set_pm_enabled(false));
	zassert_ok(rzi_power_set_pm_enabled(true));
	zassert_ok(rzi_power_get_pm_enabled(&enabled));
	zassert_true(enabled);
	zassert_ok(rzi_power_lock_put(RZI_POWER_LOCK_APP));
	zassert_true(pm_policy_state_lock_is_active(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES));
	zassert_ok(rzi_power_lock_put(RZI_POWER_LOCK_APP));
	zassert_false(pm_policy_state_lock_is_active(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES));

	/* Native state/substate locks owned by other code survive toggling. */
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
	zassert_ok(rzi_power_set_pm_enabled(false));
	zassert_ok(rzi_power_set_pm_enabled(true));
	zassert_true(pm_policy_state_lock_is_active(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES));
	zassert_false(pm_policy_state_lock_is_active(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES));
	pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
	zassert_false(pm_policy_state_lock_is_active(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES));
}

ZTEST(rzi_power_api, test_pm_deadline_update_expiry_and_clear)
{
	int64_t deadline_ms = k_uptime_get() + 1000;
	int64_t first_ticks;

	zassert_ok(rzi_power_set_wake_deadline(-1));
	zassert_equal(pm_policy_next_event_ticks(), -1);
	zassert_ok(rzi_power_set_wake_deadline(deadline_ms));
	first_ticks = pm_policy_next_event_ticks();
	zassert_true(first_ticks > 0);
	zassert_ok(rzi_power_set_wake_deadline(deadline_ms + 1000));
	zassert_true(pm_policy_next_event_ticks() > first_ticks);

	zassert_ok(rzi_power_set_wake_deadline(k_uptime_get()));
	zassert_equal(pm_policy_next_event_ticks(), 0);
	zassert_ok(rzi_power_get_wake_deadline(&deadline_ms));
	zassert_equal(pm_policy_next_event_ticks(), 0);
	zassert_ok(rzi_power_set_wake_deadline(k_uptime_get() + 1000));
	zassert_true(pm_policy_next_event_ticks() > 0);
	zassert_ok(rzi_power_set_wake_deadline(-1));
	zassert_equal(pm_policy_next_event_ticks(), -1);
}
#endif

ZTEST_SUITE(rzi_power_api, NULL, NULL, NULL, NULL, NULL);
