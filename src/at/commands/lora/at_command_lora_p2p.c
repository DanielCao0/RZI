/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief RUI3 AT command domain for LoRa P2P and FSK operation.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "at_command_lora_priv.h"

static int respond_ok(int rc)
{
	return rc == 0 ? rzi_at_respond_status(RZI_AT_STATUS_OK) : rc;
}

static int parse_colon_u32(const char *argument, uint32_t *values, size_t expected)
{
	const char *cursor = argument;
	size_t count = 0;

	while (*cursor != '\0' && count < expected) {
		char *end;
		unsigned long parsed = strtoul(cursor, &end, 10);

		if (end == cursor) {
			return -RZI_ERR_INVALID;
		}
		values[count++] = (uint32_t)parsed;
		if (*end == '\0') {
			break;
		}
		if (*end != ':' || end[1] == '\0') {
			return -RZI_ERR_INVALID;
		}
		cursor = end + 1;
	}
	return count == expected ? 0 : -RZI_ERR_INVALID;
}

static int write_u32(const struct rzi_at_request *request, const char *name, uint32_t min,
		     uint32_t max, void (*store)(struct rzi_lora_config *, uint32_t),
		     uint32_t (*load)(const struct rzi_lora_config *))
{
	struct rzi_lora_config config;
	long parsed;
	int rc = rzi_lora_get_config(&config);

	if (rc != 0) {
		return rc;
	}
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+%s=%u", name, load(&config));
	}
	rc = rzi_at_lora_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < (long)min || parsed > (long)max) {
		return -RZI_ERR_INVALID;
	}
	store(&config, (uint32_t)parsed);
	return respond_ok(rzi_lora_set_config(&config));
}

static int write_bool(const struct rzi_at_request *request, const char *name,
		      void (*store)(struct rzi_lora_config *, bool),
		      bool (*load)(const struct rzi_lora_config *))
{
	struct rzi_lora_config config;
	bool enabled;
	int rc = rzi_lora_get_config(&config);

	if (rc != 0) {
		return rc;
	}
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+%s=%d", name, load(&config) ? 1 : 0);
	}
	rc = rzi_at_lora_parse_bool(request->argument, &enabled);
	if (rc != 0) {
		return rc;
	}
	store(&config, enabled);
	return respond_ok(rzi_lora_set_config(&config));
}

static int write_key(const struct rzi_at_request *request, const char *name, uint8_t *field,
		     struct rzi_lora_config *config)
{
	char hex[33];
	int rc;

	if (request->operation == RZI_AT_OP_READ) {
		rzi_at_lora_bin_to_hex(field, 16U, hex);
		return rzi_at_respond_value("AT+%s=%s", name, hex);
	}
	rc = rzi_at_lora_hex_to_bin(request->argument, field, 16U);
	if (rc != 0) {
		return rc;
	}
	return respond_ok(rzi_lora_set_config(config));
}

static void set_freq(struct rzi_lora_config *config, uint32_t value)
{
	config->frequency_hz = value;
}

static uint32_t get_freq(const struct rzi_lora_config *config)
{
	return config->frequency_hz;
}

static void set_sf(struct rzi_lora_config *config, uint32_t value)
{
	config->spreading_factor = (uint8_t)value;
}

static uint32_t get_sf(const struct rzi_lora_config *config)
{
	return config->spreading_factor;
}

static void set_bw(struct rzi_lora_config *config, uint32_t value)
{
	config->bandwidth = value;
}

static uint32_t get_bw(const struct rzi_lora_config *config)
{
	return config->bandwidth;
}

static void set_cr(struct rzi_lora_config *config, uint32_t value)
{
	config->coding_rate = (uint8_t)value;
}

static uint32_t get_cr(const struct rzi_lora_config *config)
{
	return config->coding_rate;
}

static void set_pl(struct rzi_lora_config *config, uint32_t value)
{
	config->preamble_length = (uint16_t)value;
}

static uint32_t get_pl(const struct rzi_lora_config *config)
{
	return config->preamble_length;
}

static void set_tp(struct rzi_lora_config *config, uint32_t value)
{
	config->tx_power_dbm = (int8_t)value;
}

