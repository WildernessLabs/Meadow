#!/bin/bash

#set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

#
#   Work out the OS so that we can change actions per OS where necessary.
#
shopt -s nocasematch
case "$(uname -a)" in
  *darwin*)
    OS="mac"
    ;;
  *linux*)
    OS="linux"
    ;;
  cygwin*|mingw32*|msys*|mingw*)
    OS="windows"
    ;;
  *)
    OS="unknown"
    ;;
esac


# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=true
FORCE=false
CLEAN=false
MONO=false
CONFIGURE_ONLY=false
CONFIG=mono
NETCORE=false
WLCLEAN=false
DEBUG=false
HELP=false
UNITTEST=false

for i in "$@"
do
case $i in
    -h|--help)
    HELP=true
    ;;
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    -c|--clean)
    CLEAN=true
    ;;
    --wlclean)
    WLCLEAN=true;;
    -m|--mono)
    MONO=true
    ;;
    --netcore)
    NETCORE=true
    ;;
    --configure)
    CONFIGURE_ONLY=true
    ;;
    --debug)
    DEBUG=true
    ;;
    --config=*)
    CONFIG=$(echo $i | cut -f2 -d=)
    ;;
    --u|--unit-test)
    UNITTEST=true
    ;;
    *)
    echo "Unknown option $i"
    exit 1
    ;;
esac
done

if $UNITTEST && ( $CLEAN || $FORCE ); then
  echo "--unit-test is incompatible with --clean and --force."
  exit 1
fi

if [ "$HELP" = true ]; then
  echo "Usage: build.sh [options]"
  echo " "
  echo "Options:"
  echo "  -h|--help                    Show this help message"
  echo "  -v|--verbose                 Show verbose output"
  echo "  -f|--force                   Force build"
  echo "  -c|--clean                   Clean build"
  echo "  --wlclean                    Clean the Wilderness Labs object files"
  echo "  -m|--mono                    Build with Mono"
  echo "  --netcore                    Build with .NET Core"
  echo "  --configure                  Configure the build"
  echo "  --debug                      Build with debug symbols"
  echo "  -u|--unit-test               Configure for unit test output to /dev/console"
  echo "  --config=mono|netcore        Select Mono or .NET Core builds (default Mono)"
  exit 0
fi

run_command() {
  if $VERBOSE; then
    echo
    $1
  else
    $1 &>/dev/null
  fi
}

check_command_status() {
  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    printf " ${red}error${reset}\n"
    if ! $VERBOSE; then
        printf "Re-run the script with --verbose flag to see the output.\n"
    fi
    exit 1
  else
    printf " ${green}success${reset}\n"
  fi
}

get_git_commit_hash() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH rev-parse HEAD`
}

get_git_branch_or_tag() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH describe --tags --exact-match 2> /dev/null || git -C $REPO_PATH symbolic-ref -q --short HEAD`
}

generate_build_info() {
  printf "Generating build info..."

  MEADOW_GIT_HASH=$(get_git_commit_hash $scriptdir)
  MEADOW_GIT_REF=$(get_git_branch_or_tag $scriptdir)

  NUTTX_GIT_HASH=$(get_git_commit_hash $scriptdir/nuttx)
  NUTTX_GIT_REF=$(get_git_branch_or_tag $scriptdir/nuttx)

  NUTTX_APPS_GIT_HASH=$(get_git_commit_hash $scriptdir/apps)
  NUTTX_APPS_GIT_REF=$(get_git_branch_or_tag $scriptdir/apps)

  MONO_GIT_HASH=$(get_git_commit_hash $scriptdir/mono)
  MONO_GIT_REF=$(get_git_branch_or_tag $scriptdir/mono)

# Generate build-info.json file
BUILD_DATE="`date +"%F %T"`"
BUILD_HASH="`echo "$BUILD_DATE" | md5sum | awk '{print $1}'`"

JSON=$(cat <<-END
{
  "git": {
    "meadow": [ "$MEADOW_GIT_HASH", "$MEADOW_GIT_REF" ],
    "nuttx": [ "$NUTTX_GIT_HASH", "$NUTTX_GIT_REF" ],
    "nuttx-apps": [ "$NUTTX_APPS_GIT_HASH", "$NUTTX_APPS_GIT_REF" ],
    "mono": [ "$MONO_GIT_HASH", "$MONO_GIT_REF" ]
  },
  "build-date": "$BUILD_DATE",
  "build-hash": "$BUILD_HASH"
}
END
)
  echo "$JSON" > $scriptdir/nuttx/build-info.json

  printf " ${green}success${reset}\n"
}

#
#   Generate build info
#

generate_build_info

#
# Setup toolchain
#

# Make sure the toolchain submodule exists
if [ ! -d "$scriptdir/toolchain" ]; then
    printf "Toolchain was not found, make sure the submodule has been checked out.\n"
    exit 0
fi

