/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <rzi/lora/lora.h>
#include <rzi/lorawan/lorawan.h>

LOG_MODULE_REGISTER(rzi_ping_pong, LOG_LEVEL_INF);

#define READY_TIMEOUT       K_SECONDS(10)
#define TX_TIMEOUT_MS       2000
#define IDLE_LOG_MS         30000
#define POLL_INTERVAL_MS    5
#define RX_CONTINUOUS       65535U
#define PREAMBLE_LENGTH     12
#define LORA_BW_125_KHZ     0U
#define LORA_CR_4_5         0U
#define HEADER_SIZE         4
#define FRAME_SIZE          (HEADER_SIZE + 2)
#define FRAME_COUNTER_INDEX (HEADER_SIZE + 1)

static K_SEM_DEFINE(stack_ready, 0, 1);
static K_SEM_DEFINE(tx_done, 0, 1);
static K_SEM_DEFINE(rx_done, 0, 1);

static int tx_error;
static uint8_t rx_frame[RZI_LORA_MAX_PAYLOAD];
static size_t rx_size;
static int16_t rx_rssi_dbm;
static int8_t rx_snr_quarter_db;

static void on_event(const struct rzi_lorawan_event *event, void *user_data)
{
	ARG_UNUSED(user_data);
	if (event->type == RZI_LORAWAN_EVENT_READY ||
	    (event->type == RZI_LORAWAN_EVENT_STATE_CHANGED &&
	     event->state == RZI_LORAWAN_STATE_READY)) {
		k_sem_give(&stack_ready);
	}
}

static const struct rzi_lorawan_callbacks lorawan_callbacks = {
	.on_event = on_event,
};

static void on_tx_done(int error, void *user_data)
{
	ARG_UNUSED(user_data);
	tx_error = error;
	k_sem_give(&tx_done);
}

static void on_rx_done(const uint8_t *data, size_t size, int16_t rssi_dbm, int8_t snr_quarter_db,
		       void *user_data)
{
	ARG_UNUSED(user_data);
	if (k_sem_count_get(&rx_done) != 0U) {
		return;
	}
	memcpy(rx_frame, data, size);
	rx_size = size;
	rx_rssi_dbm = rssi_dbm;
	rx_snr_quarter_db = snr_quarter_db;
	k_sem_give(&rx_done);
}

static const struct rzi_lora_callbacks lora_callbacks = {
	.tx_done = on_tx_done,
	.rx_done = on_rx_done,
};

/* The USP backend delivers tx_done() and rx_done() from rzi_lora_poll(). */
static bool wait_for(struct k_sem *sem, uint32_t timeout_ms)
{
	int64_t deadline = k_uptime_get() + timeout_ms;

	do {
		rzi_lora_poll();
		if (k_sem_take(sem, K_MSEC(POLL_INTERVAL_MS)) == 0) {
			return true;
		}
	} while (k_uptime_get() < deadline);
	return false;
}

static int configure_radio(void)
{
	struct rzi_lora_config config;
	int rc = rzi_lora_get_config(&config);

	if (rc != 0) {
		return rc;
	}
	config.frequency_hz = CONFIG_APP_PING_PONG_FREQUENCY_HZ;
	config.spreading_factor = CONFIG_APP_PING_PONG_SPREADING_FACTOR;
	config.bandwidth = LORA_BW_125_KHZ;
	config.coding_rate = LORA_CR_4_5;
	config.preamble_length = PREAMBLE_LENGTH;
	config.tx_power_dbm = CONFIG_APP_PING_PONG_TX_POWER_DBM;
	return rzi_lora_set_config(&config);
}

/* The USP raw LoRa backend runs on the LoRa Basics Modem test API, so the
 * modem must be initialized, but not joined, before rzi_lora_init().
 */
