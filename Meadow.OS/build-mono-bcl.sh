#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`

VERBOSE=false
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
# Run autogen.sh
#

if [ ! -f $scriptdir/mono/configure ] || $FORCE || $CLEAN; then
    printf "Running autogen.sh...\n"
    NOCONFIGURE=1 ./autogen.sh
fi

#
# Configure Mono BCL
#

mkdir -p $scriptdir/mono/bcl
cd $scriptdir/mono/bcl

CONFIGURE="../configure
    --disable-boehm
    --disable-btls-lib
    --disable-support-build
    --with-mcs-docs=no
    --disable-nls
    --enable-minimal=interpreter,sockets"

if [ ! -f $scriptdir/mono/bcl/Makefile ] || $FORCE || $CLEAN; then
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

#
# Building Mono BCL
#

printf "Building Mono BCL...\n"
run_command "make -C $scriptdir/mono/bcl -j8"
check_command_status

printf "Packaging Mono...\n"
mkdir -p $scriptdir/mono/libs/bcl
rm -rf $scriptdir/mono/libs/bcl
cp -R $scriptdir/mono/mcs/class/lib/net_4_x $scriptdir/mono/libs/bcl
check_command_status

exit 0