case "$(uname -s)" in
    Darwin)
      export PATH=$scriptdir/toolchain/macos:$PATH
      ;;
    Linux)
      export PATH=$scriptdir/toolchain/linux:$PATH
      ;;
    CYGWIN*|MINGW32*|MSYS*|MINGW*)
      export PATH=$scriptdir/toolchain/windows:$PATH
      ;;
    *)
      printf "Toolchain setup not implemented yet for this OS.\n"
      exit 0
      ;;
esac

#
#   Build NuttX OS base code
#

#
#   First step, change the defconfig file to either debug or optimised configuration.
#
DEFCONFIG_FILE=$scriptdir/nuttx/configs/stm32f777zit6-meadow/mono/defconfig
if $DEBUG; then
  if [[ "$OS" == "mac" ]]; then
    sed -i '' 's/CONFIG_DEBUG_FULLOPT\=y/CONFIG_DEBUG_FULLOPT\=n/'  $DEFCONFIG_FILE
    sed -i '' 's/CONFIG_DEBUG_ASSERTIONS\=n/CONFIG_DEBUG_ASSERTIONS\=y/'  $DEFCONFIG_FILE
  else
    sed -i 's/CONFIG_DEBUG_FULLOPT\=y/CONFIG_DEBUG_FULLOPT\=n/'  $DEFCONFIG_FILE
    sed -i 's/CONFIG_DEBUG_ASSERTIONS\=n/CONFIG_DEBUG_ASSERTIONS\=y/'  $DEFCONFIG_FILE
  fi
else
  if [[ "$OS" == "mac" ]]; then
    sed -i '' 's/CONFIG_DEBUG_FULLOPT\=n/CONFIG_DEBUG_FULLOPT\=y/'  $DEFCONFIG_FILE
    sed -i '' 's/CONFIG_DEBUG_ASSERTIONS\=y/CONFIG_DEBUG_ASSERTIONS\=n/'  $DEFCONFIG_FILE
  else
    sed -i 's/CONFIG_DEBUG_FULLOPT\=n/CONFIG_DEBUG_FULLOPT\=y/'  $DEFCONFIG_FILE
    sed -i 's/CONFIG_DEBUG_ASSERTIONS\=y/CONFIG_DEBUG_ASSERTIONS\=n/'  $DEFCONFIG_FILE
  fi
fi

NUTTX_CONFIG="stm32f777zit6-meadow/$CONFIG"

#
#   Edit the .config and hcom_shared_common.h files to turn on unit tests
#   and direct their output to /dev/console.
#
if $UNITTEST; then
  echo "********** Configuring to run unit tests, to turn unit tests off:"
  echo "             * Edit hcom_sharded_common.h to turn off any tests that have been enabled"
  echo "             * Run build.sh --clean or build.sh --force to change the config file"
  CONFIG_FILE=$scriptdir/nuttx/.config
  SHARED_INCLUDE_FILE=$scriptdir/nuttx/include/meadow/hcom_shared_common.h
  if [[ "$OS" == "mac" ]]; then
    sed -i '' 's/# CONFIG_DEV_CONSOLE is not set/CONFIG_DEV_CONSOLE\=y/' $CONFIG_FILE
    sed -i '' 's/# CONFIG_SERIAL_CONSOLE is not set/CONFIG_SERIAL_CONSOLE\=y/' $CONFIG_FILE
    sed -i '' 's/# CONFIG_USART1_SERIAL_CONSOLE is not set/CONFIG_USART1_SERIAL_CONSOLE\=y/' $CONFIG_FILE
    sed -i '' 's/CONFIG_NO_SERIAL_CONSOLE\=y/# CONFIG_NO_SERIAL_CONSOLE is not set/' $CONFIG_FILE
    sed -i '' 's/# CONFIG_SYSLOG_WRITE is not set/CONFIG_SYSLOG_WRITE\=y/' $CONFIG_FILE
    sed -i '' 's/CONFIG_RAMLOG=y//' $CONFIG_FILE
    sed -i '' 's/CONFIG_RAMLOG_BUFSIZE\=32768//' $CONFIG_FILE
    sed -i '' 's/CONFIG_RAMLOG_NPOLLWAITERS\=4//' $CONFIG_FILE
    sed -i '' 's/# CONFIG_SYSLOG_SERIAL_CONSOLE is not set/CONFIG_SYSLOG_SERIAL_CONSOLE\=y/' $CONFIG_FILE
    sed -i '' 's/CONFIG_RAMLOG_SYSLOG\=y/CONFIG_SYSLOG_CONSOLE\=y/' $CONFIG_FILE
    sed -i '' 's/#define HCOM_INCLUDE_ESPCP_TESTS                      0/#define HCOM_INCLUDE_ESPCP_TESTS                      1/' $SHARED_INCLUDE_FILE
  else
    sed -i 's/# CONFIG_DEV_CONSOLE is not set/CONFIG_DEV_CONSOLE\=y/' $CONFIG_FILE
    sed -i 's/# CONFIG_SERIAL_CONSOLE is not set/CONFIG_SERIAL_CONSOLE\=y/' $CONFIG_FILE
    sed -i 's/# CONFIG_USART1_SERIAL_CONSOLE is not set/CONFIG_USART1_SERIAL_CONSOLE\=y/' $CONFIG_FILE
    sed -i 's/CONFIG_NO_SERIAL_CONSOLE\=y/# CONFIG_NO_SERIAL_CONSOLE is not set/' $CONFIG_FILE
    sed -i 's/# CONFIG_SYSLOG_WRITE is not set/CONFIG_SYSLOG_WRITE\=y/' $CONFIG_FILE
    sed -i 's/CONFIG_RAMLOG=y//' $CONFIG_FILE
    sed -i 's/CONFIG_RAMLOG_BUFSIZE\=32768//' $CONFIG_FILE
    sed -i 's/CONFIG_RAMLOG_NPOLLWAITERS\=4//' $CONFIG_FILE
    sed -i 's/# CONFIG_SYSLOG_SERIAL_CONSOLE is not set/CONFIG_SYSLOG_SERIAL_CONSOLE\=y/' $CONFIG_FILE
    sed -i 's/CONFIG_RAMLOG_SYSLOG\=y/CONFIG_SYSLOG_CONSOLE\=y/' $CONFIG_FILE
    sed -i 's/#define HCOM_INCLUDE_ESPCP_TESTS                      0/#define HCOM_INCLUDE_ESPCP_TESTS                      1/' $SHARED_INCLUDE_FILE
  fi
