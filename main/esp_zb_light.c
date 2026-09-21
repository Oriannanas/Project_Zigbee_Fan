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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include "alarm_timer.h"
#include "ota_file_parser.h"
#include "esp_zb_light.h"

static const char *TAG = "ESP_ZB_PWM_FAN";

/* OTA upgrade client state */
static const esp_partition_t    *s_ota_partition   = NULL;
static esp_zb_ota_file_parser_t *s_ota_file_parser = NULL;
static esp_ota_handle_t          s_ota_handle      = 0;

/********************* Define functions **************************/

static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;

    ESP_RETURN_ON_FALSE(!is_inited, ESP_OK, TAG, "Deferred driver already initialized");

    light_driver_init(LIGHT_DEFAULT_OFF, LIGHT_DEFAULT_LEVEL);
    is_inited = true;

    return ESP_OK;
}

static void esp_zigbee_alarm_bdb_commissioning(alarm_timer_arg_t arg)
{
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_bdb_start_top_level_commissioning(arg);
    esp_zigbee_lock_release();
}

static bool esp_zigbee_app_signal_handler(const ezb_app_signal_t *app_signal)
{
    ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(app_signal);

    switch (signal_type) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
        break;
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case EZB_BDB_SIGNAL_DEVICE_REBOOT: {
        ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", ezb_bdb_is_factory_new() ? "" : " non");
            if (ezb_bdb_is_factory_new()) {
                ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device reboot");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status(0x%02x), please retry", ezb_app_signal_to_string(signal_type), status);
            alarm_timer_schedule(esp_zigbee_alarm_bdb_commissioning, EZB_BDB_MODE_INITIALIZATION, 1000);
        }
    } break;
    case EZB_BDB_SIGNAL_STEERING: {
        ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
        if (status == EZB_BDB_STATUS_SUCCESS) {
            ezb_extpanid_t extended_pan_id;
            ezb_nwk_get_extended_panid(&extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully: PAN ID(0x%04hx, EXT: 0x%llx), Channel(%d), Short Address(0x%04hx)",
                     ezb_nwk_get_panid(), extended_pan_id.u64, ezb_nwk_get_current_channel(), ezb_nwk_get_short_address());
            /* Confirm this image works before the rollback-safety window expires */
            esp_ota_mark_app_valid_cancel_rollback();
        } else {
            ESP_LOGW(TAG, "Failed to join network with status(0x%02x)", status);
            alarm_timer_schedule(esp_zigbee_alarm_bdb_commissioning, EZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
    } break;
    case EZB_ZDO_SIGNAL_DEVICE_ANNCE: {
        const ezb_zdo_signal_device_annce_params_t *dev_annce_params = ezb_app_signal_get_params(app_signal);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->short_addr);
    } break;
    case EZB_ZDO_SIGNAL_LEAVE: {
        const ezb_zdo_signal_leave_params_t *leave_params = ezb_app_signal_get_params(app_signal);
        ESP_LOGI(TAG, "Left network successfully with type(0x%02x)", leave_params->leave_type);
    } break;
    default:
        ESP_LOGI(TAG, "Zigbee APP Signal: %s(type: 0x%02x)", ezb_app_signal_to_string(signal_type), signal_type);
        break;
    }
    return true;
}

static void fan_set_on_off_attribute(const ezb_zcl_attribute_t *attribute)
{
    ESP_RETURN_ON_FALSE(attribute, , TAG, "attribute is invalid");
    switch (attribute->id) {
    case EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:
        light_driver_set_power(*(uint8_t *)attribute->data.value);
        ESP_LOGI(TAG, "Fan sets to %s", *(uint8_t *)attribute->data.value ? "On" : "Off");
        break;
    default:
        ESP_LOGW(TAG, "Unsupported attribute ID(0x%04x)", attribute->id);
        break;
    }
}

static void fan_set_level_attribute(const ezb_zcl_attribute_t *attribute)
{
    ESP_RETURN_ON_FALSE(attribute, , TAG, "attribute is invalid");
    switch (attribute->id) {
    case EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID:
        light_driver_set_level(*(uint8_t *)attribute->data.value);
        ESP_LOGI(TAG, "Fan speed sets to %u/254", *(uint8_t *)attribute->data.value);
        break;
    case EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID: {
        const uint16_t deciseconds = *(uint16_t *)attribute->data.value;
        light_driver_set_fade_time_ms((uint32_t)deciseconds * 100);
        ESP_LOGI(TAG, "Fan fade time sets to %u ms", (unsigned)deciseconds * 100);
    } break;
    default:
        ESP_LOGW(TAG, "Unsupported attribute ID(0x%04x)", attribute->id);
        break;
    }
}

