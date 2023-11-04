/*
 *  Bluetooth.cpp
 *
 *  Created on: 24 Jan 2019
 *  Author: mark
 */

#include "sdkconfig.h"

#include "BluetoothRequestHandler.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "Logging.hpp"
#include "Encoders.hpp"
#include "Mapping.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Determine if this class is instantiated already (i.e. this should be a singleton).
 */
bool BluetoothRequestHandler::_instantiated = false;
BluetoothManager BluetoothRequestHandler::_manager;
BluetoothRequestHandler *BluetoothRequestHandler::_instance = NULL;

const char *BluetoothRequestHandler::COMPONENT_NAME = "BTTask";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the BluetoothRequestHandler class.
 */
BluetoothRequestHandler::BluetoothRequestHandler()
{
    if (_instantiated)
    {
        throw new MultipleInstancesException();
    }
    else
    {
        _instantiated = true;
    }
}

BluetoothRequestHandler* BluetoothRequestHandler::GetInstance()
{
    return _instance;
}

BluetoothRequestHandler* BluetoothRequestHandler::GetInstance(IMessageDispatcher *d)
{
    if (!_instance)
    {
        _instance = new BluetoothRequestHandler(d);
        _instance->Setup();
    }

    return _instance;
}

/**
 *  @brief Create a new BluetoothRequestHandler object.
 *
 *  @param messageDispatcher
 *      Message Dispatcher that can be used to send messages to the STM32.
 */
BluetoothRequestHandler::BluetoothRequestHandler(IMessageDispatcher *messageDispatcher) : BluetoothRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  @brief Default destructor for the bluetooth class.
 */
BluetoothRequestHandler::~BluetoothRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the bluetooth system.
 */
