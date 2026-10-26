/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Public access to cached, service-owned LoRaWAN configuration.
 */
#include <rzi/lorawan/lorawan.h>
#include "../lorawan_priv.h"
#include "../lorawan_settings.h"

int rzi_lorawan_config_get_region(enum rzi_lorawan_region *region)
{
	uint8_t stored;
	bool found;
	int rc = rzi_lorawan_setting_read("region", &stored, sizeof(stored), &found);

	if (rc == 0) {
		*region = (enum rzi_lorawan_region)stored;
	}
	return rc;
}

int rzi_lorawan_config_set_region(enum rzi_lorawan_region region)
{
	uint8_t stored = (uint8_t)region;

	return rzi_lorawan_setting_write("region", &stored, sizeof(stored));
}

static int read_config(const char *key, void *value, size_t size, bool *found)
{
	int rc = rzi_lorawan_check_thread();

	if (rc != 0 || value == NULL) {
		return rc != 0 ? rc : -RZI_ERR_INVALID;
	}
	return rzi_lorawan_setting_read(key, value, size, found);
}

static int write_config(const char *key, const void *value, size_t size)
{
	int rc = rzi_lorawan_check_thread();

	return rc != 0 ? rc : rzi_lorawan_setting_write(key, value, size);
}

int rzi_lorawan_get_confirmed(bool *enabled)
{
	uint8_t stored;
	bool found;
	int rc = rzi_lorawan_check_thread();

	if (rc != 0 || enabled == NULL) {
		return rc != 0 ? rc : -RZI_ERR_INVALID;
	}
	rc = read_config("cfm", &stored, sizeof(stored), &found);
	if (rc == 0) {
		*enabled = stored != 0U;
	}
	return rc;
}

int rzi_lorawan_set_confirmed(bool enabled)
{
	uint8_t stored = enabled ? 1U : 0U;

	return write_config("cfm", &stored, sizeof(stored));
}

int rzi_lorawan_get_retries(uint8_t *count)
{
	bool found;

	return read_config("rety", count, sizeof(*count), &found);
}

int rzi_lorawan_set_retries(uint8_t count)
{
	return write_config("rety", &count, sizeof(count));
}

int rzi_lorawan_config_get_net_id(uint32_t *net_id)
{
	uint32_t stored;
	bool found;
	int rc = rzi_lorawan_setting_read("netid", &stored, sizeof(stored), &found);

	if (rc == 0 && !found) {
		return -RZI_ERR_NO_DATA;
	}
	if (rc == 0) {
		*net_id = stored;
	}
	return rc;
}

int rzi_lorawan_set_net_id(uint32_t net_id)
{
	return write_config("netid", &net_id, sizeof(net_id));
}

int rzi_lorawan_clear_settings(void)
{
	int rc = rzi_lorawan_check_thread();

	return rc != 0 ? rc : rzi_lorawan_credentials_clear_settings();
}
