/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file
 * @brief Internal schema, cache and persistence for service-owned settings.
 */
#ifndef RZI_LORAWAN_SETTINGS_H
#define RZI_LORAWAN_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fixed byte encoding, independent of struct padding and host byte order. */
#define RZI_JOIN_POLICY_SIZE     3U
#define RZI_JOIN_POLICY_VERSION  1U
#define RZI_JOIN_POLICY_AUTO     0U
#define RZI_JOIN_POLICY_INTERVAL 1U
#define RZI_JOIN_POLICY_ATTEMPTS 2U

/* Known keys only. Missing keys return defaults and found=false. */
int rzi_lorawan_setting_read(const char *key, void *value, size_t size, bool *found);
/* Persist first, then publish to the cache. Unchanged cached values need no write. */
int rzi_lorawan_setting_write(const char *key, const void *value, size_t size);
/* Delete every registered current/legacy key. Only invalidates cache on success. */
int rzi_lorawan_settings_clear(void);
/* Clear persisted settings and the credential cache under credential_lock. */
int rzi_lorawan_credentials_clear_settings(void);

#endif /* RZI_LORAWAN_SETTINGS_H */