void BluetoothRequestHandler::Setup()
{
    TRACE_MESSAGE("Setup: Enter");

    RequestHandlerBase::Setup();

    _xBtEventGroup = xEventGroupCreate();
    xEventGroupSetBits(_xBtEventGroup, BT_READY_BIT);


    ESP_LOGI(__func__, "mem release BT controller...");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_LOGI(__func__, "enable BT controller...");
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGI(__func__, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    xTaskCreate(Task, _taskName, 4096, this, configMAX_PRIORITIES - 1, &_taskHandle);

    TRACE_MESSAGE("Setup: Exit");
}

void BluetoothRequestHandler::GetHandles(Message *request)
{
    Esp32Messaging::BTGetHandlesResponse response;
    response.HandleCount = _manager.GetHandleCount();
    response.HandlesLength = response.HandleCount * 2;
    response.Handles = static_cast<uint8_t *>(pvPortMalloc(response.HandlesLength));
    _manager.GetHandles((uint16_t*)response.Handles);

    request->PayloadLength = Encoders::EncodedBTGetHandlesResponseBufferSize(&response);
    request->Payload = static_cast<uint8_t *>(pvPortMalloc(request->PayloadLength));
    Encoders::EncodeBTGetHandlesResponse(&response, request->Payload);
    request->MessageType = MessageTypes::Response;
    request->StatusCode = StatusCodes::CompletedOk;

    _messageDispatcher->QueueMessageForStm32(request);
}

/**
 *  @brief Dispatch bluetooth messages to the appropriate method.
 *
 *  @param request
 *      Bluetooth request being made by the STM32.
 */
void BluetoothRequestHandler::DispatchRequest(Message *request)
{
    TRACE_MESSAGE("DispatchRequest: Enter");

    TRACE_MESSAGE("BT Message: function %i", request->Function);

    xEventGroupClearBits(_xBtEventGroup, BT_READY_BIT);
    switch (request->Function)
    {
        case BluetoothFunction::Start:
            CreateAttributeGraph(request);

            request->PayloadLength = 0;
            request->MessageType = MessageTypes::Response;
            request->StatusCode = StatusCodes::CompletedOk;

            _messageDispatcher->QueueMessageForStm32(request);

            break;
        case BluetoothFunction::ServerDataSet:
            {
                Esp32Messaging::BTServerDataSet *ds = Encoders::ExtractBTServerDataSet(request->Payload);
                vPortFree(request->Payload);
                request->Payload = NULL; // we're re-using the message.  We free this, but we *must* null it, else the receiver when we send back will try to free it again

                TRACE_MESSAGE("Server wrote %i bytes to 0x%04x", ds->SetDataLength, ds->Handle);

                // update the local
                _manager.SetCurrentValueForAttributeHandle(ds->Handle, ds->SetData, ds->SetDataLength);

                request->PayloadLength = 0;
                request->MessageType = MessageTypes::Response;
                request->StatusCode = StatusCodes::CompletedOk;

                _messageDispatcher->QueueMessageForStm32(request);
            }
            break;
        case BluetoothFunction::GetHandles:
            GetHandles(request);
            break;
        case BluetoothFunction::Stop:
            TeardownStack(request);
            break;
        default:
            TRACE_MESSAGE("Unknown Bluetooth message received, function %d", request->Function);
            if (request->Payload != NULL)
            {
                TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
            }
            break;
    }
//    xEventGroupWaitBits(_xBtEventGroup, BT_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    TRACE_MESSAGE("DispatchRequest: Exit");
}

/**
 *  @brief Event loop for the IDF system.
 *
 *  Perform the actual event loop handling on behalf of the static method set up for the
 *  IDF event handler.
 *
 *  See the notes for the WiFiRequestHandler::EventHandler for more information as to why this
 *  method is necessary.
 *
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 *
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 *
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void BluetoothRequestHandler::EventHandlerHelper(esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    TRACE_MESSAGE("EventHandlerHelper: Enter with base %s and id 0x%x", eventBase, eventId);

    TRACE_MESSAGE("EventHandlerHelper: Exit");
}

/**
 *  @brief Event loop for the IDF system.
 *
 *  Event loop (handler) for the Bluetooth events as discussed in the following ESP-IDF document:
 *
 *  https://docs.espressif.com/projects/esp-idf/en/release-v4.1/api-reference/system/esp_event.html
 *
 *  This method passes the data on to an instance helper method.  This is done for convenience as it
 *  allows access to the member variables and methods without having to use the sender context or
 *  convert methods / variables / constants to static members.
 *
 *  @param arg
 *      Argument data specified when the event loop was first registered with
 *      esp_event_handler_register.  arg should point to the instance of the WiFiRequestHandler
 *      that registered the event loop.
 *
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 *
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 *
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void BluetoothRequestHandler::EventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    TRACE_MESSAGE("EventHandler: Enter");

    BluetoothRequestHandler *sender = static_cast<BluetoothRequestHandler *>(arg);
    sender->EventHandlerHelper(eventBase, eventId, eventData);

    TRACE_MESSAGE("EventHandler: Exit");
}

/**
 * @brief Prepare the Bluetooth interface for deep sleep.
 */
StatusCodes::StatusCodes BluetoothRequestHandler::PrepareForDeepSleep()
{
    TRACE_MESSAGE("PrepareForDeepSleep(): Enter");

    BluetoothRequestHandler *bluetoothHandler = BluetoothRequestHandler::GetInstance();
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (bluetoothHandler != NULL)
    {
        result = bluetoothHandler->TeardownStack(nullptr);
    }
    return(result);

    TRACE_MESSAGE("PrepareForDeepSleep(): Enter");
}

void BluetoothRequestHandler::StartStack()
{
    TRACE_MESSAGE("StartStack: Enter");

    esp_err_t ret;

    TRACE_MESSAGE("Enabling BT controller");

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ERROR_MESSAGE("esp_bt_controller_enable failed %i\n", ret);
        return;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ERROR_MESSAGE("esp_bluedroid_init failed %i\n", ret);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ERROR_MESSAGE("%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    // register for GATTS events
    ret = esp_ble_gatts_register_callback(this->GATTSProfileEventHandler);
    if (ret)
    {
        ERROR_MESSAGE("gatts register error, error code = %x", ret);
        return;
    }

    // register for GAP events
    ret = esp_ble_gap_register_callback(this->GAPEventHandler);
    if (ret)
    {
        ERROR_MESSAGE("gap register error, error code = %x", ret);
        return;
    }

    // register our one (and only) profile
    ret = esp_ble_gatts_app_register(MEADOW_PROFILE_APP_ID);
    if (ret)
    {
        ERROR_MESSAGE("gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(BT_GATT_MTU);
    if (local_mtu_ret)
    {
        ERROR_MESSAGE("set local  MTU failed, error code = %x", local_mtu_ret);
    }

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    //set static passkey
    uint32_t passkey = 123456;
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));

    /* If your BLE device acts as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the master;
    If your BLE device acts as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    xEventGroupSetBits(_xBtEventGroup, BT_READY_BIT);

    TRACE_MESSAGE("BT controller enabled");

    TRACE_MESSAGE("StartStack: Exit");
}

/**
 * @brief Teardown the bluetooth stack by turning off the bluetooth adapter.
 * 
 * @param msg
 * 
 * @return StatusCodes::StatusCodes
 *          Result of turning off the bluetooth stack.
 */
StatusCodes::StatusCodes BluetoothRequestHandler::TeardownStack(Message *msg)
{
    TRACE_MESSAGE("TeardownStack: Enter");

    esp_err_t result = esp_bt_controller_disable();

    TRACE_MESSAGE("TeardownStack(): Exit, ESP error code %d (%s)", result, esp_err_to_name(result));

    return(Mapping::GetStatusCode(result));
}

void BluetoothRequestHandler::OnDataWriteRequest(uint16_t handle, uint8_t* data, uint16_t dataLength)
{
    TRACE_MESSAGE("Creating _writeRequest for 0x%04x with %i bytes", handle, dataLength);

    Esp32Messaging::BTDataWriteRequest *request = static_cast<Esp32Messaging::BTDataWriteRequest *>(pvPortMalloc(10));
    TRACE_MESSAGE("request allocated with %i bytes", 6 + dataLength);
    request->Handle = handle;
    request->DataLength = dataLength;
    request->Data = data;
    int payload_length = Encoders::EncodedBTDataWriteRequestBufferSize(request);
    TRACE_MESSAGE("I think payload_length should be %i bytes", payload_length);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payload_length));
    Encoders::EncodeBTDataWriteRequest(request, payload);
    vPortFree(request);   // We don't need it anymore as the data is in the encoded payload.

    TRACE_MESSAGE("Sending event data. payload[6] should be 0x%02x", payload[6]);
    GetInstance()->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, 0, payload, payload_length);

    TRACE_MESSAGE("OnDataWriteRequest: Exit");
}

