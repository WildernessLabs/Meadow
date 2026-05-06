#!/bin/bash
#
# setup-ci-host.sh — One-time setup for a Linux CI host to build Meadow.OS
#
# Run as root (or with sudo):
#   sudo ./setup-ci-host.sh
#
# Installs:
#   - arm-none-eabi-gcc 10.3 (ARM cross-compiler)
#   - cmake >= 3.26
#   - srecord (srec_cat)
#   - python3 + pip + kconfiglib + pyyaml
#   - GNU make, git, curl, ccache
#   - .NET SDK 9.0
#
set -e

MIN_CMAKE_VERSION="3.26"
MIN_ARM_GCC_VERSION="10.3"
ARM_GCC_VERSION="10.3-2021.10"
ARM_GCC_URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-${ARM_GCC_VERSION}-x86_64-linux.tar.bz2"
ARM_GCC_INSTALL_DIR="/opt/arm-gcc"
CMAKE_VERSION="3.30.5"

# ── Must be root ──

if [ "$(id -u)" -ne 0 ]; then
  echo "ERROR: Run this script as root (sudo ./setup-ci-host.sh)"
  exit 1
fi

echo "=== Meadow.OS CI Host Setup ==="
echo ""

# ── System packages ──

echo "--- Installing system packages ---"
apt-get update -qq
apt-get install -y --no-install-recommends \
  build-essential \
  git \
  curl \
  wget \
  tar \
  bzip2 \
  python3 \
  python3-pip \
  srecord \
  ccache \
  autoconf \
  automake \
  libtool \
  pkg-config \
  libssl-dev \
  zlib1g-dev

echo ""

# ── ARM cross-compiler ──

echo "--- ARM cross-compiler (arm-none-eabi-gcc) ---"
if command -v arm-none-eabi-gcc &>/dev/null; then
  CURRENT=$(arm-none-eabi-gcc --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)
  echo "Found: $CURRENT"
else
  CURRENT=""
  echo "Not found"
fi

NEED_ARM_GCC=false
if [ -z "$CURRENT" ]; then
  NEED_ARM_GCC=true
elif printf '%s\n%s\n' "$MIN_ARM_GCC_VERSION" "$CURRENT" | sort -V -C; then
  echo "Version $CURRENT >= $MIN_ARM_GCC_VERSION (ok)"
else
  NEED_ARM_GCC=true
fi

if $NEED_ARM_GCC; then
  echo "Installing ARM GCC $ARM_GCC_VERSION from ARM developer site..."
  mkdir -p "$ARM_GCC_INSTALL_DIR"
  curl -sSL "$ARM_GCC_URL" | tar xj -C "$ARM_GCC_INSTALL_DIR" --strip-components=1

  # Symlink into /usr/local/bin so it's on PATH
  for bin in "$ARM_GCC_INSTALL_DIR"/bin/arm-none-eabi-*; do
    ln -sf "$bin" /usr/local/bin/
  done

  echo "Installed: $(arm-none-eabi-gcc --version | head -1)"
fi

echo ""

# ── CMake >= 3.26 ──

echo "--- CMake ---"
CMAKE_CURRENT=$(cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)

NEED_CMAKE=false
if [ -z "$CMAKE_CURRENT" ]; then
  NEED_CMAKE=true
  echo "Not found"
elif printf '%s\n%s\n' "$MIN_CMAKE_VERSION" "$CMAKE_CURRENT" | sort -V -C; then
  echo "Found: $CMAKE_CURRENT (ok)"
else
  NEED_CMAKE=true
  echo "Found: $CMAKE_CURRENT (too old, need >= $MIN_CMAKE_VERSION)"
fi

if $NEED_CMAKE; then
  ARCH=$(uname -m)
  CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${ARCH}.tar.gz"
  echo "Installing CMake $CMAKE_VERSION..."
  curl -sSL "$CMAKE_URL" | tar xz -C /usr/local --strip-components=1
  echo "Installed: $(cmake --version | head -1)"
fi

echo ""

# ── Python packages ──

echo "--- Python packages ---"
pip3 install --break-system-packages kconfiglib pyyaml 2>/dev/null \
  || pip3 install kconfiglib pyyaml

echo ""

# ── .NET SDK ──

echo "--- .NET SDK ---"
if command -v dotnet &>/dev/null; then
  echo "Found: $(dotnet --version)"
else
  echo "Installing .NET SDK 9.0..."
  curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 9.0 --install-dir /usr/local/share/dotnet
  ln -sf /usr/local/share/dotnet/dotnet /usr/local/bin/dotnet
  echo "Installed: $(dotnet --version)"
fi

echo ""

# ── Verify ──

echo "=== Verification ==="
echo -n "arm-none-eabi-gcc: "; arm-none-eabi-gcc --version 2>/dev/null | head -1 || echo "MISSING"
echo -n "cmake:             "; cmake --version 2>/dev/null | head -1 || echo "MISSING"
echo -n "make:              "; make --version 2>/dev/null | head -1 || echo "MISSING"
echo -n "srec_cat:          "; command -v srec_cat &>/dev/null && echo "ok" || echo "MISSING"
echo -n "python3:           "; python3 --version 2>/dev/null || echo "MISSING"
echo -n "kconfiglib:        "; python3 -c "import kconfiglib; print('ok')" 2>/dev/null || echo "MISSING"
echo -n "pyyaml:            "; python3 -c "import yaml; print('ok')" 2>/dev/null || echo "MISSING"
echo -n "dotnet:            "; dotnet --version 2>/dev/null || echo "MISSING"
echo -n "git:               "; git --version 2>/dev/null || echo "MISSING"
echo -n "ccache:            "; ccache --version 2>/dev/null | head -1 || echo "MISSING"
echo ""
echo "=== Done ==="
