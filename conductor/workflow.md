# Development Workflow

## Track Execution Order
Tracks are executed sequentially as listed in `tracks.md`. Each track has phases that are also sequential.

## Build Environment Prerequisites

The emulator build script requires these tools. Install before building:

```bash
# ARM cross-compiler (already installed at /opt/homebrew/bin/ or /Applications/ARM/bin/)
arm-none-eabi-gcc --version   # expect 10.3.x

# GNU Make 4.x+ (macOS ships 3.81 which is too old)
brew install make              # installs as 'gmake'

# Python 3 with PyYAML and kconfiglib
pip3 install --break-system-packages pyyaml kconfiglib

# ld.lld (LLVM linker) — needed for user-space ELF linking
brew install lld               # installs at /opt/homebrew/bin/ld.lld

# .NET SDK (for compiling managed test assemblies)
dotnet --version               # 9.0+ works for compiling .NET 10-compatible IL
```

The build script adds `Meadow/toolchain/macos/` to PATH which provides an x86_64
`ld.lld` (LLD 11.0.0) that runs via Rosetta on Apple Silicon. The homebrew `lld`
(`/opt/homebrew/bin/ld.lld`, LLD 22.x) is a native arm64 binary and takes precedence
if installed.

### Renode Emulator

Renode 1.16.1 is used for emulation. It lives at `/Users/lexas/renode/` (built from source).
The `renode` command is NOT on PATH — invoke as `/Users/lexas/renode/renode`.

To run headless (background):
```bash
cd Meadow.OS.Emulator/
/Users/lexas/renode/renode --disable-gui --port <PORT> --plain \
    -e "i @scripts/meadow-dotnet10-headless.resc" > /tmp/renode-console.log 2>&1 &
```

Use `--port <PORT>` (not `--console`) to keep Renode running as a background process.
The `--console` mode exits when stdin closes. Pick a unique port each run to avoid
"Address already in use" errors from previous instances.

Syslog output goes to `/tmp/renode-uart-dotnet10.txt`. Renode rotates this file on
each start (appends `.N` suffix to the old one). Always check the un-suffixed file
for current output.

## Building Firmware

**ALWAYS use the emulator build script. NEVER run `make` directly in Meadow/nuttx/.**

```bash
cd Meadow.OS.Emulator/
bash build-meadow.os-emulated.sh
```

The build script handles: correct toolchain (ARM GCC 10.3, NOT homebrew v15), kconfig,
version stamps, multi-pass protected-mode build, artifact extraction, and hook generation.
Output goes to `build/dotnet10/`. See `build-meadow.os-emulated.sh` for details.

### After Rebuilding

The `.mono_bss` section address and size change with each build. After rebuilding:

```bash
arm-none-eabi-readelf -S build/dotnet10/nuttx_user.elf | grep mono_bss
# Example output: [26] .mono_bss  NOBITS  c0252c50 ... 0ad3b0
```

Then update:
1. `mono_bss_zero.bin`: `dd if=/dev/zero bs=1 count=<SIZE_DECIMAL> of=build/dotnet10/mono_bss_zero.bin`
2. `scripts/meadow-dotnet10-headless.resc`: update the address in the `sysbus LoadBinary` line

### Building the LFS Image

The LFS image contains managed assemblies deployed to `/meadow0/` on the device.
The firmware scans this directory (non-recursively) for .dll files at boot.

```bash
# Stage assemblies
mkdir -p build/dotnet10/assemblies/
cp <SPCL> <framework dlls> <app Meadow.dll> build/dotnet10/assemblies/

# Build image (assemblies go at root via app_dir argument)
mkdir -p /tmp/empty_bcl
tools/build_lfs_v1_image /tmp/empty_bcl build/dotnet10/littlefs.bin build/dotnet10/assemblies/
```

Framework assemblies are at: `runtime/.dotnet/shared/Microsoft.NETCore.App/11.0.0-preview.3.26161.119/`

## Debugging in Emulator

Use GDB with Renode for debugging. Start emulator with `machine StartGdbServer 3333`,
then connect: `arm-none-eabi-gdb -ex "target remote localhost:3333" build/dotnet10/nuttx_user.elf`
Use `nuttx.elf` for kernel frames, `nuttx_user.elf` for user/mono frames.

## Validation Strategy
1. **Build validation**: Code compiles without errors
2. **Emulator validation**: Runs correctly in Renode emulator
3. **Hardware validation**: Runs correctly on physical F7FeatherV2 / CCMv2 (manual step)

## Commit Strategy
- Use conventional commits: `feat(scope): description`, `fix(scope): description`
- Never include AI attribution in commits
- Never push without explicit approval
- Commit working milestones within each phase

## Phase Completion Protocol
1. All tasks in the phase are checked off
2. Emulator validation passes (where applicable)
3. User signs off before proceeding to next phase

## Deployment for Testing
- **Emulator**: Boot Renode with the headless script (see above)
- **App deploy (baked)**: Rebuild LFS image with assemblies, reboot emulator
- **App deploy (CLI)**: `meadow config route socket://localhost:4242` then `meadow app deploy`
- **Hardware**: Flash firmware via SWD, deploy app via USB with Meadow.CLI

## Key Branches
- Work on feature branches per track
- Merge to develop/main after track completion and validation