void BluetoothRequestHandler::CreateAttributeGraph(Message *msg)
{
    TRACE_MESSAGE("CreateAttributeGraph. payload len = %i", msg->PayloadLength);

    Esp32Messaging::BTStackConfig *config = Encoders::ExtractBTStackConfig(msg->Payload);
    vPortFree(msg->Payload);
    msg->Payload = NULL;

    _manager.BuildGraphFromJson(config->Config);

    _manager.GenerateAdvertisingData();
    _manager.GenerateAttributeTable();

    StartStack();
}

//////////////////////////
// BELOW HERE IS THE IMPLEMENTATION OF THE ESP GATT SERVER
//////////////////////////
#define INSTANCE_ID 0

static bool advertising_data_configured = false;
static bool response_data_configured = false;

void BluetoothRequestHandler::GATTSProfileEventHandler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param)
{
    DEBUG_MESSAGE_SPECIFY_COMPONENT(__func__, "event = %x\n", event);
    switch (event)
    {
        case ESP_GATTS_REG_EVT:
            {
                // set the device name
                esp_ble_gap_set_device_name(_manager.DeviceName);

                // turn on some security
//               esp_ble_gap_config_local_privacy(true);

                // configure the advertising data
                esp_err_t ret = esp_ble_gap_config_adv_data(_manager.GetAdvertisingData());
                if (ret)
                {
                    ERROR_MESSAGE("config adv data failed, error code = %x", ret);
                }
                advertising_data_configured = true;

                // configure our BLE scan response
                ret = esp_ble_gap_config_adv_data(_manager.GetScanResponseData());
                if (ret)
                {
                    ERROR_MESSAGE("config scan response data failed, error code = %x", ret);
                }
                response_data_configured = true;

                TRACE_MESSAGE("BLE advertising configured.  Creating attributes...");

                // set the attribute table - this requires the services be already-defined in the Manager
                esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(_manager.GetAttributeTable(), gatts_if, _manager.GetAttributeTableLength(), INSTANCE_ID);
                if (create_attr_ret)
                {
                    ERROR_MESSAGE("create attr table failed, error code = %x", create_attr_ret);
                }
                else
                {
                    TRACE_MESSAGE("BLE attribute table created");
                }

            }
            break;
        case ESP_GATTS_READ_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_READ_EVT, reading handle 0x%04x need response=%i", param->read.handle, param->read.need_rsp);

                if (param->read.need_rsp)
                {
                    // the app needs to build a response - this is (currently) set for Characteristic Values - so call to the manager to get the value for the handle
                    esp_gatt_rsp_t rsp;
                    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));

                    rsp.attr_value.handle = param->read.handle;

                    // go get the currently known value from the manager
                    _manager.GetCurrentValueForAttributeHandle(param->read.handle, rsp.attr_value.value, &rsp.attr_value.len);

                    TRACE_MESSAGE("  Sending read response of %i bytes", rsp.attr_value.len);
                    esp_err_t ret = esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);

                    if (ret)
                    {
                        ERROR_MESSAGE("esp_ble_gatts_send_response failed, error code = %x", ret);
                    }

                }
            }
            break;
        case ESP_GATTS_WRITE_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_WRITE_EVT, write handle 0x%04x", param->write.handle);
                if (param->write.need_rsp)
                {
                    OnDataWriteRequest(param->write.handle, param->write.value, param->write.len);

                    esp_err_t ret = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    if (ret)
                    {
                        ERROR_MESSAGE("esp_ble_gatts_send_response failed, error code = %x", ret);
                    }
                    else
                    {
                        TRACE_MESSAGE("esp_ble_gatts_send_response succeeded");
                    }
                }
            }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            TRACE_MESSAGE("ESP_GATTS_EXEC_WRITE_EVT");
            break;
        case ESP_GATTS_MTU_EVT:
            TRACE_MESSAGE("ESP_GATTS_MTU_EVT");
            break;
        case ESP_GATTS_CONF_EVT:
            TRACE_MESSAGE("ESP_GATTS_CONF_EVT");
            break;
        case ESP_GATTS_UNREG_EVT:
            TRACE_MESSAGE("ESP_GATTS_UNREG_EVT");
            break;
        case ESP_GATTS_DELETE_EVT:
            TRACE_MESSAGE("ESP_GATTS_DELETE_EVT");
            break;
        case ESP_GATTS_START_EVT:
            TRACE_MESSAGE("ESP_GATTS_START_EVT");
            break;
        case ESP_GATTS_STOP_EVT:
            TRACE_MESSAGE("ESP_GATTS_STOP_EVT");
            break;
        case ESP_GATTS_CONNECT_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_CONNECT_EVT");
                /* start security connect with peer device when receive the connect event sent by the master */
