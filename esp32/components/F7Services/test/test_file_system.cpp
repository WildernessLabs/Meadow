/*
 *  test_file_system.cpp
 *
 *  Implementation of the test methods for the file system.
 */

#include "sdkconfig.h"

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <string.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "FileSystem.hpp"

//
//  Name of the files to be used for testing.
//
#define TEST_FILE_NAME          "hello.txt"
#define TEST_FILE_NAME2         "test.txt"

//
//  Text to be written into the text files.
//
#define TEST_FILE_CONTENTS      "Hello from ESP32 unit tests\n"
#define TEST_FILE_CONTENTS2     "More text\n"

//
//  Message ID to be used for testing.
//
#define TEST_MESSAGE_ID         25

/**
 * @brief Count the number of files in the root directory of the file system.
 * 
 * @return int
 *      Number of files present or -1 if there is a problem.
 */
int CountFiles()
{
    DIR *dp = opendir(FileSystem::PARTITION_PATH_NAME);
    int result = 0;
    if (dp == NULL)
    {
        result = -1;
    }
    else
    {
        struct dirent *ep;
        while ((ep = readdir(dp)) != NULL)
        {
            result++;
        }            
        (void) closedir(dp);
    }
    return(result);
}

/**
 * @brief Write a test file to the file system.
 * 
 * @param filename Name of the file to write.
 * @param buffer Pointer to a block of memory holding the data to write.
 * @param length Number of bytes to write to the file.
 *
 * @return Number of bytes written if successful, -1 otherwise.
 */
int WriteTestFile(char *filename, uint8_t *buffer, int length)
{
    int result = 0;

    std::string s = FileSystem::PARTITION_PATH_NAME;
    s += "/";
    s += filename;
    FILE *file = fopen(s.c_str(), "w");
    if (file != NULL)
    {
        result = fwrite((void *) buffer, 1, length, file);
        fclose(file);
    }
    else
    {
        result = -1;
    }
    return(result);
}

/**
 * @brief Read a file from the file system.
 * 
 * @param filename Name of the file to read.
 * @param buffer Pointer to a block of memory to hold the contents of the file.
 * @param maxLength Maximum number of bytes that the buffer can hold.
 *
 * @return int Number of bytes read of -1 if there is a problem.
 */
int ReadTestFile(char *filename, uint8_t *buffer, int maxLength)
{
    int result = 0;

    std::string s = FileSystem::PARTITION_PATH_NAME;
    s += "/";
    s += filename;
    FILE *file = fopen(s.c_str(), "r");
    if (file != NULL)
    {
        result = fread((void *) buffer, 1, maxLength, file);
        fclose(file);
    }
    else
    {
        result = -1;
    }
    return(result);
}

/**
 * @brief Check that we can mount the file system.
 */
TEST_CASE("File System set up", "[FileSystem]")
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    TEST_ASSERT_TRUE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));
    delete fs;
    TEST_ASSERT_FALSE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));
}

/**
 * @brief Test formatting the LittleFS file system.
 * 
 *  This test will test formatting the LittleFS file system as follows:
 *      - Format the file system and verify that a single success response is received from
 *        the FileSystem class.
 *      - Write a single file to the newly formatted file system using c stdio methods and 
 *        verify that there is exactly one file on the file system.
 *      - Reformat the file system and verify that no files can be found.
 */
TEST_CASE("Formatting file system", "[FileSystem]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    //
    //  Create a message to format the file system and call the format method.
    //
    Message *formatMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemFormat, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(formatMessage);

    fs->Format(formatMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemFormat, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());
    //
    //  Write a single file to the file system.
    //
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));
    //
    //  Check that we have one file on the file system.
    //
    TEST_ASSERT_EQUAL(1, CountFiles());
    //
    //  Reformat the file system (reuse the message from above).
    //
    fs->Format(formatMessage);
    //
    //  Verify that the file has now gone.
    //
    TEST_ASSERT_EQUAL(0, CountFiles());

    delete fs;
}

/**
 * @brief Test case for writing a file to the file system.
 * 
 *  Writing a file will be tested by using the messaging system to pass the file
 *  details and contents and then using C stdio to read the file and verify that
 *  the correct contents have been written to the file system.
 * 
 *  The file system will then be unmounted and remounted and the files contents
 *  checked once more using C stdio.
 */
TEST_CASE("Write file to file system", "[FileSystem]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    //
    //  Start with an empty file system.
    //
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));
    //
    //  Create a message to write a file to the file system.
    //
    Esp32Messaging::FileNameAndContents fileData;
    bzero(&fileData, sizeof(fileData));
    fileData.Name = (char *) TEST_FILE_NAME;
    fileData.Length = strlen(TEST_FILE_CONTENTS);
    fileData.Contents = (uint8_t *) TEST_FILE_CONTENTS;
    fileData.ContentsLength = fileData.Length;
    uint32_t payloadLength = Encoders::EncodedFileNameAndContentsBufferSize(&fileData);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileNameAndContents(&fileData, encodedPayload);
    Message *writeMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile,
                                                  StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(writeMessage);

    fs->WriteFile(writeMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemWriteFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    int length = 200;
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(length));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), ReadTestFile((char *) TEST_FILE_NAME, buffer, length));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(TEST_FILE_CONTENTS, buffer, strlen(TEST_FILE_CONTENTS), 1);

    vPortFree(buffer);
    Message::DeleteMessage(writeMessage);

    delete fs;
}

