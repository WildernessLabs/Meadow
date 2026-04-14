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
# Locate the runtime repo (sibling directory)
#
RUNTIME_DIR="$scriptdir/../runtime"

if [ ! -d "$RUNTIME_DIR" ]; then
  printf "${red}ERROR: ../runtime directory not found.${reset}\n"
  printf "Clone the dotnet/runtime fork as a sibling directory.\n"
  exit 1
fi

RUNTIME_DIR="$(cd "$RUNTIME_DIR" && pwd)"

#
# Verify .NET SDK is provisioned
#
DOTNET="$RUNTIME_DIR/.dotnet/dotnet"
if [ ! -x "$DOTNET" ]; then
  printf "${red}ERROR: .NET SDK not found at $DOTNET${reset}\n"
  printf "Provision it with: curl -sSL https://builds.dotnet.microsoft.com/dotnet/scripts/v1/dotnet-install.sh | bash -s -- --install-dir $RUNTIME_DIR/.dotnet\n"
  exit 1
fi

#
# Output directory
#
OUTPUT_DIR="$scriptdir/artifacts/meadow_assemblies"

if $CLEAN && [ -d "$OUTPUT_DIR" ]; then
  printf "Cleaning output directory...\n"
  rm -rf "$OUTPUT_DIR"
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

# NuttX-specific: FeaturePerfTracing=false (native has DISABLE_EVENTPIPE)
export DOTNET_ROOT="$RUNTIME_DIR/.dotnet"

"$DOTNET" build "$CORELIB_PROJ" \
  -c "$BUILD_CONFIG" \
  -p:TargetArchitecture=arm \
  -p:TargetOS=linux \
  -p:FeaturePerfTracing=false \
  -p:RuntimeFlavor=Mono

if [ ! -f "$SPCL_DLL" ]; then
  printf "${red}ERROR: CoreLib build produced no output at $SPCL_DLL${reset}\n"
  exit 1
fi

SPCL_SIZE=$(ls -lh "$SPCL_DLL" | awk '{print $5}')
printf "CoreLib: $SPCL_SIZE\n"

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
