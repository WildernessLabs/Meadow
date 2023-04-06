/**
 *  ThreadPoolException.cpp
 * 
 *  Implementation of the ThreadPoolException class methods.
 */
#include "ThreadPoolException.hpp"

/**
 *  @brief Default constructor for the ThreadPoolException class.
 */
ThreadPoolException::ThreadPoolException() : std::runtime_error("ThreadPoolException")
{
}

/**
 *  @brief Create an instance of the ThreadPoolException class with the specified message.
 * 
 *  @param message
 *      Message to be used in the construction of the instance of the ThreadPoolException class.
 */
ThreadPoolException::ThreadPoolException(const std::string &message) : std::runtime_error(message)
{
}
