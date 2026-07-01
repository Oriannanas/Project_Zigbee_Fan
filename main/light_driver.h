/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Espressif Systems
 *    integrated circuit in a product or a software update for such product,
 *    must reproduce the above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PWM output configuration */
#ifdef CONFIG_APP_OUTPUT_GPIO
#define LIGHT_OUTPUT_GPIO        CONFIG_APP_OUTPUT_GPIO
#else
#define LIGHT_OUTPUT_GPIO        1
#endif

#ifdef CONFIG_APP_PWM_FREQUENCY_HZ
#define LIGHT_PWM_FREQUENCY_HZ   CONFIG_APP_PWM_FREQUENCY_HZ
#else
#define LIGHT_PWM_FREQUENCY_HZ   5000
#endif
#define LIGHT_DEFAULT_ON         1
#define LIGHT_DEFAULT_OFF        0
#define LIGHT_DEFAULT_LEVEL      255

/**
* @brief Set light power (on/off).
*
* @param  power  The light power to be set
*/
void light_driver_set_power(bool power);

/**
* @brief Set PWM level on GPIO output.
*
* @param level PWM duty level in the 0-255 range
*/
void light_driver_set_level(uint8_t level);

/**
* @brief Initialize PWM output driver.
*
* @param power initial power on/off
* @param level initial PWM duty level (0-255)
*/
void light_driver_init(bool power, uint8_t level);

#ifdef __cplusplus
} // extern "C"
#endif
