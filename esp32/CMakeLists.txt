# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#
#   Debugging macro to dump all of the variables.
#
macro(print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro()

#
#   Work out what we want the sdkconfig file to contain.
#
#   The build system allows the replacement of values using several
#   partial sdkconfig files.  This code works out which partial files
#   to include in this build.
#
file(REMOVE sdkconfig)
set(SDKCONFIG_FILES "sdkconfig.defaults")
if (COREDUMP EQUAL 1)
    message(STATUS "Enabling COREDUMP functionality.")
    list(APPEND SDKCONFIG_FILES "\;sdkconfig-coredump")
endif()
if (HEAPTRACING EQUAL 1)
    message(STATUS "Enabling heap tracing.")
    list(APPEND SDKCONFIG_FILES "\;sdkconfig-heaptracing")
endif()
set(SDKCONFIG_DEFAULTS ${SDKCONFIG_FILES})

#
#   Add extra components.
#
list(APPEND EXTRA_COMPONENT_DIRS components/esp_littlefs)
list(APPEND EXTRA_COMPONENT_DIRS components/F7Services)

#
#   Now work out which compiler defines to set to turn various
#   features on or off (e.g. WiFi logging etc.).
#
if (DEBUG EQUAL 1)
    add_compile_definitions(DEBUG)
endif()

if (TRACE_MESSAGES EQUAL 1)
    add_compile_definitions(TRACE_MESSAGES)
endif()

if (WIFI_DEBUG EQUAL 1)
    add_compile_definitions(WIFI_DEBUG)
endif()

if (SPI_DEBUG EQUAL 1)
    add_compile_definitions(SPI_DEBUG)
endif()

if (SYSTEM_DEBUG EQUAL 1)
    add_compile_definitions(SYSTEM_DEBUG)
endif()

if (MESSAGES_DEBUG EQUAL 1)
    add_compile_definitions(MESSAGES_DEBUG)
endif()

if (BT_DEBUG EQUAL 1)
    add_compile_definitions(BT_DEBUG)
endif()

if (THREAD_DEBUG EQUAL 1)
    add_compile_definitions(THREAD_DEBUG)
endif()

if (FILE_SYSTEM_DEBUG EQUAL 1)
    add_compile_definitions(FILE_SYSTEM_DEBUG)
endif()

if (PERFORMANCE_LOGGING EQUAL 1)
    add_compile_definitions(PERFORMANCE_LOGGING)
endif()

if (HEAP_SIZE GREATER 0)
    add_compile_definitions(HEAP_SIZE=${HEAP_SIZE})
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("MeadowComms")