static uint32_t get_tp(const struct rzi_lora_config *config)
{
	return (uint32_t)config->tx_power_dbm;
}

static void set_sw(struct rzi_lora_config *config, uint32_t value)
{
	config->sync_word = (uint16_t)value;
}

static uint32_t get_sw(const struct rzi_lora_config *config)
{
	return config->sync_word;
}

static void set_br(struct rzi_lora_config *config, uint32_t value)
{
	config->fsk_bitrate = value;
}

static uint32_t get_br(const struct rzi_lora_config *config)
{
	return config->fsk_bitrate;
}

static void set_dev(struct rzi_lora_config *config, uint32_t value)
{
	config->fsk_deviation = value;
}

static uint32_t get_dev(const struct rzi_lora_config *config)
{
	return config->fsk_deviation;
}

static void set_enc(struct rzi_lora_config *config, bool value)
{
	config->encrypt = value;
}

static bool get_enc(const struct rzi_lora_config *config)
{
	return config->encrypt;
}

static void set_iq(struct rzi_lora_config *config, bool value)
{
	config->iq_inverted = value;
}

static bool get_iq(const struct rzi_lora_config *config)
{
	return config->iq_inverted;
}

static void set_cad(struct rzi_lora_config *config, bool value)
{
	config->cad = value;
}

static bool get_cad(const struct rzi_lora_config *config)
{
	return config->cad;
}

static int handle_p2p(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_lora_config config;
	uint32_t values[6];
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		rc = rzi_lora_get_config(&config);
		return rc != 0 ? rc
			       : rzi_at_respond_value("AT+P2P=%u:%u:%u:%u:%u:%d",
						      config.frequency_hz, config.spreading_factor,
						      config.bandwidth, config.coding_rate,
						      config.preamble_length, config.tx_power_dbm);
	}
	rc = parse_colon_u32(request->argument, values, ARRAY_SIZE(values));
	if (rc != 0) {
		return rc;
	}
	rc = rzi_lora_get_config(&config);
	if (rc != 0) {
		return rc;
	}
	config.frequency_hz = values[0];
	config.spreading_factor = (uint8_t)values[1];
	config.bandwidth = values[2];
	config.coding_rate = (uint8_t)values[3];
	config.preamble_length = (uint16_t)values[4];
	config.tx_power_dbm = (int8_t)values[5];
	return respond_ok(rzi_lora_set_config(&config));
}

static int handle_pfreq(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PFREQ", 150000000U, 960000000U, set_freq, get_freq);
}

static int handle_psf(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PSF", 5U, 12U, set_sf, get_sf);
}

static int handle_pbw(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PBW", 0U, 960000000U, set_bw, get_bw);
}

static int handle_pcr(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PCR", 0U, 3U, set_cr, get_cr);
}

static int handle_ppl(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PPL", 5U, 65535U, set_pl, get_pl);
}

static int handle_ptp(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PTP", 5U, 22U, set_tp, get_tp);
}

static int handle_syncword(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "SYNCWORD", 0U, 65535U, set_sw, get_sw);
}

static int handle_pbr(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PBR", 600U, 300000U, set_br, get_br);
}

static int handle_pfdev(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_u32(request, "PFDEV", 600U, 200000U, set_dev, get_dev);
}

static int handle_encry(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_bool(request, "ENCRY", set_enc, get_enc);
}

static int handle_iqinver(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_bool(request, "IQINVER", set_iq, get_iq);
}

static int handle_cad(const struct rzi_at_request *request, void *user_data)
{
	ARG_UNUSED(user_data);
	return write_bool(request, "CAD", set_cad, get_cad);
}

static int handle_enckey(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_lora_config config;
	int rc = rzi_lora_get_config(&config);

	ARG_UNUSED(user_data);
	if (rc != 0) {
		return rc;
	}
	return write_key(request, "ENCKEY", config.key, &config);
}

static int handle_crypiv(const struct rzi_at_request *request, void *user_data)
{
	struct rzi_lora_config config;
	int rc = rzi_lora_get_config(&config);

	ARG_UNUSED(user_data);
	if (rc != 0) {
		return rc;
	}
	return write_key(request, "CRYPIV", config.iv, &config);
}

