/*
 *  SystemRequestHandler.cpp
 *
 *  Implementation of the methods to provide the System Request Handler functionality.
 */
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
// #include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_sleep.h"
#include <string.h>
#include <stdlib.h>

#include "Esp32Messaging.hpp"
#include "RequestHandlerBase.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "MessageDispatcher.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "WiFiRequestHandler.hpp"
#include "BluetoothRequestHandler.hpp"
#include "Mapping.hpp"
#include "version.h"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the FreeRTOS component / task for the System task.
 */
const char *SystemRequestHandler::COMPONENT_NAME = "SystemTask";

/**
 *  @brief Determine if this class is instantiated already (i.e. this should be a singleton).
 */
bool SystemRequestHandler::_instantiated = false;

/**
 *  @brief Default value for _maximumMessageQueueLength.
 */
uint32_t SystemRequestHandler::_maximumMessageQueueLength = 10;

/**
 *  @brief Name of the NVS namespace holding the configuration values.
 */
const char *SystemRequestHandler::_configurationName = "Meadow";

/**
 *  @brief Private instance of the shared threadpool.
 */
ThreadPool *SystemRequestHandler::_threadPool = new ThreadPool(4);

/**
 *  @brief Name of the key holding the _maximumMessageQueueLength value.
 */
const char *SystemRequestHandler::MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME = "MaxQueueLen";

/**
 *  @brief Name of the device on the network.
 */
char *SystemRequestHandler::_deviceName = nullptr;

/**
 *  @brief Name of the storage in NVS holding the device name on the network.
 */
const char *SystemRequestHandler::DEVICE_NAME_NAME = "DeviceName";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the system request handler class.
 */
SystemRequestHandler::SystemRequestHandler()
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

/**
 *  @brief Initialise a new System Request handler object.
 *
 *  @param messageDispatcher
 *      Message dispatcher object that will deal with and communication between the ESP32 and the STM32.
 */
SystemRequestHandler::SystemRequestHandler(IMessageDispatcher *messageDispatcher) : SystemRequestHandler()
{
    //
    //  Get configuration from NVS.
    //
    _maximumMessageQueueLength = GetConfigurationValue(MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME, _maximumMessageQueueLength);
    _deviceName = GetConfigurationValue(DEVICE_NAME_NAME, const_cast<char *>("MeadowF7"));
    _messageDispatcher = messageDispatcher;

    _fileSystem = new FileSystem(_messageDispatcher);
    _fileSystem->Setup();
}

/**
 *  @brief Default destructor for the system request handler class.
 */
SystemRequestHandler::~SystemRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Getters and setters.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Get the maximum length of a message queue.
 *
 *  @return
 *      Current value for the maximum length of the message queues.
 */
uint32_t SystemRequestHandler::GetMaximumMessageQueueLength()
{
    return(_maximumMessageQueueLength);
}

/**
 *  @brief Get a pointer to the shared threadpool.
 */
ThreadPool *SystemRequestHandler::SharedThreadPool()
{
    return(_threadPool);
}

/**
 *  @brief Set the maximum length of the message queues.
 *
 *  Note that changes to this value will not take place until the next
 *  time the board is restarted.
 *
 *  @param value
 *      New length for the message queues.
 */
StatusCodes::StatusCodes SystemRequestHandler::SetMaximumMessageQueueLength(uint32_t value)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (_maximumMessageQueueLength != value)
    {
        if (SetConfigurationValue(MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _maximumMessageQueueLength = value;
        }
    }
    return(result);
}

/**
 *  @brief Get the device name.
 */
char *SystemRequestHandler::GetDeviceName()
{
    return(_deviceName);
}

/**
 *  @brief Set the device name.
 *
 *  @param value
 *      New name for the device.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes SystemRequestHandler::SetDeviceName(char *value)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (strcmp(value, _deviceName) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(DEVICE_NAME_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _deviceName = strdup(value);
        }
    }
    return(result);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the system request handler.
 */
