/* SPDX-FileCopyrightText: Copyright (c) 2026 RAKwireless */
/* SPDX-License-Identifier: Apache-2.0 */

#include "backend/lorawan_backend.h"

/* This suite exercises stored policy before the radio is initialized. */
const struct rzi_lorawan_backend_api rzi_lorawan_backend = {
	.capabilities = RZI_LORAWAN_CAP_OTAA | RZI_LORAWAN_CAP_ABP | RZI_LORAWAN_CAP_CHANNEL,
};
