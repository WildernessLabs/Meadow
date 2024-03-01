/*
 *  SpiTransportProvider.cpp
 *
 *  Hardware transport provider for messages between the ESP32 and
 *  the STM32 using the SPI interface.
 */
#include "sdkconfig.h"

#include "SpiTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include <string.h>

#include "esp_heap_caps.h"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of this component / task.
 */
const char *SpiTransportProvider::COMPONENT_NAME = "SPITask";

/**
 *  @brief Default state is that the class has not been fully constructed
 *  and so there is no current instance.
 */
SpiTransportProvider *SpiTransportProvider::_spiInstance = nullptr;

/**
 *  @brief Initial state of the system is Invalid.  The constructor will set
 *  up the SPI interface and then move the state to one where we can
 *  receive data.
 */
SpiTransportProvider::SpiStates SpiTransportProvider::_currentState = Invalid;

TaskHandle_t SpiTransportProvider::_synchronisationTaskHandle = NULL;

/**
 *  Buffers will be allocated as needed and so they are initially null.
 */
uint8_t *SpiTransportProvider::_rxBuffer = nullptr;
uint8_t *SpiTransportProvider::_txBuffer = nullptr;
uint8_t *SpiTransportProvider::_transportFrameBuffer = nullptr;
uint32_t SpiTransportProvider::_headerFrameSize = 0;

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the SPI transport provider.
 */
