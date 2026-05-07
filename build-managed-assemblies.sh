#!/bin/bash
#
# Build and package the .NET 10 managed assemblies for Meadow NuttX.
#
# Step 1: Build System.Private.CoreLib (Mono, ARM, FeaturePerfTracing=false)
# Step 2: Package CoreLib + SDK framework assemblies into output directory
#
# Requires: ../runtime (dotnet/runtime fork) with .NET SDK provisioned at .dotnet/
#
# Output: artifacts/meadow_assemblies/*.dll
#
set -euo pipefail

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Source common helpers if available (standalone-safe)
if [ -f "$scriptdir/scripts/common_methods.sh" ]; then
  . "$scriptdir/scripts/common_methods.sh"
  check_if_interactive
else
  red='' green='' reset=''
fi

FORCE=false
CLEAN=false
KEEP_PDBS=false
HELP=false
BUILD_CONFIG="Release"

for i in "$@"
do
case $i in
  -h|--help)
  HELP=true
  ;;
  -f|--force)
  FORCE=true
  ;;
  -c|--clean)
  CLEAN=true
  ;;
  -k|--keeppdbs)
  KEEP_PDBS=true
  ;;
  -d|--debug)
  BUILD_CONFIG="Debug"
  ;;
  *)
  echo "Unknown option $i"
  exit 1
  ;;
esac
done

if [ "$HELP" = true ]; then
  echo "Usage: build-managed-assemblies.sh [options]"
  echo ""
  echo "Builds System.Private.CoreLib for NuttX and packages it with the"
  echo "framework assemblies into artifacts/meadow_assemblies/."
  echo ""
  echo "Requires ../runtime (dotnet/runtime fork) to be present with the"
  echo ".NET SDK provisioned at ../runtime/.dotnet/."
  echo ""
  echo "Options:"
  echo "  -h|--help      Show this help message"
  echo "  -f|--force     Force rebuild even if output exists"
  echo "  -c|--clean     Clean output directory before building"
  echo "  -d|--debug     Build Debug configuration (default: Release)"
  echo "  -k|--keeppdbs  Keep PDB files in output"
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
    git -C "$RUNTIME_DIR" checkout -f -B "$TARGET_BRANCH" "origin/$TARGET_BRANCH"
  fi

  printf "Runtime HEAD: %s\n" "$(git -C "$RUNTIME_DIR" log -1 --pretty=format:'%h %s')"
fi

# SDK provisioning is delegated to runtime's own bootstrap (eng/common/tools.sh
# via runtime/build.sh). Doing it manually with dotnet-install.sh produced an
# incomplete state — SDK was installed but targeting packs / NuGet packages
# weren't, leading to CS0234 "type does not exist" errors at compile time.
DOTNET="$RUNTIME_DIR/.dotnet/dotnet"

#
# Output directory
#
OUTPUT_DIR="$scriptdir/artifacts/meadow_assemblies"

if $CLEAN; then
  if [ -d "$OUTPUT_DIR" ]; then
    printf "Cleaning output directory...\n"
    rm -rf "$OUTPUT_DIR"
  fi
  # Wipe runtime obj/ caches too — stale package paths from a previous
  # SDK install can survive `dotnet restore` and produce confusing
  # CS0234 "type does not exist" errors on CI agents.
  if [ -d "$RUNTIME_DIR/artifacts/obj" ]; then
    printf "Cleaning runtime obj/ cache...\n"
    rm -rf "$RUNTIME_DIR/artifacts/obj"
  fi
fi