void SystemRequestHandler::Setup()
{
    RequestHandlerBase::Setup();
    xTaskCreate(Task, COMPONENT_NAME, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/**
 *  @brief Get a string configuration value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param name
 *      Name of the configuration value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
char *SystemRequestHandler::GetConfigurationValue(const char *name, const char *defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set a string configuration value.
 *
 *  Store a string value in NVS
 *
 *  @param name
 *      Name of the configuration value to set in NVS.
 *
 *  @param value
 *      Value to be stored.
 *
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const char *value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get an unsigned integer configuration value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param name
 *      Name of the configuration value to read from NVS.
 *
 *  @param value
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
uint32_t SystemRequestHandler::GetConfigurationValue(const char *name, const uint32_t defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set an unsigned integer configuration value.
 *
 *  Set an unsigned integer value in NVS
 *
 *  @param name
 *      Name of the configuration value to set in NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const uint32_t value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get an integer configuration value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param name
 *      Name of the configuration value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
int SystemRequestHandler::GetConfigurationValue(const char *name, const int32_t defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set an integer configuration value.
 *
 *  Set an integer value in NVS
 *
 *  @param name
 *      Name of the configuration value to set in NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const int32_t value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get a byte configuration value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param name
 *      Name of the configuration value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
uint8_t SystemRequestHandler::GetConfigurationValue(const char *name, const uint8_t defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set a byte configuration value.
 *
 *  Set a byte value in NVS
 *
 *  @param name
 *      Name of the configuration value to set in NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure status.
*/
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const uint8_t value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Get a boolean configuration value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param name
 *      Name of the configuration value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
bool SystemRequestHandler::GetConfigurationValue(const char *name, const bool defaultValue)
{
    return(NvsManager::GetValue(_configurationName, name, defaultValue));
}

/**
 *  @brief Set a boolean configuration value.
 *
 *  Set a boolean value in NVS
 *
 *  @param name
 *      Name of the configuration value to set in NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure status.
 */
NvsManager::ErrorCodes SystemRequestHandler::SetConfigurationValue(const char *name, const bool value)
{
    return(NvsManager::SetValue(_configurationName, name, value));
}

/**
 *  @brief Populate the config object with system configuration data.
 *
 *  @param config
 *      Pointer to the Esp32Messaging::SystemConfiguration to be populated.
 */
void SystemRequestHandler::PopulateConfiguration(Esp32Messaging::SystemConfiguration *config)
{
    TRACE_MESSAGE("PopulateConfiguration: Enter");

    memset(config, 0, sizeof(Esp32Messaging::SystemConfiguration));
    esp_reset_reason_t resetReason = esp_reset_reason();
    Mapping::GetStmValue(Mapping::ResetReasons, resetReason, (int8_t *) &config->ResetReason);
    config->Antenna = WiFiRequestHandler::GetAntenna();
    config->MaximumRetryCount = WiFiRequestHandler::GetMaximumRetryCount();
    config->DefaultAccessPoint = WiFiRequestHandler::GetDefaultAccessPoint();
    config->DeviceName = GetDeviceName();
    config->VersionMajor = g_application_version.major;
    config->VersionMinor = g_application_version.minor;
    config->VersionRevision = g_application_version.revision;
    config->VersionBuild = g_application_version.build;
    config->BuildDay = g_application_version.day;
    config->BuildMonth = g_application_version.month;
    config->BuildYear = g_application_version.year;
    config->BuildHour = g_application_version.hour;
    config->BuildMinute = g_application_version.minute;
    config->BuildSecond = g_application_version.second;
    config->BuildHash = g_application_version.hash;
    config->BuildBranchName = g_application_version.branch_name;
    WiFiRequestHandler::GetMacAddress(config->BoardMacAddress, WIFI_IF_STA);
    WiFiRequestHandler::GetMacAddress(config->SoftApMacAddress, WIFI_IF_AP);
    //
    //  Bluetooth MAC address will be zeroed from the memset above.
    //
    
    TRACE_MESSAGE("PopulateConfiguration: Exit");
}

/**
 *  @brief Send the current system configuration as an interrupt message.
 *
 *  This method allows the system to send its configuration to the STM32 when the
 *  system first starts.
 */
void SystemRequestHandler::SendConfigurationAsInterruptMessage()
{
    TRACE_MESSAGE("SendConfigurationAsInterruptMessage: Enter");

    Esp32Messaging::SystemConfiguration config = { };
    PopulateConfiguration(&config);

    Message *message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));
    bzero(static_cast<void *>(message), sizeof(Message));
    message->Interface = Esp32Interfaces::System;
    message->MessageType = MessageTypes::Event;
    message->MessageID = _messageDispatcher->GetNextMessageID();
    message->StatusCode = StatusCodes::CompletedOk;
    message->PayloadLength = Encoders::EncodedSystemConfigurationBufferSize(&config);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeSystemConfiguration(&config, message->Payload);
    _messageDispatcher->QueueMessageForStm32(message);
    TRACE_MESSAGE("SendConfigurationAsInterruptMessage: Exit");
}

