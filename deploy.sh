#!/bin/bash

#set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

QEMU=false
APP_EXE=$scriptdir/../Meadow.Core/source/Tests/HelloLED/bin/Debug/App.exe
NETCORE=false

CLI_ARGS=()

function parseOptions {
  for i in $@
  do
  case $i in
    --qemu)
    QEMU=true
    ;;
    --netcore)
    NETCORE=true
    ;;
    --app=*)
    APP_EXE=$(echo $i | cut -f2 -d=)
    ;;
    *)
    CLI_ARGS+=($i)
    ;;
  esac
  done
}

function validateOptions {
  if [ ! -f "$APP_EXE" ]; then
    printf " ${red}Error:${reset} App executable not found\n"
    exit 0
  fi
}

function buildFiles {
  MONO_BCL_PATH=$scriptdir/mono/libs/bcl
  MONO_BCL_FILES=()

  DOTNET_SDK_VERSION=3.1.2
  DOTNET_SDK_PATH=/usr/local/share/dotnet/shared/Microsoft.NETCore.App/$DOTNET_SDK_VERSION

  if $NETCORE; then
    MONO_BCL_FILES+=(
      $MONO_BCL_PATH/System.Private.CoreLib.dll
      $DOTNET_SDK_PATH/System.dll
      $DOTNET_SDK_PATH/System.Core.dll
    )
  else
    MONO_BCL_FILES+=(
      $MONO_BCL_PATH/mscorlib.dll
      $MONO_BCL_PATH/System.dll
      $MONO_BCL_PATH/System.Core.dll
    )
  fi

  MANAGED_APP_FILES=($APP_EXE)

  # Check for Meadow.dll and Meadow.Core.dll
  APP_PATH=$(dirname "${APP_EXE}")

  if [ -f "$APP_PATH/Meadow.dll" ]; then
    MANAGED_APP_FILES+=($APP_PATH/Meadow.dll)
  fi

  if [ -f "$APP_PATH/Meadow.Core.dll" ]; then
    MANAGED_APP_FILES+=($APP_PATH/Meadow.Core.dll)
  fi

  if [ -f "$APP_PATH/Meadow.Foundation.dll" ]; then
    MANAGED_APP_FILES+=($APP_PATH/Meadow.Foundation.dll)
  fi

  DEPLOY_FILES=(
    "${MONO_BCL_FILES[@]}"
    "${MANAGED_APP_FILES[@]}"
  )
}

function packFlashImage {
  QEMU_DEPLOY_PATH=$scriptdir/qemu/deploy

  LFS_IMAGE_NAME=meadow_qspi_flash.raw
  LFS_IMAGE_SIZE=33554432
  LFS_IMAGE_SIZE_MB=32
  LFS_BLOCK_SIZE=4096

  LFS_IMAGE_PATH=$scriptdir/qemu/$LFS_IMAGE_NAME
  
  if [ -f "$LFS_IMAGE_PATH" ]; then
    rm -r $LFS_IMAGE_PATH
  fi

  dd if=/dev/zero of=$LFS_IMAGE_PATH bs=1m count=$LFS_IMAGE_SIZE_MB 2> /dev/null

  rm -rf $QEMU_DEPLOY_PATH
  mkdir -p $QEMU_DEPLOY_PATH

  for f in ${DEPLOY_FILES[@]}; do
    cp $f $QEMU_DEPLOY_PATH
  done

  printf "Packing files...\n"

  $scriptdir/mklittlefs/mklittlefs \
    -c $QEMU_DEPLOY_PATH -s $LFS_IMAGE_SIZE \
    -b $LFS_BLOCK_SIZE $LFS_IMAGE_PATH
}

function deployFile {
  $scriptdir/cli.sh "${CLI_ARGS[@]}" --WriteFile -f $1 
}

function deploy {
  if $QEMU; then
    packFlashImage
  else
    for f in ${DEPLOY_FILES[@]}; do
      deployFile $f
    done
  fi
}

parseOptions "$@"
validateOptions
buildFiles
deploy
printf " ${green}success${reset}\n"