static void zcl_core_set_attr_value_handler(ezb_zcl_set_attr_value_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, , TAG, "message is empty");
    ESP_LOGI(TAG, "ZCL SetAttributeValue message for endpoint(%d) cluster(0x%04x) %s with status(0x%02x)", message->info.dst_ep,
             message->info.cluster_id, message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client",
             message->info.status);
    switch (message->info.cluster_id) {
    case EZB_ZCL_CLUSTER_ID_ON_OFF:
        fan_set_on_off_attribute(&message->in.attribute);
        break;
    case EZB_ZCL_CLUSTER_ID_LEVEL:
        fan_set_level_attribute(&message->in.attribute);
        break;
    default:
        ESP_LOGW(TAG, "Unsupported cluster ID(0x%04x)", message->info.cluster_id);
    }
}

static void zcl_ota_upgrade_client_progress_handler(ezb_zcl_ota_upgrade_client_progress_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "-- OTA Upgrade Client Progress");

    switch (message->in.progress) {
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_START:
        ESP_LOGI(TAG, "OTA Start: manuf_code=0x%04x, image_type=0x%04x, file_version=0x%08lx, image_size=%ld",
                 message->in.start.manuf_code, message->in.start.image_type, message->in.start.file_version,
                 message->in.start.image_size);

        s_ota_partition = esp_ota_get_next_update_partition(NULL);
        ESP_GOTO_ON_FALSE(s_ota_partition, ESP_ERR_NOT_FOUND, exit, TAG, "No OTA partition found");
        s_ota_file_parser = esp_zb_create_ota_file_parser(message->in.start.image_size);
        ESP_GOTO_ON_FALSE(s_ota_file_parser, ESP_ERR_NOT_FOUND, exit, TAG, "Failed to create OTA file parser");
        ret = esp_ota_begin(s_ota_partition, 0, &s_ota_handle);
        ESP_GOTO_ON_FALSE(s_ota_handle, ESP_ERR_NOT_FOUND, exit, TAG, "Failed to start OTA");
        break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_RECEIVING:
        ESP_LOGI(TAG, "OTA Receiving Block: file_offset=%ld, block_size=%d", message->in.receiving.file_offset,
                 message->in.receiving.block_size);
        esp_zb_ota_file_parser_setup(s_ota_file_parser, message->in.receiving.block_size, message->in.receiving.block);
        do {
            ret = esp_zb_ota_file_parser_process(s_ota_file_parser);
            ESP_LOGW(TAG, "In progress: [%ld / %ld]", message->in.receiving.file_offset + message->in.receiving.block_size, s_ota_file_parser->total_image_size);
            if (esp_zb_ota_file_parser_is_element_value(s_ota_file_parser)) {
                switch (s_ota_file_parser->element.type) {
                case UPGRADE_IMAGE:
                    ESP_GOTO_ON_FALSE(s_ota_file_parser->element.total <= s_ota_partition->size, ESP_ERR_INVALID_SIZE, exit,
                                      TAG, "OTA image size exceeds partition size");
                    ESP_GOTO_ON_ERROR(esp_ota_write(s_ota_handle, s_ota_file_parser->element.val, s_ota_file_parser->element.length), exit,
                                      TAG, "Failed to write OTA image");
                    break;
                default:
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, s_ota_file_parser->element.val, s_ota_file_parser->element.length,
                                             ESP_LOG_WARN);
                    break;
                }
            }
        } while (ret == ESP_ERR_NOT_FINISHED);
        ret = ESP_OK;
        break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_CHECK:
        ESP_LOGI(TAG, "OTA Check: manuf_code=0x%04x, image_type=0x%04x, file_version=0x%08lx", message->in.check.manuf_code,
                 message->in.check.image_type, message->in.check.file_version);
        ret = esp_zb_ota_file_parser_check(s_ota_file_parser);
        break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_APPLY:
        ESP_LOGI(TAG, "OTA Apply: manuf_code=0x%04x, image_type=0x%04x, file_version=0x%08lx", message->in.apply.manuf_code,
                 message->in.apply.image_type, message->in.apply.file_version);
        ret = esp_ota_end(s_ota_handle);
        ESP_GOTO_ON_ERROR(ret, exit, TAG, "Failed to end OTA: error: %s", esp_err_to_name(ret));
        ret = esp_ota_set_boot_partition(s_ota_partition);
        ESP_GOTO_ON_ERROR(ret, exit, TAG, "Failed to set OTA boot partition, error: %s", esp_err_to_name(ret));
        break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_FINISH:
        ESP_LOGI(TAG, "OTA Finish: count_down_delay=%ld seconds", message->in.finish.count_down_delay);
        esp_restart();
        break;
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_ABORT:
        ret = esp_ota_abort(s_ota_handle);
        ESP_LOGW(TAG, "OTA Abort");
        break;
    default:
        ESP_LOGW(TAG, "Unknown OTA progress status: %d", message->in.progress);
        message->out.result = EZB_ZCL_STATUS_SUCCESS;
        break;
    }