static int handle_psend(const struct rzi_at_request *request, void *user_data)
{
	uint8_t payload[RZI_LORA_MAX_PAYLOAD];
	size_t hex_len = strlen(request->argument);
	int rc;

	ARG_UNUSED(user_data);
	if (hex_len == 0U || (hex_len % 2U) != 0U) {
		return -RZI_ERR_INVALID;
	}
	if (hex_len > sizeof(payload) * 2U) {
		return -RZI_ERR_TOO_LARGE;
	}
	rc = rzi_at_lora_hex_to_bin(request->argument, payload, hex_len / 2U);
	if (rc != 0) {
		return rc;
	}
	rc = rzi_at_lora_ensure_started();
	if (rc != 0) {
		return rc;
	}
	return respond_ok(rzi_lora_send(payload, hex_len / 2U));
}

static int handle_precv(const struct rzi_at_request *request, void *user_data)
{
	long parsed;
	int rc;

	ARG_UNUSED(user_data);
	if (request->operation == RZI_AT_OP_READ) {
		return rzi_at_respond_value("AT+PRECV=%d", rzi_lora_receive_active() ? 65535 : 0);
	}
	rc = rzi_at_lora_parse_long(request->argument, &parsed);
	if (rc != 0 || parsed < 0 || parsed > 65535) {
		return -RZI_ERR_INVALID;
	}
	rc = rzi_at_lora_ensure_started();
	if (rc != 0) {
		return rc;
	}
	return respond_ok(rzi_lora_receive((uint32_t)parsed));
}

static const struct rzi_at_command commands[] = {
	{
		.name = "P2P",
		.help = "get or set the P2P configuration (Freq:SF:BW:CR:PPL:PTP)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_p2p,
	},
	{
		.name = "PFREQ",
		.help = "get or set the P2P frequency (Hz)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pfreq,
	},
	{
		.name = "PSF",
		.help = "get or set the P2P spreading factor (5-12)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_psf,
	},
	{
		.name = "PBW",
		.help = "get or set the P2P bandwidth (0-9 for LoRa, Hz for FSK)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pbw,
	},
	{
		.name = "PCR",
		.help = "get or set the P2P coding rate (0-3)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pcr,
	},
	{
		.name = "PPL",
		.help = "get or set the P2P preamble length",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_ppl,
	},
	{
		.name = "PTP",
		.help = "get or set the P2P TX power (dBm)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_ptp,
	},
	{
		.name = "PSEND",
		.help = "send data in P2P mode",
		.allowed_operations = RZI_AT_ALLOW_WRITE,
		.handler = handle_psend,
	},
	{
		.name = "PRECV",
		.help = "set the P2P receive window (0 = stop, 65535 = continuous)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_precv,
	},
	{
		.name = "CAD",
		.help = "get or set P2P CAD (0 = off, 1 = on)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_cad,
	},
	{
		.name = "ENCRY",
		.help = "get or set P2P encryption (0 = off, 1 = on)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_encry,
	},
	{
		.name = "ENCKEY",
		.help = "get or set the P2P encryption key (16 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_enckey,
	},
	{
		.name = "CRYPIV",
		.help = "get or set the P2P encryption IV (16 bytes in hex)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_crypiv,
	},
	{
		.name = "IQINVER",
		.help = "get or set P2P IQ inversion (0 = off, 1 = on)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_iqinver,
	},
	{
		.name = "SYNCWORD",
		.help = "get or set the P2P sync word",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_syncword,
	},
	{
		.name = "PBR",
		.help = "get or set the P2P FSK bitrate (bps)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pbr,
	},
	{
		.name = "PFDEV",
		.help = "get or set the P2P FSK frequency deviation (Hz)",
		.allowed_operations = RZI_AT_ALLOW_READ | RZI_AT_ALLOW_WRITE,
		.handler = handle_pfdev,
	},
};

const struct rzi_at_lora_command_group rzi_at_lora_p2p_group = {
	.commands = commands,
	.count = ARRAY_SIZE(commands),
};
