# Implementing `poll`

The general idea of `poll` is to allow the system to wait for events on specific file descriptors.  File descriptors are not just associated with file but also with pipes, network sockets etc.

Each file descriptor can be asked to wait on different types of events.

The `poll` call is asked to wait for a specific time period or indefinitely (-1 used as the timeout).

`poll` will return when any of the file descriptors see the event it has been asked to wait for.

`poll` is implemented using different techniques on all four levels (Mono, NuttX, usrsock and ESP32).

From this point on, unless specified explicitly, we will be assuming that the file descriptor represents a network socket connection through to a server.

We will also be considering a `poll` request from a .NET application which has been passed to Mono.

Additionally, we will only be considering `poll` requests through to the usrsock implementations.  It is assumed that `poll` requests to builtin network drivers will work as designed in Nuttx.

## Mono

The .NET application will pass a `poll` request for a single socket through to Mono.  This will eventually be passed on to the threadpool for processing.

The threadpool will add a file descriptor for a pipe to the list of descriptors being passed on the the actual `poll` implementation (in this case Nuttx).  The file descriptor for the pipe can be used by Mono to terminate the `poll` request if necessary.

Mono finally calls the OS implementation of `poll` with the two file descriptors with a timeout set to -1 (infinite timeout).

If at some point in the future Mono decides that the `poll` request has been executing for too long then it will write to the pipe file descriptor which will terminate the `poll` request.

## Nuttx

Nuttx receives the `poll` request from the Mono threadpool and this request now has two file descriptors and associated events to wait for.  The two file descriptors represent:

* Pipe added by the Mono threadpool
* Network socket that the .NET runtime is connected to

The Nuttx implementation of `poll` breaks down into three phases:

* Setup
* Teardown
* Signalling from the file descriptor

The system creates a semaphore prior to the setup phase.  This can be used in the signalling phase of a `poll` request.

Additionally, the `poll` method calls different setup and teardown methods depending upon the type of the file descriptor.

### Setup and Teardown

In the setup and teardown stage, the usrsock driver is passed information about the socket and the type of events to be monitored along with the semaphore that was created at the start of the `poll` process.

Nuttx calls the same method for both setup and teardown.  The final parameter in the methods parameter list indicates if the system is setting up or tearing down a `poll` request.

For setup the usrsock driver should allocate any resources required to monitor the events.  Conversely, for teardown the usrsock driver should free any resources that were allocated in the setup phase.

### Signalling

If a call to `poll` receives the event that it is waiting for then the semaphore is used to signal the calling code (in this case Nuttx) that the result of the call is ready for processing.  Nuttx will respond by recording the events and proceeding to the Teardown stage for all of the file descriptors.

## ESP32 usrsock Driver (STM32)

The usrsock driver on the STM32 provides the following service:

* Setup
* Teardown
* Signalling through an interrupt/event handler

### usrsock Setup and Teardown

The setup method sends the setup request through to the ESP32 and then records the setup request in a list of pending requests.

The teardown method sends the teardown request through to the ESP32 and then removes the request from the list of pending requests.

There is a slight complication her as the teardown request is running in a different thread to the interrupt/event handler.  As a result, the teardown request may be preempted by the interrupt/event handler.  The interrupt event handler may respond to the events being generated and so remove the information about the requests from the list of pending requests.

Similarly, the interrupt/event handler may be preempted by the teardown request.

The system will always assume success in both of these cases.

### usrsock Signalling

The interrupt/event handler will receive notifications from the ESP32 that the `poll` request conditions have been met.  It will then search the list of in flight requests and signal, through the semaphore, that the event has been raised.  The request will be removed from the list of pending requests as part of the signalling process.

## ESP32 Handler

The ESP32 handler deals with the actual `poll` request on the network socket.  It does this through a similar mechanism to the usrsock driver:

* Setup
* Teardown
* Poll handler

### ESP32 Setup and Teardown

The ESP32 receives the same requests as the usrsock driver on the ESP32, namely setup and teardown through the same method call with a parameter indicating if the request is a setup or teardown request.  The ESP32 driver directs the request accordingly.

#### ESP32 Setup

Setup follows much the same process as the other POSIX calls, translating the various flags from Nuttx values to ESP-IDF values and preparing for the call.  `poll` deviates form the usual process in that it does not actually make the `poll` request and then return the result.  Instead, `poll` passes the responsibility of making the actual `poll` call to one of the threads in the shared threadpool.  

The setup process for the `poll` operations works in a manner similar to the Mono operation.  The single file descriptor becomes two file descriptors, one for the original `poll` request and a second "dummy" file descriptor pointing to port 80 on the ESP32.  The dummy file descriptor is used to signal the threadpool that the `poll` request should be terminated.  This is required as Mono specified -1 as the timeout period making the `poll` request execute indefinitely.

The information about the `poll` request is stored in a C++ `map` collection once the additional file descriptor has been created.

Setup ends the process by returning a success or failure code back to usrsock driver running on the STM32.

#### ESP32 Teardown

Teardown is executed at the request of Mono.  It first attempts to find the information for the specified socket in the `map` collection.  It is possible that the information cannot be found in the collection.  This can occur if the actual call ro `poll` on the ESP32 has returned due to the events condition being met.  This can occur due to the timing of the various threads running on the ESP32 (and the STM32).

In the case where the information for the socket is found, Teardown will send a byte of data to the dummy socket to terminate the `poll` request.  The dummy socket is then closed and the method indicates success.

#### ESP32 Poll Handler

The `poll` handler method executes the actual call to `poll` on the ESP32.  The call will terminate if either the socket or the dummy socket indicate that the the events being monitored have been raised.  The call will process the request and return the results to the STM32 through an interrupt event.

This method must also take into account that the information about the `poll` request may not exist in the `map` collection as Teardown may have removed this before the `poll` method has exited.
