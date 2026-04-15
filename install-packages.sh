#!/bin/bash
#
# install-packages.sh — Install/upgrade build dependencies for Meadow.OS
#
# Supports macOS (Homebrew) and Linux (apt + pip).
# Run once on a new host, or after upgrading the runtime.
# Safe to re-run — skips already-satisfied dependencies.
#
set -e

MIN_CMAKE_VERSION="3.26"
MIN_ARM_GCC_VERSION="10.3"

# ── Helpers ──

version_ge() {
  # Returns 0 (true) if $1 >= $2 using version comparison
  printf '%s\n%s\n' "$2" "$1" | sort -V -C
}

check_tool() {
  local tool="$1" min_ver="$2"
  local current
  current=$($tool --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)
  if [ -z "$current" ]; then
    echo "MISSING"
    return
  fi
  if [ -n "$min_ver" ] && ! version_ge "$current" "$min_ver"; then
    echo "OLD:$current"
    return
  fi
  echo "OK:$current"
}

# ── Detect OS ──

OS="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
  OS="macos"
elif [[ -f /etc/os-release ]]; then
  OS="linux"
fi

echo "=== Meadow.OS Build Dependencies ==="
echo "Platform: $OS"
echo ""

# ── CMake >= 3.26 (required by dotnet/runtime) ──

CMAKE_STATUS=$(check_tool cmake "$MIN_CMAKE_VERSION")
echo -n "cmake >= $MIN_CMAKE_VERSION: "
case "$CMAKE_STATUS" in
  OK:*)
    echo "${CMAKE_STATUS#OK:} (ok)"
    ;;
  OLD:*|MISSING)
    ver="${CMAKE_STATUS#OLD:}"
    [ "$CMAKE_STATUS" = "MISSING" ] && ver="not found"
    echo "$ver (upgrading...)"
    if [ "$OS" = "macos" ]; then
      brew install cmake || brew upgrade cmake
    else
      # pip3 cmake package provides modern cmake without needing root
      pip3 install --user --upgrade cmake 2>/dev/null || pip install --user --upgrade cmake 2>/dev/null || {
        echo "ERROR: Failed to install cmake via pip. Install cmake >= $MIN_CMAKE_VERSION manually."
        exit 1
      }
      export PATH="$HOME/.local/bin:$PATH"
    fi
    CMAKE_STATUS=$(check_tool cmake "$MIN_CMAKE_VERSION")
    echo "  -> now: ${CMAKE_STATUS#OK:}"
    ;;
esac

# ── arm-none-eabi-gcc ──

GCC_STATUS=$(check_tool arm-none-eabi-gcc "$MIN_ARM_GCC_VERSION")
echo -n "arm-none-eabi-gcc >= $MIN_ARM_GCC_VERSION: "
case "$GCC_STATUS" in
  OK:*)
    echo "${GCC_STATUS#OK:} (ok)"
    ;;
  *)
    ver="${GCC_STATUS#OLD:}"
    [ "$GCC_STATUS" = "MISSING" ] && ver="not found"
    echo "$ver (MANUAL INSTALL REQUIRED)"
    echo "  macOS:  brew install osx-cross/arm/arm-gcc-bin"
    echo "  Linux:  apt-get install gcc-arm-none-eabi"
    echo "  Or download from: https://developer.arm.com/downloads/-/gnu-rm"
    ;;
esac

# ── Python 3 + packages (kconfiglib, pyyaml) ──

echo -n "python3: "
PY_STATUS=$(check_tool python3 "")
if [ "$PY_STATUS" = "MISSING" ]; then
  echo "not found (MANUAL INSTALL REQUIRED)"
else
  echo "${PY_STATUS#OK:} (ok)"
  for pkg in kconfiglib pyyaml; do
    echo -n "  $pkg: "
    if python3 -c "import $pkg" 2>/dev/null; then
      echo "ok"
    else
      echo "installing..."
      pip3 install --user "$pkg" 2>/dev/null || pip install --user "$pkg" 2>/dev/null
    fi
  done
fi

# ── gmake ──

echo -n "make (GNU): "
MAKE_VER=$(make --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
if [ -n "$MAKE_VER" ]; then
  echo "$MAKE_VER (ok)"
else
  echo "not found"
fi

# ── dotnet SDK ──

echo -n "dotnet SDK: "
DOTNET_STATUS=$(check_tool dotnet "")
if [ "$DOTNET_STATUS" = "MISSING" ]; then
  echo "not found (MANUAL INSTALL REQUIRED)"
  echo "  https://dot.net/download"
else
  echo "${DOTNET_STATUS#OK:} (ok)"
fi

# ── srecord (srec_cat) ──

echo -n "srec_cat: "
if command -v srec_cat &>/dev/null; then
  echo "ok"
else
  echo "not found"
  if [ "$OS" = "macos" ]; then
    echo "  Installing via brew..."
    brew install srecord
  else
    echo "  Install: apt-get install srecord"
  fi
fi

# ── macOS-only extras ──

if [ "$OS" = "macos" ]; then
  for pkg in ccache autoconf libtool automake libusb dfu-util; do
    echo -n "$pkg: "
    if command -v "$pkg" &>/dev/null; then
      echo "ok"
    else
      echo "installing..."
      brew install "$pkg"
    fi
  done
fi

echo ""
echo "=== Done ==="
echo ""
echo "If cmake was installed via pip, ensure ~/.local/bin is on PATH:"
echo '  export PATH="$HOME/.local/bin:$PATH"'
