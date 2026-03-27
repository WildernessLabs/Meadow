# Track 07: Blinky on Interpreter — Handoff

## Status: Phase 4D Complete (Meadow.F7 DigitalOutputPort Blink)

### What Works

1. **Direct GPIO blink via P/Invoke** (Phase 2) — `stm32_configgpio()` + `stm32_gpiowrite()` through `DllImport("nuttx")`. PA2 toggles.

2. **Meadow.F7 DigitalOutputPort blink** (Phase 4D) — SimplifiedBlinky.dll creates `F7FeatherV2` directly, creates `DigitalOutputPort(OnboardLedRed)`, toggles 10× at 500ms. Full managed stack: .NET 10 Mono → Meadow.F7 → UPD emulation → GPIO registers. Exit code 42.

3. **UPD driver emulation** — `open("/dev/upd")` returns fake fd 999 via shim in `mono_main.c`. Ioctl handlers for GetSetConfig (returns F7FeatherV2 platform), SetRegister, GetRegister, UpdateRegister, GetLastError, RegisterGpioIrq.

4. **Thread.Sleep** — Uses `LowLevelMonitor_TimedWait` path. Works correctly for 500ms intervals.

5. **Console.WriteLine via HCOM** — Output to TCP:4242 works reliably.

6. **APP_CONTEXT_BASE_DIRECTORY** — Set to `/meadow0/` in monovm properties.

7. **Incremental build** — `--clean` flag for full rebuild.

### What Doesn't Work Yet

1. **Full MeadowOS.Main() → App<F7FeatherV2> path** — `Assembly.GetTypes()` on assemblies containing types that reference the Meadow.F7 type hierarchy throws `NullReferenceException` in the Mono interpreter's type loader. This blocks `FindAppType()` which scans for `IApp` implementations. Root cause is likely a Mono interpreter bug with complex generic type resolution (`App<F7FeatherV2>` where `F7FeatherV2 : F7FeatherBase : F7MicroBase`).

2. **UPD driver in emulator** — **Root cause found**: The UPD driver (`meadow-upd.c`) only defines `.open`/`.close`/`.ioctl` in `file_operations` — no `.read` or `.write`. NuttX VFS `inode_checkflags()` returns EACCES when opening with `O_RDONLY`(1) or `O_RDWR`(3) because the driver lacks those handlers. Meadow.Core opens with `DriverFlags.DontCare=0` which passes because NuttX `O_RDONLY=1` (not POSIX 0), so flags=0 means "no access mode requested". The diagnostic C test was using `O_RDWR` which caused the EACCES. The emulation shim is still useful because real UPD ioctl handlers talk to STM32 hardware not present in Renode.

3. **GPIO register read/write in emulator** — UPD register operations (SetRegister/GetRegister) are no-ops in the emulation. Direct GPIO via `stm32_configgpio`/`stm32_gpiowrite` syscalls works instead.

### Key Fixes This Session

| Fix | File | Description |
|-----|------|-------------|
| UPD EACCES root cause | `fs_open.c`, `meadow-upd.c` | UPD driver has no .read/.write in file_operations. NuttX O_RDONLY=1 (not 0), so flags=0 (DontCare) passes inode_checkflags(). Shim removed — emulation belongs in Renode peripherals. |
| Assembly name conflict | `DiagWrapper.csproj` | Changed to `MeadowDiag` to avoid "Meadow" identity conflict with MeadowCore |
| --root flag | `mono_main.c` | Firmware passes `--root /meadow0` to monovm_execute_assembly so MeadowOS.FindAppType searches for App.dll on disk |
| SimplifiedBlinky entry | `mono_main.c` | Firmware checks for `/meadow0/SimplifiedBlinky.dll` before falling back to `Meadow.dll` |

### Assembly Layout

```
/meadow0/
├── SimplifiedBlinky.dll    (entry assembly, identity "SimplifiedBlinky")
├── Meadow.dll              (MeadowCore, identity "Meadow")
├── Meadow.Contracts.dll
├── Meadow.F7.dll
├── Meadow.Logging.dll
├── Meadow.Units.dll
├── MicroJson.dll
├── App.dll                 (BlinkyCS, for future MeadowOS.Main path)
├── System.Private.CoreLib.dll
├── System.Console.dll
├── ... (58 framework assemblies total)
└── meadow.config.yaml     (MonoControl: --interp)
```

### Important Gotchas

1. **mappings-meadow.h comment block** — Lines 119+ are `/* ... */` commented. Only ~70 entries active. New mappings BEFORE line 119.

2. **Stale LFS images** — After changing any DLL, MUST rebuild with `tools/build_lfs_v1_image`.

3. **Assembly naming** — Entry assembly filename is always `/meadow0/Meadow.dll` (hardcoded). If entry != MeadowCore, use SimplifiedBlinky.dll fallback mechanism.

4. **Incremental build failures** — After changing `mono_main.c` or headers, incremental often fails with `Error 2`. Use `--clean`.

5. **Syslog truncation** — After `LowLevelMonitor_Wait`, syslog stops. Use HCOM (TCP:4242) for managed output.

6. **UPD open flags** — UPD driver has no `.read`/`.write` handlers. Must open with flags=0 (Meadow.Core's `DriverFlags.DontCare`). Opening with O_RDONLY(1) or O_RDWR(3) returns EACCES.

### Next Steps

1. **Debug MeadowOS.Main GetTypes() NullRef** — The Mono interpreter crashes during type resolution of complex generic hierarchies. May need runtime-level debugging (GDB on Mono internals).

2. **Phase 3: Library retarget** — Meadow.Contracts/Core/F7 are already built for net9.0. Verify all work correctly.

3. **Phase 5: Documentation** — Document System.Native coverage, assembly list, SDRAM budget.

### Test Commands

```bash
# Build firmware (clean — recommended after header changes)
cd Meadow.OS.Emulator && bash build-meadow.os-emulated.sh --clean

# Build SimplifiedBlinky
cd conductor/tracks/mono-upgrade-07-blinky/simplified-blinky
dotnet build SimplifiedBlinky.csproj -c Release

# Deploy assemblies
cp bin/Release/net9.0/SimplifiedBlinky.dll ../../Meadow.OS.Emulator/build/dotnet10/assemblies/
# Ensure Meadow.dll = MeadowCore (NOT the entry app)
cp Meadow.Core/source/Meadow.Core/obj/Release/net9.0/Meadow.dll Meadow.OS.Emulator/build/dotnet10/assemblies/

# Build LFS
cd Meadow.OS.Emulator
tools/build_lfs_v1_image build/dotnet10/assemblies build/dotnet10/littlefs.bin build/dotnet10/assemblies

# Run in Renode
/Users/lexas/renode/renode --port 9999 --disable-xwt \
  -e "path set '/Users/lexas/Meadow.OS.Emulator'; include @scripts/meadow-dotnet10-headless.resc; start"

# Check output
tail -f /tmp/renode-uart-dotnet10.txt    # syslog
nc localhost 4242                         # HCOM (managed Console.WriteLine)
```
