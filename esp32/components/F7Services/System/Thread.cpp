/**
 *  Thread.cpp
 *
 *  Implementation of the methods for the Thread class as used by the ThreadPool class.
 */
#include "sdkconfig.h"

#include "Logging.hpp"
#include "Thread.hpp"
#include "Exceptions/ThreadPoolException.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task for the Thread class.
 */
const char *Thread::COMPONENT_NAME = "Thread";

/**
 *  @brief Default constructor.
 *
 *  Ths is not intended to be called by the user.
 */
Thread::Thread() : _mutex(xSemaphoreCreateBinary()), _executeMethod(xSemaphoreCreateBinary())
{
    _method = nullptr;
    _parameter = nullptr;
    if (_mutex == NULL)
    {
        throw new ThreadPoolException("Cannot create mutex for thread.");
    }
    if (_executeMethod == NULL)
    {
        throw new ThreadPoolException("Cannot create execute mutex.");
    }
    //
    //  Note from the FreeRTOS documentation:
    //
    //  The semaphore is created in the 'empty' state, meaning the semaphore must first be given using 
    //  the xSemaphoreGive() API function before it can subsequently be taken (obtained) using the 
    //  xSemaphoreTake() function.
    //
    xSemaphoreGive(_mutex);
    xSemaphoreGive(_executeMethod);
    //
    //  We take the execution semaphore before starting the task as the first thing the
    //  task will do is to try and take the semaphore.  Once it has taken the semaphore
    //  the task will start to execute the method.
    //
    TakeExecutionSemaphore();
}

/**
 *  @brief Constructor for the Thread class.
 *
 *  @param name
 *      Name of the thread.  This allows the thread to be easily identified.
 *
 *  @param taskCompletedMethod
 *      Pointer to the method that should be called when the task has completed and the
 *      thread is ready to run a new task.
 */
Thread::Thread(char *name, task_completed_method_t taskCompletedMethod) : Thread()
{
    TRACE_MESSAGE("Thread: Enter creating thread %s", name);
    _name = name;
    _taskCompletedMethod = taskCompletedMethod;
    if (xTaskCreate(Thread::Task, _name, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle) != pdPASS)
    {
        throw new ThreadPoolException("Cannot create task.");
    }
    TRACE_MESSAGE("Thread: Exit creating thread %s", name);
}

/**
 *  @brief Destructor for the class.
 */
Thread::~Thread()
{
    vPortFree(_name);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Get the name of this task.
 *
 *  Note that a mutex is not required to guard access to the backing variable as
 *  this is only set up in the constructor.
 */
char *Thread::GetTaskName()
{
    return(_name);
}

/**
 *  @brief Get the method that should be executed in this thread.
 */
method_t Thread::GetMethod()
{
    method_t method = NULL;

    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        method = _method;
        xSemaphoreGive(_mutex);
    }

    return(method);
}

/**
 *  @brief Get the parameter to be passed to the method.
 */
void *Thread::GetParameter()
{
    void *parameter = NULL;

    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        parameter = _parameter;
        xSemaphoreGive(_mutex);
    }

    return(parameter);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Is this thread busy executing a method?
 */
bool Thread::IsBusy()
{
    bool busy = true;

    if (xSemaphoreTake(_mutex, 10 / portTICK_PERIOD_MS) == pdTRUE)
    {
        busy = _isBusy;
        xSemaphoreGive(_mutex);
    }
    return(busy);
}

/**
 *  @brief Execute the specified method in this thread.
 *
 *  @param method
 *      Pointer to the method that should be executed.
 *
 *  @param parameter
 *      Parameter to be passed to the method.
 */
void Thread::Execute(method_t method, void *parameter)
{
    TRACE_MESSAGE("Execute: Thread %s Enter", _name);
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        if (_isBusy)
        {
            throw new ThreadPoolException("Attempt to execute method on a busy thread.");
        }
        _isBusy = true;
        _method = method;
        _parameter = parameter;
        xSemaphoreGive(_mutex);
        xSemaphoreGive(_executeMethod);
    }
    TRACE_MESSAGE("Execute: Thread %s Exit", _name);
}

/**
 *  @brief Take the execution semaphore.
 *
 *  Taking the semaphore is used to put the FreeRTOS task in a waiting state.
 */
void Thread::TakeExecutionSemaphore()
{
    if (xSemaphoreTake(_executeMethod, portMAX_DELAY) == pdTRUE)
    {
        return;
    }
    else
    {
        throw new ThreadPoolException("Task cannot take the execution mutex.");
    }
}

/**
 *  @brief Put the thread into a completed state making it ready for a new task to be assigned.
 */
void Thread::TaskCompleted()
{
    TRACE_MESSAGE("TaskCompleted: Thread %s Enter", _name);
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        _isBusy = false;
        _method = nullptr;
        _parameter = nullptr;
        xSemaphoreGive(_mutex);
    }
    if (_taskCompletedMethod != NULL)
    {
        (_taskCompletedMethod());
    }
    TRACE_MESSAGE("TaskCompleted: Thread %s Exit", _name);
}

/**
 *  @brief FreeRTOS task that will execute the methods passed to it.
 *
 *  @param taskParameters
 *      Parameters passed to the task when it is created by xCreateTask.
 *      This should be set to be a pointer to the instantiated class that
 *      created the FreeRTOS task.
 */
void Thread::Task(void *taskParameters)
{
    Thread *sender = static_cast<Thread *>(taskParameters);
    while (true)
    {
        sender->TakeExecutionSemaphore();
        (sender->GetMethod())(sender->GetParameter());
        sender->TaskCompleted();
    }
}