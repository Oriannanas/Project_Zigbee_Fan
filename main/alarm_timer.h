/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Vendored from espressif/esp-zigbee-sdk examples/utils/alarm_timer
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The argument type for the alarm callback.
 */
typedef uintptr_t alarm_timer_arg_t;

/**
 * @brief The callback for the timer.
 */
typedef void (*alarm_timer_callback_t)(alarm_timer_arg_t arg);

/**
 * @brief Schedule an alarm to fire at a given time.
 *
 * @param cb The callback to execute when the alarm fires.
 * @param arg The argument to pass to the callback.
 * @param time_ms The time to fire the timer in milliseconds.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t alarm_timer_schedule(alarm_timer_callback_t cb, alarm_timer_arg_t arg, uint32_t time_ms);

#ifdef __cplusplus
}
#endif
