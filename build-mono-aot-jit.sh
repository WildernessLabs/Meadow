#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=false
DEBUG=false
DESTDIR=/tmp
DESTSUB="mono.jit"
DEST=${DESTDIR}/${DESTSUB}
MONO_DIR=${scriptdir}/mono-libs

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
  if ${VERBOSE}; then
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
    if ! ${VERBOSE}; then
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
  rm -rf ${MONO_DIR}
  cd ${scriptdir}
  RSYNC_FLAGS="--exclude .libs --exclude *.o --exclude *.a --exclude *.lo --exclude *.la"
  echo "Copying mono sources to build area"
  if [[ "$OS" == "mac" ]]; then
    cd ${scriptdir}/mono
    rsync -ar ${RSYNC_FLAGS} --delete . ${MONO_DIR}
    cd ..
  else
    rsync -a ${RSYNC_FLAGS} --delete mono/ ${MONO_DIR}
  fi
  pushd ${MONO_DIR}
  echo "Configuring..."
  AUTOGEN="./autogen.sh --prefix=${DEST}
    --disable-boehm
    --disable-btls-lib
    --disable-support-build
    --with-mcs-docs=no
    --enable-mbedtls
    --disable-nls"
  # This step does not use run_command because of bash string escaping issues.
  if ${VERBOSE}; then
      ${AUTOGEN} 
  else
      ${AUTOGEN} &>/dev/null
  fi
  check_command_status
  echo "Building..."
  run_command "make -C ${MONO_DIR}"
  check_command_status
  echo "Installing libraries..."
  run_command "make -C ${MONO_DIR} install"
  rm -rf ${DEST}/bin ${MONO_DIR}
  popd
}

build_cross_compiler() {
  #
  # Build Mono
  #
  
  pushd ${scriptdir}/mono
  # Set flags to build 32-bit thumb2
  CFLAGS="-D__THUMB__"
  CXXFLAGS="$CFLAGS"
  LDFLAGS=""
  export CMAKE_C_FLAGS="${CFLAGS}"
  export CMAKE_CXX_FLAGS="${CXXFLAGS}"
  # export LLVM_CMAKE_ARGS="-DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32"
  
  if $DEBUG; then
    DEBUG_CFLAGS="-ggdb"
    CFLAGS="${CFLAGS} ${DEBUG_CFLAGS}"
  fi
  
  cd ${scriptdir}/mono

  case `uname -m` in
    x86_64|s390x|ppc64|ppc64le|Darwin|arm64|aarch64)
      CROSS_OFFSET="--with-cross-offsets=mono/arch/arm/thumb-offsets.h"
      ;;
    i686|arm|ppc)
      CROSS_OFFSET=""
      ;;
    *)
      echo "Assuming this platform is 64-bit" >&2
      CROSS_OFFSET="--with-cross-offsets=mono/arch/arm/thumb-offsets.h"
      ;;
  esac

  AUTOGEN="./autogen.sh
      --target=arm-linux-eabi 
      --prefix=${DEST}
      --enable-llvm
      --with-mcs-docs=no
      ${CROSS_OFFSET}
      --disable-boehm 
      --disable-support-build 
      --enable-cooperative-suspend 
      --enable-interpreter 
      --enable-nls=no 
      --enable-minimal=pinvoke,debug,appdomains,verifier,large_code,com,attach,perfcounters,normalization,desktop_loader,shared_perfcounters,remoting,security,lldb,mdb,shadowcopy
      --enable-maintainer-mode
      --enable-compile-warnings"
  
#      --host=i686-linux-gnu 
#      --build=i686-linux-gnu 
  printf "Configuring Mono AOT compiler...\n"

  # This step does not use run_command because of bash string escaping issues.
  if ${VERBOSE}; then
      ${AUTOGEN} CFLAGS="${CFLAGS}" CPPFLAGS="${CPPFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}" 
  else
      ${AUTOGEN} CFLAGS="${CFLAGS}" CPPFLAGS="${CPPFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}" &>/dev/null
  fi
  check_command_status
  
  printf "Building Mono AOT compiler...\n"
  run_command "make -C ${scriptdir}/mono"
  check_command_status
  
  printf "Creating archive...\n"
  run_command "make -C ${scriptdir}/mono install"
  rm -f ${DEST}/bin/mono
  ln -f ${DEST}/bin/arm-linux-eabi-mono-sgen ${DEST}/bin/mono
  tar -cJf ${DEST}.tar.xz -C ${DESTDIR} ${DESTSUB}
  popd
}

build_mono_libs
build_cross_compiler
