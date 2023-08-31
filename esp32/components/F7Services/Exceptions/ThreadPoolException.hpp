/**
 *  ThreadPoolException.hpp
 * 
 *  Outline the ThreadPoolException class.
 */
#include <stdexcept>
#include <string>

/**
 *  @breif Exceptions thrown by the Thread and the ThreadPool classes.
 */
class ThreadPoolException : public std::runtime_error
{
public:
    /**
     *  @brief Default constructor.
     */
    ThreadPoolException();

    /**
     *  @brief Constructor using a given text message.
     */
    explicit ThreadPoolException(const std::string &message);
};
