#!/bin/bash

if [[ $(command -v brew) == "" ]]; then
    echo "Installing Homebrew"
    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
else
    echo "Updating Homebrew"
    brew update
fi

PACKAGES=(
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

echo "Installing packages..."
for pkg in ${PACKAGES[@]}
do
    if brew ls --versions $pkg > /dev/null; then
        if brew outdated | grep -q $pkg; then
            echo "Upgrading package $pkg..."
            brew upgrade $pkg
        else
            echo "Package $pkg already installed."
        fi
    else
        echo "Installing package $pkg..."
        brew install $pkg
    fi
done
