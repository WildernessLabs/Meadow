/*
 *  MeshNetworkRequestHandler.hpp
 *
 *  Mesh Network request handler object definition.
 */

#ifndef _MESH_NETWORK_REQUEST_HANDLER_HPP_
#define _MESH_NETWORK_REQUEST_HANDLER_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "RequestHandlerBase.hpp"
#include "Esp32Messaging.hpp"

/**
 *  @brief Mesh network request handler
 */
class MeshNetworkRequestHandler : public RequestHandlerBase
{
private:
    /**
     *  @brief Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /**
     *  @brief This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *request) override;

    /**
     *  Private constructor(s) and destructor.
     */
    MeshNetworkRequestHandler();
    ~MeshNetworkRequestHandler();

public:
    /**
     *  @brief Name of the FreeRTOS component / task for the WiFi system.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Constructor for a Mesh Network request handler object.
     */
    explicit MeshNetworkRequestHandler(IMessageDispatcher *messageDispatcher);

    /**
     *  @brief Provide and additional setup operations not covered by the constructors.
     */
    void Setup();
};

#endif /* _MESH_NETWORK_REQUEST_HANDLER_HPP_ */
