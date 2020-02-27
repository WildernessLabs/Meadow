#!/bin/bash

set -e

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
MONO=false
CONFIGURE_ONLY=false
CONFIG=mono

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
    -m|--mono)
    MONO=true
    ;;
    --configure)
    CONFIGURE_ONLY=true
    ;;
    --config=*)
    CONFIG=$(echo $i | cut -f2 -d=)
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

get_git_commit_hash() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH rev-parse HEAD`
}

get_git_branch_or_tag() {
  REPO_PATH=$1
  echo `git -C $REPO_PATH describe --tags --exact-match 2> /dev/null || git -C $REPO_PATH symbolic-ref -q --short HEAD`
}

generate_build_info() {
  printf "Generating build info..."

  MEADOW_GIT_HASH=$(get_git_commit_hash $scriptdir)
  MEADOW_GIT_REF=$(get_git_branch_or_tag $scriptdir)

  NUTTX_GIT_HASH=$(get_git_commit_hash $scriptdir/nuttx)
  NUTTX_GIT_REF=$(get_git_branch_or_tag $scriptdir/nuttx)

  NUTTX_APPS_GIT_HASH=$(get_git_commit_hash $scriptdir/apps)
  NUTTX_APPS_GIT_REF=$(get_git_branch_or_tag $scriptdir/apps)

  MONO_GIT_HASH=$(get_git_commit_hash $scriptdir/mono)
  MONO_GIT_REF=$(get_git_branch_or_tag $scriptdir/mono)

  # Generate build-info.json file
JSON=$(cat <<-END
{
  "git": {
    "meadow": [ "$MEADOW_GIT_HASH", "$MEADOW_GIT_REF" ],
    "nuttx": [ "$NUTTX_GIT_HASH", "$NUTTX_GIT_REF" ],
    "nuttx-apps": [ "$NUTTX_APPS_GIT_HASH", "$NUTTX_APPS_GIT_REF" ],
    "mono": [ "$MONO_GIT_HASH", "$MONO_GIT_REF" ]
  },
  "build-date": "`date +"%F %T"`"
}
END
)
  echo "$JSON" > $scriptdir/nuttx/build-info.json

  printf " ${green}success${reset}\n"
}

#
#   Generate build info
#

generate_build_info

#
#   Build NuttX OS base code
#

NUTTX_CONFIG="stm32f777zit6-meadow/$CONFIG"

if [ -r "$scriptdir/nuttx/.config" ] && ($FORCE || $CLEAN); then
    printf "Cleaning NuttX (already configured)..."
    run_command "make -C $scriptdir/nuttx distclean -j8"
    run_command "rm -f $scriptdir/nuttx/configs/stm32f777zit6-meadow/src/hcom/**/*.o"
    run_command "rm -f $scriptdir/nuttx/Meadow.OS.bin"
    check_command_status
fi

if [ ! -r "$scriptdir/nuttx/.config" ] || $FORCE; then
    printf "Configuring NuttX...\n"
    run_command "$scriptdir/nuttx/tools/configure.sh $NUTTX_CONFIG"
    run_command "make -C $scriptdir/nuttx context"
    check_command_status
else
    printf "NuttX already configured (use --force to override)\n"
fi

if $CONFIGURE_ONLY; then
  exit 0
fi

printf "Building NuttX (kernel pass)...\n"
# Build mksyscall first due to issues with concurrency and makefile dependencies
run_command "make -C $scriptdir/nuttx/tools -f Makefile.host mksyscall"
run_command "make -C $scriptdir/nuttx -j8 pass2 pass1deps"
check_command_status

#
#   Build Mono
#

if $MONO; then
  if [[ $NUTTX_CONFIG == *"mono"* ]]; then
    $scriptdir/build-mono.sh "$@"
    if [ $? -ne 0 ]; then
        exit 1
    fi
  fi
fi

if ! grep -q "CONFIG_BUILD_FLAT=y" $scriptdir/nuttx/.config; then
  printf "Building NuttX (user pass)..."
  if $NETCORE; then
    export ENABLE_NETCORE=1
  fi
  run_command "make -C $scriptdir/nuttx -j8 pass1"
  check_command_status
fi

#
#   Package Meadow.OS
#

if [ $CONFIG = "mono" ]; then
  MEADOW_OS_BIN=$scriptdir/nuttx/Meadow.OS.bin
  dd if=/dev/zero bs=1024 count=2048 of=${MEADOW_OS_BIN} 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx.bin bs=1024 of=${MEADOW_OS_BIN} conv=notrunc 2> /dev/null
  dd if=$scriptdir/nuttx/nuttx_user.bin bs=512 skip=1 seek=1 of=${MEADOW_OS_BIN} conv=notrunc 2> /dev/null
fi

printf "Build finished!\n"
