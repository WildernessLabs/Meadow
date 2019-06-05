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
# Build Mono
#

NUTTX_HOME=$scriptdir/nuttx

WARNING_FLAGS="\
 -Wno-discarded-qualifiers -Wno-unused-function -Wno-missing-prototypes \
 -Wno-misleading-indentation -Wno-unused-variable -Wno-int-conversion \
 -Wno-shift-count-overflow -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
 -Wno-maybe-uninitialized -Wno-overflow -Wno-incompatible-pointer-types -Wno-format"

COMMON_FLAGS="\
 -D_POSIX_VERSION=201112L -DHAVE_USR_INCLUDE_MALLOC_H=1 -DLACKS_SYS_PARAM_H=1 \
 -D__NuttX__=1 -DSA_RESTART=0 -DSTDIN_FILENO=0 -DSTDOUT_FILENO=1 -DSTDERR_FILENO=2 \
 -I$NUTTX_HOME/include -nostdinc -nostdlib -fno-builtin -Os $WARNING_FLAGS"

CFLAGS="--specs=nosys.specs -mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 $COMMON_FLAGS"
CXXFLAGS="-DCONFIG_WCHAR_BUILTIN"
CPPFLAGS="$COMMON_FLAGS"

CC="ccache arm-none-eabi-gcc"

cd $scriptdir/mono

AUTOGEN="./autogen.sh
    --host=arm-none-eabi
    --enable-maintainer-mode
    --enable-compile-warnings
    --disable-boehm
    --disable-mcs
    --disable-executables
    --disable-support-build
    --enable-interpreter
    --enable-nls=no
    --enable-minimal=jit,profiler,decimal,pinvoke,debug,appdomains,verifier,large_code,logging,\
com,attach,simd,perfcounters,normalization,desktop_loader,shared_perfcounters,\
remoting,security,lldb,mdb,shadowcopy,sockets"

# if [ -f $scriptdir/mono/Makefile ] && $FORCE; then
#     printf "\n"
#     printf "Mono repository needs to be cleaned up manually.\n"
#     printf "Please run \'git -C mono clean -xfdd\' to clean up the repository.\n"
#     printf "Note: This will discard local changes, so make sure to use git stash if needed.\n"
#     exit 1
# fi

if [ ! -f $scriptdir/mono/Makefile ] || $FORCE; then
    printf "Configuring Mono..."

    # This step does not use run_command because of bash string escaping issues.
    if $VERBOSE; then
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" CC="$CC"
    else
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" CC="$CC" &>/dev/null
    fi
    check_command_status
else
    printf "Mono already configured (use --force to override)\n"
fi

printf "Building Mono..."
run_command "make -C $scriptdir/mono -j8"
check_command_status
exit 0