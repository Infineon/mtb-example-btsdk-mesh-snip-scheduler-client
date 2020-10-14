/*
* Copyright 2016-2020, Cypress Semiconductor Corporation or a subsidiary of
* Cypress Semiconductor Corporation. All Rights Reserved.
*
* This software, including source code, documentation and related
* materials ("Software"), is owned by Cypress Semiconductor Corporation
* or one of its subsidiaries ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products. Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*/

/** @file
 *
 *
 * This file shows how to create a device which implements mesh scheduler client.
 */
#include "wiced_bt_ble.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_mesh_models.h"
#include "wiced_bt_trace.h"
#include "wiced_bt_mesh_app.h"

#ifdef HCI_CONTROL
#include "wiced_transport.h"
#include "hci_control_api.h"
#endif

#include "wiced_bt_cfg.h"
extern wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

/******************************************************
 *          Constants
 ******************************************************/
#define MESH_PID                0x3020
#define MESH_VID                0x0002
#define MESH_CACHE_REPLAY_SIZE  0x0008

/******************************************************
 *          Structures
 ******************************************************/

/******************************************************
 *          Function Prototypes
 ******************************************************/
static void mesh_app_init(wiced_bool_t is_provisioned);
static uint32_t mesh_app_proc_rx_cmd(uint16_t opcode, uint8_t *p_data, uint32_t length);
static void mesh_scheduler_client_message_handler(uint16_t event, wiced_bt_mesh_event_t *p_event, void *p_data);
static void mesh_scheduler_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_scheduler_action_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_scheduler_action_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_scheduler_hci_scheduler_status_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_scheduler_status_t *p_data);
static void mesh_scheduler_hci_scheduler_action_status_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_scheduler_action_data_t *p_data);

/******************************************************
 *          Variables Definitions
 ******************************************************/
uint8_t mesh_mfr_name[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MANUFACTURER_NAME] = { 'C', 'y', 'p', 'r', 'e', 's', 's', 0 };
uint8_t mesh_model_num[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MODEL_NUMBER]     = { '1', '2', '3', '4', 0, 0, 0, 0 };
uint8_t mesh_system_id[8]                                                  = { 0xbb, 0xb8, 0xa1, 0x80, 0x5f, 0x9f, 0x91, 0x71 };

wiced_bt_mesh_core_config_model_t   mesh_element1_models[] =
{
        WICED_BT_MESH_DEVICE,
        WICED_BT_MESH_MODEL_SCHEDULER_CLIENT,
};
#define MESH_APP_NUM_MODELS  (sizeof(mesh_element1_models) / sizeof(wiced_bt_mesh_core_config_model_t))

#define MESH_SCHEDULER_CLIENT_ELEMENT_INDEX   0

wiced_bt_mesh_core_config_element_t mesh_elements[] =
{
    {
        .location = MESH_ELEM_LOC_MAIN,                                 // location description as defined in the GATT Bluetooth Namespace Descriptors section of the Bluetooth SIG Assigned Numbers
        .default_transition_time = MESH_DEFAULT_TRANSITION_TIME_IN_MS,  // Default transition time for models of the element in milliseconds
        .onpowerup_state = WICED_BT_MESH_ON_POWER_UP_STATE_RESTORE,     // Default element behavior on power up
        .default_level = 0,                                             // Default value of the variable controlled on this element (for example power, lightness, temperature, hue...)
        .range_min = 1,                                                 // Minimum value of the variable controlled on this element (for example power, lightness, temperature, hue...)
        .range_max = 0xffff,                                            // Maximum value of the variable controlled on this element (for example power, lightness, temperature, hue...)
        .move_rollover = 0,                                             // If true when level gets to range_max during move operation, it switches to min, otherwise move stops.
        .properties_num = 0,                                            // Number of properties in the array models
        .properties = NULL,                                             // Array of properties in the element.
        .sensors_num = 0,                                               // Number of sensors in the sensor array
        .sensors = NULL,                                                // Array of sensors of that element
        .models_num = MESH_APP_NUM_MODELS,                              // Number of models in the array models
        .models = mesh_element1_models,                                 // Array of models located in that element. Model data is defined by structure wiced_bt_mesh_core_config_model_t
    },
};

