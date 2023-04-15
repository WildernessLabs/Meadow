#!/bin/bash

set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

. $scriptdir/scripts/common_methods.sh
. $scriptdir/scripts/version_methods.sh

set_os_name
check_if_interactive

#
# Setup some of the variables used by this script.
#
VERBOSE=true
CLEAN=false
FULLCLEAN=false
COREDUMP=false
MENUCONFIG=false
HELP=false
DRYRUN=false
HEAPTRACING=false
IDF_FLAGS=""
IDF_DEFINES=""
DEBUG_SELECTIONS=""
FLASH=false

#
#   Work out which options are on the command line.
#
for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -s|--silent)
    VERBOSE=false
    ;;
    -b|--build)
    IDF_FLAGS+=" build"
    ;;
    -c|--clean)
    IDF_FLAGS+=" clean"
    ;;
    --fullclean)
    FULLCLEAN=true
    ;;
    --flash)
    FLASH=true
    ;;
    --coredump)
    IDF_DEFINES+=" -DCOREDUMP=1"
    ;;
    -th|--heaptracing)
    IDF_DEFINES+=" -DHEAPTRACING=1"
    ;;
    -tm|--tracemessages)
    IDF_DEFINES+=" -DTRACE_MESSAGES=1"
    ;;
    --MENUCONFIG)
    MENUCONFIG=true
    ;;
    --debug)
    DEBUG=true
    ;;
    --dryrun)
    DRYRUN=true
    ;;
    -f|--flash)
    # IDF_FLAGS+=" flash"
    FLASH=true
    ;;
    -m|--monitor)
    IDF_FLAGS+=" monitor"
    ;;
    -d=*|--debug=*)
    DEBUG_SELECTIONS="${i#*=}"
    ;;
    -h|--help)
    HELP=true
    ;;
    *)
    printf "Uknown command line option ($i)."
    printf "Run build.sh --help for information on valid options."
    exit 1
    ;;
esac
done

if $HELP; then
    echo "$0: Build the ESP code for Meadow"
    echo ""
    echo "Default action: Build the ESP application in release mode."
    echo ""
    echo "Options:"
    echo "-h | --help             Display this message"
    echo "-c | --clean            Perform a clean build"
    echo "-f | --flash            Flash the ESP32 with the bootloader, partition table and application code"
    echo "-m | --monitor          Connect to the serial port for application monitoring"
    echo "--fullclean             Perform a full clean followed by a build"
    echo "-v | --verbose          Display commands as they are executed (default)"
    echo "-s | --silent           Execute commands silently"
    echo "--coredump              Add core dump functionality to the application"
    echo "-th | --heaptracing     Enable heap tracing."
    echo "-tm | --tracemessages   Enable heap tracing."
    echo "--menuconfig            Enter the menu configuration system overwriting sdkconfig.defaults"
    echo "--dryrun                Perform a dry run (do not execute commands)"
    echo "-d=* | --debug=*        Turn on debug for selected system(s). Valid values (comma separated): WIFI, SPI, BT, SYSTEM."
    echo ""
    exit 0
fi

generate_build_info "ESP32"

#
#   Enter the configuration system for the application.  This takes care
#   of the fact that we have a default configuration system and makes sure
#   we edit the release configuration file.
#
if $MENUCONFIG; then
    SDKCONFIG="sdkconfig"
    DEFAULT_SDKCONFIG="sdkconfig.defaults"
    if test -f "$SDKCONFIG"; then
        run_command "rm $SDKCONFIG"
    fi
    run_command "mv $DEFAULT_SDKCONFIG $SDKCONFIG"
    run_command "idf.py menuconfig"
    run_command "mv $SDKCONFIG $DEFAULT_SDKCONFIG"
    exit 0
fi

#
#   Work out if we need to add any defintions to enable logging.
#
if [ ! -z "$DEBUG_SELECTIONS" ]; then
    subsystems=$(echo $DEBUG_SELECTIONS | tr "," "\n")
    for subsystem in $subsystems
    do
        case $subsystem in
            wifi)
            IDF_DEFINES+=" -DWIFI_DEBUG=1"
            ;;
            spi)
            IDF_DEFINES+=" -DSPI_DEBUG=1"
            ;;
            system)
            IDF_DEFINES+=" -DSYSTEM_DEBUG=1 -DFILE_SYSTEM_DEBUG=1"
            ;;
            messages)
            IDF_DEFINES+=" -DMESSAGES_DEBUG=1"
            ;;
            bt)
            IDF_DEFINES+=" -DBT_DEBUG=1"
            ;;
            thread)
            IDF_DEFINES+=" -DTHREAD_DEBUG=1"
            ;;
            *)
            printf "Uknown debug subsystem $subsystem."
            printf "Run build.sh --help for information on valid options."
            exit 1
            ;;
        esac
    done
fi

#
#   There are sometimes problems with old configuration in the build directory
#   so this removes the build directory before executing the full clean.
#
if $FULLCLEAN; then
    run_command "rm -R $scriptdir/esp32/build"
    run_command "idf.py fullclean"
fi

#
#   If we get here then we have stuff to do so fire up the build system.
#
#   First thing, delete the object file containing the version information so
#   we always get an updated version number and build time.
#
find . -name "AppMain.*.obj" -exec rm {} \;

if ! $NO_BUILD; then
    IDF_FLAGS="build $IDF_FLAGS"
fi
if [[ ! -z $IDF_FLAGS ]]; then
    idf.py -C $scriptdir/esp32 $IDF_DEFINES $IDF_FLAGS
fi

if [ $? -eq 0 ]; then
  if $FLASH; then
    $scriptdir/flash.sh
  fi
fi

restore_versioned_files

print_build_summary
