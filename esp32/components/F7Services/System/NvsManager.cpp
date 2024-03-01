/*
 *  NvsManager.hpp
 *
 *  Provide a mechanism for storing and retrieving data in non-Volatile storage (NVS).
 */
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "NvsManager.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task.
 */
const char *NvsManager::COMPONENT_NAME = "NvsManager";

/**
 *  @brief Determine if this object has been created already.
 */
bool NvsManager::_nvsAvailable = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the NVS manager class.
 */
NvsManager::NvsManager()
{
}

/**
 * @brief Destructor for the NVS manager class.
 */
NvsManager::~NvsManager()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the non-volatile storage on the ESP32.
 */
void NvsManager::Setup()
{
    esp_err_t result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_LOGI(__func__, "Formatting NVS.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    _nvsAvailable = (result == ESP_OK);
}

/**
 *  @brief Get a string value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to read the value from.
 *
 *  @param name
 *      Name of the value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
char * NvsManager::GetValue(const char *nameSpace, const char *name, const char *defaultValue)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    char *result = const_cast<char *>(defaultValue);

    if (_nvsAvailable)
    {
        error = nvs_open(nameSpace, NVS_READONLY, &nvsHandle);
        if (error == ESP_OK)
        {
            size_t length = 0;
            if (nvs_get_str(nvsHandle, name, NULL, &length) == ESP_OK)
            {
                result = static_cast<char *>(pvPortMalloc(length));
                if (result != NULL)
                {
                    if (nvs_get_str(nvsHandle, name, result, &length) != ESP_OK)
                    {
                        vPortFree(result);
                        result = const_cast<char *>(defaultValue);
                    }
                }
            }
            nvs_close(nvsHandle);
        }
    }

    return(result);
}

/**
 *  @brief Write string value into NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to write to.
 *
 *  @param name
 *      Name of the value to be written to NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure.
 */
NvsManager::ErrorCodes NvsManager::SetValue(const char *nameSpace, const char *name, const char *value)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    NvsManager::ErrorCodes result = ErrorCodes::Ok;

    if (_nvsAvailable)
    {
        error = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
        if (error != ESP_OK)
        {
            result = ErrorCodes::Error;
        }
        else
        {
            error = nvs_set_str(nvsHandle, name, value);
            if (error == ESP_ERR_NVS_NOT_FOUND)
            {
                result = ErrorCodes::NotFound;
            }
            else
            {
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            if (result == ErrorCodes::Ok)
            {
                error = nvs_commit(nvsHandle);
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            nvs_close(nvsHandle);
        }
    }
    else
    {
        result = ErrorCodes::NotAvailable;
    }

    return(result);
}

/**
 *  @brief Get an unsigned integer value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to read the value from.
 *
 *  @param name
 *      Name of the value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
uint32_t NvsManager::GetValue(const char *nameSpace, const char *name, const uint32_t defaultValue)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    uint32_t result = defaultValue;

    if (_nvsAvailable)
    {
        error = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
        if (error == ESP_OK)
        {
            nvs_get_u32(nvsHandle, name, &result);      // Result is not changed if there is an error.
            nvs_close(nvsHandle);
        }
    }
    return(result);
}

/**
 *  @brief Write an unsigned integer (32-bits) value into NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to write to.
 *
 *  @param name
 *      Name of the value to be written to NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure.
 */
NvsManager::ErrorCodes NvsManager::SetValue(const char *nameSpace, const char *name, const uint32_t value)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    NvsManager::ErrorCodes result = ErrorCodes::Ok;

    if (_nvsAvailable)
    {
        error = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
        if (error != ESP_OK)
        {
            result = ErrorCodes::Error;
        }
        else
        {
            error = nvs_set_u32(nvsHandle, name, value);
            if (error == ESP_ERR_NVS_NOT_FOUND)
            {
                result = ErrorCodes::NotFound;
            }
            else
            {
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            if (result == ErrorCodes::Ok)
            {
                error = nvs_commit(nvsHandle);
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            nvs_close(nvsHandle);
        }
    }
    else
    {
        result = ErrorCodes::NotAvailable;
    }

    return(result);
}

/**
 *  @brief Get an integer value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to read the value from.
 *
 *  @param name
 *      Name of the value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
int32_t NvsManager::GetValue(const char *nameSpace, const char *name, const int32_t defaultValue)
{
    return((int) GetValue(nameSpace, name, (uint32_t) defaultValue));
}

/**
 *  @brief Write an signed integer (32-bits) value into NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to write to.
 *
 *  @param name
 *      Name of the value to be written to NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure.
 */
NvsManager::ErrorCodes NvsManager::SetValue(const char *nameSpace, const char *name, const int32_t value)
{
    return(SetValue(nameSpace, name, (uint32_t) value));
}

/**
 *  @brief Get a byte value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to read the value from.
 *
 *  @param name
 *      Name of the value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
uint8_t NvsManager::GetValue(const char *nameSpace, const char *name, const uint8_t defaultValue)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    uint8_t result = defaultValue;

    if (_nvsAvailable)
    {
        error = nvs_open(nameSpace, NVS_READONLY, &nvsHandle);
        if (error == ESP_OK)
        {
            nvs_get_u8(nvsHandle, name, &result);   // Result is not changed if there is an error.
            nvs_close(nvsHandle);
        }
    }

    return(result);
}

/**
 *  @brief Write a byte value into NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to write to.
 *
 *  @param name
 *      Name of the value to be written to NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure.
 */
NvsManager::ErrorCodes NvsManager::SetValue(const char *nameSpace, const char *name, const uint8_t value)
{
    nvs_handle nvsHandle;
    esp_err_t error;
    NvsManager::ErrorCodes result = ErrorCodes::Ok;

    if (_nvsAvailable)
    {
        error = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
        if (error != ESP_OK)
        {
            result = ErrorCodes::Error;
        }
        else
        {
            error = nvs_set_u8(nvsHandle, name, value);
            if (error == ESP_ERR_NVS_NOT_FOUND)
            {
                result = ErrorCodes::NotFound;
            }
            else
            {
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            if (result == ErrorCodes::Ok)
            {
                error = nvs_commit(nvsHandle);
                if (error != ESP_OK)
                {
                    result = ErrorCodes::Error;
                }
            }
            nvs_close(nvsHandle);
        }
    }
    else
    {
        result = ErrorCodes::NotAvailable;
    }

    return(result);
}

/**
 *  @brief Get a boolean value.
 *
 *  Get a value from NVS.  If a value with the specified name cannot be found
 *  then the default value is returned and the default value is written to
 *  NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to read the value from.
 *
 *  @param name
 *      Name of the value to read from NVS.
 *
 *  @param defaultValue
 *      Default value to be used if the value is not found.
 *
 *  @returns
 *      Current boolean value or the default value if a value could not be found.
 */
bool NvsManager::GetValue(const char *nameSpace, const char *name, const bool defaultValue)
{
    return(GetValue(nameSpace, name, (uint8_t) (defaultValue ? 1 : 0)) == 1);
}

/**
 *  @brief Write a boolean value into NVS.
 *
 *  @param nameSpace
 *      Name of the namespace to write to.
 *
 *  @param name
 *      Name of the value to be written to NVS.
 *
 *  @param value
 *      Value to be set.
 *
 *  @returns
 *      Error code indicating success or failure.
 */
NvsManager::ErrorCodes NvsManager::SetValue(const char *nameSpace, const char *name, const bool value)
{
    return(SetValue(nameSpace, name, (uint8_t) (value ? 1 : 0)));
}
