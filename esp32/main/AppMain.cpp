/*
 *      Main application for the ESP32 code for Meadow.
 */
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "version.h"

#include "AppMain.hpp"
#include "NvsManager.hpp"
#include "Logging.hpp"
#include "MessageDispatcher.hpp"
#include "SpiTransportProvider.hpp"
#include "WiFiRequestHandler.hpp"
#include "SpiTransportProvider.hpp"

/**
 * @brief global variable holding the system build information.
 */
meadow_version_t g_application_version = 
{
    .major = VERSION_MAJOR,
    .minor = VERSION_MINOR,
    .revision = VERSION_REVISION,
    .build = VERSION_BUILD,
    .day = VERSION_BUILD_DAY,
    .month = VERSION_BUILD_MONTH,
    .year = VERSION_BUILD_YEAR,
    .hour = VERSION_BUILD_HOUR,
    .minute = VERSION_BUILD_MINUTE,
    .second = VERSION_BUILD_SECOND,
    .hash = VERSION_BUILD_HASH_NUMBER,
    .branch_name = (char *) VERSION_GIT_REF
};

//
//  Checks to see if TRACE or DEBUG is turned on and issues a warning if they are.
//
//  These checks are included in here so that they are processed only once.
//
#if defined(TRACE_MESSAGES)

#warning "Trace message output is enabled - turn this feature off for release builds."

#endif

#if defined(DEBUG) || defined(WIFI_DEBUG) || defined(SPI_DEBUG) || defined(THREAD_DEBUG) || defined(SYSTEM_DEBUG) || defined(BT_DEBUG) || defined(MESSAGES_DEBUG)
    #warning "Debug is enabled."
#endif

//
//  This definition is required for the trace message macros.
//  See Logging.hpp for more information on the trace message macros
//
#define COMPONENT_NAME APP_MAIN_COMPONENT_NAME

//
//  Work out if we have heap tracing enabled and issue a warning..
//
#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    #warning "Standalone heap tracing turned on."
#endif

#if !defined(CONFIG_WIFI_SSID)
#define CONFIG_WIFI_SSID "SSID"
#endif

#if !defined(CONFIG_WIFI_PASSWORD)
#define CONFIG_WIFI_PASSWORD "PASSWORD"
#endif

void TestBluetoothMessage(MessageDispatcher *dispatcher);

/**
 *  @brief Callback for the heap allocation failures.
 *
 *  The method prints some information about the method that caused the problem.
 *  It then waits for a while for the message to be output and then generates an
 *  exception to force a core dump (if enabled) and a restart.
 *
 *  @param requestedSize
 *      Amount of space requested.
 *
 *  @param caps
 *      Capability requested.
 *
 *  @param functionName
 *      Method that requested the memory.
 */
void heap_caps_alloc_failed_hook(size_t requestedSize, uint32_t caps, const char *functionName)
{
    if (requestedSize != 0)
    {
        printf("%s was called but failed to allocate %lu bytes with 0x%X capabilities.\n", functionName, (unsigned long) requestedSize, (unsigned int) caps);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        char *ptr = NULL;
        *ptr = 0;
    }
}

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
// cppcheck-suppress unusedFunction symbolName=app_main
extern "C" void app_main()
{
#if defined(DEBUG)
    esp_err_t error = heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);
    ESP_ERROR_CHECK(error);
#endif

    Logging::Setup();
    esp_log_level_set(COMPONENT_NAME, ESP_LOG_INFO);

    struct tm t;
    memset(&t, 0, sizeof(struct tm));
    t.tm_mon = g_application_version.month - 1;
    char month_text[4];
    strftime(month_text, 4, "%b", &t);

    ESP_LOGI(COMPONENT_NAME, "Version: %u.%u.%u.%u, built %02u %s 20%02u %02u:%02u:%02u UTC (%08x:%s)", \
                (unsigned int) g_application_version.major, (unsigned int) g_application_version.minor,
                (unsigned int) g_application_version.revision, (unsigned int) g_application_version.build, \
                (unsigned int) g_application_version.day, month_text, (unsigned int) g_application_version.year, 
                (unsigned int) g_application_version.hour, (unsigned int) g_application_version.minute, \
                (unsigned int) g_application_version.second, (unsigned int) g_application_version.hash, \
                g_application_version.branch_name);
    NvsManager::Setup();
    /*
     *  Set up the messaging system to use SPI hardware communications.
     */
    SpiTransportProvider *spi = new SpiTransportProvider();
    MessageDispatcher * __attribute__((unused)) messageDispatcher = new MessageDispatcher(spi);

    SystemRequestHandler::SendConfigurationAsInterruptMessage();