SpiTransportProvider::SpiTransportProvider() : TransportProviderBase()
{
    if (_spiInstance)
    {
        throw new MultipleInstancesException();
    }
    /*
     *  Should only get here if this is the first instance.
     */
    _spiInstance = this;

    /*
     *  Set the handshake pins to output and set the output level low.
     *
     *  First pin is the pin indicating that the ESP32 SPI interface is ready
     *  to send / receive data.  This is the BOOT pin on the ESP32.
     *
     *  The second pin is the pin used by the ESP32 to indicate that a message
     *  is waiting.  This should be the UART0 Rx pin.
     */
    gpio_pad_select_gpio(Gpio::ESP32_SPI_READY_PIN);
    gpio_set_direction(Gpio::ESP32_SPI_READY_PIN, GPIO_MODE_OUTPUT);
    //
    gpio_pad_select_gpio(Gpio::ESP32_MESSAGE_WAITING_PIN);
    gpio_set_direction(Gpio::ESP32_MESSAGE_WAITING_PIN, GPIO_MODE_OUTPUT);

    /*
     *  Configure the pull-ups on the SPI pins.
     */
    gpio_set_pull_mode(Gpio::MOSI_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(Gpio::CLOCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(Gpio::CHIP_SELECT_PIN, GPIO_PULLUP_ONLY);

    /*
     *  Configure the chip select pin for the SPI bus to allow interrupts.
     */
    gpio_config_t chipSelectConfiguration;
    chipSelectConfiguration.intr_type = GPIO_INTR_DISABLE;
    chipSelectConfiguration.mode = GPIO_MODE_INPUT;
    chipSelectConfiguration.pull_down_en = GPIO_PULLDOWN_DISABLE;
    chipSelectConfiguration.pull_up_en = GPIO_PULLUP_DISABLE;
    chipSelectConfiguration.pin_bit_mask = (1 << Gpio::CHIP_SELECT_PIN);
    gpio_config(&chipSelectConfiguration);

    /*
     *  Setup the SPI bus.
     */
    spi_bus_config_t spiBusConfiguration;
    spiBusConfiguration.mosi_io_num = Gpio::MOSI_PIN;
    spiBusConfiguration.miso_io_num = Gpio::MISO_PIN;
    spiBusConfiguration.sclk_io_num = Gpio::CLOCK_PIN;
    spiBusConfiguration.quadhd_io_num = -1;
    spiBusConfiguration.quadwp_io_num = -1;
    spiBusConfiguration.max_transfer_sz = 0;
    spiBusConfiguration.flags = SPICOMMON_BUSFLAG_SLAVE;
    spiBusConfiguration.intr_flags = 0;
    /*
     *  Configuration for the SPI slave interface.
     */
    spi_slave_interface_config_t slaveConfiguration;
    slaveConfiguration.mode = 3;
    slaveConfiguration.spics_io_num = Gpio::CHIP_SELECT_PIN;
    slaveConfiguration.queue_size = 3;
    slaveConfiguration.flags = 0;
    slaveConfiguration.post_trans_cb = SpiPostTransmissionCallback;
    slaveConfiguration.post_setup_cb = SpiPostSetupCallback;

    ESP_ERROR_CHECK(spi_slave_initialize(VSPI_HOST, &spiBusConfiguration, &slaveConfiguration, 1));

    /*
     *  The initial state is to wait for a message header to be sent by the STM32.
     *
     *  The _rxBuffer and _txBuffer are permanently allocated
     *  as they will be frequently used for header and transport packets.
     */
    _headerFrameSize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE);
    TRACE_MESSAGE("Header frame size: %d", _headerFrameSize);
    _txBuffer = static_cast<uint8_t *>(heap_caps_calloc(Message::MAXIMUM_SPI_FRAME_SIZE, 1, MALLOC_CAP_DMA));
    _rxBuffer = static_cast<uint8_t *>(heap_caps_calloc(Message::MAXIMUM_SPI_FRAME_SIZE, 1, MALLOC_CAP_DMA));
    ClearTransmitAndReceiveBuffers();

    /*
     *  Set the state machine to be ready to receive messages.
     */
    _currentState = WaitingForPacket;

    /*
     *  Set up the task and that will process the SPI messages.
     */
    _synchronisationSemaphore = xSemaphoreCreateBinary();
    //
    //  FreeRTOS documentation states that a binary semaphore must be given after creation as it is created "empty".
    //  For more information, see https://www.freertos.org/xSemaphoreCreateBinary.html
    //
    xSemaphoreGive(_synchronisationSemaphore);
    if (xSemaphoreTake(_synchronisationSemaphore, pdMS_TO_TICKS(500)) != pdTRUE)
    {
        ERROR_MESSAGE("Cannot take the synchronisation semaphore.");
    }
    xTaskCreate(SynchronisationTask, "SPISyncTask", 2048 * 2, this, configMAX_PRIORITIES - 1, &SpiTransportProvider::_synchronisationTaskHandle);

    /*
     *  Set up the task and that will process the SPI messages.
     */
    xTaskCreate(Task, COMPONENT_NAME, 2048 * 2, this, configMAX_PRIORITIES - 1, &SpiTransportProvider::_transportProviderTaskHandle);

    /*
     *  The SPI code on the STM32 will wait for both SPI ready and
     *  message waiting to both go low as a signal that the SPI code
     *  has completed the initialisation process.
     */
    gpio_set_level(Gpio::ESP32_MESSAGE_WAITING_PIN, 0);
    gpio_set_level(Gpio::ESP32_SPI_READY_PIN, 0);

}

/**
 *  @brief Default destructor for the SPI transport provider.
 */
SpiTransportProvider::~SpiTransportProvider()
{
    spi_slave_free(VSPI_HOST);
    if (_rxBuffer)
    {
        vPortFree(_rxBuffer);
    }
    if (_txBuffer)
    {
        vPortFree(_txBuffer);
    }
    if (_incomingMessage)
    {
        Message::DeleteMessage(_incomingMessage);
    }
    if (_outboundMessage)
    {
        Message::DeleteMessage(_outboundMessage);
    }
    _currentState = Invalid;
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/* ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Convert a state into a meaningful message for debugging.
 *
 *  @param state
 *      State to be decoded.
 *
 *  @returns
 *      Text representation of an SpiState.
 */
const char *SpiTransportProvider::SpiStateString(SpiTransportProvider::SpiStates state)
{
    const char *stateString = NULL;

    switch (state)
    {
        case Invalid:
            stateString = "Invalid";
            break;
        case WaitingForPacket:
            stateString = "WaitingFoMessage";
            break;
        case AcknowledgingPacket:
            stateString = "AcknowledgingPacket";
            break;
        case SendingPacketDetails:
            stateString = "SendingPacketDetails";
            break;
        case SendingPacket:
            stateString = "SendingPacket";
            break;
        case WaitingForPacketAcknowledgement:
            stateString = "WaitingForPacketAcknowledgement";
            break;
    }
    return (stateString);
}

/**
 *  @brief Clear the transaction receive and transmit buffers.
 *
 *  Note that it is assumed that the receive and transmit buffers are the same size.
 *
 */
void SpiTransportProvider::ClearTransmitAndReceiveBuffers()
{
    memset(_rxBuffer, 0, Message::MAXIMUM_SPI_FRAME_SIZE);
    memset(_txBuffer, 0, Message::MAXIMUM_SPI_FRAME_SIZE);
}

/**
 *  @brief Create a new acknowledgement transaction (ACK or NAK) ready to transmit to the STM32.
 *
 *  This method allocates the transaction and two buffers on the heap.  It is essential
 *  that the caller cleans up both the transaction and the buffers after they are no
 *  longer necessary.
 *
 *  @param message
 *      Message to acknowledge.
 *
 *  @param code
 *      Status code to return.
 *
 *  @param messageType
 *      Type of message to return.
 *
 *  @return pointer to spi_slave_transaction_t object
 *      This is the transaction that is returned by the ESP-IDF.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateAcknowledgementTransaction(Message *message, StatusCodes::StatusCodes code, uint8_t messageType)
{
    TRACE_MESSAGE("CreateAcknowledgementTransaction: Enter");

    TRACE_MESSAGE("Message type: 0x%02x", messageType);
    Message *copyOfMessage = message->CreateCopyOnHeap(false);
    copyOfMessage->MessageType = messageType;
    copyOfMessage->StatusCode = static_cast<uint32_t>(code);
    spi_slave_transaction_t *transaction = CreateSpiTransaction(copyOfMessage);
    Message::DeleteMessage(copyOfMessage);

    TRACE_MESSAGE("CreateAcknowledgementTransaction: Exit");
    return (transaction);
}

/**
 *  @brief Create a new SPI transaction.
 *
 *  This method allocates the transaction and two buffers on the heap.  It is essential
 *  that the caller cleans up both the transaction and the buffers after they are no
 *  longer necessary.
 * 
 *  @param message
 *      Message to send, this can be set to nullptr.  If this parameter is not nullptr
 *      then the Tx buffer will contain an encoded version of the message ready for
 *      transmission to the STM32.
 *
 *  @return pointer to spi_slave_transaction_t object
 *      This is the transaction that is returned by the ESP-IDF.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateSpiTransaction(Message *message)
{
    TRACE_MESSAGE("CreateSpiTransaction: Enter");
    spi_slave_transaction_t *transaction = static_cast<spi_slave_transaction_t *>(pvPortMalloc(sizeof(spi_slave_transaction_t)));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    ClearTransmitAndReceiveBuffers();
    if (message)
    {
        Encoders::EncodeMessage(message, _txBuffer, false);
    }
    transaction->rx_buffer = _rxBuffer;
    transaction->tx_buffer = _txBuffer;
    transaction->length = Message::MAXIMUM_SPI_FRAME_SIZE * 8;

    TRACE_MESSAGE("CreateSpiTransaction: Exit");
    return (transaction);
}

/**
 *  @brief Update the packet information in the message to prepare the message for the next transmission.
 * 
 *  This method takes the existing packet information and updates the offset and length fields ready for the
 *  next packet.
 * 
 *  Hint - set both the PacketOffset and PacketLength to 0 to calculate the correct starting values for the first packet.
 */
void SpiTransportProvider::UpdatePacketInformation(Message *message)
{
    TRACE_MESSAGE("%s: Enter", __func__);

    TRACE_MESSAGE("Initial offset %d, length %d", message->PacketOffset, message->PacketLength);
    message->PacketOffset += message->PacketLength;
    uint16_t amountRemaining = message->PayloadLength - message->PacketOffset;
    if (amountRemaining <= Message::MAXIMUM_PACKET_SIZE)
    {
        message->PacketLength = amountRemaining;
    }
    else
    {
        message->PacketLength = Message::MAXIMUM_PACKET_SIZE;
    }
    TRACE_MESSAGE("New offset %d, length %d", message->PacketOffset, message->PacketLength);

    TRACE_MESSAGE("%s: Exit", __func__);
}

/**
 *  @brief Release any storage associated with the SPI header transaction.
 *
 *  It is important that the storage used for the rx_buffer and tx_buffer(s) is only released if it is not 
 *  the _rxBuffer and the _txBuffer as these two buffers are used frequently so they need to remain in place.
 *
 *  @param transaction
 *      Pointer to the transaction (on the heap)
 */
void SpiTransportProvider::DeleteTransaction(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("DeleteTransaction: Enter");
    if (transaction)
    {
        if (transaction->rx_buffer && (transaction->rx_buffer != _rxBuffer))
        {
            heap_caps_free(static_cast<void *>(transaction->rx_buffer));
        }
        if (transaction->tx_buffer && (transaction->tx_buffer != _txBuffer))
        {
            heap_caps_free(const_cast<void *>(transaction->tx_buffer));
        }
        vPortFree(transaction);
    }
    TRACE_MESSAGE("DeleteTransaction: Exit");
}

/**
 *  @brief SPI Post Transmission callback
 *
 *  This method is called when the STM32 has ended the SPI transmission.
 *
 *  @param transaction
 *      spi_slave_transaction_t structure containing information about the
 *      SPI transaction that has just completed.
 */
void SpiTransportProvider::SpiPostTransmissionCallback(spi_slave_transaction_t *transaction)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    gpio_set_level(Gpio::ESP32_SPI_READY_PIN, 0);
    xTaskNotifyFromISR(_transportProviderTaskHandle, TransportProviderBase::STM32_INTERRUPT_NOTIFICATION_BIT, eSetBits, &xHigherPriorityTaskWoken);
    if (_spiInstance->_synchronisationSemaphore != NULL)
    {
        xSemaphoreGiveFromISR(_spiInstance->_synchronisationSemaphore, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR();
}

/**
 *  @brief SPI Post setup callback.
 *
 *  This method is called when the SPI interface has been setup and is
 *  ready to send / receive data.
 *
 *  @param transaction
 *      spi_slave_transaction_t structure containing information about the
 *      SPI transaction that has just completed.
*/
void SpiTransportProvider::SpiPostSetupCallback(spi_slave_transaction_t *transaction)
{
    gpio_set_level(Gpio::ESP32_SPI_READY_PIN, 1);
}

/**
 *  @brief Setup the SPI transport provider.
 *
 *  @param messageDispatcher
 *      MessageDispatcher object used to send messages from the SPI
 *      interface through to the interfaces on the ESP32.
 */
void SpiTransportProvider::Setup(MessageDispatcher *messageDispatcher)
{
    TRACE_MESSAGE("Setup: Enter");
    TransportProviderBase::Setup(messageDispatcher);
    TRACE_MESSAGE("Setup: Exit");
}

/**
 *  @brief Process the transport message and prepare the next transaction and
 *  change to the appropriate state.
 *
 *  @param request
 *      Inbound request from the STM32.
 *
 *  @returns
 *      Pointer to a new transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessTransportMessage(Message *request)
{
    TRACE_MESSAGE("ProcessTransportMessage: Enter");
    spi_slave_transaction_t *newTransaction = nullptr;
    switch (request->Function)
    {
        case TransportFunction::SendResponse:
            Message *message;
            message = Message::CreateOnHeap(MessageTypes::Transport | MessageTypes::Ack, Esp32Interfaces::Transport, TransportFunction::SendResponse, StatusCodes::CompletedOk);
            message->MessageID = request->MessageID;
            if (OutboundMessageQueueLength() > 0)
            {
                _outboundMessage = GetOutboundMessage();
                if (_outboundMessage)
                {
                    _outboundMessage->PacketOffset = 0;             // Initial state for the packet.
                    _outboundMessage->PacketLength = 0;

                    TRACE_MESSAGE("");
                    TRACE_MESSAGE("Preparing to send message:");
                    Logging::DumpMessage(COMPONENT_NAME, _outboundMessage);
                    TRACE_MESSAGE("");

                    _currentState = SendingPacketDetails;
                    message->PayloadLength = _outboundMessage->PayloadLength;
                }
            }
            else
            {
                message->StatusCode = StatusCodes::NoMessagesWaiting;
            }
            newTransaction = CreateSpiTransaction(nullptr);
            Encoders::EncodeMessage(message, _txBuffer, true);
            Message::DeleteMessage(message);
            _currentState = SendingPacketDetails;
            break;
        default:
            TRACE_MESSAGE("ProcessTransportMessage: Transport function %d not recognised.", request->Function);
            break;
    }
    TRACE_MESSAGE("ProcessTransportMessage: Exit");
    return(newTransaction);
}

/**
 *  @brief Process the newly received packet of data and return transaction to be used as the response.
 * 
 *  @param packet
 *      Packet of data that has just been received.
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessInboundPacket(Message *packet)
{
    TRACE_MESSAGE("ProcessInboundPacket: Enter");

    spi_slave_transaction_t *result;
    StatusCodes::StatusCodes responseCode = StatusCodes::CompletedOk;       //  We will assume success as this is what will happen most of the time.
    uint8_t messageType = MessageTypes::Ack;
    bool deletePacket;
    bool queuePacket;
    if (_incomingMessage)
    {
        TRACE_MESSAGE("Top up packet received, offset %d bytes, length %d bytes", packet->PacketOffset, packet->PacketLength);
        if (_incomingMessage->MessageID == packet->MessageID)
        {
            TRACE_MESSAGE("Packet matches incoming packet");
            if ((packet->PacketOffset + packet->PacketLength) <= _incomingMessage->PayloadLength)
            {
                TRACE_MESSAGE("Copying data.");
                memcpy(static_cast<void *>(_incomingMessage->Payload + packet->PacketOffset), static_cast<const void *>(packet->Payload), packet->PacketLength);
                if ((packet->PacketOffset + packet->PacketLength) == _incomingMessage->PayloadLength)
                {
                    TRACE_MESSAGE("Full message received, sending to queue.");
                    //
                    //  We have received the last packet in the message so hand it over to the system for processing.
                    //
                    _messageDispatcher->QueueMessageForEsp32(_incomingMessage);
                    _incomingMessage = nullptr;
                }
            }
            else
            {
                messageType = MessageTypes::Nak;
                responseCode = StatusCodes::InvalidPacket;
                Message::DeleteMessage(_incomingMessage);
                _incomingMessage = nullptr;
            }
        }
        else
        {
            responseCode = StatusCodes::InvalidPacket;
            messageType = MessageTypes::Nak;
            Message::DeleteMessage(_incomingMessage);
            _incomingMessage = nullptr;
        }
        queuePacket = false;
        deletePacket = true;
    }
    else
    {
        if (packet->PacketLength == packet->PayloadLength)
        {
            //
            //  If packet length and payload length are the same then we have a simple message and we just hand it
            //  over to the system for processing.
            //
            queuePacket = true;
            deletePacket = false;
        }
        else
        {
            //
            //  We are expecting a multi-packet message and this is the first packet.  This means that the payload memory
            //  represents the size of the packet and not the final payload.  We must therefore create a new buffer large
            //  enough to hold the full payload, copy the packet into the new buffer, release the current payload buffer
            //  and replace it with the new, larger, buffer.
            //
            TRACE_MESSAGE("First packet in a multipacket message, expecting %d bytes, received %d bytes", packet->PayloadLength, packet->PacketLength);
            uint8_t *payloadBuffer = static_cast<uint8_t *>(pvPortMalloc(packet->PayloadLength));
            memcpy(static_cast<void *>(payloadBuffer), static_cast<const void *>(packet->Payload), packet->PacketLength);
            vPortFree(packet->Payload);
            packet->Payload = payloadBuffer;
            _incomingMessage = packet;
            //
            //  We don't want to queue the message (packet) as we are expecting more data.
            //
            queuePacket = false;
            deletePacket = false;
        }
    }
    result = CreateAcknowledgementTransaction(packet, responseCode, messageType);
    if (deletePacket)
    {
        Message::DeleteMessage(packet);
    }
    if (queuePacket)
    {
        _messageDispatcher->QueueMessageForEsp32(packet);
    }

    TRACE_MESSAGE("ProcessInboundPacket: Exit");

    return(result);
}

/**
 *  @brief The STM32 has sent a request (message to the ESP32) so work out what is the appropriate next action.
 *
 *  @param transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::PacketReceived(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("InboundMessageReceived: Entering method");

    int dataSize = transaction->trans_len / 8;
    TRACE_MESSAGE("InboundMessageReceived: Received %d bytes.", dataSize);
    TRACE_HEX_BUFFER(transaction->rx_buffer, dataSize);
    
    spi_slave_transaction_t *newTransaction = nullptr;
    _currentState = AcknowledgingPacket;
    StatusCodes::StatusCodes responseCode;
    uint8_t messageType;

    if (dataSize >= _headerFrameSize)
    {
        Message *incomingPacket = Encoders::ExtractMessage(static_cast<uint8_t *>(transaction->rx_buffer), dataSize, false);
        if (!incomingPacket)             // nullptr indicates a CRC error.
        {
            ERROR_MESSAGE("InboundMessageReceived: CRC Error.");
            //
            //  If we are part way through a message then we abandon what we have on an error and
            //  the STM will requeue the message for later tranmission.
            //
            if (_incomingMessage)
            {
                Message::DeleteMessage(_incomingMessage);
                _incomingMessage = nullptr;
            }
            responseCode = StatusCodes::CrcError;
            messageType = MessageTypes::Nak;
            _outboundMessage = Message::CreateOnHeap(messageType, Esp32Interfaces::Transport, 0, responseCode);
            newTransaction = CreateAcknowledgementTransaction(_outboundMessage, responseCode, messageType);
        }
        else
        {
            switch (incomingPacket->MessageType)
            {
                case MessageTypes::Header:
                    newTransaction = ProcessInboundPacket(incomingPacket);
                    break;
                case MessageTypes::Transport:
                    newTransaction = ProcessTransportMessage(incomingPacket);
                    Message::DeleteMessage(incomingPacket);
                    break;
                default:
                    responseCode = StatusCodes::UnexpectedData;
                    messageType = MessageTypes::Nak;
                    Message::DeleteMessage(incomingPacket);
                    _outboundMessage = Message::CreateOnHeap(messageType, Esp32Interfaces::Transport, 0, responseCode);
                    newTransaction = CreateAcknowledgementTransaction(_outboundMessage, responseCode, messageType);
                    break;
            }
        }
    }
    else
    {
        responseCode = StatusCodes::InvalidHeader;
        messageType = MessageTypes::Nak;
        _outboundMessage = Message::CreateOnHeap(messageType, Esp32Interfaces::Transport, 0, responseCode);
        newTransaction = CreateAcknowledgementTransaction(_outboundMessage, responseCode, messageType);
    }
    DeleteTransaction(transaction);
    TRACE_MESSAGE("InboundMessageReceived: Exit");
    return(newTransaction);
}

/**
 *  @brief The inbound message has been acknowledged so clean up and go back to waiting for a message.
 *
 *  @param transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::PacketAcknowledged(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("PacketAcknowledged: Entering method");

    DeleteTransaction(transaction);
    spi_slave_transaction_t *newTransaction = CreateSpiTransaction(nullptr);
    Message::DeleteMessage(_outboundMessage);
    _outboundMessage = nullptr;
    _currentState = WaitingForPacket;
    
    TRACE_MESSAGE("PacketAcknowledged: Exit");
    return(newTransaction);
}

/**
 *  @brief Put the outbound message in the Tx buffer in case the STM32 wants to pick it up.
 *
 *  @param transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      SPI transaction created to receive the message acknowledgement.
 */
spi_slave_transaction_t *SpiTransportProvider::PacketDetailsSent(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("PacketDetailsSent: Enter");
    DeleteTransaction(transaction);
    //
    //  TODO: Deal with errors (outbound message is null etc.)
    //
    spi_slave_transaction_t *newTransaction;
    if (_outboundMessage)
    {
        TRACE_MESSAGE("Sending outbound message");
        UpdatePacketInformation(_outboundMessage);
        ClearTransmitAndReceiveBuffers();
        Encoders::EncodeMessage(_outboundMessage, _txBuffer, false);
        newTransaction = CreateSpiTransaction(_outboundMessage);
        _currentState = SendingPacket;
    }
    else
    {
        TRACE_MESSAGE("Outbound message is null");
        newTransaction = CreateSpiTransaction(nullptr);
    }
    TRACE_MESSAGE("PacketDetailsSent: Exit");
    return(newTransaction);
}

/**
 *  @brief Message has been sent so wait for the acknowledgement from the STM32.
 *
 *  @param transaction
 *      SPI transaction that was used to get the message header acknowledgement.
 *
 *  @returns
 *      New SPI transaction that can be used to communicate with the STM32.
 *
 */
spi_slave_transaction_t *SpiTransportProvider::PacketSent(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("PacketSent: Enter");

    DeleteTransaction(transaction);
    spi_slave_transaction_t *newTransaction = nullptr;

    TRACE_MESSAGE("PacketSent: Preparing message transaction.");
    newTransaction = CreateSpiTransaction(_outboundMessage);
    _currentState = WaitingForPacketAcknowledgement;
    TRACE_MESSAGE("PacketSent: Exit");
    return(newTransaction);
}

/**
 *  @brief We have received an acknowledgement from the STM32 so work out if ACK / NAK and take appropriate action.
 *
 *  @param transaction
 *      Last SPI transaction that was executed.
 *
 *  @returns
 *      New SPI transaction with data.
 */
spi_slave_transaction_t *SpiTransportProvider::PacketAcknowledgementReceived(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("PacketAcknowledgementReceived: Enter");

    Message *incomingMessage = Encoders::ExtractMessage(static_cast<uint8_t *>(transaction->rx_buffer), _headerFrameSize, true);
    DeleteTransaction(transaction);
    spi_slave_transaction_t *newTransaction = nullptr;
    //
    //  Assume that this is the last packet in the message so we prepare to process the next message
    //  unless the state is changed.
    //
    _currentState = WaitingForPacket;
    if (incomingMessage)
    {
        if (incomingMessage->MessageType == MessageTypes::Ack)
        {
            #if defined(PERFORMANCE_LOGGING)
            PERFORMANCE_LOGGING_EXIT();
            Logging::AddPerformanceCounterEntry(Logging::MessageSummary(_outboundMessage), _outboundMessage->PerformanceLoggingStart, performance_logging_end);
            #endif

            UpdatePacketInformation(_outboundMessage);
            if (_outboundMessage->PacketLength == 0)
            {
                Message::DeleteMessage(_outboundMessage);
                TRACE_MESSAGE("Final packet sent.");
            }
            else
            {
                TRACE_MESSAGE("Preparing next packet.");
                ClearTransmitAndReceiveBuffers();
                Encoders::EncodeMessage(_outboundMessage, _txBuffer, false);
                newTransaction = CreateSpiTransaction(_outboundMessage);
                _currentState = SendingPacket;
            }
        }
        else
        {
            TRACE_MESSAGE("Message is not ACK %02x", incomingMessage->MessageType);
            QueueMessageForStm32(_outboundMessage);
        }
    }
    else
    {
        TRACE_MESSAGE("Incoming message cannot be decoded.");
        QueueMessageForStm32(_outboundMessage);
    }
    Message::DeleteMessage(incomingMessage);
    if (_currentState == WaitingForPacket)
    {
        _outboundMessage = nullptr;
        newTransaction = CreateSpiTransaction(nullptr);
    }

    TRACE_MESSAGE("PacketAcknowledgementReceived: Exit");
    return(newTransaction);
}

/**
 *  @brief Check to see if the message buffer contains any data.
 *
 *  A message cannot be all zeros as there must be non-zero bytes in the CRC.
 *
 *  @param buffer
 *      Pointer to a buffer of data.
 *
 *  @param bufferLength
 *      Number of bytes in the buffer.
 *
 *  @returns
 *      true if the message contains valid (i.e. non-zero bytes) data.
 *      false if the buffer is not a valid encoded message.
 */
bool SpiTransportProvider::MessageContainsData(const uint8_t *buffer, uint32_t bufferLength)
{
    TRACE_MESSAGE("MessageContainsData: Enter");
    bool result = false;
    for (uint32_t index = 0; index < bufferLength; index++)
    {
        if (buffer[index] != 0)
        {
            result = true;
            break;
        }
    }
    TRACE_MESSAGE("MessageContainsData: Exit");
    return(result);
}

/**
 *  @brief Check to see if the ESP32 is sending data.
 *
 *  @param state
 *      State to check.
 *
 *  @returns
 *      true if the state indicates that the ESP32 is sending data,
 *      false otherwise.
 */
bool SpiTransportProvider::SendingData(SpiStates state)
{
    TRACE_MESSAGE("SendingData: Enter");
    TRACE_MESSAGE("SendingData: Exit");
    return((_currentState == AcknowledgingPacket)
            || (_currentState == SendingPacketDetails)
            || (_currentState == SendingPacket));
}

/**
 *  @brief Start the SPI communication and with for a signal that we have messages to process.
 *  Process the incoming message and return a new transaction that can be used for the
 *  next stage in the process.
 *
 *  @param transaction
 *      Valid SPI transaction that can be used to communicate with the STM32.
 *
 *  @returns
 *      Newly setup SPI transaction that can be used in the next stage of the process.
 *
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessMessages(spi_slave_transaction_t *transaction)
{
    spi_slave_transaction_t *newTransaction = nullptr;
    uint32_t notificationBits = 0;

    TRACE_MESSAGE("ProcessMessages: Waiting for notification, current state: %s", SpiStateString(_currentState));
    ESP_ERROR_CHECK(spi_slave_queue_trans(VSPI_HOST, transaction, portMAX_DELAY));
    xTaskNotifyWait(0x00000000, ULONG_MAX, &notificationBits, portMAX_DELAY);

    //
    //  ESP-IDF documentation states that a call to spi_slave_get_trans_result
    //  is mandatory when spi_slave_queue_trans is used.
    //
    ESP_ERROR_CHECK(spi_slave_get_trans_result(VSPI_HOST, &transaction, portMAX_DELAY));
    if (SendingData(_currentState) || MessageContainsData(static_cast<uint8_t *>(transaction->rx_buffer), transaction->length / 8))
    {
        switch (_currentState)
        {
            //
            //  STM sending requests.
            //
            case WaitingForPacket:
                newTransaction = PacketReceived(transaction);
                break;
            case AcknowledgingPacket:
                newTransaction = PacketAcknowledged(transaction);
                break;
            //
            //  STM asking for the result of a request or event data.
            //
            case SendingPacketDetails:
                newTransaction = PacketDetailsSent(transaction);
                break;
            case SendingPacket:
                newTransaction = PacketSent(transaction);
                break;
            case WaitingForPacketAcknowledgement:
                newTransaction = PacketAcknowledgementReceived(transaction);
                break;

            default:
                ERROR_MESSAGE("ProcessMessages: Unhandled state: %s", SpiStateString(_currentState));
                break;
        }
    }
    else
    {
        TRACE_MESSAGE("ProcessMessages: Empty buffer, discarding data and staying in state %s", SpiStateString(_currentState));
        newTransaction = transaction;
    }
    return(newTransaction);
}

/**
 *  @brief Process any messages for the controller.
 *
 *  The messages will be generated by the incoming SPI interface.
 *
 *  Note that this method is static in order to allow the C methods in
 *  FreeRTOS to call the method.
 *
 *  @param pvParameters
 *      Pointer to the instance of the request handler receiving the
 *      messages for dispatch through the instance specific handler.
 */
void SpiTransportProvider::Task(void *pvParameters)
{
    SpiTransportProvider *sender = static_cast<SpiTransportProvider *>(pvParameters);

    //
    //  It is important to wait for the outbound message queue to be
    //  created before we try to process any messages.
    //
    while (!_outboundMessageQueueHandle)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    spi_slave_transaction_t *transaction = sender->CreateSpiTransaction(nullptr);
    while (true)
    {
        TRACE_MESSAGE("");
        TRACE_MESSAGE("****************************** New State ******************************");
        TRACE_MESSAGE("");        
        transaction = sender->ProcessMessages(transaction);
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(sender->_transportProviderTaskHandle);
}

/**
 *  @brief Task to send synchronisation pulses to the STM32.
 *
 *  There can sometimes be a problem with the synchronisation between the ESP32 and the
 *  STM32.  This arises due to the real-time nature of the startup process.  So the signal
 *  the STM32 indicating the readiness of the ESP32 may be missed as the interrupt
 *  handler on the STM32 is not set up when the ESP32 sends the "Message waiting"
 *  signal to the STM32.
 *
 *  To overcome this, this task will send the message waiting signal to the STM32 every 500ms.
 *  The binary synchronisation semaphore indicates if the request has been made or not.
 *
 *  Note that this method is static in order to allow the C methods in
 *  FreeRTOS to call the method.
 *
 *  @param pvParameters
 *      Pointer to the instance of the request handler.
 */
void SpiTransportProvider::SynchronisationTask(void *pvParameters)
{
    SpiTransportProvider *sender = static_cast<SpiTransportProvider *>(pvParameters);
    TRACE_MESSAGE_SPECIFY_COMPONENT(sender->COMPONENT_NAME, "Synchronising with the STM32.");
    //
    //  It is important to wait for the outbound message queue to be
    //  created before we try to process any messages.
    //
    while (xSemaphoreTake(sender->_synchronisationSemaphore, pdMS_TO_TICKS(2000)) != pdTRUE)
    {
        TRACE_MESSAGE_SPECIFY_COMPONENT(sender->COMPONENT_NAME, "Sending synchronisation pulse to STM32.");
        sender->ToggleEsp32MessageWaitingPin();
    }
    vSemaphoreDelete(sender->_synchronisationSemaphore);
    sender->_synchronisationSemaphore = NULL;

    TRACE_MESSAGE_SPECIFY_COMPONENT(sender->COMPONENT_NAME, "Synchronisation complete.");

    vTaskDelete(sender->_synchronisationTaskHandle);
    sender->_synchronisationTaskHandle = NULL;
}
