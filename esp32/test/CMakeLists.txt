# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.5)

# Include the components directory of the main application:
#
set(EXTRA_COMPONENT_DIRS "../components")

set(SDKCONFIG_DEFAULTS "sdkconfig.defaults\;sdkconfig.tests")

file(REMOVE sdkconfig)
set(SDKCONFIG_FILES "sdkconfig.defaults")
list(APPEND SDKCONFIG_FILES "\;sdkconfig.tests")
set(SDKCONFIG_DEFAULTS ${SDKCONFIG_FILES})

# Set the components to include the tests for.
# This can be overriden from CMake cache:
# - when invoking CMake directly: cmake -D TEST_COMPONENTS="xxxxx" ..
# - when using idf.py: idf.py -T xxxxx build
#
set(TEST_COMPONENTS "F7Services" CACHE STRING "List of components to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(unit_test_test)
