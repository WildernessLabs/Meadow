/*
 *  NvsManager.hpp
 *
 *  Provide a mechanism for storing and retrieving data in Non-Volatile Storage (NVS).
 */

#ifndef _NVS_MANAGER_HPP_
#define _NVS_MANAGER_HPP_

#include "sdkconfig.h"

#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

/**
 *  @brief Manage the non-volatile storage.
 */
class NvsManager
{
public:
    /**
     *  @brief Error codes for the configuration manager.
     */
    enum ErrorCodes { Ok, Error, NotAvailable, NotFound, BufferSizeMismatch };

private:
    /**
     *  @brief Indicate if the NVS memory can be used.
     */
    static bool _nvsAvailable;

public:
    /**
     *  @brief Name of the component / task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Default constructor.
     */
    NvsManager();

    /**
     *  @brief Destructor.
     */
    virtual ~NvsManager();

    /**
     *  @brief Setup the non-volatile storage.
     */
    static void Setup();

    /**
     *  @brief Get a string value.
     */
    static char *GetValue(const char *nameSpace, const char *name, const char *defaultValue);

    /**
     *  @brief Set a string value.
     */
    static ErrorCodes SetValue(const char *nameSpace, const char *name, const char *value);

    /**
     *  @brief Get an unsigned integer value.
     */
    static uint32_t GetValue(const char *nameSpace, const char *name, const uint32_t defaultValue);

    /**
     *  @brief Set an unsigned integer value.
     */
    static ErrorCodes SetValue(const char *nameSpace, const char *name, const uint32_t value);

    /**
     *  @brief Get an integer value.
     */
    static int32_t GetValue(const char *nameSpace, const char *name, const int32_t defaultValue);

    /**
     *  @brief Set an integer value.
     */
    static ErrorCodes SetValue(const char *nameSpace, const char *name, const int32_t value);

    /**
     *  @brief Get a byte value.
     */
    static uint8_t GetValue(const char *nameSpace, const char *name, const uint8_t defaultValue);

    /**
     *  @brief Set a byte value.
     */
    static ErrorCodes SetValue(const char *nameSpace, const char *name, const uint8_t value);

    /**
     *  @brief Get a boolean value.
     */
    static bool GetValue(const char *nameSpace, const char *name, const bool defaultValue);

    /**
     *  @brief Set a boolean value.
     */
    static ErrorCodes SetValue(const char *nameSpace, const char *name, const bool value);
};

#endif /* _NVS_MANAGER_HPP_ */
