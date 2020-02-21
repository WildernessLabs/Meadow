#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=true
FORCE=false
CLEAN=false
DEBUG=false

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    -c|--clean)
    CLEAN=true
    ;;
    -d|--debug)
    DEBUG=true
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

CFLAGS="-m32"
CXXFLAGS="$CFLAGS"
LDFLAGS="$CFLAGS"

if $DEBUG; then
  DEBUG_CFLAGS="-ggdb"
  CFLAGS="$CFLAGS $DEBUG_CFLAGS"
fi

cd $scriptdir/mono

AUTOGEN="./autogen.sh
    --target=arm-linux-eabi
    --build=
    --enable-maintainer-mode
    --enable-compile-warnings
    --disable-boehm
    --disable-mcs
    --disable-support-build
    --disable-interpreter
    --enable-nls=no
    --enable-minimal=interpreter,sockets,soft_debug"

if [ ! -f $scriptdir/mono/Makefile ] || $FORCE || $CLEAN; then
    printf "Configuring Mono AOT compiler...\n"

    # This step does not use run_command because of bash string escaping issues.
    if $VERBOSE; then
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" 
    else
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" &>/dev/null
    fi
    check_command_status
else
    printf "Mono already configured (use --force to override)\n"
fi

printf "Building Mono AOT compiler...\n"
run_command "make -C $scriptdir/mono -j8 mono/mini"
check_command_status

printf "Packaging Mono AOT compiler...\n"
mkdir -p $scriptdir/mono/libs

cp $scriptdir/mono/mono/mini/mono-sgen \
  $scriptdir/mono/libs

#MONO_PATH="/Users/joao/Dev/WildernessLabs/Meadow/Meadow.OS/assemblies/lib/net45" mono/mini/mono-sgen --aot=full,tool-prefix=/usr/local/bin/arm-none-eabi-,mtriple=arm-linux-eabi /Users/joao/Dev/WildernessLabs/Meadow/Meadow.Core/source/Meadow.Core/bin/Debug/net472/Meadow.dll

exit 0