#!/bin/bash

set -eo
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
MONO_DIR=$scriptdir/mono
NETCORE=false

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
  --netcore)
  NETCORE=true
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

function configureMonoBCL {
  cd $MONO_DIR

  if $CLEAN; then
    printf "Cleaning the Mono build tree...\n"
    git clean -xfd
    git submodule foreach --recursive git clean -xfd
  fi

  if [ ! -f $MONO_DIR/configure ] || $FORCE || $CLEAN; then
    printf "Running autogen.sh...\n"
    NOCONFIGURE=1 ./autogen.sh
  fi

  CONFIGURE="./configure
      --disable-boehm
      --disable-btls-lib
      --disable-support-build
      --with-mcs-docs=no
      --enable-mbedtls=yes
      --disable-nls"

  if [ ! -f $MONO_DIR/bcl/Makefile ] || $FORCE || $CLEAN; then
      printf "Configuring Mono BCL...\n"

      # This step does not use run_command because of bash string escaping issues.
      if $VERBOSE; then
          $CONFIGURE
      else
          $CONFIGURE &>/dev/null
      fi
      check_command_status
  else
      printf "Mono already configured (use --force to override)\n"
  fi
}

function buildMonoBCL {
  printf "Building Mono BCL...\n"
  run_command "make -C $MONO_DIR -j8"
  check_command_status
}

function packageMonoBCL {
  printf "Packaging Mono...\n"
  mkdir -p $MONO_DIR/libs/bcl
  rm -rf $MONO_DIR/libs/bcl
  cp -R $MONO_DIR/mcs/class/lib/net_4_x $MONO_DIR/libs/bcl
  check_command_status
}

function generateNetCoreBCLConfig {
  printf "Generating Mono .NET Core config.make...\n"
  # Generate config.make file
  CONFIG_MAKE=$(cat <<-END
VERSION = 6.9.0
RID = linux-arm
COREARCH = arm
CORETARGETS = -p:TargetsUnix=true 
MONO_CORLIB_VERSION = 423e7794-9279-49a3-a477-f1cb2432e9f4
HOST_PLATFORM ?= linux
END
)
  echo "$CONFIG_MAKE" > $MONO_DIR/netcore/config.make
}

function buildNetCoreBCL {
  printf "Building Mono .NET Core BCL...\n"
  COREARCH=arm make -C $MONO_DIR/netcore bcl
}

function packageNetCoreBCL {
  printf "Packaging Mono .NET Core BCL...\n"
  rm -rf $MONO_DIR/libs/bcl
  mkdir -p $MONO_DIR/libs/bcl
  cp $MONO_DIR/netcore/System.Private.CoreLib/bin/arm/*System.Private.CoreLib.{dll,pdb,xml} $MONO_DIR/libs/bcl
}

mkdir -p $MONO_DIR/bcl
cd $MONO_DIR/bcl

#
# Configure, build and package Mono / .NET Core BCL
#

if $NETCORE; then
  generateNetCoreBCLConfig
  buildNetCoreBCL
  packageNetCoreBCL
else
  configureMonoBCL
  buildMonoBCL
  packageMonoBCL
fi

exit 0
