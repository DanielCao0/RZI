/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <rzi/capabilities.h>
#include <rzi/power/power.h>

ZTEST(rzi_power_api, test_init_capability_and_default_policy)
{
	enum rzi_power_policy policy = RZI_POWER_POLICY_SHUTDOWN;
	enum rzi_power_wake_reason reason = RZI_POWER_WAKE_REASON_DEBUG;
	uint32_t sources = 0U;
	uint32_t blockers = 0xFFFFFFFFU;

	zassert_ok(rzi_power_init());
	zassert_ok(rzi_power_init());
	zassert_equal(rzi_get_capabilities() & RZI_CAP_POWER, RZI_CAP_POWER);
	zassert_ok(rzi_power_get_policy(&policy));
	zassert_equal(policy, RZI_POWER_POLICY_IDLE);
	zassert_true(rzi_power_is_sleep_allowed());
	zassert_ok(rzi_power_get_wake_reason(&reason));
	zassert_ok(rzi_power_get_wake_sources(&sources));
	zassert_true(sources != 0U);
	zassert_ok(rzi_power_get_blockers(&blockers));
	zassert_equal(blockers, 0U);
}

ZTEST(rzi_power_api, test_arguments_and_blockers)
{
	enum rzi_power_policy policy;
	uint32_t blockers = 0U;

	zassert_equal(rzi_power_get_policy(NULL), -EINVAL);
	zassert_equal(rzi_power_get_blockers(NULL), -EINVAL);
	zassert_equal(rzi_power_get_wake_deadline(NULL), -EINVAL);
	zassert_equal(rzi_power_get_wake_sources(NULL), -EINVAL);
	zassert_equal(rzi_power_get_wake_reason(NULL), -EINVAL);
	zassert_equal(rzi_power_set_policy((enum rzi_power_policy)4), -EINVAL);
	zassert_equal(rzi_power_block(RZI_POWER_BLOCK_COUNT), -EINVAL);
	zassert_equal(rzi_power_unblock(RZI_POWER_BLOCK_APP), -EINVAL);

	zassert_ok(rzi_power_block(RZI_POWER_BLOCK_APP));
	zassert_ok(rzi_power_block(RZI_POWER_BLOCK_TRANSPORT));
	zassert_false(rzi_power_is_sleep_allowed());
	zassert_ok(rzi_power_get_blockers(&blockers));
	zassert_equal(blockers, BIT(RZI_POWER_BLOCK_APP) | BIT(RZI_POWER_BLOCK_TRANSPORT));
	zassert_ok(rzi_power_unblock(RZI_POWER_BLOCK_TRANSPORT));
	zassert_ok(rzi_power_unblock(RZI_POWER_BLOCK_APP));
	zassert_true(rzi_power_is_sleep_allowed());

	zassert_ok(rzi_power_set_policy(RZI_POWER_POLICY_RUN));
	zassert_false(rzi_power_is_sleep_allowed());
	zassert_ok(rzi_power_set_policy(RZI_POWER_POLICY_SUSPEND));
	zassert_true(rzi_power_is_sleep_allowed());
	zassert_ok(rzi_power_get_policy(&policy));
	zassert_equal(policy, RZI_POWER_POLICY_SUSPEND);
	zassert_ok(rzi_power_set_policy(RZI_POWER_POLICY_IDLE));
}

ZTEST(rzi_power_api, test_deadline_sleep_and_shutdown_refused)
{
	int64_t deadline = 1;
	int64_t now;

	zassert_ok(rzi_power_set_wake_deadline(-1));
	zassert_ok(rzi_power_get_wake_deadline(&deadline));
	zassert_true(deadline < 0);
	now = k_uptime_get();
	zassert_ok(rzi_power_set_wake_deadline(now + 5));
	zassert_ok(rzi_power_sleep(1000));
	zassert_true(k_uptime_get() >= now);
	zassert_ok(rzi_power_set_wake_deadline(-1));
	zassert_ok(rzi_power_sleep(1));

	zassert_ok(rzi_power_set_policy(RZI_POWER_POLICY_IDLE));
	zassert_equal(rzi_power_shutdown(), -EBUSY);
	zassert_ok(rzi_power_set_policy(RZI_POWER_POLICY_SHUTDOWN));
	zassert_ok(rzi_power_block(RZI_POWER_BLOCK_APP));
	zassert_equal(rzi_power_shutdown(), -EBUSY);
	zassert_ok(rzi_power_unblock(RZI_POWER_BLOCK_APP));
	zassert_ok(rzi_power_set_policy(RZI_POWER_POLICY_IDLE));
}

ZTEST_SUITE(rzi_power_api, NULL, NULL, NULL, NULL, NULL);