fi

#
# Added the ability to clean only the code acced by Wilderness Labs
#
# This option allows for a clean of the frequently edit files which
# reduces the compilation time.
#
if $WLCLEAN || $CLEAN || $FORCE; then
    find $scriptdir/apps/examples -name "*.o" -type f -exec rm {} \;
    find $scriptdir/nuttx/configs/stm32f777zit6-meadow -name "*.o" -type f -exec rm {} \;
fi

if [ -r "$scriptdir/nuttx/.config" ] && ($FORCE || $CLEAN); then
    printf "Cleaning NuttX (already configured)..."
    run_command "make -C $scriptdir/nuttx distclean -j8"
    run_command "rm -f $scriptdir/nuttx/Meadow.OS.bin"
    check_command_status
fi

if [ ! -r "$scriptdir/nuttx/.config" ] || $FORCE; then
    printf "Configuring NuttX...\n"
    run_command "$scriptdir/nuttx/tools/configure.sh $NUTTX_CONFIG"
    run_command "make -C $scriptdir/nuttx context"
    check_command_status
else
    printf "NuttX already configured (use --force to override)\n"
fi

if $CONFIGURE_ONLY; then
  exit 0
fi

printf "Building NuttX (kernel pass)...\n"
# Build mksyscall first due to issues with concurrency and makefile dependencies
run_command "make -C $scriptdir/nuttx/tools -f Makefile.host mksyscall"
run_command "make -C $scriptdir/nuttx -j8 pass2"
check_command_status

#
#   Build Mono
#

$scriptdir/build-mono.sh "$@"
if [ $? -ne 0 ]; then
    exit 1
fi

#
#   Build mbedTLS
#
$scriptdir/build-mbedtls.sh "$@"
if [ $? -ne 0 ]; then
    exit 1
fi

run_command "make -C $scriptdir/nuttx -j8 pass1deps"
check_command_status

if ! grep -q "CONFIG_BUILD_FLAT=y" $scriptdir/nuttx/.config; then
  printf "Building NuttX (user pass)..."
  if $NETCORE; then
    export ENABLE_NETCORE=1
  fi
  run_command "make -C $scriptdir/nuttx -j8 pass1"
  check_command_status
fi

#
#   Package Meadow.OS
#

if ! grep -q "CONFIG_BUILD_FLAT=y" $scriptdir/nuttx/.config; then
  MEADOW_OS_BIN=$scriptdir/nuttx/Meadow.OS.bin
  dd if=/dev/zero bs=1024 count=2048 of=${MEADOW_OS_BIN} 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx.bin bs=1024 of=${MEADOW_OS_BIN} conv=notrunc 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx_user.bin bs=512 skip=1 seek=1 count=2047 of=${MEADOW_OS_BIN} conv=notrunc 2> /dev/null

  MEADOW_OS_RUNTIME_BIN=$scriptdir/nuttx/Meadow.OS.Runtime.bin
  dd if=/dev/zero bs=1024 count=2048 of=${MEADOW_OS_RUNTIME_BIN} 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx_user.bin bs=1024 skip=3014656 seek=0 count=2048 of=${MEADOW_OS_RUNTIME_BIN} conv=notrunc 2> /dev/null
fi

printf "Build finished!\n"
