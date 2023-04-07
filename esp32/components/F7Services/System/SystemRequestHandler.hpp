/*
 *  SystemRequestHandler.hpp
 *
 *  System message request handler
 */

#ifndef _SYSTEM_REQUEST_HANDLER_HPP_
#define _SYSTEM_REQUEST_HANDLER_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <string.h>
#include <stdlib.h>

#include "IMessageDispatcher.hpp"
#include "RequestHandlerBase.hpp"
#include "NvsManager.hpp"
#include "ThreadPool.hpp"
#include "FileSystem.hpp"

/**
 *  @brief Object to deal with system requests such as configuration settings etc.
 */
class SystemRequestHandler : public RequestHandlerBase
{
private:
    /**
     *  @brief Name of the NVS store holding the system configuration.
     */
    static const char *_configurationName;

    /**
     *  @brief Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /**
     *  @brief Maximum length of a message queue.
     */
    static uint32_t _maximumMessageQueueLength;

    /**
     *  @brief Name of the device on the network.
     */
    static char *_deviceName;

    /**
     *  @brief Private instance fo the sheared thread pool.
     */
    static ThreadPool *_threadPool;

    /**
     *  @brief Name of the storage in NVS holding the name of the device.
     */
    static const char *DEVICE_NAME_NAME;

    /**
     *  @brief Name of the NVS storage holding the _maximumMessagequeueLength value.
     */
    static const char *MAXIMUM_MESSAGE_QUEUE_LENGTH_NAME;

    /**
     * @brief Container for the methods and data required to access the file
     *        system on the ESP32.
     */
    FileSystem *_fileSystem;

    /**
     *  T@brief his method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *) override;

    /**
     *  @brief Constructor(s) and destructor.
     */
    SystemRequestHandler();
    ~SystemRequestHandler();

    /**
     *  @brief Get the system configuration.
     */
    void GetConfiguration(Message *);

    /**
     *  @brief Set a system configuration item.
     */
    void SetConfigurationItem(Message *);

    /**
     *  @brief Put the ESP32 into deep sleep.
     */
    void DeepSleep(Message *);

    /**
     *  @brief Get the battery level.
     */
    void GetBatteryChargeLevel(Message *);

    /**
     *  @brief Start heap tracing.
     */
    static void StartHeapTrace(Message *);

    /**
     *  @brief Stop heap tracing.
     */
    void StopHeapTrace(Message *);

    /**
     *  @brief Populate the configuration object with the current configuration.
     */
    static void PopulateConfiguration(Esp32Messaging::SystemConfiguration *);

public:
    /**
     *  @brief Name of the FreeRTOS component / task for the System task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Public constructor.
     */
    explicit SystemRequestHandler(IMessageDispatcher *);

    /**
     *  @brief Perform any class level setup.
     */
    void Setup();

    /**
     *  @brief Get a string configuration value.
     */
    static char *GetConfigurationValue(const char *, const char *);

    /**
     *  @brief Get an unsigned integer configuration value.
     */
    static uint32_t GetConfigurationValue(const char *, const uint32_t);

    /**
     *  @brief Get an integer configuration value.
     */
    static int GetConfigurationValue(const char *, const int32_t);

    /**
     *  @brief Get a byte configuration value.
     */
    static uint8_t GetConfigurationValue(const char *, const uint8_t);

    /**
     *  @brief Get a boolean configuration value.
     */
    static bool GetConfigurationValue(const char *, const bool);

    /**
     *  @brief Get / Set the maximum message queue length.
     */
    static uint32_t GetMaximumMessageQueueLength();
    static StatusCodes::StatusCodes SetMaximumMessageQueueLength(uint32_t);

    /**
     *  @brief Set a string configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const char *);

    /**
     *  @brief Set an unsigned integer configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const uint32_t);

    /**
     *  @brief Set an integer configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const int32_t);

    /**
     *  @brief Set a byte configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const uint8_t);

    /**
     *  @brief Set a boolean configuration value.
     */
    static NvsManager::ErrorCodes SetConfigurationValue(const char *, const bool);

    /**
     *  @brief Get / Set the name of the device on the network.
     */
    static char *GetDeviceName();
    static StatusCodes::StatusCodes SetDeviceName(char *);

    /**
     *  @brief Send the configuration data as an interrupt message.
     */
    static void SendConfigurationAsInterruptMessage();

    static ThreadPool *SharedThreadPool();

};

#endif /* _SYSTEM_REQUEST_HANDLER_HPP_ */
