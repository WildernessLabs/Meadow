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
  echo "  -d|--debug     Build Debug configuration (default: Release)"
  exit 0
fi

#
# Locate (or clone) the runtime repo as a sibling directory, then sync it
# to the branch matching this Meadow branch (or main if no match). Set
# MEADOW_RUNTIME_NO_SYNC=1 to skip the sync (e.g. local dev with a custom
# runtime branch).
#
RUNTIME_REPO="WildernessLabs/runtime"
RUNTIME_DIR="$scriptdir/../runtime"

if [ ! -d "$RUNTIME_DIR" ]; then
  printf "Cloning %s into %s\n" "$RUNTIME_REPO" "$RUNTIME_DIR"
  if [ -n "${GITHUB_PERSONAL_ACCESS_TOKEN:-}" ]; then
    RUNTIME_URL="https://${GITHUB_PERSONAL_ACCESS_TOKEN}@github.com/${RUNTIME_REPO}.git"
  else
    RUNTIME_URL="https://github.com/${RUNTIME_REPO}.git"
  fi
  git clone "$RUNTIME_URL" "$RUNTIME_DIR" || {
    printf "${red}ERROR: Failed to clone runtime repo.${reset}\n"
    exit 1
  }
fi

RUNTIME_DIR="$(cd "$RUNTIME_DIR" && pwd)"

# Determine Meadow branch — Azure Pipelines checks out in detached HEAD,
# so prefer BUILD_SOURCEBRANCHNAME when set.
if [ -n "${BUILD_SOURCEBRANCHNAME:-}" ] && [ "$BUILD_SOURCEBRANCHNAME" != "HEAD" ]; then
  MEADOW_BRANCH="$BUILD_SOURCEBRANCHNAME"
else
  MEADOW_BRANCH=$(git -C "$scriptdir" rev-parse --abbrev-ref HEAD 2>/dev/null || echo HEAD)
fi

if [ "${MEADOW_RUNTIME_NO_SYNC:-}" = "1" ]; then
  printf "Skipping runtime branch sync (MEADOW_RUNTIME_NO_SYNC=1)\n"
else
  # Always fetch latest from origin, even if we can't determine a branch —
  # ensures cached agents pick up new commits. Uses --force in case branch
  # tips were rewritten upstream.
  printf "Fetching latest from runtime origin\n"
  git -C "$RUNTIME_DIR" fetch --prune --force origin

  if [ "$MEADOW_BRANCH" = "HEAD" ] || [ -z "$MEADOW_BRANCH" ]; then
    printf "No Meadow branch detected, leaving runtime on its current ref\n"
  else
    if git -C "$RUNTIME_DIR" rev-parse --verify "origin/$MEADOW_BRANCH" &>/dev/null; then
      TARGET_BRANCH="$MEADOW_BRANCH"
    else
      printf "Runtime branch '%s' not found upstream, falling back to main\n" "$MEADOW_BRANCH"
      TARGET_BRANCH="main"
    fi
    printf "Syncing runtime to origin/%s\n" "$TARGET_BRANCH"
    # -f forces working tree reset; -B (re)creates the local branch.
    git -C "$RUNTIME_DIR" checkout -f -B "$TARGET_BRANCH" "origin/$TARGET_BRANCH"
  fi

  printf "Runtime HEAD: %s\n" "$(git -C "$RUNTIME_DIR" log -1 --pretty=format:'%h %s')"
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
if $DEBUG; then
  MONO_BUILD_TYPE="Debug"
else
  MONO_BUILD_TYPE="Release"
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
