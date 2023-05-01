/*
 *  FileSystem.hpp
 *
 *  File system message request handlers
 */

#ifndef _FILE_SYSTEM_HPP_
#define _FILE_SYSTEM_HPP_

#include "sdkconfig.h"

#include "esp_littlefs.h"

#include "Esp32Messaging.hpp"

class FileSystem
{
private:
    /**
     *  @brief Object that will be used to send messages to the STM32 from the ESP32.
     */
    IMessageDispatcher *_messageDispatcher;

    /**
     * @brief LittleFS file system configuration.
     */
    esp_vfs_littlefs_conf_t _config;

    /**
     * @brief Private File System constructor.
     */
    FileSystem();

    /**
     * @brief Mount the file system
     * 
     * @return true If file system mounted.
     * @return false If the file system could not be mounted.
     */
    bool Mount();

    /**
     * @brief Mount the file system if necessary.
     * 
     * @return true If the file system is mounted (or was mounted successfully)
     * @return false If the file system could not be mounted.
     */
    bool MountIfNecessary();

public:
    /**
     *  @brief Name of the FreeRTOS component.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Name of the partition in the partition table.
     * 
     *  Note that this can be different from the pARTITION_LABEL.
     */
    static const char *PARTITION_PATH_NAME;

    /**
     * @brief Label for the Little file system.
     */
    static const char *PARTITION_LABEL;

    /**
     * @brief Public File System constructor.
     */
    FileSystem(IMessageDispatcher *);

    /**
     * @brief Destroy the File System object
     */
    ~FileSystem();

    /**
     *  @brief Perform any class level setup.
     */
    StatusCodes::StatusCodes Setup();
    
    /**
     * @brief Mount the file system.
     * 
     * @return true If the file system is mounted OK.
     * @return false If the file is not mounted.
     */
    bool IsMounted();

    /**
     * @brief Format the file system.
     */
    void Format(Message *);

    /**
     * @brief Read a file from the file system.
     */
    void ReadFile(Message *);

    /**
     * @brief Write a file to the file system.
     */
    void WriteFile(Message *);

    /**
     * @brief Delate a file from the file system.
     */
    void DeleteFile(Message *);

    /**
     * @brief Get a list of the files on the file system.
     */
    void ListFiles(Message *);
};

#endif /* _FILE_SYSTEM_HPP_ */