exit:
    message->out.result = ret == ESP_OK ? EZB_ZCL_STATUS_SUCCESS : EZB_ZCL_STATUS_ABORT;
}

static void zcl_ota_upgrade_client_query_next_image_rsp_handler(ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *message)
{
    ESP_LOGI(TAG, "-- OTA Upgrade Query Next Image Response");

    if (message->in.image.status == EZB_ZCL_OTA_UPGRADE_STATUS_CODE_SUCCESS) {
        ESP_LOGI(TAG, "New image available:");
        ESP_LOGI(TAG, "  Manufacturer code: 0x%04x", message->in.image.manuf_code);
        ESP_LOGI(TAG, "  Image type: 0x%04x", message->in.image.image_type);
        ESP_LOGI(TAG, "  File version: 0x%08lx", message->in.image.file_version);
        ESP_LOGI(TAG, "  Image size: %ld bytes", message->in.image.size);
    } else {
        ESP_LOGI(TAG, "No image available, status: 0x%02x", message->in.image.status);
    }
    message->out.result = EZB_ZCL_STATUS_SUCCESS;
}

static void esp_zigbee_zcl_core_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message)
{
    switch (callback_id) {
    case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
        zcl_core_set_attr_value_handler(message);
        break;
    case EZB_ZCL_CORE_OTA_UPGRADE_CLIENT_PROGRESS_CB_ID:
        zcl_ota_upgrade_client_progress_handler(message);
        break;
    case EZB_ZCL_CORE_OTA_UPGRADE_QUERY_NEXT_IMAGE_RSP_CB_ID:
        zcl_ota_upgrade_client_query_next_image_rsp_handler(message);
        break;
    case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID: {
        ezb_zcl_cmd_default_rsp_message_t *default_rsp = (ezb_zcl_cmd_default_rsp_message_t *)message;
        ESP_LOGI(TAG, "Received ZCL Default Response with status(0x%02x)", default_rsp->in.status_code);
    } break;
    default:
        ESP_LOGW(TAG, "Application Action: 0x%08lx", callback_id);
        break;
    }
}

