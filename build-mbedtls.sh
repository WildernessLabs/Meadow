#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=true

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    *)
    # unknown option
    ;;
esac
done

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

#
# Build Mono
#

NUTTX_HOME=$scriptdir/nuttx

WARNING_FLAGS="\
 -Wno-discarded-qualifiers -Wno-unused-function -Wno-missing-prototypes \
 -Wno-misleading-indentation -Wno-unused-variable -Wno-int-conversion \
 -Wno-shift-count-overflow -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
 -Wno-maybe-uninitialized -Wno-overflow -Wno-incompatible-pointer-types -Wno-format"

COMMON_FLAGS="\
 -D_POSIX_VERSION=201112L -DHAVE_USR_INCLUDE_MALLOC_H=1 -DLACKS_SYS_PARAM_H=1 \
 -D__NuttX__=1 -DSA_RESTART=0 -DSTDIN_FILENO=0 -DSTDOUT_FILENO=1 -DSTDERR_FILENO=2 \
 -I$NUTTX_HOME/include -I$NUTTX_HOME/include/nuttx/lib -I$NUTTX_HOME/include/sys -nostdinc -nostdlib -fno-builtin -fno-common -Os $WARNING_FLAGS"

CFLAGS="-g -mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 $COMMON_FLAGS"
CXXFLAGS="-DCONFIG_WCHAR_BUILTIN"
CPPFLAGS="$CFLAGS"
LDFLAGS="$CFLAGS"

CC="arm-none-eabi-gcc"
CPP="arm-none-eabi-cpp"
CXX="arm-none-eabi-g++"
LD="arm-non-eabi-ld"

cd $scriptdir/mbedtls/library

make -j8 V=1 CFLAGS="$CFLAGS" CC="$CC" LDFLAGS="$CFLAGS -static" LD="$LD"