/**
 *  Thread.hpp
 *
 *  Outline the Thread class used by the ThreadPool.
 */
#ifndef _THREAD_HPP_
#define _THREAD_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"

/**
 *  @brief Type definition for the method that will be called by the Task method.
 */
typedef void (*method_t)(void *);

/**
 *  @brief ThreadPool method that will be called to indicate that this Thread has completed the execution of the method.
 */
typedef void (*task_completed_method_t)();

/**
 *  @brief Thread class used by ThreadPool to execute tasks/methods.
 */
class Thread
{
private:
    /**
     *  @brief Name used to identify this thread.
     */
    char *_name = nullptr;

    /**
     *  @brief Task handle for the FreeRTOS thread associated with this thread.
     */
    TaskHandle_t _taskHandle = nullptr;

    /**
     *  @brief Lock for controlling access to the members of this class.
     */
    SemaphoreHandle_t _mutex = NULL;

    /**
     *  @brief Sempahore that will be used to trigger the execution of the _method.
     */
    SemaphoreHandle_t _executeMethod = NULL;

    /**
     *  @brief Method to be execute by the task.
     */
    method_t _method;

    /**
     *  @brief First parameter to the passed to the method.
     */
    void *_parameter;

    /**
     *  @brief Task handle for the task that will actually execute the method.
     */
    task_completed_method_t _taskCompletedMethod = NULL;

    /**
     *  @brief Indicate if this thread is actually running a method.
     */
    bool _isBusy = false;

    /**
     *  @brief Constructor for the thread.
     */
    Thread();

    /**
     *  @brief Task that is spawned off into the FreeRTOS task that will execute methods.
     */
    static void Task(void *taskParameters);

public:
    /**
     *  @brief Name of the componet / task for the Thread class.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Constructor that should be used by the application.
     */
    explicit Thread(char *name, task_completed_method_t taskCompletedMethod);

    /**
     *  @brief Destructor for the thread.
     */
    ~Thread();

    /**
     *  @brief Execute a method on this threads task passing the required parameters
     */
    void Execute(method_t method, void *parameter);

    /**
     *  @brief Indicate if this thread is busy.
     */
    bool IsBusy();

    /**
     *  @brief Take the exectution semaphore putting the thread into a holding state.
     */
    void TakeExecutionSemaphore();

    /**
     *  @brief Set the class as completed and ready to execute another method.
     */
    void TaskCompleted();

    /**
     *  @brief Get the task name (used for debugging and identification purposes).
     */
    char *GetTaskName();

    /**
     *  @brief Get the method that should be executed in the FreeRTOS task.
     */
    method_t GetMethod();

    /**
     *  @brief Get the parameters that should be passed to the method.
     */
    void *GetParameter();
};

#endif // #ifndef _THREAD_HPP_