/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Stored LoRaWAN credentials and join policy.
 */

#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>

#if defined(CONFIG_RZI_STORAGE)
#include <rzi/storage/storage.h>
#endif

#include "../backend/lorawan_backend.h"
#include "../lorawan_priv.h"

#define CREDENTIAL_NS "lorawan"
#define LEGACY_NS     "rzi"

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
	bool join_active;
	bool loaded;
};

static struct credential_store credentials = {
	.njm = RZI_LORAWAN_ACTIVATION_OTAA,
	.join_interval = RZI_LORAWAN_JOIN_INTERVAL_DEFAULT,
};

K_MUTEX_DEFINE(credential_lock);

static int persist(const char *key, const void *value, size_t len)
{
#if defined(CONFIG_RZI_STORAGE)
	int rc = rzi_storage_init();

	if (rc != 0) {
		return rc;
	}
	return rzi_storage_write(CREDENTIAL_NS, key, value, len);
#else
	ARG_UNUSED(key);
	ARG_UNUSED(value);
	ARG_UNUSED(len);
	return 0;
#endif
}

#if defined(CONFIG_RZI_STORAGE)
static int load_bytes(const char *key, void *dst, size_t len, bool *valid)
{
	int rc = rzi_storage_read(CREDENTIAL_NS, key, dst, len);

	if (rc == 0) {
		*valid = true;
		return 0;
	}
	if (rc != -RZI_ERR_NOT_FOUND) {
		return rc;
	}
	rc = rzi_storage_read(LEGACY_NS, key, dst, len);
	if (rc == -RZI_ERR_NOT_FOUND) {
		*valid = false;
		return 0;
	}
	if (rc != 0) {
		return rc;
	}
	*valid = true;
	return rzi_storage_write(CREDENTIAL_NS, key, dst, len);
}

static int load_u8(const char *key, uint8_t *value, bool *found)
{
	bool valid = false;
	uint8_t stored = 0;
	int rc = load_bytes(key, &stored, sizeof(stored), &valid);

	if (rc == 0 && valid) {
		*value = stored;
	}
	*found = valid;
	return rc;
}
#endif

static int ensure_loaded_locked(void)
{
#if defined(CONFIG_RZI_STORAGE)
	uint8_t value;
	bool found;
	int rc;

	if (credentials.loaded) {
		return 0;
	}
	rc = rzi_storage_init();
	if (rc != 0) {
		return rc;
	}
	rc = load_bytes("deveui", credentials.dev_eui, sizeof(credentials.dev_eui),
			&credentials.dev_eui_set);
	if (rc == 0) {
		rc = load_bytes("joineui", credentials.app_eui, sizeof(credentials.app_eui),
				&credentials.app_eui_set);
	}
	if (rc == 0) {
		rc = load_bytes("appkey", credentials.app_key, sizeof(credentials.app_key),
				&credentials.app_key_set);
	}
	if (rc == 0) {
		rc = load_bytes("genappkey", credentials.gen_app_key,
				sizeof(credentials.gen_app_key), &credentials.gen_app_key_set);
	}
	if (rc == 0) {
		rc = load_bytes("devaddr", &credentials.dev_addr, sizeof(credentials.dev_addr),
				&credentials.dev_addr_set);
	}
	if (rc == 0) {
		rc = load_bytes("nwkskey", credentials.nwk_skey, sizeof(credentials.nwk_skey),
				&credentials.nwk_skey_set);
	}
	if (rc == 0) {
		rc = load_bytes("appskey", credentials.app_skey, sizeof(credentials.app_skey),
				&credentials.app_skey_set);
	}
	if (rc == 0) {
		rc = load_u8("njm", &value, &found);
		if (rc == 0 && found && value <= NJM_WIRE_OTAA) {
			credentials.njm = value == NJM_WIRE_ABP ? RZI_LORAWAN_ACTIVATION_ABP
								: RZI_LORAWAN_ACTIVATION_OTAA;
		}
	}
	if (rc == 0) {
		rc = load_u8("autojoin", &value, &found);
		if (rc == 0 && found) {
			credentials.auto_join = value != 0U;
		}
	}
	if (rc == 0) {
		rc = load_u8("join_interval", &value, &found);
		if (rc == 0 && found && value >= RZI_LORAWAN_JOIN_INTERVAL_MIN) {
			credentials.join_interval = value;
		}
	}
	if (rc == 0) {
		rc = load_u8("join_attempts", &value, &found);
		if (rc == 0 && found) {
			credentials.join_attempts = value;
		}
	}
	if (rc != 0) {
		return rc;
	}
#else
	if (credentials.loaded) {
		return 0;
	}
#endif
	credentials.loaded = true;
	return 0;
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
		memcpy(dst, src, expected);
		*valid = true;
		rc = persist(key, dst, expected);
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
	if (rzi_lorawan_backend.is_joined == NULL) {
		return -RZI_ERR_NOT_SUPPORTED;
	}
	return rzi_lorawan_backend.is_joined(joined);
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
		credentials.dev_addr = dev_addr;
		credentials.dev_addr_set = true;
		rc = persist("devaddr", &credentials.dev_addr, sizeof(credentials.dev_addr));
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
		credentials.njm = mode;
		wire = njm_wire(mode);
		rc = persist("njm", &wire, sizeof(wire));
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
	if (rc == 0 && auto_join != RZI_LORAWAN_JOIN_KEEP) {
		uint8_t stored = auto_join != 0 ? 1U : 0U;

		credentials.auto_join = stored != 0U;
		rc = persist("autojoin", &stored, sizeof(stored));
	}
	if (rc == 0 && interval != RZI_LORAWAN_JOIN_KEEP) {
		credentials.join_interval = (uint8_t)interval;
		rc = persist("join_interval", &credentials.join_interval,
			     sizeof(credentials.join_interval));
	}
	if (rc == 0 && attempts != RZI_LORAWAN_JOIN_KEEP) {
		credentials.join_attempts = (uint8_t)attempts;
		rc = persist("join_attempts", &credentials.join_attempts,
			     sizeof(credentials.join_attempts));
	}
	k_mutex_unlock(&credential_lock);
	return rc;
}

int rzi_lorawan_credentials_build_config(struct rzi_lorawan_join_config *config)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0) {
		return rc;
	}
	if (config == NULL) {
		return -RZI_ERR_INVALID;
	}

	k_mutex_lock(&credential_lock, K_FOREVER);
	rc = ensure_loaded_locked();
	if (rc == 0) {
		memset(config, 0, sizeof(*config));
		config->activation = credentials.njm;
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
