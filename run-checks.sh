#!/bin/bash -e

#set -e
scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

. $scriptdir/scripts/common_methods.sh
. $scriptdir/scripts/version_methods.sh

set_os_name
check_if_interactive

#
# Setup some of the variables used by this script.
#
HELP=false
CPPCHECK_NUTTX_IGNORE_LIST=-i$scriptdir/nuttx/configs/stm32f777zit6-meadow/src/libyaml

for i in "$@"
do
case $i in
    -h|--help)
    HELP=true
    ;;
    -i3p)
    CPPCHECK_IGNORE_LIST=
    ;;
    *)
    echo "${0##*/} - Unknown option $i"
    exit 1
    ;;
esac
done

if [ "$HELP" = true ]; then
  echo "Usage: ${0##*/} [options]"
  echo " "
  echo "Options:"
  echo "  -h|--help                     Show this help message"
  echo "  -i3p                          Include third party libraries in cppcheck"
  exit 0
fi

run_command() {
  if $VERBOSE || $DRYRUN; then
    echo "Executing command: $1"
    if ! $DRYRUN; then
        $1
    fi
  else
    if ! $DRYRUN; then
        $1 &>/dev/null
    fi
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

cppcheck --error-exitcode=1 --quiet --check-level=exhaustive --force --inline-suppr $CPPCHECK_NUTTX_IGNORE_LIST $scriptdir/nuttx/configs/stm32f777zit6-meadow/src
cppcheck --error-exitcode=1 --quiet --check-level=exhaustive --force --inline-suppr $scriptdir/apps/examples/hcom