wiced_bt_mesh_core_config_t  mesh_config =
{
    .company_id         = MESH_COMPANY_ID_CYPRESS,                  // Company identifier assigned by the Bluetooth SIG
    .product_id         = MESH_PID,                                 // Vendor-assigned product identifier
    .vendor_id          = MESH_VID,                                 // Vendor-assigned product version identifier
    .replay_cache_size  = MESH_CACHE_REPLAY_SIZE,                   // Number of replay protection entries, i.e. maximum number of mesh devices that can send application messages to this device.
#if LOW_POWER_NODE
    .features           = WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER, // A bit field indicating the device features. In Low Power mode no Relay, no Proxy and no Friend
    .friend_cfg         =                                           // Empty Configuration of the Friend Feature
    {
        .receive_window = 0,                                        // Receive Window value in milliseconds supported by the Friend node.
        .cache_buf_len  = 0,                                        // Length of the buffer for the cache
        .max_lpn_num    = 0                                         // Max number of Low Power Nodes with established friendship. Must be > 0 if Friend feature is supported.
    },
    .low_power          =                                           // Configuration of the Low Power Feature
    {
        .rssi_factor           = 2,                                 // contribution of the RSSI measured by the Friend node used in Friend Offer Delay calculations.
        .receive_window_factor = 2,                                 // contribution of the supported Receive Window used in Friend Offer Delay calculations.
        .min_cache_size_log    = 3,                                 // minimum number of messages that the Friend node can store in its Friend Cache.
        .receive_delay         = 100,                               // Receive delay in 1 ms units to be requested by the Low Power node.
        .poll_timeout          = 36000                              // Poll timeout in 100ms units to be requested by the Low Power node.
    },
#else
    .features           = WICED_BT_MESH_CORE_FEATURE_BIT_FRIEND | WICED_BT_MESH_CORE_FEATURE_BIT_RELAY,   // In Friend mode support friend, relay
    .friend_cfg         =                                           // Configuration of the Friend Feature(Receive Window in Ms, messages cache)
    {
        .receive_window        = 20,
        .cache_buf_len         = 300,                               // Length of the buffer for the cache
        .max_lpn_num           = 4                                  // Max number of Low Power Nodes with established friendship. Must be > 0 if Friend feature is supported.
    },
    .low_power          =                                           // Configuration of the Low Power Feature
    {
        .rssi_factor           = 0,                                 // contribution of the RSSI measured by the Friend node used in Friend Offer Delay calculations.
        .receive_window_factor = 0,                                 // contribution of the supported Receive Window used in Friend Offer Delay calculations.
        .min_cache_size_log    = 0,                                 // minimum number of messages that the Friend node can store in its Friend Cache.
        .receive_delay         = 0,                                 // Receive delay in 1 ms units to be requested by the Low Power node.
        .poll_timeout          = 0                                  // Poll timeout in 100ms units to be requested by the Low Power node.
    },
#endif
    .gatt_client_only          = WICED_FALSE,                       // Can connect to mesh over GATT or ADV
    .elements_num  = (uint8_t)(sizeof(mesh_elements) / sizeof(mesh_elements[0])),   // number of elements on this device
    .elements      = mesh_elements                                  // Array of elements for this device
};

/*
 * Mesh application library will call into application functions if provided by the application.
 */
wiced_bt_mesh_app_func_table_t wiced_bt_mesh_app_func_table =
{
    mesh_app_init,          // application initialization
    NULL,                   // Default SDK platform button processing
    NULL,                   // GATT connection status
    NULL,                   // attention processing
    NULL,                   // notify period set
    mesh_app_proc_rx_cmd,   // WICED HCI command
    NULL,                   // LPN sleep
    NULL                    // factory reset
};

