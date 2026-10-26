/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Stored LoRaWAN credentials and join policy.
 */

#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"
#include "../lorawan_settings.h"

/** RUI3 NJM wire value: ABP is 0 and OTAA is 1. */
#define NJM_WIRE_ABP  0U
#define NJM_WIRE_OTAA 1U

struct credential_store {
	uint8_t dev_eui[8];
	bool dev_eui_set;
	uint8_t app_eui[8];
	bool app_eui_set;
	uint8_t app_key[16];
	bool app_key_set;
	uint8_t gen_app_key[16];
	bool gen_app_key_set;
	uint32_t dev_addr;
	bool dev_addr_set;
	uint8_t nwk_skey[16];
	bool nwk_skey_set;
	uint8_t app_skey[16];
	bool app_skey_set;
	enum rzi_lorawan_activation njm;
	bool auto_join;
	uint8_t join_interval;
	uint8_t join_attempts;
	uint16_t channel_mask[RZI_LORAWAN_CHANNEL_MASK_WORDS];
	bool channel_mask_set;
	bool join_active;
	bool loaded;
};

static struct credential_store credentials;

K_MUTEX_DEFINE(credential_lock);

static int persist(const char *key, const void *value, size_t len)
{
	return rzi_lorawan_setting_write(key, value, len);
}

static bool channel_mask_valid(const uint16_t *mask)
{
	bool any = false;

	for (uint32_t channel = 0; channel < RZI_LORAWAN_CHANNEL_MASK_WORDS * 16U; ++channel) {
		bool enabled = (mask[channel / 16U] & BIT(channel % 16U)) != 0U;

		if (enabled && channel >= RZI_LORAWAN_FIXED_PLAN_CHANNELS) {
			return false;
		}
		any = any || enabled;
	}
	return any;
}

static int load_bytes(const char *key, void *dst, size_t len, bool *valid)
{
	return rzi_lorawan_setting_read(key, dst, len, valid);
}

static int ensure_loaded_locked(void)
{
	struct credential_store candidate = {0};
	uint8_t mode;
	uint8_t policy[RZI_JOIN_POLICY_SIZE];
	bool found;
	int rc;

	if (credentials.loaded) {
		return 0;
	}
	/* Do not expose a partially loaded configuration if a later read fails. */
	rc = load_bytes("deveui", candidate.dev_eui, sizeof(candidate.dev_eui),
			&candidate.dev_eui_set);
	if (rc == 0) {
		rc = load_bytes("joineui", candidate.app_eui, sizeof(candidate.app_eui),
				&candidate.app_eui_set);
	}
	if (rc == 0) {
		rc = load_bytes("appkey", candidate.app_key, sizeof(candidate.app_key),
				&candidate.app_key_set);
	}
	if (rc == 0) {
		rc = load_bytes("genappkey", candidate.gen_app_key, sizeof(candidate.gen_app_key),
				&candidate.gen_app_key_set);
	}
	if (rc == 0) {
		rc = load_bytes("devaddr", &candidate.dev_addr, sizeof(candidate.dev_addr),
				&candidate.dev_addr_set);
	}
	if (rc == 0) {
		rc = load_bytes("nwkskey", candidate.nwk_skey, sizeof(candidate.nwk_skey),
				&candidate.nwk_skey_set);
	}
	if (rc == 0) {
		rc = load_bytes("appskey", candidate.app_skey, sizeof(candidate.app_skey),
				&candidate.app_skey_set);
	}
	if (rc == 0) {
		rc = load_bytes("njm", &mode, sizeof(mode), &found);
	}
	if (rc == 0) {
		candidate.njm = mode == NJM_WIRE_ABP ? RZI_LORAWAN_ACTIVATION_ABP
						     : RZI_LORAWAN_ACTIVATION_OTAA;
		rc = load_bytes("join_policy", policy, sizeof(policy), &found);
	}
	if (rc == 0) {
		candidate.auto_join = policy[RZI_JOIN_POLICY_AUTO] != 0U;
		candidate.join_interval = policy[RZI_JOIN_POLICY_INTERVAL];
		candidate.join_attempts = policy[RZI_JOIN_POLICY_ATTEMPTS];
		rc = load_bytes("chmask", candidate.channel_mask, sizeof(candidate.channel_mask),
				&candidate.channel_mask_set);
		if (rc == 0 && candidate.channel_mask_set &&
		    !channel_mask_valid(candidate.channel_mask)) {
			candidate.channel_mask_set = false;
		}
	}
	if (rc == 0) {
		candidate.loaded = true;
		candidate.join_active = credentials.join_active;
		credentials = candidate;
	}
	return rc;
}

