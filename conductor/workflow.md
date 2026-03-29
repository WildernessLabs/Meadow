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

## Running the Test Suite in Renode

The Mono mini test suite (735 tests, 10 suites) validates runtime correctness. It runs as a
standalone Exe deployed as `Meadow.dll` (the firmware's entry assembly). This bypasses the
Meadow framework lifecycle, which blocks in the emulator waiting for ESP32 coprocessor responses.

### Automated (recommended)

```bash
cd Meadow.OS.Emulator/
./run-tests.sh              # Full: firmware + test runner + LFS + Renode
./run-tests.sh --lfs-only   # Skip firmware rebuild (faster iteration)
./run-tests.sh --run-only   # Just launch Renode with existing image
```

The script handles: building the test runner, deploying it as Meadow.dll, rebuilding LFS,
checking/updating the `.mono_bss` address, launching Renode, and waiting for results.

### Manual Steps

```bash
# 1. Build test runner
cd Meadow/conductor/tracks/mono-upgrade-09-tests/test-runner-app
dotnet build TestRunnerApp.csproj -c Release

# 2. Deploy as Meadow.dll (the firmware entry assembly)
cp bin/Release/net9.0/App.dll ../../Meadow.OS.Emulator/build/dotnet10/assemblies/Meadow.dll

# 3. Rebuild LFS image
cd Meadow.OS.Emulator
tools/build_lfs_v1_image build/dotnet10/assemblies build/dotnet10/littlefs.bin build/dotnet10/assemblies

# 4. (If firmware was rebuilt) Check .mono_bss address
arm-none-eabi-nm build/dotnet10/nuttx_user.elf | grep mono_bss
# Compare with address in scripts/meadow-dotnet10-headless.resc — update if changed
# Regenerate zero bin: python3 -c "import sys; sys.stdout.buffer.write(b'\x00' * SIZE)" > build/dotnet10/mono_bss_zero.bin

# 5. Launch Renode
pkill -f renode 2>/dev/null
: > /tmp/renode-uart-dotnet10.txt
/Users/lexas/renode/renode --port 9999 --disable-xwt \
    -e "include @scripts/meadow-dotnet10-headless.resc" > /tmp/renode-console.log 2>&1 &

# 6. Wait ~2-3 min, check results
tail -f /tmp/renode-uart-dotnet10.txt
# Look for: "=== TOTAL: 735 ran, 0 skipped, N failed ==="
```

### Emulator Gotchas

1. **mono_bss address drift**: Every firmware rebuild can change the `.mono_bss` section address.
   The `scripts/meadow-dotnet10-headless.resc` has a hardcoded `sysbus LoadBinary` address for
   `mono_bss_zero.bin`. If it doesn't match, Mono globals won't be zeroed and the runtime will
   crash or behave erratically. `run-tests.sh` handles this automatically.

2. **LFS format**: MUST use `tools/build_lfs_v1_image` (C tool, LFS v1.7.2). NEVER use the Python
   `build_littlefs_image.py` — it creates LFS v2 which NuttX silently formats over, losing all files.

3. **Meadow framework deadlock**: `App<F7FeatherV2>` blocks in `Monitor.Wait()` during platform init,
   waiting for ESP32 coprocessor responses the emulator stubs don't fully provide. Workaround: deploy
   test code as a standalone Exe renamed to `Meadow.dll`. The Meadow framework + Blinky work on real
   hardware (validated Track 08).

4. **Renode port conflicts**: Use a unique `--port` each run. Old Renode processes may linger — always
   `pkill -f renode` before starting a new session.

5. **SyslogWrite for output**: Managed `Console.WriteLine` goes to HCOM FIFOs which may not be drained.
   Use `[DllImport("libSystem.Native", EntryPoint = "SystemNative_SyslogWrite")]` for reliable output
   to `/tmp/renode-uart-dotnet10.txt` (USART1 syslog).

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
