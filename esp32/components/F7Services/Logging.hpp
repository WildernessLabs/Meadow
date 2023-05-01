/*
 *  Logging.hpp
 *
 *  The Logging class provides the functionality necessary to
 *  setup and run the logging system.
 */

#ifndef MAIN_LOGGING_HPP_
#define MAIN_LOGGING_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>
#include "Gpio.hpp"

#include "SharedEnums.hpp"

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

#include "esp_heap_trace.h"

#endif

#include "Esp32Messaging.hpp"

//
//  Performance counter definitions.
//
#if defined(PERFORMANCE_LOGGING)

#define PERFORMANCE_LOGGING_ENTER()             int64_t performance_logging_start = esp_timer_get_time()

#define PERFORMANCE_LOGGING_EXIT()              int64_t performance_logging_end = esp_timer_get_time()

#define PERFORMANCE_LOGGING_ADD_ENTRY()         Logging::AddPerformanceCounterEntry(__func__, performance_logging_start, performance_logging_end)

#define PERFORMANCE_LOGGING_DUMP_RESULTS()      Logging::DumpPerformanceCounters()

#else

#define PERFORMANCE_LOGGING_ENTER()
#define PERFORMANCE_LOGGING_EXIT()
#define PERFORMANCE_LOGGING_ADD_ENTRY(method_name)
#define PERFORMANCE_LOGGING_DUMP_RESULTS()

#endif

//
//  Trace and debug output macros.
//
#if defined(TRACE_MESSAGES)

#define TRACE_MESSAGE(format, ...) ESP_LOGI(COMPONENT_NAME, format, ##__VA_ARGS__)
#define DEBUG_MESSAGE(format, ...) ESP_LOGD(COMPONENT_NAME, format, ##__VA_ARGS__)
#define ERROR_MESSAGE(format, ...) ESP_LOGE(COMPONENT_NAME, format, ##__VA_ARGS__)

#define TRACE_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...) ESP_LOGI(component_name, format, ##__VA_ARGS__)
#define DEBUG_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...) ESP_LOGD(component_name, format, ##__VA_ARGS__)
#define ERROR_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...) ESP_LOGE(component_name, format, ##__VA_ARGS__)

#define TRACE_HEX_BUFFER(buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(COMPONENT_NAME, buffer, length, ESP_LOG_INFO)
#define DEBUG_HEX_BUFFER(buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(COMPONENT_NAME, buffer, length, ESP_LOG_DEBUG)
#define ERROR_HEX_BUFFER(buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(COMPONENT_NAME, buffer, length, ESP_LOG_ERROR)

#define TRACE_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(component_name, buffer, length, ESP_LOG_INFO)
#define DEBUG_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(component_name, buffer, length, ESP_LOG_DEBUG)
#define ERROR_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(component_name, buffer, length, ESP_LOG_ERROR)

#else

#define TRACE_MESSAGE(format, ...)
#define DEBUG_MESSAGE(format, ...)
#define ERROR_MESSAGE(format, ...)

#define TRACE_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...)
#define DEBUG_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...)
#define ERROR_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...)

#define TRACE_HEX_BUFFER(buffer, length)
#define DEBUG_HEX_BUFFER(buffer, length)
#define ERROR_HEX_BUFFER(buffer, length)

#define TRACE_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length)
#define DEBUG_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length)
#define ERROR_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length)

#endif

/**
 *  @brief The Logging class provides the functionality necessary to setup and run the logging system.
 */
class Logging
{
private:
    /**
     *  @brief Structure to hold performance information for a specific event.
     */
    struct PerformanceCounterEntry
    {
        /**
         *  @brief Time that this event was logged.
         */
        uint32_t timestamp;

        /**
         *  @brief Name of the method that generated the event.
         */
        const char *method;

        /**
         *  @brief Number of microseconds elapsed during the recorded event.
         */
        uint32_t duration;
    };

    /**
     *  @brief UART Port number for the logging output.
     */
    static const uart_port_t LOGGING_UART_PORT_NUMBER = UART_NUM_0;

    /**
     *  @brief Number of bytes in the Rx buffer.
     */
    static const int RX_BUFFER_SIZE = 1024;

    /**
     *  @brief Number of bytes in the Rx buffer.
     */
    static const int TX_BUFFER_SIZE = 1024;

#if defined(PERFORMANCE_LOGGING)
    /**
     *  @brief Maximum number of performance counters to be held in the circular buffer.
     */
    static const int MAX_PERFORMANCE_COUNTERS = 500;

    /**
     *  @brief Next performance counter entry to use.
     */
    static int _nextPerformanceCounterEntry;