static int set_bytes(uint8_t *dst, bool *valid, const char *key, const uint8_t *src, size_t len,
		     size_t expected)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (src == NULL || len != expected) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0 && credentials.join_active) {
		rc = -RZI_ERR_BUSY;
	}
	if (rc == 0) {
		rc = persist(key, src, expected);
		if (rc == 0) {
			memcpy(dst, src, expected);
			*valid = true;
		}
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

static int session_joined(bool *joined)
{
	*joined = false;
	if (rzi_lorawan_check_started() != 0) {
		return 0;
	}
	return rzi_lorawan_backend.base.is_joined(joined);
}

static const struct rzi_lorawan_session_ops *session_ops(void)
{
	return rzi_lorawan_backend.session;
}

static int read_otaa_dev_addr(uint32_t *dev_addr)
{
	const struct rzi_lorawan_session_ops *ops;
	bool joined = false;
	int rc = session_joined(&joined);

	if (rc != 0) {
		return rc;
	}
	if (!joined) {
		*dev_addr = 0U;
		return 0;
	}
	ops = session_ops();
	if (ops == NULL || ops->get_dev_addr == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return ops->get_dev_addr(dev_addr);
}

static int read_otaa_key(int (*read)(uint8_t *key), uint8_t *key, size_t len)
{
	bool joined = false;
	int rc = session_joined(&joined);

	if (rc != 0) {
		return rc;
	}
	if (!joined) {
		memset(key, 0, len);
		return 0;
	}
	if (read == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return read(key);
}

static int get_bytes(const uint8_t *src, const bool *valid, uint8_t *dst, size_t len,
		     size_t expected)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (dst == NULL || len != expected) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0 && !*valid) {
		rc = -RZI_ERR_NO_DATA;
	}
	if (rc == 0) {
		memcpy(dst, src, expected);
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

static uint8_t njm_wire(enum rzi_lorawan_activation mode)
{
	return mode == RZI_LORAWAN_ACTIVATION_ABP ? NJM_WIRE_ABP : NJM_WIRE_OTAA;
}

static bool activation_supported(enum rzi_lorawan_activation mode)
{
	uint32_t capabilities = rzi_lorawan_backend.capabilities;

	if (mode == RZI_LORAWAN_ACTIVATION_OTAA) {
		return (capabilities & RZI_LORAWAN_CAP_OTAA) != 0U;
	}
	if (mode == RZI_LORAWAN_ACTIVATION_ABP) {
		return (capabilities & RZI_LORAWAN_CAP_ABP) != 0U;
	}
	return false;
}

int rzi_lorawan_get_dev_eui(uint8_t *eui, size_t len)
{
	return get_bytes(credentials.dev_eui, &credentials.dev_eui_set, eui, len,
			 sizeof(credentials.dev_eui));
}

int rzi_lorawan_set_dev_eui(const uint8_t *eui, size_t len)
{
	return set_bytes(credentials.dev_eui, &credentials.dev_eui_set, "deveui", eui, len,
			 sizeof(credentials.dev_eui));
}

int rzi_lorawan_get_app_eui(uint8_t *eui, size_t len)
{
	return get_bytes(credentials.app_eui, &credentials.app_eui_set, eui, len,
			 sizeof(credentials.app_eui));
}

int rzi_lorawan_set_app_eui(const uint8_t *eui, size_t len)
{
	return set_bytes(credentials.app_eui, &credentials.app_eui_set, "joineui", eui, len,
			 sizeof(credentials.app_eui));
}

int rzi_lorawan_get_app_key(uint8_t *key, size_t len)
{
	return get_bytes(credentials.app_key, &credentials.app_key_set, key, len,
			 sizeof(credentials.app_key));
}

int rzi_lorawan_set_app_key(const uint8_t *key, size_t len)
{
	return set_bytes(credentials.app_key, &credentials.app_key_set, "appkey", key, len,
			 sizeof(credentials.app_key));
}

int rzi_lorawan_get_gen_app_key(uint8_t *key, size_t len)
{
	return get_bytes(credentials.gen_app_key, &credentials.gen_app_key_set, key, len,
			 sizeof(credentials.gen_app_key));
}

int rzi_lorawan_set_gen_app_key(const uint8_t *key, size_t len)
{
	return set_bytes(credentials.gen_app_key, &credentials.gen_app_key_set, "genappkey", key,
			 len, sizeof(credentials.gen_app_key));
}

int rzi_lorawan_get_dev_addr(uint32_t *dev_addr)
{
	enum rzi_lorawan_activation mode;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (dev_addr == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	mode = credentials.njm;
	if (rc == 0 && mode == RZI_LORAWAN_ACTIVATION_ABP) {
		if (!credentials.dev_addr_set) {
			rc = -RZI_ERR_NO_DATA;
		} else {
			*dev_addr = credentials.dev_addr;
		}
	}
	k_mutex_unlock(&credential_lock);
	if (rc != 0 || mode == RZI_LORAWAN_ACTIVATION_ABP) {
		return rc;
	}
	return read_otaa_dev_addr(dev_addr);
}

int rzi_lorawan_set_dev_addr(uint32_t dev_addr)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0 && credentials.join_active) {
		rc = -RZI_ERR_BUSY;
	}
	if (rc == 0) {
		rc = persist("devaddr", &dev_addr, sizeof(dev_addr));
		if (rc == 0) {
			credentials.dev_addr = dev_addr;
			credentials.dev_addr_set = true;
		}
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_get_nwk_skey(uint8_t *key, size_t len)
{
	const struct rzi_lorawan_session_ops *ops;
	enum rzi_lorawan_activation mode;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (key == NULL || len != sizeof(credentials.nwk_skey)) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	mode = credentials.njm;
	if (rc == 0 && mode == RZI_LORAWAN_ACTIVATION_ABP) {
		if (!credentials.nwk_skey_set) {
			rc = -RZI_ERR_NO_DATA;
		} else {
			memcpy(key, credentials.nwk_skey, sizeof(credentials.nwk_skey));
		}
	}
	k_mutex_unlock(&credential_lock);
	if (rc != 0 || mode == RZI_LORAWAN_ACTIVATION_ABP) {
		return rc;
	}
	ops = session_ops();
	return read_otaa_key(ops != NULL ? ops->get_nwk_skey : NULL, key, len);
}

int rzi_lorawan_set_nwk_skey(const uint8_t *key, size_t len)
{
	return set_bytes(credentials.nwk_skey, &credentials.nwk_skey_set, "nwkskey", key, len,
			 sizeof(credentials.nwk_skey));
}

int rzi_lorawan_get_app_skey(uint8_t *key, size_t len)
{
	const struct rzi_lorawan_session_ops *ops;
	enum rzi_lorawan_activation mode;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (key == NULL || len != sizeof(credentials.app_skey)) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	mode = credentials.njm;
	if (rc == 0 && mode == RZI_LORAWAN_ACTIVATION_ABP) {
		if (!credentials.app_skey_set) {
			rc = -RZI_ERR_NO_DATA;
		} else {
			memcpy(key, credentials.app_skey, sizeof(credentials.app_skey));
		}
	}
	k_mutex_unlock(&credential_lock);
	if (rc != 0 || mode == RZI_LORAWAN_ACTIVATION_ABP) {
		return rc;
	}
	ops = session_ops();
	return read_otaa_key(ops != NULL ? ops->get_app_skey : NULL, key, len);
}

int rzi_lorawan_set_app_skey(const uint8_t *key, size_t len)
{
	return set_bytes(credentials.app_skey, &credentials.app_skey_set, "appskey", key, len,
			 sizeof(credentials.app_skey));
}

int rzi_lorawan_get_activation(enum rzi_lorawan_activation *mode)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (mode == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		*mode = credentials.njm;
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_set_activation(enum rzi_lorawan_activation mode)
{
	uint8_t wire;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (!activation_supported(mode)) {
		return mode == RZI_LORAWAN_ACTIVATION_OTAA || mode == RZI_LORAWAN_ACTIVATION_ABP
			       ? -RZI_ERR_NOT_SUPPORTED
			       : -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0 && credentials.join_active) {
		rc = -RZI_ERR_BUSY;
	}
	if (rc == 0) {
		wire = njm_wire(mode);
		rc = persist("njm", &wire, sizeof(wire));
		if (rc == 0) {
			credentials.njm = mode;
		}
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_get_auto_join(bool *enabled)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (enabled == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		*enabled = credentials.auto_join;
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_get_join_interval(uint8_t *interval_s)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (interval_s == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		*interval_s = credentials.join_interval;
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_get_join_attempts(uint8_t *attempts)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (attempts == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		*attempts = credentials.join_attempts;
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_credentials_update_policy(int32_t auto_join, int32_t interval, int32_t attempts)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if ((auto_join != RZI_LORAWAN_JOIN_KEEP && auto_join != 0 && auto_join != 1) ||
	    (interval != RZI_LORAWAN_JOIN_KEEP && (interval < RZI_LORAWAN_JOIN_INTERVAL_MIN ||
						   interval > RZI_LORAWAN_JOIN_INTERVAL_MAX)) ||
	    (attempts != RZI_LORAWAN_JOIN_KEEP && (attempts < 0 || attempts > UINT8_MAX))) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		uint8_t policy[RZI_JOIN_POLICY_SIZE] = {
			auto_join == RZI_LORAWAN_JOIN_KEEP ? credentials.auto_join : auto_join,
			interval == RZI_LORAWAN_JOIN_KEEP ? credentials.join_interval : interval,
			attempts == RZI_LORAWAN_JOIN_KEEP ? credentials.join_attempts : attempts,
		};

		rc = persist("join_policy", policy, sizeof(policy));
		if (rc == 0) {
			credentials.auto_join = policy[RZI_JOIN_POLICY_AUTO] != 0U;
			credentials.join_interval = policy[RZI_JOIN_POLICY_INTERVAL];
			credentials.join_attempts = policy[RZI_JOIN_POLICY_ATTEMPTS];
		}
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_credentials_get_channel_mask(uint16_t *mask, bool *set)
{
	int rc;

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		*set = credentials.channel_mask_set;
		memcpy(mask, credentials.channel_mask, sizeof(credentials.channel_mask));
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_credentials_set_channel_mask(const uint16_t *mask)
{
	int rc;

	if (mask != NULL && !channel_mask_valid(mask)) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0 && credentials.join_active) {
		rc = -RZI_ERR_BUSY;
	}
	if (rc == 0) {
		uint16_t candidate[RZI_LORAWAN_CHANNEL_MASK_WORDS] = {0};

		if (mask != NULL) {
			memcpy(candidate, mask, sizeof(candidate));
		}
		/* Store an explicit default, so old rzi/chmask can never reappear. */
		rc = persist("chmask", candidate, sizeof(candidate));
		if (rc == 0) {
			memcpy(credentials.channel_mask, candidate, sizeof(candidate));
			credentials.channel_mask_set = mask != NULL;
		}
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_credentials_build_config(struct rzi_lorawan_join_config *config)
{
	enum rzi_lorawan_region region;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (config == NULL) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_lorawan_get_region(&region);
	if (rc != 0) {
		return rc;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		memset(config, 0, sizeof(*config));
		config->activation = credentials.njm;
		config->retry_interval_s = credentials.join_interval;
		if (credentials.channel_mask_set && rzi_lorawan_region_has_fixed_plan(region) &&
		    (rzi_lorawan_backend.capabilities & RZI_LORAWAN_CAP_CHANNEL) != 0U) {
			config->channel_mask_set = true;
			memcpy(config->channel_mask, credentials.channel_mask,
			       sizeof(config->channel_mask));
		}
		if (credentials.njm == RZI_LORAWAN_ACTIVATION_ABP) {
			config->abp.dev_addr = credentials.dev_addr;
			memcpy(config->abp.network_session_key, credentials.nwk_skey,
			       sizeof(config->abp.network_session_key));
			memcpy(config->abp.application_session_key, credentials.app_skey,
			       sizeof(config->abp.application_session_key));
		} else {
			memcpy(config->otaa.dev_eui, credentials.dev_eui,
			       sizeof(config->otaa.dev_eui));
			memcpy(config->otaa.join_eui, credentials.app_eui,
			       sizeof(config->otaa.join_eui));
			memcpy(config->otaa.network_key, credentials.app_key,
			       sizeof(config->otaa.network_key));
			memcpy(config->otaa.application_key,
			       credentials.gen_app_key_set ? credentials.gen_app_key
							   : credentials.app_key,
			       sizeof(config->otaa.application_key));
		}
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

void rzi_lorawan_credentials_set_join_active(bool active)
{
	k_mutex_lock(&credential_lock, K_FOREVER);
	credentials.join_active = active;
	k_mutex_unlock(&credential_lock);
}

int rzi_lorawan_credentials_clear_settings(void)
{
	int rc;

	k_mutex_lock(&credential_lock, K_FOREVER);
	if (credentials.join_active) {
		rc = -RZI_ERR_BUSY;
	} else {
		rc = rzi_lorawan_settings_clear();
		if (rc == 0) {
			memset(&credentials, 0, sizeof(credentials));
		}
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}
