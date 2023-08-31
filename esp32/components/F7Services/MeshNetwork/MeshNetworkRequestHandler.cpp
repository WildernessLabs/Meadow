/*
 *  MeshNetworkRequestHandler.cpp
 *
 *  Provide the implementation of the Mesh Networking functions.
 */
#include "sdkconfig.h"

#include "MeshNetworkRequestHandler.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "Encoders.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "Logging.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the FreeRTOS component / task for the mesh network system.
 */
const char *MeshNetworkRequestHandler::COMPONENT_NAME = "MeshNetworkTask";

/**
 *  @brief Determine if this class is instantiated already (i.e. this should be a singleton).
 */
bool MeshNetworkRequestHandler::_instantiated = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the mesh network.
 */
MeshNetworkRequestHandler::MeshNetworkRequestHandler()
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
 *  @brief Create a new Mesh Network handler object.
 *
 *  @param messageDispatcher
 *      Message dispatcher that will allow communication with the STM32.
 */
MeshNetworkRequestHandler::MeshNetworkRequestHandler(IMessageDispatcher *messageDispatcher) : MeshNetworkRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  Default destructor for the mesh network class.
 */
MeshNetworkRequestHandler::~MeshNetworkRequestHandler()
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
 *  @brief Setup the mesh network.
 */
void MeshNetworkRequestHandler::Setup()
{
    RequestHandlerBase::Setup();
    xTaskCreate(Task, COMPONENT_NAME, 1024 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/**
 *  @brief Dispatch the message
 * 
 *  @param
 *      Message containing the data for the request.
 */
void MeshNetworkRequestHandler::DispatchRequest(Message *request)
{
    ESP_LOGI(COMPONENT_NAME, "Message handling not implemented, message will be deleted");
    //
    //  Memory for the payload is allocated on the heap and it needs to be released.
    //
    if(request->Payload != NULL)
    {
        TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
        vPortFree(request->Payload);
    }
}
