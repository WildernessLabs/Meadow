/*
 *  FileSystem.cpp
 *
 *  Implementation of the methods to provide the file system functionality for the STM32.
 */

#include "sdkconfig.h"

#include <string>
#include <string.h>
#include <dirent.h>

#include "IMessageDispatcher.hpp"
#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "FileSystem.hpp"
#include "SharedEnums.hpp"
#include "Mapping.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the FreeRTOS component.
 */
const char *FileSystem::COMPONENT_NAME = "FileSystem";

/**
 *  @brief Name of the partition in the partitions.csv file.
 */
const char *FileSystem::PARTITION_PATH_NAME = "/littlefs";

/**
 *  @brief Label used for the partition when formatting.
 */
const char *FileSystem::PARTITION_LABEL = "littlefs";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new FileSystem object (Private)
 */
FileSystem::FileSystem()
{
    _messageDispatcher = nullptr;
    _config =
    {
        .base_path = PARTITION_PATH_NAME,
        .partition_label = PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
}

/**
 * @brief Construct a new File System object
 * 
 * @param messageDispatcher
 *      Message dispatcher object used to queue messages for the STM32.
 */
FileSystem::FileSystem(IMessageDispatcher *messageDispatcher) : FileSystem()
{
    _messageDispatcher = messageDispatcher;
}

/**
 * @brief Destroy the FileSystem object
 */
FileSystem::~FileSystem()
{
    esp_vfs_littlefs_unregister(_config.partition_label);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Getters and setters.
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Mount the file system.
 * 
 * @return true If the file system is mounted OK.
 * @return false If the file is not mounted.
 */
bool FileSystem::IsMounted()
{
    return(esp_littlefs_mounted(_config.partition_label));
}

/**
 * @brief Mount the file system
 * 
 * @return true If file system mounted.
 * @return false If the file system could not be mounted.
 */
bool FileSystem::Mount()
{
    bool result = true;

    esp_err_t ret = esp_vfs_littlefs_register(&_config);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            TRACE_MESSAGE("Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            TRACE_MESSAGE("Failed to find LittleFS partition");
        }
        else
        {
            TRACE_MESSAGE("Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
    }

    return(result);
}

/**
 * @brief Mount the file system if necessary.
 * 
 * @return true If the file system is mounted (or was mounted successfully)
 * @return false If the file system could not be mounted.
 */
bool FileSystem::MountIfNecessary()
{
    bool result = IsMounted();
    if (!result)
    {
        result = Mount();
    }

    return(result);
}

/**
 * @brief Setup the file system.
 * 
 * @return StatusCodes::CompletedOk on success, StatusCodes::Failure otherwise.
 */
StatusCodes::StatusCodes FileSystem::Setup()
{
    TRACE_MESSAGE("Setup: Enter");

    StatusCodes::StatusCodes result;
    if (Mount())
    {
        result = StatusCodes::CompletedOk;
    }
    else
    {
        result = StatusCodes::Failure;
    }

    TRACE_MESSAGE("Setup: Exit");

    return(result);
}

/**
 * @brief Format the file system.
 * 
 * @param message
 *      Pointer to the originating message object.
 */
void FileSystem::Format(Message *message)
{
    TRACE_MESSAGE("Format: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    message->DeletePayload();                                   // There should be no payload.

    esp_err_t ret = esp_littlefs_format(PARTITION_LABEL);
    if (ret == ESP_OK)
    {
        size_t total = 0, used = 0;
        ret = esp_littlefs_info(PARTITION_LABEL, &total, &used);
        if (ret == ESP_OK)
        {
            TRACE_MESSAGE("Partition size: total: %d, used: %d", total, used);
            result = StatusCodes::CompletedOk;
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Format: Exit");
}

/**
 * @brief Read a file from the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::ReadFile(Message *message)
{
    TRACE_MESSAGE("ReadFile: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::FileNameAndContents *fileData = Encoders::ExtractFileNameAndContents(message->Payload);
    message->DeletePayload();

    if (fileData != NULL)
    {
        if (fileData->Name != NULL)
        {
            std::string s = PARTITION_PATH_NAME;
            s += "/";
            s += fileData->Name;
            struct stat fileInfo;
            if (stat(s.c_str(), &fileInfo) == 0)
            {
                fileData->ContentsLength = 0;
                fileData->Contents = static_cast<uint8_t *>(pvPortMalloc(fileInfo.st_size));
                if (fileData->Contents != NULL)
                {
                    TRACE_MESSAGE("Reading %d bytes from file %s", (int) fileInfo.st_size, fileData->Name);
                    FILE *file = fopen(s.c_str(), "r");
                    if (file != NULL)
                    {
                        int amountRead = fread(static_cast<void *>(fileData->Contents), 1, fileInfo.st_size, file);
                        if (amountRead > 0)
                        {
                            fileData->ContentsLength = amountRead;
                            TRACE_MESSAGE("Read %d bytes", amountRead);
                            TRACE_HEX_BUFFER(fileData->Contents, amountRead);
                            fileData->ContentsLength = amountRead;
                        }
                        fclose(file);
                        message->PayloadLength = Encoders::EncodedFileNameAndContentsBufferSize(fileData);
                        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
                        if (message->Payload == NULL)
                        {
                            message->PayloadLength = 0;
                        }
                        else
                        {
                            Encoders::EncodeFileNameAndContents(fileData, message->Payload);
                            result = StatusCodes::CompletedOk;
                        }
                    }
                    if (result != StatusCodes::CompletedOk)
                    {
                        vPortFree(fileData->Contents);
                        fileData->Contents = NULL;
                        fileData->ContentsLength = 0;
                    }
                }
                else
                {
                    fileData->ContentsLength = 0;
                }
            }
            else
            {
                result = StatusCodes::FileNotFound;
            }
        }
        vPortFree(fileData->Name);
        vPortFree(fileData);
    }
    
    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("ReadFile: Exit");
}

/**
 * @brief Write a file to the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::WriteFile(Message *message)
{
    TRACE_MESSAGE("WriteFile: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::FileNameAndContents *fileData = Encoders::ExtractFileNameAndContents(message->Payload);
    message->DeletePayload();

    if (fileData != NULL)
    {
        if (fileData->Contents != NULL)
        {
            std::string s = PARTITION_PATH_NAME;
            s += "/";
            s += fileData->Name;
            TRACE_MESSAGE("Writing %d bytes into file %s", fileData->ContentsLength, s.c_str());
            TRACE_HEX_BUFFER(fileData->Contents, fileData->ContentsLength);
            FILE *file = fopen(s.c_str(), "w");
            if (file != NULL)
            {
                int amount = fwrite((void *) fileData->Contents, 1, fileData->ContentsLength, file);
                TRACE_MESSAGE("Written %d bytes", amount);
                if (amount == fileData->ContentsLength)
                {
                    result = StatusCodes::CompletedOk;
                }
                fclose(file);
            }

            vPortFree(fileData->Contents);
        }
        vPortFree(fileData);
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("WriteFile: Exit");
}

/**
 * @brief Delete a file on the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::DeleteFile(Message *message)
{
    TRACE_MESSAGE("DeleteFile: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    Esp32Messaging::FileDetails *details = Encoders::ExtractFileDetails(message->Payload);
    message->DeletePayload();

    if (details != NULL)
    {
        if (details->Name != NULL)
        {
            std::string s = PARTITION_PATH_NAME;
            s += "/";
            s += details->Name;

            if (remove(s.c_str()) == 0)
            {
                result = StatusCodes::CompletedOk;
            }
            else
            {
                if (errno == ENOENT)
                {
                    result = StatusCodes::FileNotFound;
                }
            }
        }
        vPortFree(details->Name);
        vPortFree(details);
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("DeleteFile: Exit");
}

/**
 * @brief List the files on the file system.
 * 
 * @param request
 *      Pointer to the originating message object.
 */
void FileSystem::ListFiles(Message *message)
{
    TRACE_MESSAGE("ListFiles: Enter");

    StatusCodes::StatusCodes result = StatusCodes::Failure;
    message->DeletePayload();

    int count = 0;
    DIR *dp = opendir(FileSystem::PARTITION_PATH_NAME);
    if (dp != NULL)
    {
        TRACE_MESSAGE("Directory %s opened.", FileSystem::PARTITION_PATH_NAME);
        struct dirent *ep;
        while ((ep = readdir(dp)) != NULL)
        {
            count++;
        }
        rewinddir(dp);
        Esp32Messaging::FileNameList fileNameList;
        bzero(&fileNameList, sizeof(fileNameList));
        uint16_t bufferLength = 0;
        TRACE_MESSAGE("Found %d files.", count);
        if (count > 0)
        {
            while ((ep = readdir(dp)) != NULL)
            {
                Esp32Messaging::FileDetails details;
                bzero(&details, sizeof(details));
                details.Name = ep->d_name;
                std::string s = PARTITION_PATH_NAME;
                s += "/";
                s += ep->d_name;
                struct stat fileInfo;
                if (stat(s.c_str(), &fileInfo) == 0)
                {
                    details.Length = fileInfo.st_size;
                }
                else
                {
                    details.Length = 0;
                }
                TRACE_MESSAGE("    File: %s, length: %d", details.Name, details.Length);
                bufferLength += Encoders::EncodedFileDetailsBufferSize(&details);
                fileNameList.FileDetails = static_cast<uint8_t *>(realloc(fileNameList.FileDetails, bufferLength));
                if (fileNameList.FileDetails != NULL)
                {
                    Encoders::EncodeFileDetails(&details, fileNameList.FileDetails + fileNameList.FileDetailsLength);
                    fileNameList.FileDetailsLength = bufferLength;
                }
            }
        }
        closedir(dp);
        fileNameList.NumberOfFiles = count;
        message->PayloadLength = Encoders::EncodedFileNameListBufferSize(&fileNameList);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == NULL)
        {
            message->PayloadLength = 0;
        }
        else
        {
            Encoders::EncodeFileNameList(&fileNameList, message->Payload);
            result = StatusCodes::CompletedOk;
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("ListFiles: Exit");
}