/******************************************************
 *               Function Definitions
 ******************************************************/
void mesh_app_init(wiced_bool_t is_provisioned)
{
    wiced_bt_cfg_settings.device_name = (uint8_t *)"Scheduler Client";
    wiced_bt_cfg_settings.gatt_cfg.appearance = APPEARANCE_GENERIC_TAG;
    // Adv Data is fixed. Spec allows to put URI, Name, Appearance and Tx Power in the Scan Response Data.
    if (!is_provisioned)
    {
        wiced_bt_ble_advert_elem_t  adv_elem[3];
        uint8_t                     buf[2];
        uint8_t                     num_elem = 0;
        adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
        adv_elem[num_elem].len = (uint16_t)strlen((const char*)wiced_bt_cfg_settings.device_name);
        adv_elem[num_elem].p_data = wiced_bt_cfg_settings.device_name;
        num_elem++;

        adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_APPEARANCE;
        adv_elem[num_elem].len = 2;
        buf[0] = (uint8_t)wiced_bt_cfg_settings.gatt_cfg.appearance;
        buf[1] = (uint8_t)(wiced_bt_cfg_settings.gatt_cfg.appearance >> 8);
        adv_elem[num_elem].p_data = buf;
        num_elem++;

        wiced_bt_mesh_set_raw_scan_response_data(num_elem, adv_elem);
    }

    // register with the library to receive parsed data
    wiced_bt_mesh_model_scheduler_client_init(mesh_scheduler_client_message_handler, is_provisioned);
}

/*
 * Process event received from the scheduler Server.
 */
void mesh_scheduler_client_message_handler(uint16_t event, wiced_bt_mesh_event_t *p_event, void *p_data)
{
    wiced_bt_mesh_scheduler_status_t      *p_scheduler_status;
    wiced_bt_mesh_scheduler_action_data_t *p_scheduler_action_status;
#if defined HCI_CONTROL
    wiced_bt_mesh_hci_event_t *p_hci_event = wiced_bt_mesh_create_hci_event(p_event);
    if (p_hci_event == NULL)
        return;
#endif

    WICED_BT_TRACE("scheduler clt msg:%d\n", event);

    switch (event)
    {
    case WICED_BT_MESH_TX_COMPLETE:
        WICED_BT_TRACE("tx complete status:%d\n", p_event->status.tx_flag);
#if defined HCI_CONTROL
        wiced_bt_mesh_send_hci_tx_complete(p_hci_event, p_event);
#endif
        break;

    case WICED_BT_MESH_SCHEDULER_STATUS:
        p_scheduler_status = (wiced_bt_mesh_scheduler_status_t *)p_data;
#if defined HCI_CONTROL
        mesh_scheduler_hci_scheduler_status_event_send(p_hci_event, p_scheduler_status);
#endif
        break;

    case WICED_BT_MESH_SCHEDULER_ACTION_STATUS:
        p_scheduler_action_status = (wiced_bt_mesh_scheduler_action_data_t *)p_data;
#if defined HCI_CONTROL
        mesh_scheduler_hci_scheduler_action_status_event_send(p_hci_event, p_scheduler_action_status);
#endif
        break;

    default:
        WICED_BT_TRACE("unknown\n");
        break;
    }
    wiced_bt_mesh_release_event(p_event);
}

/*
 * In 2 chip solutions MCU can send commands to change scheduler state.
 */
