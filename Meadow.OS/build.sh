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
#   Build NuttX OS base code
#

if [ -r "$scriptdir/nuttx/.config" ] && $FORCE; then
    printf "Cleaning NuttX (already configured)..."
    run_command "make -C $scriptdir/nuttx distclean"
    check_command_status
fi

if [ ! -r "$scriptdir/nuttx/.config" ] || $FORCE; then
    printf "Configuring NuttX..."
    run_command "$scriptdir/nuttx/tools/configure.sh stm32f777zit6-meadow/mono"
    check_command_status
else
    printf "NuttX already configured (use --force to override)\n"
fi

printf "Building NuttX (kernel pass)..."
# Build mksyscall first due to issues with concurrency and makefile dependencies
run_command "make -C $scriptdir/nuttx/tools -f Makefile.host mksyscall"
run_command "make -C $scriptdir/nuttx -j8 pass2 pass1deps"
check_command_status

#
#   Build Mono
#
$scriptdir/build-mono.sh "$@"
if [ $? -ne 0 ]; then
    exit 1
fi

printf "Building NuttX (user pass)..."
run_command "make -C $scriptdir/nuttx -j8 pass1 "
check_command_status

printf "Build finished!\n"