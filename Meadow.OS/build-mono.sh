#!/bin/bash

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
DEBUG=false
NETCORE=false

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
    --netcore)
    NETCORE=true
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
 -I$NUTTX_HOME/include -I$NUTTX_HOME/include/nuttx/lib -nostdinc -nostdlib -fno-builtin -fno-common -Os $WARNING_FLAGS"

CFLAGS="-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 $COMMON_FLAGS"
CXXFLAGS="-DCONFIG_WCHAR_BUILTIN"
CPPFLAGS="$COMMON_FLAGS"

CC="arm-none-eabi-gcc"
CPP="arm-none-eabi-cpp"
CXX="arm-none-eabi-g++"

if $DEBUG; then
  DEBUG_CFLAGS="-ggdb"
  CFLAGS="$CFLAGS $DEBUG_CFLAGS"
fi

cd $scriptdir/mono

CONFIGURE="./configure
    --host=arm-none-eabi
    --enable-maintainer-mode
    --enable-compile-warnings
    --disable-boehm
    --disable-mcs
    --disable-executables
    --disable-support-build
    --enable-cooperative-suspend
    --enable-interpreter
    --enable-nls=no
    --enable-minimal=jit,profiler,pinvoke,debug,appdomains,verifier,large_code,logging,\
com,attach,simd,perfcounters,normalization,desktop_loader,shared_perfcounters,\
remoting,security,lldb,mdb,shadowcopy"

if $NETCORE; then
  CONFIGURE="$CONFIGURE --with-core=only"
fi

# if [ -f $scriptdir/mono/Makefile ] && $FORCE; then
#     printf "\n"
#     printf "Mono repository needs to be cleaned up manually.\n"
#     printf "Please run \'git -C mono clean -xfdd\' to clean up the repository.\n"
#     printf "Note: This will discard local changes, so make sure to use git stash if needed.\n"
#     exit 1
# fi

if [ ! -f $scriptdir/mono/configure ] || $FORCE || $CLEAN; then
    printf "Running autogen.sh...\n"
    NOCONFIGURE=1 ./autogen.sh
fi

if [ ! -f $scriptdir/nuttx/include/nuttx/config.h ]; then
    printf "NuttX includes not found, please run NuttX configure step.\n"
    exit 1
fi

if [ ! -f $scriptdir/mono/Makefile ] || $FORCE || $CLEAN; then
    printf "Configuring Mono...\n"

    # This step does not use run_command because of bash string escaping issues.
    if $VERBOSE; then
        $CONFIGURE CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" CC="$CC" CXX="$CXX" CPP="$CPP" 
    else
        $CONFIGURE CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" CC="$CC" CXX="$CXX" CPP="$CC" &>/dev/null
    fi
    check_command_status
else
    printf "Mono already configured (use --force to override)\n"
fi

printf "Building Mono...\n"
run_command "make -C $scriptdir/mono -j8"
check_command_status

printf "Packaging Mono...\n"
mkdir -p $scriptdir/mono/libs

cp $scriptdir/mono/mono/sgen/.libs/libmonosgen.a \
  $scriptdir/mono/mono/mini/.libs/libmini.a \
  $scriptdir/mono/mono/mini/.libs/libmono-dbg.a \
  $scriptdir/mono/mono/mini/.libs/libmono-ee-interp.a \
  $scriptdir/mono/mono/utils/.libs/libmonoutils.a \
  $scriptdir/mono/mono/eglib/.libs/libeglib.a \
  $scriptdir/mono/mono/metadata/.libs/libmonoruntime-config.a \
  $scriptdir/mono/mono/metadata/.libs/libmonoruntimesgen.a \
  $scriptdir/mono/libs

if [ -f $scriptdir/mono/mono/dis/libmonodis.a ]; then
  cp $scriptdir/mono/mono/dis/libmonodis.a $scriptdir/mono/libs
fi

if [ -f $scriptdir/mono/mono/metadata/.libs/libmonoruntime-support.a ]; then
  cp $scriptdir/mono/mono/metadata/.libs/libmonoruntime-support.a $scriptdir/mono/libs
fi

if [ -f $scriptdir/mono/mono/metadata/.libs/libmono-system-native.a ]; then
  cp $scriptdir/mono/mono/metadata/.libs/libmono-system-native.a $scriptdir/mono/libs
fi

if [ -f $scriptdir/mono/mono/native/.libs/libmono-native.a ]; then
  cp $scriptdir/mono/mono/native/.libs/libmono-native.a $scriptdir/mono/libs
fi

if [ -f $scriptdir/mono/mono/utils/.libs/libmonomath.a ]; then
  cp $scriptdir/mono/mono/utils/.libs/libmonomath.a $scriptdir/mono/libs
fi

exit 0
