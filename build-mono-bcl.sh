#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Source common helpers if available (standalone-safe)
if [ -f "$scriptdir/scripts/common_methods.sh" ]; then
  . "$scriptdir/scripts/common_methods.sh"
  check_if_interactive
else
  red='' green='' reset=''
fi

VERBOSE=false
FORCE=false
CLEAN=false
KEEP_PDBS=false
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
  -k|--keeppdbs)
  KEEP_PDBS=true
  ;;
  *)
  echo "Unknown option $i"
  exit 1
  ;;
esac
done

if [ "$HELP" = true ]; then
  echo "Usage: build-mono-bcl.sh [options]"
  echo ""
  echo "Builds/packages the .NET 10 managed assemblies (SPCL + BCL)."
  echo "Requires ../runtime (dotnet/runtime fork) to be present and pre-built."
  echo ""
  echo "Outputs:"
  echo "  bcl/System.Private.CoreLib.dll   — System.Private.CoreLib (SPCL)"
  echo "  bcl/*.dll                        — Base Class Library assemblies"
  echo ""
  echo "Options:"
  echo "  -h|--help         Show this help message"
  echo "  -v|--verbose      Show verbose output"
  echo "  -f|--force        Force re-package even if bcl/ exists"
  echo "  -c|--clean        Clean bcl/ directory before packaging"
  echo "  -k|--keeppdbs     Keep PDB files in output"
  exit 0
fi

#
# Locate the runtime repo (sibling directory)
#
RUNTIME_DIR="$scriptdir/../runtime"

if [ ! -d "$RUNTIME_DIR" ]; then
  printf "${red}ERROR: ../runtime directory not found.${reset}\n"
  printf "The .NET 10 BCL build requires the dotnet/runtime fork as a sibling directory.\n"
  printf "Clone it with: git clone <runtime-repo-url> ../runtime\n"
  exit 1
fi

RUNTIME_DIR="$(cd "$RUNTIME_DIR" && pwd)"

#
# Locate SPCL (System.Private.CoreLib)
#
SPCL_DIR="$RUNTIME_DIR/artifacts/bin/mono/linux.arm.Release"
SPCL_DLL="$SPCL_DIR/System.Private.CoreLib.dll"

if [ ! -f "$SPCL_DLL" ]; then
  printf "${red}ERROR: System.Private.CoreLib.dll not found at:${reset}\n"
  printf "  $SPCL_DLL\n"
  printf "Build it with: cd ../runtime && ./build.sh mono -os linux -arch arm -c Release\n"
  exit 1
fi

#
# Locate BCL framework assemblies
# Auto-detect the version directory under the SDK shared framework
#
BCL_SHARED="$RUNTIME_DIR/.dotnet/shared/Microsoft.NETCore.App"

if [ ! -d "$BCL_SHARED" ]; then
  printf "${red}ERROR: BCL shared framework not found at:${reset}\n"
  printf "  $BCL_SHARED\n"
  printf "Build the runtime SDK first.\n"
  exit 1
fi

# Pick the newest version directory
BCL_VERSION=$(ls -1 "$BCL_SHARED" | sort -V | tail -1)
BCL_DIR="$BCL_SHARED/$BCL_VERSION"

if [ ! -d "$BCL_DIR" ]; then
  printf "${red}ERROR: No BCL version directory found under $BCL_SHARED${reset}\n"
  exit 1
fi

printf "SPCL source:  $SPCL_DLL\n"
printf "BCL source:   $BCL_DIR (v$BCL_VERSION)\n"

#
# Output directory
#
OUTPUT_DIR="$scriptdir/bcl"

if $CLEAN && [ -d "$OUTPUT_DIR" ]; then
  printf "Cleaning BCL output directory...\n"
  rm -rf "$OUTPUT_DIR"
fi

#
# Package assemblies
#
if [ -d "$OUTPUT_DIR" ] && ! $FORCE && ! $CLEAN; then
  printf "BCL already packaged in $OUTPUT_DIR (use --force to re-package)\n"
else
  printf "Packaging .NET 10 assemblies...\n"
  mkdir -p "$OUTPUT_DIR"

  # Copy SPCL
  cp "$SPCL_DLL" "$OUTPUT_DIR/"
  if [ -f "$SPCL_DIR/System.Private.CoreLib.pdb" ] && $KEEP_PDBS; then
    cp "$SPCL_DIR/System.Private.CoreLib.pdb" "$OUTPUT_DIR/"
  fi

  # Copy BCL framework assemblies (DLLs only, skip native/host binaries).
  # Skip System.Private.CoreLib.dll — already placed from Mono SPCL above.
  #
  # The device is NuttX, a POSIX OS -- NOT osx. But $BCL_DIR ($.dotnet/shared)
  # is the OSX host runtime pack: its OS-specific assemblies (System.Net.Sockets,
  # System.Net.Security, ...) are net11.0-OSX flavor, and every assembly is an
  # R2R (crossgen) image bloated with osx-arm64 native code that Mono ignores.
  # So instead of copying $BCL_DIR verbatim, source each assembly from the
  # freshly-built pure-IL library outputs under artifacts/bin, preferring the
  # POSIX flavor: net11.0-unix > net11.0-linux > net11.0 (agnostic). Fall back
  # to the shared pack only if a library isn't in artifacts.
  LIBS_BIN="$RUNTIME_DIR/artifacts/bin"
  for dll in "$BCL_DIR"/*.dll; do
    [ -f "$dll" ] || continue
    name=$(basename "$dll" .dll)
    [ "$name" = "System.Private.CoreLib" ] && continue
    src="$dll"
    for flavor in net11.0-unix net11.0-linux net11.0; do
      cand="$LIBS_BIN/$name/Release/$flavor/$name.dll"
      if [ -f "$cand" ]; then src="$cand"; break; fi
    done
    cp "$src" "$OUTPUT_DIR/"
  done

  # Copy PDBs if requested
  if $KEEP_PDBS; then
    for pdb in "$BCL_DIR"/*.pdb; do
      [ -f "$pdb" ] || continue
      cp "$pdb" "$OUTPUT_DIR/"
    done
  fi

  # Remove assemblies that are never needed on embedded NuttX
  # (These are host-only or platform-specific assemblies that waste flash space)
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
fi

#
# Summary
#
DLL_COUNT=$(ls -1 "$OUTPUT_DIR"/*.dll 2>/dev/null | wc -l | tr -d ' ')
TOTAL_SIZE=$(du -sh "$OUTPUT_DIR" 2>/dev/null | awk '{print $1}')
printf "Packaged $DLL_COUNT assemblies ($TOTAL_SIZE) in $OUTPUT_DIR\n"

exit 0
