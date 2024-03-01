/**
 *  ThreadPool.hpp
 *
 *  Define the methods in the ThreadPool class.
 */
#ifndef _THREAD_POOL_HPP_
#define _THREAD_POOL_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Thread.hpp"

/**
 *  @brief Define the ThreadPool class.
 */
class ThreadPool
{
private:
    /**
     *  @brief Maximum number of threads we should allow.
     */
    const int MAX_THREADS = 99;

    /**
     *  @brief Size of the thread pool.
     */
    const int THREAD_POOL_SIZE = 10;

    /**
     *  @brief Number of threads in the threadpool.
     */
    uint32_t _threadPoolSize = 0;

    /**
     *  @brief Threads in the thread pool.
     */
    Thread **_threads = nullptr;

    /**
     *  @brief Counting semaphore used to indicate if a thread is available.
     */
    static SemaphoreHandle_t _threadsAvailable;

    /**
     *  @brief Default constructor.
     */
    ThreadPool();

public:
    /**
     *  @brief Name of the componet / task for the ThreadPool.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Create a ThreadPool of the specified size.
     */
    explicit ThreadPool(uint32_t numberOfThreads);

    /**
     *  @brief Destructor.
     */
    ~ThreadPool();

    /**
     *  @brief Use of the the thread to execute a method.
     */
    void Execute(method_t method, void *parameter);

    /**
     *  @brief A call back method to allow a thread to inform the ThreadPool that is has completed a task an is available.
     */
    static void ThreadCompleted();
};

#endif // #ifndef _THREAD_POOL_HPP_