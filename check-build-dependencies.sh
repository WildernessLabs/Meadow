#!/bin/bash

MAC_PACKAGES=(
    osx-cross/arm/arm-gcc-bin
    ccache
    autoconf
    libtool
    cmake
    libusb
    automake
    dfu-util
    srecord
)

LINUX_PACKAGES=(
    gcc-arm-none-eabi
    ccache
    kconfig-frontends
    git
    python2
    python-is-python2
    srecord
)

MISSING_PACKAGE=0
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

printf "Checking dependencies...\r\n"

if [[ "$OS" == "linux" ]]; then

  for pkg in ${LINUX_PACKAGES[@]}
    do
      installed_version=$(apt-cache policy $pkg | grep Installed | tr -d '[:space:]')
      if [[ "$installed_version" != *"(none)"* ]]; then
        printf "\t-> $pkg installed. Version: ${installed_version//"Installed:"/}\r\n"
      else
        printf "\t-> $pkg MISSING\r\n"
        MISSING_PACKAGE=1
      fi
    done

elif [[ "$OS" == "mac" ]]; then

  for pkg in ${MAC_PACKAGES[@]}
  do
    if brew ls --versions $pkg > /dev/null; then
      printf "\t-> $pkg installed\r\n"
    else
      printf "\t-> $pkg MISSING\r\n"
    fi
  done

else
  printf "Cannot build on target host.\r\n"
fi

if [[ "$MISSING_PACKAGE" == 1 ]]; then
  printf "At least one build dependency is missing\r\n"
fi