static int start_radio(void)
{
	int rc = rzi_lorawan_register_callbacks(&lorawan_callbacks);

	if (rc != 0) {
		LOG_ERR("Callback registration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lorawan_init(RZI_LORAWAN_REGION_EU_868);
	if (rc != 0) {
		LOG_ERR("LoRaWAN init failed: %s", rzi_err_str(rc));
		return rc;
	}
	if (k_sem_take(&stack_ready, READY_TIMEOUT) != 0) {
		LOG_ERR("LoRaWAN ready timed out");
		return -RZI_ERR_TIMEOUT;
	}
	rc = rzi_lora_register_callbacks(&lora_callbacks);
	if (rc != 0) {
		LOG_ERR("LoRa callback registration failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = configure_radio();
	if (rc != 0) {
		LOG_ERR("LoRa config failed: %s", rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lora_init(RZI_LORA_MOD_LORA);
	if (rc != 0) {
		LOG_ERR("LoRa init failed: %s", rzi_err_str(rc));
		return rc;
	}
	return 0;
}

static int send_frame(const char *header, uint8_t counter)
{
	uint8_t frame[FRAME_SIZE];
	int rc;

	memcpy(frame, header, HEADER_SIZE);
	frame[HEADER_SIZE] = 0U;
	frame[FRAME_COUNTER_INDEX] = counter;
	k_sem_reset(&tx_done);
	rc = rzi_lora_receive(0);
	if (rc != 0) {
		LOG_WRN("%s %u rejected: %s", header, counter, rzi_err_str(rc));
		return rc;
	}
	rc = rzi_lora_send(frame, sizeof(frame));
	if (rc != 0) {
		LOG_WRN("%s %u rejected: %s", header, counter, rzi_err_str(rc));
		return rc;
	}
	if (!wait_for(&tx_done, TX_TIMEOUT_MS)) {
		LOG_WRN("%s %u not sent", header, counter);
		return -RZI_ERR_TIMEOUT;
	}
	return tx_error;
}

static int start_receive(void)
{
	int rc;

	k_sem_reset(&rx_done);
	rc = rzi_lora_receive(RX_CONTINUOUS);
	if (rc != 0) {
		LOG_WRN("Receive rejected: %s", rzi_err_str(rc));
	}
	return rc;
}

static bool frame_is(const char *header)
{
	return rx_size == FRAME_SIZE && memcmp(rx_frame, header, HEADER_SIZE) == 0 &&
	       rx_frame[HEADER_SIZE] == 0U;
}

static void run_manager(void)
{
	uint32_t received = 0;
	uint32_t lost = 0;
	uint8_t counter = 0;

	LOG_INF("Manager: sending PING every %d ms", CONFIG_APP_PING_PONG_DELAY_MS);
	for (;;) {
		if (send_frame("PING", counter) == 0 && start_receive() == 0 &&
		    wait_for(&rx_done, CONFIG_APP_PING_PONG_RX_TIMEOUT_MS) && frame_is("PONG") &&
		    rx_frame[FRAME_COUNTER_INDEX] == counter) {
			++received;
			LOG_INF("PONG %u: RSSI %d dBm, SNR %d/4 dB (%u received, %u lost)", counter,
				rx_rssi_dbm, rx_snr_quarter_db, received, lost);
		} else {
			++lost;
			LOG_WRN("No PONG for PING %u (%u received, %u lost)", counter, received,
				lost);
		}
		++counter;
		k_sleep(K_MSEC(CONFIG_APP_PING_PONG_DELAY_MS));
	}
}

static void run_subordinate(void)
{
	uint8_t counter;

	LOG_INF("Subordinate: waiting for PING");
	for (;;) {
		if (start_receive() != 0) {
			k_sleep(K_SECONDS(1));
			continue;
		}
		while (!wait_for(&rx_done, IDLE_LOG_MS)) {
			LOG_INF("No PING for %d s", IDLE_LOG_MS / MSEC_PER_SEC);
		}
		if (!frame_is("PING")) {
			LOG_WRN("Ignored a %u-byte frame", (unsigned int)rx_size);
			continue;
		}
		counter = rx_frame[FRAME_COUNTER_INDEX];
		LOG_INF("PING %u: RSSI %d dBm, SNR %d/4 dB", counter, rx_rssi_dbm,
			rx_snr_quarter_db);
		k_sleep(K_MSEC(CONFIG_APP_PING_PONG_REPLY_DELAY_MS));
		if (send_frame("PONG", counter) != 0) {
			LOG_WRN("PONG %u failed", counter);
		}
	}
}

int main(void)
{
	int rc;

	LOG_INF("RZI LoRa ping-pong: %d Hz, SF%d, %d dBm", CONFIG_APP_PING_PONG_FREQUENCY_HZ,
		CONFIG_APP_PING_PONG_SPREADING_FACTOR, CONFIG_APP_PING_PONG_TX_POWER_DBM);
	rc = start_radio();
	if (rc != 0) {
		return rc;
	}
	if (IS_ENABLED(CONFIG_APP_PING_PONG_MANAGER)) {
		run_manager();
	} else {
		run_subordinate();
	}
	return 0;
}