//              esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);

                esp_ble_conn_update_params_t conn_params;
                memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
                /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
                conn_params.latency = 0;
                conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
                conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
                conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
                //start sent the update connection parameters to the peer device.
                esp_ble_gap_update_conn_params(&conn_params);
            }
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
                /* start advertising again when missing the connect */
                esp_ble_gap_start_advertising(_manager.GetAdvertisingParameters());
            }
            break;
        case ESP_GATTS_OPEN_EVT:
            break;
        case ESP_GATTS_CANCEL_OPEN_EVT:
            break;
        case ESP_GATTS_CLOSE_EVT:
            break;
        case ESP_GATTS_LISTEN_EVT:
            break;
        case ESP_GATTS_CONGEST_EVT:
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            {
                if (param->add_attr_tab.status != ESP_GATT_OK)
                {
                    ERROR_MESSAGE("create attribute table failed, error code=0x%x", param->add_attr_tab.status);
                }
                else if (param->add_attr_tab.num_handle != _manager.GetAttributeTableLength())
                {
                    ERROR_MESSAGE("create attribute table abnormally, num_handle (%d) \
                            doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, _manager.GetAttributeTableLength());
                }
                else
                {
                    TRACE_MESSAGE("create attribute table successfully, handle count = %d\n",param->add_attr_tab.num_handle);
                    // the assigned handles with be in param->add_attr_tab.handles - copy them off
                    _manager.SetHandles(param->add_attr_tab.handles, param->add_attr_tab.num_handle);
                    // now fire up the service (which is the first attribute in the attribute table) by handle.
                    // TODO: right now we're cheating
                    TRACE_MESSAGE("starting service with handle 0x%4x", param->add_attr_tab.handles[0]);
                    esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
                }
            }
            break;
        default:
            break;
    }
}