uint32_t mesh_app_proc_rx_cmd(uint16_t opcode, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_event_t *p_event;

    WICED_BT_TRACE("[%s] cmd_opcode 0x%02x\n", __FUNCTION__, opcode);

    switch (opcode)
    {
    case HCI_CONTROL_MESH_COMMAND_SCHEDULER_GET:
    case HCI_CONTROL_MESH_COMMAND_SCHEDULER_ACTION_GET:
    case HCI_CONTROL_MESH_COMMAND_SCHEDULER_ACTION_SET:
        break;

    default:
        WICED_BT_TRACE("unknown\n");
        return WICED_FALSE;
    }
    p_event = wiced_bt_mesh_create_event_from_wiced_hci(opcode, MESH_COMPANY_ID_BT_SIG, WICED_BT_MESH_CORE_MODEL_ID_SCHEDULER_CLNT, &p_data, &length);
    if (p_event == NULL)
    {
        WICED_BT_TRACE("bad hdr\n");
        return WICED_TRUE;
    }
    switch (opcode)
    {
    case HCI_CONTROL_MESH_COMMAND_SCHEDULER_GET:
        mesh_scheduler_get(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_SCHEDULER_ACTION_GET:
        mesh_scheduler_action_get(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_SCHEDULER_ACTION_SET:
        mesh_scheduler_action_set(p_event, p_data, length);
        break;
    }
    return WICED_TRUE;
}

/*
 * Send Scheduler Get command
 */
void mesh_scheduler_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_model_scheduler_client_send_get(p_event);
}

/*
 * Send Scheduler Action Get command
 */
void mesh_scheduler_action_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_scheduler_action_get_t request;

    STREAM_TO_UINT8(request.action_number, p_data);

    wiced_bt_mesh_model_scheduler_client_send_action_get(p_event, &request);
}

/*
 * Send Scheduler Action Get command
 */
void mesh_scheduler_action_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_scheduler_action_data_t request;

    STREAM_TO_UINT8(request.action_number, p_data);
    STREAM_TO_UINT8(request.year, p_data);
    STREAM_TO_UINT16(request.month, p_data);
    STREAM_TO_UINT8(request.day, p_data);
    STREAM_TO_UINT8(request.hour, p_data);
    STREAM_TO_UINT8(request.minute, p_data);
    STREAM_TO_UINT8(request.second, p_data);
    STREAM_TO_UINT8(request.day_of_week, p_data);
    STREAM_TO_UINT8(request.action, p_data);
    STREAM_TO_UINT32(request.transition_time, p_data);
    STREAM_TO_UINT16(request.scene_number, p_data);

    wiced_bt_mesh_model_scheduler_client_send_action_set(p_event, &request);
}

#ifdef HCI_CONTROL
/*
 * Send Scheduler Status event over transport
 */
void mesh_scheduler_hci_scheduler_status_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_scheduler_status_t *p_data)
{
    int i;
    uint8_t *p = p_hci_event->data;

    UINT16_TO_STREAM(p, p_data->actions);

    mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_SCHEDULER_STATUS, (uint8_t *)p_hci_event, (uint16_t)(p - (uint8_t *)p_hci_event));
}

/*
 * Send scheduler Status event over transport
 */
void mesh_scheduler_hci_scheduler_action_status_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_scheduler_action_data_t *p_data)
{
    uint8_t *p = p_hci_event->data;

    UINT8_TO_STREAM(p, p_data->action_number);
    UINT8_TO_STREAM(p, p_data->year);
    UINT16_TO_STREAM(p, p_data->month);
    UINT8_TO_STREAM(p, p_data->day);
    UINT8_TO_STREAM(p, p_data->hour);
    UINT8_TO_STREAM(p, p_data->minute);
    UINT8_TO_STREAM(p, p_data->second);
    UINT8_TO_STREAM(p, p_data->day_of_week);
    UINT8_TO_STREAM(p, p_data->action);
    UINT32_TO_STREAM(p, p_data->transition_time);
    UINT16_TO_STREAM(p, p_data->scene_number);

    mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_SCHEDULER_ACTION_STATUS, (uint8_t *)p_hci_event, (uint16_t)(p - (uint8_t *)p_hci_event));
}

#endif
