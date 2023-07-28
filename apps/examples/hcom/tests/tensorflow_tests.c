/****************************************************************************
 * \apps\examples\hcom\tests\bbreg_tests.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#if defined(CONFIG_TENSORFLOW_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

#include <dlfcn.h>

#include "../hcom_common.h"


/****************************************************************************
 * Name: tensorflow_tests_load_tensorflow_dll
 *
 * Description:
 *  Open the DLL containing Tensorflow and the test code.
 *
 * Input Parameters:
 *  name - Name of the DLL file to be opened
 *
 * Returned Value:
 *  Handle to the DLL if successful, NULL if the DLL cannot be opened.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void tensorflow_tests_load_tensorflow_dll(char *name)
{
    char path[128];

    syslog(2, "Opening %s DLL", name);
    snprintf(path, 128, "%s/%s", MONO_MEADOW_EXECUTABLE_PARTITION_NAME, name);
    void *handle = dlopen(path, RTLD_NOW);
    syslog(2, "Handle: %p\n", handle);

    return(handle);
}

/****************************************************************************
 * Name: tensorflow_tests_hello_world
 *
 * Description:
 *  Load the Tensorflow DLL and execute the Hello World test.
 *
 * Input Parameters:
 *  value - Developer value passed to CLI.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
void tensorflow_tests_hello_world(uint32_t value)
{
    void *handle = tensorflow_tests_load_tensorflow_dll("Tensorflow.so");
    
    if (handle > 0)
    {
        void (*testsetup)(void) = dlsym(handle, "tensorflow_hello_world_test_setup");
        syslog(2, "tensorflow_hello_world_test_setup: %p\n", testsetup);
        if (testsetup != 0)
        {
            testsetup();
            void (*testloop)(void) = dlsym(handle,"tensorflow_hello_world_test_loop");
            syslog(2, "tensorflow_hello_world_test_loop: %p\n", testloop); 
            if (testloop != 0)
            {
                testloop();
            } 
            else
            {
                syslog(2, "Cannot locate the tensorflow_hello_world_test_loop method.\n");
            }
        }
        else
        {
            syslog(2, "Cannot locate the tensorflow_hello_world_test_setup method.\n");
        }
    }

    dlclose(handle);
}

#endif // defined(CONFIG_TENSORFLOW_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)