void BluetoothRequestHandler::GAPEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    TRACE_MESSAGE("GAP event %i", event);
    switch (event)
    {
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: // 1
            if (advertising_data_configured)
            {
                TRACE_MESSAGE("Scan data set.  Starting advertising...");
                esp_ble_gap_start_advertising(_manager.GetAdvertisingParameters());
            }
            break;
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 0
            // triggered by the gatts_event_handler for ESP_GATTS_REG_EVT where we set the advertising data
            if (advertising_data_configured)
            {
                TRACE_MESSAGE("Advertising data set.  Starting advertising...");
                esp_ble_gap_start_advertising(_manager.GetAdvertisingParameters());
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: // 6
            // advertising has started (well, unless an error is set, so check for that)
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
            {
                ERROR_MESSAGE("Advertising start failed");
            }
            TRACE_MESSAGE("Advertising start success");
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT: // 12
            /* passkey request event */
            TRACE_MESSAGE("ESP_GAP_BLE_PASSKEY_REQ_EVT");
            /* Call the following function to input the passkey which is displayed on the remote device */
            //esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda, true, 0x00);
            break;
        case ESP_GAP_BLE_OOB_REQ_EVT: // 13
            {
                TRACE_MESSAGE("ESP_GAP_BLE_OOB_REQ_EVT");
                uint8_t tk[16] = {1}; //If you paired with OOB, both devices need to use the same tk
                esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
            }
            break;
        case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
            TRACE_MESSAGE("ESP_GAP_BLE_LOCAL_IR_EVT");
            break;
        case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
            TRACE_MESSAGE("ESP_GAP_BLE_LOCAL_ER_EVT");
            break;
        case ESP_GAP_BLE_NC_REQ_EVT: // 16
            /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
            show the passkey number to the user to confirm it with the number displayed by peer device. */
            esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
            TRACE_MESSAGE("ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT: // 10
            // security request
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: // 11
            ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
            ///show the passkey number to the user to input it in the peer device.
            TRACE_MESSAGE("The passkey Notify number:%d", param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_KEY_EVT: // 9
            //shows the ble key info share with peer device to the user.
            TRACE_MESSAGE("key type = %d", param->ble_security.ble_key.key_type);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT: // 8
            {
                // auth complete
                if (param->ble_security.auth_cmpl.success)
                {
                    TRACE_MESSAGE("Auth success. mode = %d", param->ble_security.auth_cmpl.auth_mode);
                }
                else
                {
                    ERROR_MESSAGE("Auth failed. reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
                }
            }
            break;
        case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: // 23
            {
                DEBUG_MESSAGE("ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %d", param->remove_bond_dev_cmpl.status);
                TRACE_MESSAGE("ESP_GAP_BLE_REMOVE_BOND_DEV");
                TRACE_MESSAGE("-----ESP_GAP_BLE_REMOVE_BOND_DEV----");
                TRACE_HEX_BUFFER(param->remove_bond_dev_cmpl.bd_addr, sizeof(esp_bd_addr_t));
                TRACE_MESSAGE("------------------------------------");
            }
            break;
        case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT: // 22
            {
                if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS)
                {
                    ERROR_MESSAGE("config local privacy failed, error status = %x", param->local_privacy_cmpl.status);
                    break;
                }

                TRACE_MESSAGE("GAP 22: local privacy set. Configuring advertising");
                esp_err_t ret = esp_ble_gap_config_adv_data(_manager.GetAdvertisingData());
                if (ret)
                {
                    ERROR_MESSAGE("config adv data failed, error code = %x", ret);
                }
                else
                {
                    advertising_data_configured = true;
                }

                ret = esp_ble_gap_config_adv_data(_manager.GetScanResponseData());
                if (ret)
                {
                    ERROR_MESSAGE("config adv data failed, error code = %x", ret);
                }
                else
                {
                    response_data_configured = true;
                }
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: // 20
            TRACE_MESSAGE("GAP connection parameters successfully changed\n");
            break;
        default:
            break;
    }
}