TEST_CASE("Reading a file", "[FileSystem]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    //
    //  Start with an empty file system.
    //
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));

    Esp32Messaging::FileNameAndContents fileData;
    bzero(&fileData, sizeof(fileData));
    fileData.Name = (char *) TEST_FILE_NAME;
    uint32_t payloadLength = Encoders::EncodedFileNameAndContentsBufferSize(&fileData);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileNameAndContents(&fileData, encodedPayload);
    Message *readMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemReadFile,
                                                  StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(readMessage);

    fs->ReadFile(readMessage);
    
    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemReadFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    TEST_ASSERT_NOT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    Esp32Messaging::FileNameAndContents *fileContents = Encoders::ExtractFileNameAndContents(response->Payload);
    TEST_ASSERT_NOT_NULL(fileContents);
    TEST_ASSERT_GREATER_THAN(0, fileContents->ContentsLength);
    TEST_ASSERT_NOT_NULL(fileContents->Contents);
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(TEST_FILE_CONTENTS, fileContents->Contents, strlen(TEST_FILE_CONTENTS), 1);

    vPortFree(fileContents->Contents);
    vPortFree(fileContents);
    Message::DeleteMessage(readMessage);

    delete fs;
}

/**
 * @brief Test getting a list of files on the file system.
 */
TEST_CASE("Get list of files", "[FileSystem]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    //
    //  Start with an empty file system.
    //
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));
    Message *listMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemListFiles, StatusCodes::Failure);
    listMessage->MessageID = TEST_MESSAGE_ID;

    fs->ListFiles(listMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemListFiles, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    TEST_ASSERT_NOT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    //
    //  We should have no files on the file system.
    //
    Esp32Messaging::FileNameList *files = Encoders::ExtractFileNameList(response->Payload);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL(0, files->NumberOfFiles);
    TEST_ASSERT_EQUAL(0, files->FileDetailsLength);
    TEST_ASSERT_NULL(files->FileDetails);
    vPortFree(files);
    response->DeletePayload();

    //
    //  Now write two files to the file system and repeat the request reusing the listMessage from above.
    //
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS2), WriteTestFile((char *) TEST_FILE_NAME2, (uint8_t *) TEST_FILE_CONTENTS2, strlen(TEST_FILE_CONTENTS2)));
    listMessage->StatusCode = StatusCodes::Failure;

    fs->ListFiles(listMessage);

    //
    //  We have already tested the message queuing above so we will not repeat those checks, we will
    //  just check that we can retrieve the file information.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    files = Encoders::ExtractFileNameList(response->Payload);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL(2, files->NumberOfFiles);
    TEST_ASSERT_GREATER_THAN(0, files->FileDetailsLength);
    TEST_ASSERT_NOT_NULL(files->FileDetails);
    uint8_t *buffer = files->FileDetails;
    for (int index = 0; index < files->NumberOfFiles; index++)
    {
        Esp32Messaging::FileDetails *file = Encoders::ExtractFileDetails(buffer);
        TEST_ASSERT_NOT_NULL(file);
        buffer += strlen(file->Name) + 1 + sizeof(uint16_t);
        if (strcmp(file->Name, TEST_FILE_NAME) == 0)
        {
            TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), file->Length);
        }
        if (strcmp(file->Name, TEST_FILE_NAME2) == 0)
        {
            TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS2), file->Length);
        }
    }

    Message::DeleteMessage(listMessage);

    delete fs;
}

/**
 * @brief Check that we can delete a file from the file system.
 */
TEST_CASE("Delete file", "[FileSystem]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    //
    //  Start with an empty file system.
    //
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));

    //
    //  Create a message to delete a file from the file system.
    //
    Esp32Messaging::FileDetails details;
    bzero(&details, sizeof(details));
    details.Name = (char *) TEST_FILE_NAME;
    uint32_t payloadLength = Encoders::EncodedFileDetailsBufferSize(&details);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileDetails(&details, encodedPayload);
    Message *deleteMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemDeleteFile,
                                                  StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(deleteMessage);

    fs->DeleteFile(deleteMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemDeleteFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::FileNotFound, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    //
    //  Now create a file and try to delete it.
    //
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));
    TEST_ASSERT_EQUAL(1, CountFiles());

    deleteMessage->PayloadLength = payloadLength;
    encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));  // Re-encode as the above call will have freed the memory.
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileDetails(&details, encodedPayload);
    deleteMessage->Payload = encodedPayload;

    fs->DeleteFile(deleteMessage);

    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);

    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(deleteMessage);

    delete fs;
}