    /**
     *  @brief Number of performance counter slots in the circular buffer.
     */
    static int _numberOfPerformanceCounterSlots;

    /**
     *  @brief Table to hold the performance counters;
     */
    static PerformanceCounterEntry *_performanceCounters[MAX_PERFORMANCE_COUNTERS];
#endif

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

public:
    /**
     *  Number of heap trace records.
     */
    static const uint32_t NUMBER_OF_HEAP_TRACE_RECORDS = 400;

    /**
     *  @brief Array holding the heap trace records.
     */
    static heap_trace_record_t _heapTraceRecords[Logging::NUMBER_OF_HEAP_TRACE_RECORDS];

private:
    /**
     * @brief Has the heap tracing system already setup?
     */
    static bool _isStandaloneHeapTracingSetup;
    /**
     *  @brief Has standalone heap tracing been started?
     */
    static bool _isStandaloneHeapTracingActive;

    /**
     *  @brief Number of events remaining before we dump the standalone heap trace data.
     */
    static uint32_t _heapEventsRemaining;

    /**
     *  @brief Number of events that can occur before the system dumps the trace data.
     *
     *  This value is used to reset the _heapEventsRemaining counter.
     */
    static uint32_t _heapEventCounterResetValue;

#endif

public:
    /**
     *  @brief Name of the component / task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Default constructor for the logging system.
     */
    Logging();

    /**
     *  @brief Default destructor for the logging system.
     */
    ~Logging();

    /**
     * @brief Setup this class.
     */
    static void Setup();

    /**
     *  @brief Process the request to output an informational message to the logging system.
     */
    static void Information(char *methodName, char *formatString, ...);

    /**
     *  @brief Process the request to output an error message to the logging system
     */
    static void Error(char *methodName, char *formatString, ...);

    /**
     *  @brief Translate the ESP-IDF error codes into StatusCodes.
     */
    static StatusCodes::StatusCodes TranslateEspErrorCode(esp_err_t);

    /**
     *  @brief Convert a message type into a meaningful string.
     */
    static char *MessageTypeName(uint8_t type);

    /**
     *  @brief Convert the interface enum (Esp32Interfaces) from an interface type into a string name.
     */
    static char *InterfaceName(uint32_t interface);

    /**
     *  @brief Convert the request enum into a WiFi message name.
     */
    static char *WiFiFunctionName(uint32_t function);

    /**
     *  @brief Convert the request enum into a System message name.
     */
    static char *SystemFunctionName(uint32_t function);

    /**
     *  @brief Convert the request enum into a Bluetooth message name.
     */
    static char *BluetoothFunctionName(uint32_t function);

    /**
     *  @brief Convert the request enum into a Transport message name.
     */
    static char *TransportFunctionName(uint32_t function);

    /**
     *  @brief Convert the specified function ID into a string name.  The function name will vary between the interfaces.
     */
    static char *FunctionName(uint32_t interface, uint32_t function);

    /**
     *  @brief Convert a status code into a meaningful name.
     */
    static char *StatusCodeName(uint32_t code);

    /**
     *  @brief Decode the message structure and output the components.
     */
    static void DumpMessage(const char *componentName, const Message *message);

    /**
     *  @brief Add an entry to the table of performance counters.
     */
    static void AddPerformanceCounterEntry(const char *method, int64_t start, int64_t end);

    /**
     *  @brief Dump the table of performance counters.
     */
    static void DumpPerformanceCounters();

    /**
     *  @brief Generate a single line summary of the message.
     */
    static char *MessageSummary(const Message *message);

    /**
     *  @brief Start standalone heap tracing.
     */
    static void StandaloneHeapTracingStart();

    /**
     *  @brief Stop standalone heap tracing.
     */
    static void StandaloneHeapTracingStop();

    /**
     *  @brief Dump the standalone heap tracing data to the serial port.
     */
    static void DumpStandaloneHeapTraceData();

    /**
     *  @brief Is heap tracing active?
     */
    static bool IsStandaloneHeapTracingActive();

    /**
     *  @brief Register a heap event (used to allow a number of events to occur before we dump the trace data.)
     */
    static void RegisterHeapEvent();

    /**
     *  @brief Set the number of events we will let happen before we dump the trace data.
     */
    static void SetStandaloneHeapEventResetCounter(uint32_t);

    /**
     *  @brief Check the number of heap events that have happened and dump the trace data (and reset) if necessary.
     */
    static void StandaloneHeapEventCheck();
};


#endif /* MAIN_LOGGING_HPP_ */
