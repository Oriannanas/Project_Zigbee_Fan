/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier:  LicenseRef-Included
 *
 * Grille Fan - Zigbee PWM fan controller
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "light_driver.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false                      /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                EZB_NWK_ED_TIMEOUT_64MIN   /* aging timeout of device */
#define ED_KEEP_ALIVE                   3000                       /* 3000 millisecond */
#define HA_ESP_FAN_ENDPOINT             10                         /* esp fan device endpoint, used to process fan on/off + speed commands */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     0x07FFF800U                /* channel 11-26, all 2.4GHz Zigbee channels */
#define ESP_ZB_SECONDARY_CHANNEL_MASK   0U

/* OTA upgrade client configuration */
#define OTA_UPGRADE_MANUFACTURER               0x131B                        /* Manufacturer code, must match the OTA image pushed by the coordinator/Z2M */
#define OTA_UPGRADE_IMAGE_TYPE                  0x1011                       /* Product/image type, must match the OTA image */
#define OTA_UPGRADE_RUNNING_FILE_VERSION        0x01010001                   /* Bump this (and the built .ota's fileVersion) on every release */
#define OTA_UPGRADE_DOWNLOAD_BLOCK_SIZE          223                         /* Max bytes requested per OTA image block */

/* Basic manufacturer information.
 * This is the (manufacturerName, modelIdentifier) pair a Zigbee2MQTT external
 * converter matches on to expose this endpoint as a fan instead of a light -
 * see docs/zigbee2mqtt-fan-converter.md. Changing these strings changes the
 * device signature, so Z2M will treat it as a new device on next join. */
#define ESP_MANUFACTURER_NAME "\x08""DIY_Fans"     /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x0A""C6_PWM_Fan"    /* Customized model identifier */

#define ESP_ZIGBEE_STORAGE_PARTITION_NAME "nvs"

#define ESP_ZIGBEE_ZED_CONFIG()                        \
    {                                                  \
        .device_type = EZB_NWK_DEVICE_TYPE_END_DEVICE, \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
        .zed_config = {                                \
            .ed_timeout = ED_AGING_TIMEOUT,            \
            .keep_alive = ED_KEEP_ALIVE,               \
        },                                             \
    }

#define ESP_ZIGBEE_PLATFORM_CONFIG()                                 \
    {                                                                \
        .storage_partition_name = ESP_ZIGBEE_STORAGE_PARTITION_NAME, \
        .radio_config = {                                            \
            .radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE,              \
        },                                                           \
    }

#define ESP_ZIGBEE_DEFAULT_CONFIG()                      \
    {                                                     \
        .device_config = ESP_ZIGBEE_ZED_CONFIG(),        \
        .platform_config = ESP_ZIGBEE_PLATFORM_CONFIG(), \
    }
