#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Source common helpers if available (standalone-safe)
if [ -f "$scriptdir/scripts/common_methods.sh" ]; then
  . "$scriptdir/scripts/common_methods.sh"
  check_if_interactive
else
  # Minimal fallback
  red='' green='' reset=''
  run_command() { $1; }
  check_command_status() {
    if [ $? -ne 0 ]; then printf " error\n"; exit 1; else printf " success\n"; fi
  }
fi

VERBOSE=true
FORCE=false
CLEAN=false
DEBUG=false
HELP=false

for i in "$@"
do
case $i in
    -h|--help)
    HELP=true
    ;;
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

if [ "$HELP" = true ]; then
  echo "Usage: build-mono.sh [options]"
  echo ""
  echo "Builds the .NET 10 Mono runtime for NuttX (ARM Cortex-M7)."
  echo "Requires ../runtime (dotnet/runtime fork) to be present."
  echo ""
  echo "Options:"
  echo "  -h|--help      Show this help message"
  echo "  -v|--verbose   Show verbose output"
  echo "  -f|--force     Force rebuild even if library exists"
  echo "  -c|--clean     Clean build directory before building"
  echo "  -d|--debug     Build Debug configuration (default: Debug)"
  exit 0
fi

#
# Locate the runtime repo (sibling directory)
#
RUNTIME_DIR="$scriptdir/../runtime"

if [ ! -d "$RUNTIME_DIR" ]; then
  printf "${red}ERROR: ../runtime directory not found.${reset}\n"
  printf "The .NET 10 Mono build requires the dotnet/runtime fork as a sibling directory.\n"
  printf "Clone it with: git clone <runtime-repo-url> ../runtime\n"
  exit 1
fi

RUNTIME_DIR="$(cd "$RUNTIME_DIR" && pwd)"

#
# Verify CMake >= 3.26 (required by dotnet/runtime)
#
CMAKE_MIN_MAJOR=3
CMAKE_MIN_MINOR=26
CMAKE_VER=$(cmake --version 2>/dev/null | head -1 | sed 's/[^0-9]*\([0-9]*\.[0-9]*\).*/\1/')
if [ -z "$CMAKE_VER" ]; then
  printf "${red}ERROR: cmake not found.${reset}\n"
  exit 1
fi
CMAKE_MAJOR=${CMAKE_VER%%.*}
CMAKE_MINOR=${CMAKE_VER##*.}
if [ "$CMAKE_MAJOR" -lt "$CMAKE_MIN_MAJOR" ] || \
   { [ "$CMAKE_MAJOR" -eq "$CMAKE_MIN_MAJOR" ] && [ "$CMAKE_MINOR" -lt "$CMAKE_MIN_MINOR" ]; }; then
  printf "CMake $CMAKE_VER found, but >= $CMAKE_MIN_MAJOR.$CMAKE_MIN_MINOR required.\n"
  printf "Attempting to install CMake via pip...\n"
  pip3 install --user cmake 2>/dev/null || pip install --user cmake 2>/dev/null
  # Add pip user bin to PATH
  export PATH="$HOME/.local/bin:$PATH"
  CMAKE_VER=$(cmake --version 2>/dev/null | head -1 | sed 's/[^0-9]*\([0-9]*\.[0-9]*\).*/\1/')
  printf "CMake now: $CMAKE_VER\n"
fi

#
# Verify NuttX headers are available (needed by CMake build)
#
if [ ! -f "$scriptdir/nuttx/include/nuttx/config.h" ]; then
  printf "${red}ERROR: NuttX includes not found.${reset}\n"
  printf "Run NuttX configure step first (build.sh --configure or make context).\n"
  exit 1
fi

#
# Build configuration
#
# Always use Debug — Make.defs hardcodes build-nuttx-debug path.
# For Release builds, update MONO_BUILD_DIR in apps/examples/mono/Make.defs too.
if $DEBUG; then
  MONO_BUILD_TYPE="Debug"
else
  MONO_BUILD_TYPE="Debug"
fi

BUILD_DIR="$RUNTIME_DIR/src/mono/build-nuttx-$(echo "$MONO_BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
MONO_LIB="$BUILD_DIR/mono/mini/libmonosgen-2.0.a"

#
# Clean if requested
#
if $CLEAN && [ -d "$BUILD_DIR" ]; then
  printf "Cleaning Mono build directory...\n"
  rm -rf "$BUILD_DIR"
fi

#
# Build .NET 10 Mono runtime via CMake
#
if [ ! -f "$MONO_LIB" ] || $FORCE || $CLEAN; then
  printf "Building .NET 10 Mono runtime...\n"
  printf "  Runtime repo:   $RUNTIME_DIR\n"
  printf "  Build type:     $MONO_BUILD_TYPE\n"
  printf "  Build dir:      $BUILD_DIR\n"

  export NUTTX_INCLUDE_DIR="$scriptdir/nuttx/include"

  if $VERBOSE; then
    "$RUNTIME_DIR/src/mono/build-nuttx.sh" "$MONO_BUILD_TYPE"
  else
    "$RUNTIME_DIR/src/mono/build-nuttx.sh" "$MONO_BUILD_TYPE" &>/dev/null
  fi

  if [ $? -ne 0 ]; then
    printf "${red}ERROR: Mono runtime build failed.${reset}\n"
    exit 1
  fi

  printf "Mono runtime build ${green}success${reset}\n"
else
  printf ".NET 10 Mono runtime already built (use --force to rebuild)\n"
  printf "  Library: $MONO_LIB\n"
fi

#
# Verify output
#
if [ ! -f "$MONO_LIB" ]; then
  printf "${red}ERROR: Expected library not found: $MONO_LIB${reset}\n"
  exit 1
fi

printf "Mono runtime library: $(ls -lh "$MONO_LIB" | awk '{print $5}')\n"

exit 0
