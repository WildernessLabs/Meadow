#!/bin/bash -e

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
DEBUG_BL_CDC=false
DEBUG_BL_UART=false
HELP=false
ENABLE_STACK_DUMP=false
MAKE_OPTIONS=

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
    WLCLEAN=true
    ;;
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
    -mfd|--makefiledebugging)
    MAKE_OPTIONS="--debug VERBOSE=1"
    ;;
    --esd)
    ENABLE_STACK_DUMP=true
    ;;
    --dbc|--debug-bl-cdc)
    DEBUG_BL_CDC=true
    ;;
    --dbu|--debug-bl-uart)
    DEBUG_BL_UART=true
    ;;
    --config=*)
    CONFIG=$(echo $i | cut -f2 -d=)
    ;;
    *)
    echo "${0##*/} - Unknown option $i"
    exit 1
    ;;
esac
done

if [ "$HELP" = true ]; then
  echo "Usage: ${0##*/} [options]"
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
  echo "  -esd                         Enable stack dumps to be sent to USART1 (COM1)"
  echo "  --config=mono|netcore        Select Mono or .NET Core builds (default Mono)"
  echo "  -mfd|--makefiledebugging     Turn on debug options for make"
  exit 0
fi

if [[ -z "$MEADOW_ADDITIONAL_MAKE_OPTIONS" ]]; then
  MEADOW_ADDITIONAL_MAKE_OPTIONS="-j8"
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

get_version_change_distance() {
  REPO_PATH=$1
  origin=$(git log -n 1 --oneline $scriptdir/version.txt  | cut -f 1 -d " ")
  distance=$(git log --oneline ${origin}..HEAD | wc -l)
  echo $distance
}

sedFriendly() {
  result=$(echo $1 | sed -r 's/([\$\.\*\/\[\\^])/\\\1/g'|sed 's/[]]/\[]]/g')
  echo $result
}

inject_value() {
  KEY=$1
  VALUE=$(eval echo '${'$KEY'}')
  # REPLACEMENT=$(sedFriendly $VALUE)
  FILE=$2
  sed -i.bak 's/###'${KEY}'###/'${VALUE}'/g' $FILE
}