if [ -d "$OUTPUT_DIR" ] && ! $FORCE && ! $CLEAN; then
  DLL_COUNT=$(ls -1 "$OUTPUT_DIR"/*.dll 2>/dev/null | wc -l | tr -d ' ')
  TOTAL_SIZE=$(du -sh "$OUTPUT_DIR" 2>/dev/null | awk '{print $1}')
  printf "Already packaged ($DLL_COUNT assemblies, $TOTAL_SIZE) in $OUTPUT_DIR\n"
  printf "Use --force to rebuild or --clean to start fresh.\n"
  exit 0
fi

# ──────────────────────────────────────────────────────────────────────
# Step 1: Build System.Private.CoreLib
# ──────────────────────────────────────────────────────────────────────

CORELIB_PROJ="$RUNTIME_DIR/src/mono/System.Private.CoreLib/System.Private.CoreLib.csproj"
SPCL_DIR="$RUNTIME_DIR/artifacts/bin/mono/linux.arm.$BUILD_CONFIG"
SPCL_DLL="$SPCL_DIR/System.Private.CoreLib.dll"

printf "=== Step 1: Build System.Private.CoreLib ($BUILD_CONFIG) ===\n"

# eng/common/tools.sh checks $HOME (NuGet needs it) under `set -u`, so an
# unset HOME on CI agents (root with no env) crashes the script before it
# can fall back. Set a sensible default.
if [ -z "${HOME:-}" ]; then
  export HOME="$RUNTIME_DIR/artifacts/.home"
  mkdir -p "$HOME"
fi

# Delegate to runtime's own build script. eng/common/tools.sh installs the
# correct SDK pinned by global.json, restores all NuGet packages from
# runtime/NuGet.config (including the netstandard targeting packs the
# managed CoreLib build depends on), and Subsets.props 'Mono.CoreLib'
# resolves to the same csproj we want to build.
#
# NuttX-specific tweak: FeaturePerfTracing=false because the native side
# is built with DISABLE_EVENTPIPE.
(cd "$RUNTIME_DIR" && ./build.sh \
  -subset mono.corelib \
  -arch arm \
  -os linux \
  -c "$BUILD_CONFIG" \
  /p:FeaturePerfTracing=false)

if [ ! -f "$SPCL_DLL" ]; then
  printf "${red}ERROR: CoreLib build produced no output at $SPCL_DLL${reset}\n"
  exit 1
fi

SPCL_SIZE=$(ls -lh "$SPCL_DLL" | awk '{print $5}')
printf "CoreLib: $SPCL_SIZE\n"

# After runtime/build.sh, runtime/.dotnet/dotnet is fully provisioned.
# Use it for our subsequent per-library builds so they share the same
# SDK + restored package state.
export DOTNET_ROOT="$RUNTIME_DIR/.dotnet"
export DOTNET_INSTALL_DIR="$RUNTIME_DIR/.dotnet"
export DOTNET_MULTILEVEL_LOOKUP=0
export DOTNET_NOLOGO=1
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export PATH="$DOTNET_ROOT:$PATH"

# ──────────────────────────────────────────────────────────────────────
# Step 2: Package assemblies
# ──────────────────────────────────────────────────────────────────────

# Locate SDK framework assemblies (auto-detect version)
BCL_SHARED="$RUNTIME_DIR/.dotnet/shared/Microsoft.NETCore.App"

if [ ! -d "$BCL_SHARED" ]; then
  printf "${red}ERROR: SDK shared framework not found at $BCL_SHARED${reset}\n"
  exit 1
fi

BCL_VERSION=$(ls -1 "$BCL_SHARED" | sort -V | tail -1)
BCL_DIR="$BCL_SHARED/$BCL_VERSION"

printf "\n=== Step 2: Package assemblies ===\n"
printf "CoreLib:    $SPCL_DLL\n"
printf "Framework:  $BCL_DIR (v$BCL_VERSION)\n"
printf "Output:     $OUTPUT_DIR\n"

mkdir -p "$OUTPUT_DIR"

# Copy CoreLib (from Mono build — this is the only runtime-specific assembly)
cp "$SPCL_DLL" "$OUTPUT_DIR/"
if [ -f "$SPCL_DIR/System.Private.CoreLib.pdb" ] && $KEEP_PDBS; then
  cp "$SPCL_DIR/System.Private.CoreLib.pdb" "$OUTPUT_DIR/"
fi

# Copy framework assemblies from SDK (mostly platform-independent managed IL).
# Some assemblies are platform-specific (networking, crypto) and will be
# rebuilt from source for Linux below.
# Skip System.Private.CoreLib.dll — already placed from Mono build above
for dll in "$BCL_DIR"/*.dll; do
  [ -f "$dll" ] || continue
  [ "$(basename "$dll")" = "System.Private.CoreLib.dll" ] && continue
  cp "$dll" "$OUTPUT_DIR/"
done

if $KEEP_PDBS; then
  for pdb in "$BCL_DIR"/*.pdb; do
    [ -f "$pdb" ] || continue
    cp "$pdb" "$OUTPUT_DIR/"
  done
fi

# ──────────────────────────────────────────────────────────────────────
# Step 2b: Build platform-specific assemblies for Linux
# ──────────────────────────────────────────────────────────────────────
# The SDK shared framework is built for the host OS (macOS on this machine).
# Several networking/crypto assemblies compile different source files per
# platform (e.g. SslStreamPal.OSX.cs vs SslStreamPal.Unix.cs). We must
# build these from source targeting Linux so they use the OpenSSL P/Invoke
# paths that our mbedTLS PAL implements.

printf "\n=== Step 2b: Build platform-specific assemblies for Linux ===\n"

# Assemblies that need Linux builds, with their target framework moniker
LINUX_BUILDS=(
  "System.Security.Cryptography:net11.0-unix"
  "System.Net.Security:net11.0-linux"
  "System.Net.Sockets:net11.0-unix"
  "System.Net.Primitives:net11.0-unix"
  "System.Net.NameResolution:net11.0-unix"
  "System.Net.NetworkInformation:net11.0-linux"
  "System.Net.Http:net11.0-linux"
)

for entry in "${LINUX_BUILDS[@]}"; do
  name="${entry%%:*}"
  tfm="${entry##*:}"
  proj="$RUNTIME_DIR/src/libraries/$name/src/$name.csproj"
  built="$RUNTIME_DIR/artifacts/bin/$name/Release/$tfm/$name.dll"

  if [ ! -f "$proj" ]; then
    printf "${red}WARN: $proj not found, skipping${reset}\n"
    continue
  fi

  printf "  Building $name ($tfm)...\n"
  "$DOTNET" build "$proj" \
    -c Release \
    -f "$tfm" \
    -p:TargetArchitecture=arm \
    --nologo -v quiet 2>&1 | grep -E "^(Build|error)" || true

  if [ -f "$built" ]; then
    old_size=$(wc -c < "$OUTPUT_DIR/$name.dll" 2>/dev/null || echo 0)
    new_size=$(wc -c < "$built")
    cp "$built" "$OUTPUT_DIR/"
    printf "  $name: replaced (${old_size} → ${new_size} bytes)\n"
  else
    printf "${red}  ERROR: $name build produced no output at $built${reset}\n"
  fi
done

# Remove assemblies that are never needed on embedded NuttX
BLACKLIST=(
  "System.Net.Quic.dll"
  "System.Runtime.Caching.dll"
  "System.CodeDom.dll"
  "System.Diagnostics.EventLog.dll"
  "System.DirectoryServices.dll"
  "System.DirectoryServices.Protocols.dll"
  "System.Management.dll"
  "System.ServiceProcess.ServiceController.dll"
  "System.Windows.Extensions.dll"
)

for bl in "${BLACKLIST[@]}"; do
  rm -f "$OUTPUT_DIR/$bl"
done

# ──────────────────────────────────────────────────────────────────────
# Summary
# ──────────────────────────────────────────────────────────────────────

DLL_COUNT=$(ls -1 "$OUTPUT_DIR"/*.dll 2>/dev/null | wc -l | tr -d ' ')
TOTAL_SIZE=$(du -sh "$OUTPUT_DIR" 2>/dev/null | awk '{print $1}')
printf "\n=== Done: $DLL_COUNT assemblies ($TOTAL_SIZE) in $OUTPUT_DIR ===\n"

exit 0
