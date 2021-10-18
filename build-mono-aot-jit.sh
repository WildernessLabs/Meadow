#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=true
DEBUG=false
DEST=/tmp/mono.jit

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
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

build_mono_libs() {
    #
    # Build mono and libraries then clean out mono keeping libraries
    #
    rm -rf ${DIST}
    rm -rf $scriptdir/mono-libs
    cp -a $scriptdir/mono $scriptdir/mono-libs
    pushd $scriptdir/mono-libs
    ./autogen.sh --prefix=${DEST}
    check_command_status
    run_command "make -C $scriptdir/mono-libs"
    check_command_status
    run_command "make -C $scriptdir/mono-libs install"
    rm -rf ${DEST}/bin
    popd
}

build_cross_compiler() {
    #
    # Build Mono
    #
    
    pushd $scriptdir/mono
    rm -rf llvm/build
    CFLAGS="-m32 -D__THUMB__"
    CXXFLAGS="$CFLAGS"
    LDFLAGS="-m32"
    export CMAKE_C_FLAGS="$CFLAGS"
    export CMAKE_CXX_FLAGS="$CXXFLAGS"
    export LLVM_CMAKE_ARGS="-DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32"
    
    if $DEBUG; then
      DEBUG_CFLAGS="-ggdb"
      CFLAGS="$CFLAGS $DEBUG_CFLAGS"
    fi
    
    cd $scriptdir/mono
    
    AUTOGEN="./autogen.sh
        --target=arm-linux-eabi 
        --prefix=${DEST}
        --host=i686-pc-linux-gnu 
        --build=i686-pc-linux-gnu 
        --enable-llvm
        --with-mcs-docs=no
        --disable-boehm 
        --disable-support-build 
        --enable-cooperative-suspend 
        --enable-interpreter 
        --enable-nls=no 
        --enable-minimal=profiler,pinvoke,debug,appdomains,verifier,large_code,logging,com,attach,perfcounters,normalization,desktop_loader,shared_perfcounters,remoting,security,lldb,mdb,shadowcopy
        --enable-maintainer-mode
        --enable-compile-warnings"
    
    printf "Configuring Mono AOT compiler...\n"

    # This step does not use run_command because of bash string escaping issues.
    if $VERBOSE; then
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" 
    else
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" &>/dev/null
    fi
    check_command_status
    
    printf "Building Mono AOT compiler...\n"
    run_command "make -C $scriptdir/mono"
    check_command_status
    
    printf "Creating archive...\n"
    run_command "make -C $scriptdir/mono install"
    tar -cJf ${DEST}.tar.xz --strip-components=1 ${DEST}
    popd
}

build_mono_libs
build_cross_compiler
