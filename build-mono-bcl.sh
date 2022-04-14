#!/bin/bash

set -eo
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

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

VERBOSE=false
FORCE=false
CLEAN=false
DEBUG=false
MONO_BCL_DIR=$scriptdir/monobcl
MONO_DIR=$scriptdir/mono
KEEP_PDBS=false
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
  -k|--keeppdbs)
  KEEP_PDBS=true
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

function updateBCLDirectory {
  if $CLEAN; then
    printf "Cleaning BCL..."
    if [ -d $MONO_BCL_DIR ]; then
      rm -R $MONO_BCL_DIR
    fi
    git clean -xffd mono/
    git submodule update --init --recursive
    printf " - done\n"
  fi

  if [ -d $MONO_BCL_DIR ]; then
    printf "Synchronising $MONO_DIR with $MONO_BCL_DIR"
    if $VERBOSE; then
      RSYNC_FLAGS="-v --progress"
    else
      RSYNC_FLAGS=
    fi
    if [[ "$OS" == "mac" ]]; then
      pushd . &>/dev/null
      cd $MONO_DIR
      rsync -ar $RSYNC_FLAGS . $MONO_BCL_DIR
      #
      # Delete option here pushes incremental builds from 40s to 15m.
      #
      # rsync -ar $RSYNC_FLAGS --delete . $MONO_BCL_DIR
      popd &>/dev/null
    else
      rsync -a $RSYNC_FLAGS --delete mono/ $MONO_BCL_DIR
    fi
  else
    printf "Copying $MONO_DIR to $MONO_BCL_DIR"
    cp -R $MONO_DIR $MONO_BCL_DIR
  fi
  printf " - done\n"
}

function configureMonoBCL {
  cd $MONO_BCL_DIR

  if [ ! -f $MONO_BCL_DIR/configure ] || $FORCE || $CLEAN; then
    printf "Running autogen.sh...\n"
    NOCONFIGURE=1 ./autogen.sh
  fi

  CONFIGURE="./configure
      --disable-boehm
      --disable-btls-lib
      --disable-support-build
      --with-mcs-docs=no
      --enable-mbedtls
      --disable-nls"

  if [ ! -f $MONO_BCL_DIR/Makefile ] || $FORCE || $CLEAN; then
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
  OLD_VERBOSE=$VERBOSE
  VERBOSE=true
  run_command "make -C $MONO_BCL_DIR -j8"
  run_command "make -C $MONO_BCL_DIR -j8 PROFILE_PLATFORM=linux HOST_PLATFORM=linux"
  run_command "make -C ${MONO_BCL_DIR}/mcs/class/Facades/System.Memory PROFILE_PLATFORM=linux HOST_PLATFORM=linux"
  run_command "make -C ${MONO_BCL_DIR}/mcs/class/Facades/System.Buffers PROFILE_PLATFORM=linux HOST_PLATFORM=linux"
  run_command "make -C ${MONO_BCL_DIR}/mcs/class/Facades/Microsoft.Bcl.AsyncInterfaces PROFILE_PLATFORM=linux HOST_PLATFORM=linux"
  run_command "make -C ${MONO_BCL_DIR}/mcs/class/Facades/System.Threading.Tasks.Extensions PROFILE_PLATFORM=linux HOST_PLATFORM=linux"
  VERBOSE=$OLD_VERBOSE
  check_command_status
}

function packageMonoBCL {
  printf "Packaging Mono...\n"
  mkdir -p $MONO_BCL_DIR/libs/bcl
  rm -rf $MONO_BCL_DIR/libs/bcl
  cp -R $MONO_BCL_DIR/mcs/class/lib/net_4_x-linux $MONO_BCL_DIR/libs/bcl
  pushd $MONO_BCL_DIR/libs/bcl
  cat <$scriptdir/bcl-blacklist.txt | xargs -n 10 rm
  if ! $KEEP_PDBS; then
    cat <$scriptdir/bcl-pdb-blacklist.txt | xargs -n 10 rm
  fi
  popd
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
  echo "$CONFIG_MAKE" > $MONO_BCL_DIR/netcore/config.make
}

function buildNetCoreBCL {
  printf "Building Mono .NET Core BCL...\n"
  COREARCH=arm make -C $MONO_BCL_DIR/netcore bcl
}

function packageNetCoreBCL {
  printf "Packaging Mono .NET Core BCL...\n"
  rm -rf $MONO_BCL_DIR/libs/bcl
  mkdir -p $MONO_BCL_DIR/libs/bcl
  cp $MONO_BCL_DIR/netcore/System.Private.CoreLib/bin/arm/*System.Private.CoreLib.{dll,pdb,xml} $MONO_BCL_DIR/libs/bcl
}

updateBCLDirectory

mkdir -p $MONO_BCL_DIR/bcl
cd $MONO_BCL_DIR/bcl

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