/**
 *  @brief Get the configuration for the system.
 *
 *  @param request
 *      Message requesting the system configuration.
 */
void SystemRequestHandler::GetConfiguration(Message *request)
{
    TRACE_MESSAGE("GetConfiguration: Enter");
    Esp32Messaging::SystemConfiguration config = {};
    PopulateConfiguration(&config);

    request->PayloadLength = Encoders::EncodedSystemConfigurationBufferSize(&config);
    request->Payload = static_cast<uint8_t *>(pvPortMalloc(request->PayloadLength));
    Encoders::EncodeSystemConfiguration(&config, request->Payload);
    request->MessageType = MessageTypes::Response;
    request->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(request);
    TRACE_MESSAGE("GetConfiguration: Exit");
}

/**
 *  @brief Set a single configuration value.
 *
 *  @param request
 *      Message containing the new configuration value.
 */
void SystemRequestHandler::SetConfigurationItem(Message *request)
{
    TRACE_MESSAGE("SetConfigurationItem: Enter");
    Esp32Messaging::ConfigurationValue *value = Encoders::ExtractConfigurationValue(request->Payload);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    switch (value->Item)
    {
        case ConfigurationItems::Antenna:
            WiFiRequestHandler::SetAntenna(*value->Value);
            break;
        case ConfigurationItems::AutomaticallyReconnect:
            result = WiFiRequestHandler::SetAutomaticReconnect((uint8_t) (*value->Value) == 1);
            break;
        case ConfigurationItems::AutomaticallyStartNetwork:
            result = WiFiRequestHandler::SetAutomaticallyStartNetwork((uint8_t) (*value->Value) == 1);
            break;
        case ConfigurationItems::BoardMacAddress:
            result = WiFiRequestHandler::SetMacAddress(value->Value, WIFI_IF_STA);
            break;
        case ConfigurationItems::SoftApMacAddress:
            result = WiFiRequestHandler::SetMacAddress(value->Value, WIFI_IF_AP);
            break;
        case ConfigurationItems::DefaultApAndPassword:
            result = WiFiRequestHandler::SetDefaultAccessPoint(reinterpret_cast<char *>(value->Value));
            if (result == StatusCodes::CompletedOk)
            {
                result = WiFiRequestHandler::SetPassword(reinterpret_cast<char *>((value->Value + strlen(reinterpret_cast<char *>(value->Value)) + 1)));
            }
            break;
        case ConfigurationItems::DeviceName:
            result = SetDeviceName(reinterpret_cast<char *>(value->Value));
            break;
        case ConfigurationItems::DnsServer:
            result = WiFiRequestHandler::SetDnsServer((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::StaticIpAddress:
            result = WiFiRequestHandler::SetIpAddress((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::DefaultGateway:
            result = WiFiRequestHandler::SetDefaultGateway((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::MaximumMessageQueueLength:
            result = SetMaximumMessageQueueLength((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::MaximumRetryCount:
            result = WiFiRequestHandler::SetMaximumRetryCount((uint32_t) (*value->Value));
            break;
        case ConfigurationItems::UseDhcp:
            result = WiFiRequestHandler::SetUseDhcp((uint8_t) (*value->Value) == 1);
            break;
        default:
            result = StatusCodes::UnknownConfigurationItem;
            break;
    }
    request->DeletePayload();
    request->MessageType = MessageTypes::Response;
    request->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(request);
    TRACE_MESSAGE("SetConfigurationItem: Exit");
}

/**
 *  @brief Put the system into deep sleep.
 *
 *  This is one of the few messages that will not send a response to the
 *  STM32.  The ESP32 will simply go to sleep with no wakeup sources and
 *  so the only way to wake up the ESP32 will be to reset the chip hence
 *  no need to dispose of any data in memory.
 *
 *  @param request
 *      Message requesting the ESP32 got to sleep.
 */
void SystemRequestHandler::DeepSleep(Message *request)
{
    TRACE_MESSAGE("DeepSleep: Enter");

    TRACE_MESSAGE("Turning off wireless devices.");
    WiFiRequestHandler::PrepareForDeepSleep();
    BluetoothRequestHandler::PrepareForDeepSleep();

    TRACE_MESSAGE("Entering deep sleep");
    esp_deep_sleep_start();

    TRACE_MESSAGE("DeepSleep: Exit");
}

/**
 *  @brief Get the voltage reading from the battery charging circuit.
 *
 *  @param request
 *      Message requesting the battery level.
 */
void SystemRequestHandler::GetBatteryChargeLevel(Message *message)
{
    TRACE_MESSAGE("GetBatteryChargeLevel: Enter");

    const adc_channel_t channel = ADC_CHANNEL_7;
    const adc_atten_t attenuation = ADC_ATTEN_DB_11;
    const adc_unit_t unit = ADC_UNIT_1;
    const uint32_t vref = 1100;
    const uint32_t numberOfSamples = 64;

    esp_adc_cal_characteristics_t *adc_chars;
    uint32_t adc_reading = 0;

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten((adc1_channel_t) channel, attenuation);
    adc_chars = static_cast<esp_adc_cal_characteristics_t *>(calloc(1, sizeof(esp_adc_cal_characteristics_t)));
    esp_adc_cal_characterize(unit, attenuation, ADC_WIDTH_BIT_12, vref, adc_chars);

    for (int i = 0; i < numberOfSamples; i++)
    {
        adc_reading += adc1_get_raw((adc1_channel_t) channel);
    }
    adc_reading /= numberOfSamples;
    Esp32Messaging::IntegerResponse response;
    //
    //  The battery voltage will be twice the actual reading as the output from the battery is passed
    //  through a voltage divider.  This is necessary as the voltage can go as high as 4.2V which
    //  exceeds the tolerance of the ADC pins on the ESP32.
    //
    response.Result = 2 * esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    free(adc_chars);

    message->PayloadLength = Encoders::EncodedIntegerResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerResponse(&response, message->Payload);

    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);
    TRACE_MESSAGE("GetBatteryChargeLevel: Exit");
}

/**
 *  @brief Turn heap tracing on.
 *
 *  @param request
 *      Message requesting heap tracing to be turned on.
 */
void SystemRequestHandler::StartHeapTrace(Message *message)
{
#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    message->DeleteMessage(message);
    Logging::StandaloneHeapTracingStart();
#else
    ESP_LOGE(COMPONENT_NAME, "Heap tracing cannot start as it is disabled, update menuconfig.");
#endif
}

/**
 *  @brief Turn heap tracing off.
 *
 *  @param request
 *      Message requesting that heap tracing is turned off.
 */
void SystemRequestHandler::StopHeapTrace(Message *message)
{
#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    message->DeleteMessage(message);
    Logging::StandaloneHeapTracingStop();
    Logging::DumpStandaloneHeapTraceData();
#else
    ESP_LOGE(COMPONENT_NAME, "Heap tracing cannot be stopped as it is disabled, update menuconfig.");
#endif
}

/**
 *  Dispatch the System request to the appropriate method in the class.
 *
 *  @param request
 *      Request to be processed.
 */
void SystemRequestHandler::DispatchRequest(Message *request)
{
    TRACE_MESSAGE("DispatchRequest: Enter");
    switch (request->Function)
    {
        case SystemFunction::GetConfiguration:
            GetConfiguration(request);
            break;
        case SystemFunction::SetConfigurationItem:
            SetConfigurationItem(request);
            break;
        case SystemFunction::DeepSleep:
            DeepSleep(request);
            break;
        case SystemFunction::GetBatteryChargeLevel:
            GetBatteryChargeLevel(request);
            break;
        case SystemFunction::StartHeapTrace:
            StartHeapTrace(request);
            break;
        case SystemFunction::StopHeapTrace:
            StopHeapTrace(request);
            break;
        case SystemFunction::FileSystemFormat:
            _fileSystem->Format(request);
            break;
        case SystemFunction::FileSystemWriteFile:
            _fileSystem->WriteFile(request);
            break;
        case SystemFunction::FileSystemReadFile:
            _fileSystem->ReadFile(request);
            break;
        case SystemFunction::FileSystemListFiles:
            _fileSystem->ListFiles(request);
            break;
        case SystemFunction::FileSystemDeleteFile:
            _fileSystem->DeleteFile(request);
            break;
        default:
            TRACE_MESSAGE("DispatchRequest: Unknown System message received, function 0x%x", request->Function);
            if (request->Payload != NULL)
            {
                TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
            }
            break;
    }
    TRACE_MESSAGE("DispatchRequest: Exit");
}
