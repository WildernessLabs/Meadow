#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`

VERBOSE=false
FORCE=false

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
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
# Elevate to root for the following install steps
#
sudo true

#
#   Build kconfig
#

if [[ $(command -v kconfig) == "" ]] || $FORCE; then
    cd $scriptdir/tools/kconfig-frontends
    printf "Configuring kconfig..."
    run_command "./configure --enable-mconf --disable-nconf --disable-gconf --disable-qconf"
    check_command_status

    printf "Building kconfig..."
    run_command "make"
    check_command_status

    printf "Installing kconfig..."
    run_command "sudo make install"
    check_command_status
else
    printf "kconfig already installed, skipping...\n"
fi

#
#   Build ST-Link
#
if [[ $(command -v st-util) == "" ]] || $FORCE; then
    cd $scriptdir/stlink

    printf "Checking out ST-Link Meadow branch..."
    run_command "git checkout meadow"
    check_command_status

    printf "Building ST-Link..."
    run_command "make debug release"
    check_command_status

    printf "Installing ST-Link..."
    cd $scriptdir/stlink/build/Release
    run_command "sudo make install"
    check_command_status
else
    printf "ST-Link already installed, skipping...\n"
fi