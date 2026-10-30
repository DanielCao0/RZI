/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief LLEXT export table for the public RZI API.
 */

#include <zephyr/llext/symbol.h>

#include <rzi/err.h>
#include <rzi/init.h>
#include <rzi/version.h>

EXPORT_SYMBOL(rzi_init);
EXPORT_SYMBOL(rzi_err_str);
EXPORT_SYMBOL(rzi_err_from_errno);
EXPORT_SYMBOL(rzi_version_get_string);

#if defined(CONFIG_RZI_LORAWAN)
#include <rzi/lorawan/certification.h>
#include <rzi/lorawan/lorawan.h>
#include <rzi/lorawan/multicast.h>

EXPORT_SYMBOL(rzi_lorawan_register_callbacks);
EXPORT_SYMBOL(rzi_lorawan_unregister_callbacks);
EXPORT_SYMBOL(rzi_lorawan_set_region);
EXPORT_SYMBOL(rzi_lorawan_set_join_backoff_bypass);
EXPORT_SYMBOL(rzi_lorawan_init);
EXPORT_SYMBOL(rzi_lorawan_get_dev_eui);
EXPORT_SYMBOL(rzi_lorawan_set_dev_eui);
EXPORT_SYMBOL(rzi_lorawan_get_app_eui);
EXPORT_SYMBOL(rzi_lorawan_set_app_eui);
EXPORT_SYMBOL(rzi_lorawan_get_app_key);
EXPORT_SYMBOL(rzi_lorawan_set_app_key);
EXPORT_SYMBOL(rzi_lorawan_get_gen_app_key);
EXPORT_SYMBOL(rzi_lorawan_set_gen_app_key);
EXPORT_SYMBOL(rzi_lorawan_get_dev_addr);
EXPORT_SYMBOL(rzi_lorawan_set_dev_addr);
EXPORT_SYMBOL(rzi_lorawan_get_nwk_skey);
EXPORT_SYMBOL(rzi_lorawan_set_nwk_skey);
EXPORT_SYMBOL(rzi_lorawan_get_app_skey);
EXPORT_SYMBOL(rzi_lorawan_set_app_skey);
EXPORT_SYMBOL(rzi_lorawan_get_activation);
EXPORT_SYMBOL(rzi_lorawan_set_activation);
EXPORT_SYMBOL(rzi_lorawan_get_auto_join);
EXPORT_SYMBOL(rzi_lorawan_get_join_interval);
EXPORT_SYMBOL(rzi_lorawan_get_join_attempts);
EXPORT_SYMBOL(rzi_lorawan_join);
EXPORT_SYMBOL(rzi_lorawan_leave);
EXPORT_SYMBOL(rzi_lorawan_send);
EXPORT_SYMBOL(rzi_lorawan_get_link_check_mode);
EXPORT_SYMBOL(rzi_lorawan_request_link_check);
EXPORT_SYMBOL(rzi_lorawan_get_device_time_enabled);
EXPORT_SYMBOL(rzi_lorawan_request_device_time);
EXPORT_SYMBOL(rzi_lorawan_get_network_time);
EXPORT_SYMBOL(rzi_lorawan_set_class);
EXPORT_SYMBOL(rzi_lorawan_is_joined);
EXPORT_SYMBOL(rzi_lorawan_get_capabilities);
EXPORT_SYMBOL(rzi_lorawan_get_region);
EXPORT_SYMBOL(rzi_lorawan_get_class);
EXPORT_SYMBOL(rzi_lorawan_get_adr);
EXPORT_SYMBOL(rzi_lorawan_set_adr);
EXPORT_SYMBOL(rzi_lorawan_get_data_rate);
EXPORT_SYMBOL(rzi_lorawan_set_data_rate);
EXPORT_SYMBOL(rzi_lorawan_get_tx_power);
EXPORT_SYMBOL(rzi_lorawan_set_tx_power);
EXPORT_SYMBOL(rzi_lorawan_get_duty_cycle);
EXPORT_SYMBOL(rzi_lorawan_set_duty_cycle);
EXPORT_SYMBOL(rzi_lorawan_get_rx1_delay);
EXPORT_SYMBOL(rzi_lorawan_set_rx1_delay);
EXPORT_SYMBOL(rzi_lorawan_get_rx2_delay);
EXPORT_SYMBOL(rzi_lorawan_set_rx2_delay);
EXPORT_SYMBOL(rzi_lorawan_get_rx2_data_rate);
EXPORT_SYMBOL(rzi_lorawan_set_rx2_data_rate);
EXPORT_SYMBOL(rzi_lorawan_get_rx2_frequency);
EXPORT_SYMBOL(rzi_lorawan_set_rx2_frequency);
EXPORT_SYMBOL(rzi_lorawan_get_join_accept_delay1);
EXPORT_SYMBOL(rzi_lorawan_set_join_accept_delay1);
EXPORT_SYMBOL(rzi_lorawan_get_join_accept_delay2);
EXPORT_SYMBOL(rzi_lorawan_set_join_accept_delay2);
EXPORT_SYMBOL(rzi_lorawan_get_public_network);
EXPORT_SYMBOL(rzi_lorawan_set_public_network);
EXPORT_SYMBOL(rzi_lorawan_get_lbt);
EXPORT_SYMBOL(rzi_lorawan_set_lbt);
EXPORT_SYMBOL(rzi_lorawan_get_lbt_rssi);
EXPORT_SYMBOL(rzi_lorawan_set_lbt_rssi);
EXPORT_SYMBOL(rzi_lorawan_get_lbt_scan_time);
EXPORT_SYMBOL(rzi_lorawan_set_lbt_scan_time);
EXPORT_SYMBOL(rzi_lorawan_get_adr_profile);
EXPORT_SYMBOL(rzi_lorawan_set_adr_profile);
EXPORT_SYMBOL(rzi_lorawan_get_nb_trans);
EXPORT_SYMBOL(rzi_lorawan_set_nb_trans);
EXPORT_SYMBOL(rzi_lorawan_get_adr_ack);
EXPORT_SYMBOL(rzi_lorawan_set_adr_ack);
EXPORT_SYMBOL(rzi_lorawan_set_join_data_rates);
EXPORT_SYMBOL(rzi_lorawan_get_duty_cycle_status);
EXPORT_SYMBOL(rzi_lorawan_get_csma);
EXPORT_SYMBOL(rzi_lorawan_set_csma);
EXPORT_SYMBOL(rzi_lorawan_get_csma_params);
EXPORT_SYMBOL(rzi_lorawan_set_csma_params);
EXPORT_SYMBOL(rzi_lorawan_get_lost_connection_count);
EXPORT_SYMBOL(rzi_lorawan_get_no_downlink_threshold);
EXPORT_SYMBOL(rzi_lorawan_set_no_downlink_threshold);
EXPORT_SYMBOL(rzi_lorawan_get_crystal_error);
EXPORT_SYMBOL(rzi_lorawan_set_crystal_error);
EXPORT_SYMBOL(rzi_lorawan_send_empty);
EXPORT_SYMBOL(rzi_lorawan_get_channel_mask);
EXPORT_SYMBOL(rzi_lorawan_set_channel_mask);
EXPORT_SYMBOL(rzi_lorawan_get_sub_band);
EXPORT_SYMBOL(rzi_lorawan_set_sub_band);
EXPORT_SYMBOL(rzi_lorawan_get_fixed_channel);
EXPORT_SYMBOL(rzi_lorawan_set_fixed_channel);
EXPORT_SYMBOL(rzi_lorawan_get_last_rssi);
EXPORT_SYMBOL(rzi_lorawan_get_last_snr);
EXPORT_SYMBOL(rzi_lorawan_get_protocol_version);
EXPORT_SYMBOL(rzi_lorawan_get_net_id);
EXPORT_SYMBOL(rzi_lorawan_get_dev_nonce);
EXPORT_SYMBOL(rzi_lorawan_set_dev_nonce);
EXPORT_SYMBOL(rzi_lorawan_query_tx_possible);
EXPORT_SYMBOL(rzi_lorawan_is_busy);
EXPORT_SYMBOL(rzi_lorawan_get_ping_slot_periodicity);
EXPORT_SYMBOL(rzi_lorawan_set_ping_slot_periodicity);
EXPORT_SYMBOL(rzi_lorawan_get_beacon_frequency);
EXPORT_SYMBOL(rzi_lorawan_get_beacon_time);
EXPORT_SYMBOL(rzi_lorawan_get_beacon_data_rate);
EXPORT_SYMBOL(rzi_lorawan_get_beacon_gateway);
EXPORT_SYMBOL(rzi_lorawan_get_class_b_state);
EXPORT_SYMBOL(rzi_lorawan_stop_class_b);

EXPORT_SYMBOL(rzi_lorawan_add_multicast_session);
EXPORT_SYMBOL(rzi_lorawan_remove_multicast_session);
EXPORT_SYMBOL(rzi_lorawan_get_multicast_count);
EXPORT_SYMBOL(rzi_lorawan_get_multicast_session);
EXPORT_SYMBOL(rzi_lorawan_clear_multicast_sessions);

EXPORT_SYMBOL(rzi_lorawan_get_certification_mode);
EXPORT_SYMBOL(rzi_lorawan_set_certification_mode);
EXPORT_SYMBOL(rzi_lorawan_get_certification_port_enabled);
EXPORT_SYMBOL(rzi_lorawan_set_certification_port_enabled);
#endif
