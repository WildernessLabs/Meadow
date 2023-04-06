#!/bin/bash

#
#   Work out the OS so that we can change actions per OS where necessary.
#
set_os_name()
{
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
}

#
# Check if the shell is interactive.
#
check_if_interactive() {
  if [[ $- == *i* ]]; then
    red=`tput setaf 1`
    green=`tput setaf 2`
    reset=`tput sgr0`
  fi
}

#
#   Run the specified command and redirect the command output if not verbose.
#
run_command() {
  if $VERBOSE; then
    echo
    $1
  else
    $1 &>/dev/null
  fi
}

#
#   Check if the last command was successful.
#
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
# Display the build summary.
#
print_build_summary() {
  now=$(date +"%T")
  printf "Build of version $VERSION_MAJOR.$VERSION_MINOR.$VERSION_REVISION.$VERSION_BUILD (${BUILD_GIT_HASH:0-8}:$BUILD_GIT_REF) finished at $now\n"
}