esp_err_t esp_zigbee_create_zha_fan_device(void)
{
    ezb_zha_dimmable_light_config_t fan_cfg = EZB_ZHA_DIMMABLE_LIGHT_CONFIG();
    fan_cfg.on_off_cfg.on_off     = LIGHT_DEFAULT_OFF;
    fan_cfg.level_cfg.current_level = LIGHT_DEFAULT_LEVEL;

    ezb_af_device_desc_t   dev_desc   = ezb_af_create_device_desc();
    ezb_af_ep_desc_t       ep_desc    = ezb_zha_create_dimmable_light(HA_ESP_FAN_ENDPOINT, &fan_cfg);
    ezb_zcl_cluster_desc_t basic_desc = ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
    ezb_zcl_basic_cluster_desc_add_attr(basic_desc, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)ESP_MANUFACTURER_NAME);
    ezb_zcl_basic_cluster_desc_add_attr(basic_desc, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)ESP_MODEL_IDENTIFIER);

    /* Repurpose the standard Level Control "OnOffTransitionTime" attribute (in 1/10s units)
     * as a Z2M/HA-configurable fade duration for light_driver's PWM fade - see
     * fan_set_level_attribute() and light_driver_set_fade_time_ms(). */
    ezb_zcl_cluster_desc_t level_desc = ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER);
    const uint16_t default_fade_deciseconds = LIGHT_FADE_TIME_MS / 100;
    ezb_zcl_level_cluster_desc_add_attr(level_desc, EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID, (void *)&default_fade_deciseconds);

    ezb_zcl_ota_upgrade_cluster_client_config_t ota_client_cfg = {
        .upgrade_server_id    = EZB_ZCL_OTA_UPGRADE_UPGRADE_SERVER_ID_DEFAULT_VALUE,
        .file_offset          = 0,
        .image_upgrade_status = EZB_ZCL_OTA_UPGRADE_IMAGE_UPGRADE_STATUS_DEFAULT_VALUE,
        .manufacturer_id      = OTA_UPGRADE_MANUFACTURER,
        .image_type_id        = OTA_UPGRADE_IMAGE_TYPE,
    };
    ezb_zcl_cluster_desc_t ota_client_desc = ezb_zcl_ota_upgrade_create_cluster_desc(&ota_client_cfg, EZB_ZCL_CLUSTER_CLIENT);

    /* CurrentFileVersion isn't part of ezb_zcl_ota_upgrade_cluster_client_config_t, so it defaults
     * to the ZCL sentinel 0xffffffff (unknown) unless set explicitly - which left this device
     * reporting a null/unknown current version in its own queryNextImageRequest, causing Z2M/the
     * coordinator to always answer NO_IMAGE_AVAILABLE regardless of what image was offered. */
    const uint32_t running_file_version = OTA_UPGRADE_RUNNING_FILE_VERSION;
    ezb_zcl_ota_upgrade_cluster_desc_add_attr(ota_client_desc, EZB_ZCL_ATTR_OTA_UPGRADE_CURRENT_FILE_VERSION_ID, (void *)&running_file_version);

    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc, ota_client_desc));

    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
    ESP_ERROR_CHECK(ezb_af_device_desc_register(dev_desc));

    /* ezb_zha_create_dimmable_light() pre-populates the Basic cluster's ManufacturerName/
     * ModelIdentifier with its own placeholder defaults; the cluster_desc_add_attr() calls
     * above only declare the attribute at build time and don't overwrite an existing value.
     * Write the real values into the live ZCL data model now that the endpoint is registered. */
    ezb_zcl_status_t manuf_status = ezb_zcl_set_attr_value(HA_ESP_FAN_ENDPOINT, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER,
                                                            EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, EZB_ZCL_STD_MANUF_CODE,
                                                            (void *)ESP_MANUFACTURER_NAME, false);
    ezb_zcl_status_t model_status = ezb_zcl_set_attr_value(HA_ESP_FAN_ENDPOINT, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER,
                                                            EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, EZB_ZCL_STD_MANUF_CODE,
                                                            (void *)ESP_MODEL_IDENTIFIER, false);
    ESP_LOGI(TAG, "Set manufacturer/model attrs: status(0x%02x)/(0x%02x)", manuf_status, model_status);

    ezb_zcl_status_t fade_status = ezb_zcl_set_attr_value(HA_ESP_FAN_ENDPOINT, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                                                           EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID, EZB_ZCL_STD_MANUF_CODE,
                                                           (void *)&default_fade_deciseconds, false);
    ESP_LOGI(TAG, "Set fade time attr: status(0x%02x)", fade_status);

    ezb_zcl_status_t ota_version_status = ezb_zcl_set_attr_value(HA_ESP_FAN_ENDPOINT, EZB_ZCL_CLUSTER_ID_OTA_UPGRADE, EZB_ZCL_CLUSTER_CLIENT,
                                                                  EZB_ZCL_ATTR_OTA_UPGRADE_CURRENT_FILE_VERSION_ID, EZB_ZCL_STD_MANUF_CODE,
                                                                  (void *)&running_file_version, false);
    ESP_LOGI(TAG, "Set OTA current file version attr: status(0x%02x)", ota_version_status);

    ezb_zcl_ota_upgrade_set_download_block_size(HA_ESP_FAN_ENDPOINT, OTA_UPGRADE_DOWNLOAD_BLOCK_SIZE);

    ezb_zcl_core_action_handler_register(esp_zigbee_zcl_core_action_handler);

    return ESP_OK;
}

esp_err_t esp_zigbee_setup_commissioning(void)
{
    ezb_aps_secur_enable_distributed_security(false);
    ESP_ERROR_CHECK(ezb_bdb_set_primary_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(ezb_bdb_set_secondary_channel_set(ESP_ZB_SECONDARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(ezb_app_signal_add_handler(esp_zigbee_app_signal_handler));

    return ESP_OK;
}

static void esp_zigbee_stack_main_task(void *pvParameters)
{
    esp_zigbee_config_t config = ESP_ZIGBEE_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(esp_zigbee_init(&config));

    ESP_ERROR_CHECK(esp_zigbee_setup_commissioning());

    ESP_ERROR_CHECK(esp_zigbee_create_zha_fan_device());

    ESP_ERROR_CHECK(esp_zigbee_start(false));

    esp_zigbee_launch_mainloop();

    esp_zigbee_deinit();

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "Start ESP Zigbee Stack");
    xTaskCreate(esp_zigbee_stack_main_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