generate_build_info() {
  printf "Generating build info..."

  MEADOW_GIT_HASH=$(get_git_commit_hash $scriptdir)
  MEADOW_GIT_REF=$(get_git_branch_or_tag $scriptdir)

  git checkout HEAD $scriptdir/version.txt

  read -r MEADOW_VERSION_STRING<$scriptdir/version.txt || true
  IFS='.' read -ra MEADOW_VERSION <<< "$MEADOW_VERSION_STRING"
  VERSION_MAJOR=${MEADOW_VERSION[0]}
  VERSION_MINOR=${MEADOW_VERSION[1]}
  VERSION_REVISION=${MEADOW_VERSION[2]}

  VERSION_BUILD=$(get_version_change_distance $1)

  echo Calculated version: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}.${VERSION_BUILD} '('${MEADOW_GIT_HASH:0-8}:${MEADOW_GIT_REF}')'

  git checkout HEAD $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld
  git checkout HEAD $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h

  #
  # Get the date / time components in UTC format.
  #
  # These macros may look odd but the date foramtting can return date componets in the form
  # 00, 01, 02 etc and these when compiled are taken as octal numbers.  This means 09 is an
  # invalid number for the compiler so it it is necessary to remove the leading 0 and put it
  # back when formatting the date/time output for the user.
  #
  BUILD_DAY=$((10#`date -u +"%d"`))
  BUILD_TWO_DIGIT_DAY=`date -u +"%d"`
  BUILD_MONTH=$((10#`date -u +"%m"`))
  BUILD_TWO_DIGIT_MONTH=`date -u +"%m"`
  BUILD_MONTH_NAME=`date -u +"%b"`
  BUILD_YEAR=$((10#`date -u +"%y"`))
  BUILD_HOUR=$((10#`date -u +"%H"`))
  BUILD_TWO_DIGIT_HOUR=`date -u +"%H"`
  BUILD_MINUTE=$((10#`date -u +"%M"`))
  BUILD_TWO_DIGIT_MINUTE=`date -u +"%M"`
  BUILD_SECOND=$((10#`date -u +"%S"`))
  BUILD_TWO_DIGIT_SECOND=`date -u +"%S"`
  BUILD_HASH="0x${MEADOW_GIT_HASH:0-8}"
  BUILD_HASH_STRING="${MEADOW_GIT_HASH:0-8}"
  BUILD_EPOCH_TIME=`date -u +"%s"`

  for s in $(echo VERSION_MAJOR VERSION_MINOR VERSION_REVISION VERSION_BUILD BUILD_DAY BUILD_TWO_DIGIT_DAY BUILD_MONTH BUILD_TWO_DIGIT_MONTH BUILD_MONTH_NAME BUILD_YEAR BUILD_HOUR BUILD_TWO_DIGIT_HOUR HOUR BUILD_MINUTE BUILD_TWO_DIGIT_MINUTE BUILD_SECOND BUILD_TWO_DIGIT_SECOND BUILD_HASH MEADOW_GIT_REF BUILD_EPOCH_TIME BUILD_HASH_STRING)
  do
    inject_value $s $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld
    inject_value $s $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h
  done

  MONO_GIT_REF=''
  BYTE_COUNT=0
  for b in `xxd -p -c 1 <<<$MEADOW_GIT_REF`
  do
    if [ $BYTE_COUNT -lt 32 ]; then
      if [ "$b" != "0a" ]; then
        MONO_GIT_REF+="BYTE(0x$b)"
        BYTE_COUNT=$((BYTE_COUNT+1))
      fi
    fi
  done
  MONO_GIT_REF+="BYTE(00)"
  sed -i.bak 's/###MONO_GIT_REF###/'$MONO_GIT_REF'/g' $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld

# Generate build-info.json file
BUILD_DATE="`date +"%F %T"`"
BUILD_HASH="`echo "$BUILD_DATE" | shasum -a 256 | awk '{print $1}'`"

JSON=$(cat <<-END
{
  "git": {
    "meadow": [ "$MEADOW_GIT_HASH", "$MEADOW_GIT_REF" ]
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
#   The ESP unit tests require a secrets file to be present so check if there is one
#   available and copy it to the right place if it is available.  This file does not
#   want to find its way its way into source control so its existence will be checked
#   later and it will be removed (assuming success).
#
if test -f "$scriptdir/../secrets.h"; then
    cp $scriptdir/../secrets.h $scriptdir/nuttx/configs/stm32f777zit6-meadow/src/espcp
fi

#
#   Generate build info
#

generate_build_info

#
# Setup toolchain
#

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
# Added the ability to clean only the code created by Wilderness Labs
#
# This option allows for a clean of only the frequently edited files which
# reduces the compilation time.
#
if $WLCLEAN || $CLEAN || $FORCE; then
    find $scriptdir/apps/examples -name "*.o" -type f -exec rm {} \;
    find $scriptdir/nuttx/configs/stm32f777zit6-meadow -name "*.o" -type f -exec rm {} \;
    run_command "make $MEADOW_ADDITIONAL_MAKE_OPTIONS -C $scriptdir/bootloader/Debug clean"
fi

#
# Force the version number to update if it has changed.
#
rm -f $scriptdir/nuttx/configs/stm32f777zit6-meadow/src/hcom_nx/hcom_nx_config_manager.o
rm -f $scriptdir/nuttx/configs/stm32f777zit6-meadow/src/hcom_nx/diag/hcom_nx_trace_msg_proc.o
rm -f $scriptdir/apps/examples/hcom/nx_rqsts/hcom_misc_requests.o
rm -f $scriptdir/apps/examples/hcom/diag/hcom_diag_logging.o
rm -f $scriptdir/nuttx/*.bin
rm -f $scriptdir/nuttx/*.elf
rm -f $scriptdir/nuttx/*.hex

#
#   Build the bootloader
#

$scriptdir/build-bootloader.sh "$@"
if [ $? -ne 0 ]; then
    exit 1
fi


if [ -r "$scriptdir/nuttx/.config" ] && ($FORCE || $CLEAN); then
    printf "Cleaning NuttX (already configured)..."
    run_command "make -C $scriptdir/nuttx distclean -j8 $MAKE_OPTIONS"
    run_command "rm -f $scriptdir/nuttx/Meadow.OS.bin"
    check_command_status
fi

if [ ! -r "$scriptdir/nuttx/.config" ] || $FORCE; then
    printf "Configuring NuttX...\n"
    run_command "$scriptdir/nuttx/tools/configure.sh $NUTTX_CONFIG"

    if $ENABLE_STACK_DUMP; then
      #
      # This is used to turn off RAMLOG and enables stack dumps to be sent to USART1 (COM1).
      #
      printf "\n\n********** Enabling stack dump to USART1 (COM1).  This will disable RAMLOG. **********\n\n"
      kconfig-tweak --enable DEV_CONSOLE
      kconfig-tweak --enable SERIAL_CONSOLE
      kconfig-tweak --enable USART1_SERIAL_CONSOLE
      kconfig-tweak --enable SYSLOG_WRITE
      kconfig-tweak --enable SYSLOG_SERIAL_CONSOLE
      kconfig-tweak --enable SYSLOG_CONSOLE

      kconfig-tweak --undefine NO_SERIAL_CONSOLE
      kconfig-tweak --undefine RAMLOG
      kconfig-tweak --undefine RAMLOG_BUFSIZE
      kconfig-tweak --undefine RAMLOG_NPOLLWAITERS
      kconfig-tweak --undefine RAMLOG_SYSLOG
    fi

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
run_command "make -C $scriptdir/nuttx/tools $MAKE_OPTIONS -f Makefile.host mksyscall"
run_command "make -C $scriptdir/nuttx $MEADOW_ADDITIONAL_MAKE_OPTIONS $MAKE_OPTIONS pass2"
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

run_command "make -C $scriptdir/nuttx $MEADOW_ADDITIONAL_MAKE_OPTIONS $MAKE_OPTIONS pass1deps"
check_command_status

if ! grep -q "CONFIG_BUILD_FLAT=y" $scriptdir/nuttx/.config; then
  printf "Building NuttX (user pass)..."
  if $NETCORE; then
    export ENABLE_NETCORE=1
  fi
  run_command "make -C $scriptdir/nuttx $MEADOW_ADDITIONAL_MAKE_OPTIONS $MAKE_OPTIONS pass1"
  check_command_status
fi

#
#   Package Meadow.OS
#

if ! grep -q "CONFIG_BUILD_FLAT=y" $scriptdir/nuttx/.config; then
  MEADOW_OS_BIN=$scriptdir/nuttx/Meadow.OS.NoBL.bin
  MEADOW_BL_BIN=$scriptdir/bootloader/Debug/Meadow.BL.bin
  MEADOW_OS_BL_BIN=$scriptdir/nuttx/Meadow.OS.bin
  dd if=/dev/zero bs=1024 count=1792 of=${MEADOW_OS_BIN} 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx.bin bs=1024 of=${MEADOW_OS_BIN} conv=notrunc 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx_user.bin bs=512 skip=1 seek=1 count=2047 of=${MEADOW_OS_BIN} conv=notrunc 2> /dev/null
 
  # Generate CRC and overwrite to last 4 bytes of binary
  srec_cat ${MEADOW_OS_BIN} -Binary -crop 0x00000000 0x001BFFFC -STM32 0x001BFFFC -o ${MEADOW_OS_BIN} -Binary

  # Merge Meadow.BL binary with Meadow.OS binary
  srec_cat ${MEADOW_BL_BIN} -Binary ${MEADOW_OS_BIN} -Binary -offset 0x00040000 -o ${MEADOW_OS_BL_BIN} -Binary

  MEADOW_OS_RUNTIME_BIN=$scriptdir/nuttx/Meadow.OS.Runtime.bin
  dd if=/dev/zero bs=1024 count=3072 of=${MEADOW_OS_RUNTIME_BIN} 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx_user.bin bs=1024 skip=3014400 seek=0 count=3072 of=${MEADOW_OS_RUNTIME_BIN} conv=notrunc 2> /dev/null
fi

# restore auto-versioned files
git checkout HEAD $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld
git checkout HEAD $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h
rm $scriptdir/nuttx/configs/stm32f777zit6-meadow/scripts/user-space.ld.bak
rm $scriptdir/nuttx/include/meadow/hcom_nuttx_shared.h.bak

#
#   Check for the secrets.h file and remove it if found to prevent the file
#   finding its way into source control.
#
if test -f "$scriptdir/nuttx/configs/stm32f777zit6-meadow/src/espcp/secrets.h"; then
    rm $scriptdir/nuttx/configs/stm32f777zit6-meadow/src/espcp/secrets.h
fi

now=$(date +"%T")
printf "Build of version $VERSION_MAJOR.$VERSION_MINOR.$VERSION_REVISION.$VERSION_BUILD (${MEADOW_GIT_HASH:0-8}:$MEADOW_GIT_REF) finished at $now\n"