//    TestBluetoothMessage(messageDispatcher);

    // Message *message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));
    // *message = { };
    // message->MessageType = MessageTypes::Header;
    // message->Interface = Esp32Interfaces::WiFi;
    // message->Function = WiFiFunction::Socket;
    // message->MessageID = 0x8000000c;

    // Esp32Messaging::SocketRequest *request = static_cast<Esp32Messaging::SocketRequest *>(pvPortMalloc(sizeof(Esp32Messaging::SocketRequest)));
    // bzero(request, sizeof(Esp32Messaging::SocketRequest));
    // request->AddressInformation = NULL;
    // request->Domain = 2;
    // request->Type = 2;

    // message->PayloadLength = Encoders::EncodedSocketRequestBufferSize(request);
    // message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    // Encoders::EncodeSocketRequest(request, message->Payload);

    // messageDispatcher->QueueMessageForEsp32(message);

    // Esp32Messaging::AccessPointInformation info = { };
    // info.NetworkName = "TestNetwork";
    // info.Password = "ALongPassword";
    // info.IpAddress = 0x0104a8c0;
    // info.SubnetMask = 0x00ffffff;
    // info.Gateway = info.IpAddress;
    // message->PayloadLength = Encoders::EncodedAccessPointInformationBufferSize(&info);
    // message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    // Encoders::EncodeAccessPointInformation(&info, message->Payload);

    // Message *message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));
    // message->MessageType = MessageTypes::Header;
    // message->Interface = Esp32Interfaces::WiFi;
    // message->Function = WiFiFunction::ConnectToAccessPoint;
    
    // Esp32Messaging::WiFiCredentials request;

    // #include "WiFiRequestHandler.hpp"

    // request.NetworkName = (char *) CONFIG_WIFI_SSID;
    // request.Password = (char *) CONFIG_WIFI_PASSWORD;
    // message->PayloadLength = Encoders::EncodedWiFiCredentialsBufferSize(&request);
    // message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    // Encoders::EncodeWiFiCredentials(&request, message->Payload);

    // messageDispatcher->QueueMessageForEsp32(message);

    //
    //  We suspend this task as the message processing is performed by the objects created above.
    //
    vTaskSuspend(NULL);

    //
    //  Main program loop.
    //
    // while (true)
    // {
    //     vTaskDelay(500 / portTICK_PERIOD_MS);
    // }
}

const char* json_test = R"(
{
    "deviceName":"MY MEADOW",
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [
                {
                    "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
                    "permission": 17,
                    "props": 10,
                    "len": 4
                },
                {
                    "uuid": 10755,
                    "permission": 2,
                    "props": 16,
                    "len": 4
                }
            ]
        }
    ]
})";

const char* json_test2 = R"(
{
    "deviceName":"MY MEADOW",
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [
                {
                    "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
                    "permission": 17,
                    "props": 10,
                    "len": 4,
                    "descriptors": [
                        {
                        "uuid": 10498,
                        "permission": 1,
                        "len": 2,
                        "value": 0
                        }
                    ]
                },
                {
                    "uuid": 10755,
                    "permission": 2,
                    "props": 16,
                    "len": 4
                }
            ]
        }
    ]
})";

void TestBluetoothMessage(MessageDispatcher *dispatcher)
{
    // for now we'll use one message that starts and sets up everything.
    // We'll need to separate this and actually have data going in, but this is a baby step toward that

    Esp32Messaging::BTStackConfig stackConfig;
    stackConfig.Config = (char*)json_test;

    Message startMessage = {};
    startMessage.MessageType = MessageTypes::Header;
    startMessage.Interface = Esp32Interfaces::BlueTooth;
    startMessage.Function = BluetoothFunction::Start;
    startMessage.PayloadLength = Encoders::EncodedBTStackConfigBufferSize(&stackConfig);
    startMessage.Payload = (uint8_t *) pvPortMalloc(startMessage.PayloadLength);
    Encoders::EncodeBTStackConfig(&stackConfig, startMessage.Payload);
    dispatcher->QueueMessageForEsp32(&startMessage);

}
