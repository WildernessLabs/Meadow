/****************************************************************************
 * \apps\examples\hcom\tests\tensorflow_tests.c
 * 
 *   Copyright (C) 2023 - 2020 Wilderness Labs. All rights reserved.
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

#include <math.h>

#if defined(CONFIG_TENSORFLOW_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

#include <dlfcn.h>

#include "../hcom_common.h"

/****************************************************************************
 * Private types
 ****************************************************************************/

/**
 * @brief Structure to hold the results of a Hello World test iteration.
 */
typedef struct hello_world_results_s
{
    float x;
    float y;
} hello_world_results_t;

/**
 * @brief Array of expected results for the Hello World test.
 *
 * Note about these numbers.  These were obtained by running the Hello World test
 * on a Sony Spresense board.  This generated the following output from the application:
 * 
 * x_value: 1.0*2^-127, y_value: 1.0*2^-127
 * x: 0.000000, y: 0.000000
 * x_value: 1.2566366*2^-2, y_value: 1.4910722*2^-2
 * x: 0.314159, y: 0.372768
 * x_value: 1.2566366*2^-1, y_value: 1.1183041*2^-1
 * x: 0.628318, y: 0.559152
 * x_value: 1.8849551*2^-1, y_value: 1.6774564*2^-1
 * x: 0.942477, y: 0.838728
 * x_value: 1.2566366*2^0, y_value: 1.9316164*2^-1
 * x: 1.256637, y: 0.965808
 * x_value: 1.5707957*2^0, y_value: 1.0420563*2^0
 * x: 1.570796, y: 1.042057
 * x_value: 1.8849551*2^0, y_value: 1.9146728*2^-1
 * x: 1.884956, y: 0.957336
 * x_value: 1.0995567*2^1, y_value: 1.6435688*2^-1
 * x: 2.199115, y: 0.821784
 * x_value: 1.2566366*2^1, y_value: 1.0674724*2^-1
 * x: 2.513274, y: 0.533736
 * x_value: 1.4137159*2^1, y_value: 1.8977287*2^-3
 * x: 2.827433, y: 0.237216
 * x_value: 1.5707957*2^1, y_value: 1.0844163*2^-7
 * x: 3.141593, y: 0.008472
 * x_value: 1.7278753*2^1, y_value: -1.2199684*2^-2
 * x: 3.455752, y: -0.304992
 * x_value: 1.8849551*2^1, y_value: -1.0674724*2^-1
 * x: 3.769912, y: -0.533736
 * x_value: 1.0210171*2^2, y_value: -1.5588485*2^-1
 * x: 4.084070, y: -0.779424
 * x_value: 1.0995567*2^2, y_value: -1.9316164*2^-1
 * x: 4.398230, y: -0.965808
 * x_value: 1.1780966*2^2, y_value: -1.1098324*2^0
 * x: 4.712389, y: -1.109833
 * x_value: 1.2566366*2^2, y_value: -1.9655047*2^-1
 * x: 5.026548, y: -0.982752
 * x_value: 1.3351763*2^2, y_value: -1.4910722*2^-1
 * x: 5.340708, y: -0.745536
 * x_value: 1.4137159*2^2, y_value: -1.0674724*2^-1
 * x: 5.654867, y: -0.533736
 * x_value: 1.4922558*2^2, y_value: -1.4232964*2^-2
 * x: 5.969026, y: -0.355824
 */
hello_world_results_t hello_world_results[] = 
{
    { 0.000000, 0.000000 },
    { 0.314159, 0.372768 },
    { 0.628318, 0.559152 },
    { 0.942477, 0.838728 },
    { 1.256637, 0.965808 },
    { 1.570796, 1.042057 },
    { 1.884956, 0.957336 },
    { 2.199115, 0.821784 },
    { 2.513274, 0.533736 },
    { 2.827433, 0.237216 },
    { 3.141593, 0.008472 },
    { 3.455752, -0.304992 },
    { 3.769912, -0.533736 },
    { 4.084070, -0.779424 },
    { 4.398230, -0.965808 },
    { 4.712389, -1.109833 },
    { 5.026548, -0.982752 },
    { 5.340708, -0.745536 },
    { 5.654867, -0.533736 },
    { 5.969026, -0.355824 }
};

/****************************************************************************
 * Name: floats_not_equal
 *
 * Description:
 *  Compare two floating point numbers for equality.
 *
 * Input Parameters:
 *  x - First number
 *  y - Second number
 *
 * Returned Value:
 *  True if the numbers are within 1e-6 of each other, false otherwise.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static bool floats_not_equal(float x, float y)
{
    return(fabs(x - y) > 1e-6);
}

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
static void *tensorflow_tests_load_tensorflow_dll(char *name)
{
    char path[128];

    syslog(2, "    Opening %s DLL\n", name);
    snprintf(path, 128, "%s/%s", MONO_MEADOW_EXECUTABLE_PARTITION_NAME, name);
    void *handle = dlopen(path, RTLD_NOW);
    syslog(2, "    Handle: %p\n", handle);

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
    bool pass = true;

    syslog(2, "Executing Tensorflow Hello World test\n");

    void *handle = tensorflow_tests_load_tensorflow_dll("Tensorflow.so");
    if (handle > 0)
    {
        void (*testsetup)(void) = dlsym(handle, "tensorflow_hello_world_test_setup");
        syslog(2, "    tensorflow_hello_world_test_setup: %p\n", testsetup);
        if (testsetup != 0)
        {
            testsetup();
            void (*testloop)(uint32_t, uint32_t) = dlsym(handle,"tensorflow_hello_world_test_loop");
            syslog(2, "    tensorflow_hello_world_test_loop: %p\n", testloop); 
            if (testloop != 0)
            {
                for (int pass = 0; pass < 2; pass++)
                {
                    for (int index = 0; index < 20; index++)
                    {
                        float x, y;
                        testloop((uint32_t) &x, (uint32_t) &y);
                        if (floats_not_equal(x, hello_world_results[index].x) || floats_not_equal(y, hello_world_results[index].y))
                        {
                            syslog(2, "    Test %d failed\n", index);
                            syslog(2, "    Expected: %f, %f\n", hello_world_results[index].x, hello_world_results[index].y);
                            syslog(2, "    Actual: %f, %f\n", x, y);
                            pass = false;
                            break;
                        }
                    }
                }
            } 
            else
            {
                syslog(2, "    Cannot locate the tensorflow_hello_world_test_loop method.\n");
                pass = false;
            }
        }
        else
        {
            syslog(2, "    Cannot locate the tensorflow_hello_world_test_setup method.\n");
            pass = false;
        }
        dlclose(handle);
    }
    else
    {
        syslog(2, "    Cannot open Tensorflow.so\n");
        pass = false;
    }

    syslog(2, "    Tensorflow Hello World test - %s.\n", pass ? "PASS" : "FAIL");
}

#endif // defined(CONFIG_TENSORFLOW_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)