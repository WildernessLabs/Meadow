/**
 *  ThreadPool.cpp
 *
 *  Implement the methods in the ThreadPool class.
 */
#include "sdkconfig.h"

#include "Logging.hpp"
#include "ThreadPool.hpp"
#include "Exceptions/ThreadPoolException.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task for the ThreadPool.
 */
const char *ThreadPool::COMPONENT_NAME = "ThreadPool";

/**
 *  @brief Counting semaphore used to indicate if there are any thread available.
 */
SemaphoreHandle_t ThreadPool::_threadsAvailable = NULL;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor.
 */
ThreadPool::ThreadPool()
{
}

/**
 *  @brief Constructor for the ThreadPool.
 *
 *  @param numberOfThreads
 *      Number of threads that should be created in this thread pool.
 */
ThreadPool::ThreadPool(uint32_t numberOfThreads) : ThreadPool()
{
    _threadPoolSize = numberOfThreads > MAX_THREADS ? MAX_THREADS : numberOfThreads;
    _threads = static_cast<Thread **>(pvPortMalloc(_threadPoolSize * sizeof(Thread *)));
    if (_threads == NULL)
    {
        throw new ThreadPoolException("Cannot allocate memory for threads.");
    }

    _threadsAvailable = xSemaphoreCreateCounting(_threadPoolSize, 0);
    if (_threadsAvailable == NULL)
    {
        throw new ThreadPoolException("Cannot create semaphore for ThreadPool.");
    }

    for (unsigned int index = 0; index < _threadPoolSize; index++)
    {
        const unsigned int THREAD_NAME_LENGTH = 20;
        char *name = static_cast<char *>(pvPortMalloc(THREAD_NAME_LENGTH));
        if (name != NULL)
        {
            bzero(name, THREAD_NAME_LENGTH);
            snprintf(name, THREAD_NAME_LENGTH, "Thread-%02u", index);
            _threads[index] = new Thread(name, ThreadPool::ThreadCompleted);;
            if (_threads[index])
            {
                xSemaphoreGive(_threadsAvailable);
            }
        }
        else
        {
            throw new ThreadPoolException("Cannot allocate memory for thread names.");
        }
    }
}

/**
 *  @brief Destructor for the ThreadPool.
 */
ThreadPool::~ThreadPool()
{
    if (_threadsAvailable != NULL)
    {
        for (uint32_t index = 0; index < _threadPoolSize; index++)
        {
            if (_threads[index])
            {
                delete _threads[index];
            }
        }
        vPortFree(_threadsAvailable);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Execute the specified method in the thread pool.
 *
 *  @param method
 *      Pointer to the method that should be executed.
 *
 *  @param parameter
 *      Parameter to be passed to the method.
 */
void ThreadPool::Execute(method_t method, void *parameter)
{
    TRACE_MESSAGE("Execute: Enter");
    if (xSemaphoreTake(_threadsAvailable, portMAX_DELAY) == pdTRUE)
    {
        for (uint32_t index = 0; index < _threadPoolSize; index++)
        {
            if (_threads[index] && !_threads[index]->IsBusy())
            {
                _threads[index]->Execute(method, parameter);
                return;
            }
        }
    }
    else
    {
        throw new ThreadPoolException("Cannot obtain access to threads.");
    }
    TRACE_MESSAGE("Execute: Exit");
}

/**
 *  @brief Used by the Thread to indicate that it has completed execution of a method and it can be used again.
 */
void ThreadPool::ThreadCompleted()
{
    xSemaphoreGive(_threadsAvailable);
}