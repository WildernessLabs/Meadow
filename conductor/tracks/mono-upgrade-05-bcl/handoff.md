# Track 05 Handoff: Interp-to-Native + Native-to-Interp Trampolines

## Status: COMPLETE

Track 05 implemented both directions of the interpreter trampoline bridge:
- **Interp-to-native (managed calling C):** 47 pre-compiled C wrappers covering all icall/P/Invoke signatures hit during runtime initialization. Table-driven dispatch in `aot-runtime-nuttx.c`.
- **Native-to-interp (C calling managed):** 128 ARM Thumb thunks in a pool, each saving registers into a CallContext and calling `interp_entry_from_trampoline`. Used for delegate callbacks, virtual dispatch, thread entry.

## What Works

- `monovm_initialize` completes successfully
- `monovm_execute_assembly` starts executing a real .NET assembly (Meadow.dll)
- Assembly loading resolves System.Runtime.dll type forwarders correctly
- P/Invoke calls route through the trampoline infrastructure to the firmware's P/Invoke override
- TPA (Trusted Platform Assemblies) correctly built from all .dll files at `/meadow0/`
- SPCL (5.8MB), System.Runtime.dll (45KB), and a Hello World Meadow.dll (4.6KB) all load

## What Blocks Track 06

The runtime calls System.Native P/Invoke functions **during type system initialization, before reaching the app's Main() method.** A minimal `return 42` app (no Console.WriteLine, no I/O) still triggers:

1. `SystemNative_LChflagsCanSetHiddenFlag` (boolean) — no-op stub returns 0, OK
2. `SystemNative_CanGetHiddenFlag` (boolean) — no-op stub returns 0, OK
3. `SystemNative_GetEnv` (returns char*) — stub returns NULL, OK individually
4. After these 3, the runtime hangs — likely hitting additional P/Invoke calls to unknown libraries (System.Globalization.Native, etc.) that were falling through to a silent `return NULL`

The firmware's `meadow_pinvoke_override` now logs ALL P/Invoke requests (including unknown libraries) and returns a generic no-op stub. The catch-all was previously returning NULL silently, which caused undiagnosed crashes.

## Key Files for Track 06

| File | Purpose |
|------|---------|
| `Meadow/apps/examples/mono/mono_main.c` | P/Invoke override — add real stubs here |
| `runtime/src/mono/mono/mini/aot-runtime-nuttx.c` | i2n dispatch + n2i thunk pool |
| `runtime/src/mono/mono/mini/nuttx_m2n_invoke.g.h` | 47 invoke wrappers (add more if new signatures appear) |
| `Meadow.OS.Emulator/build/dotnet10/assemblies/` | Assembly staging area for LFS image |
| `Meadow.OS.Emulator/tools/build_lfs_v1_image.c` | LFS image builder |
| `Meadow.OS.Emulator/scripts/meadow-dotnet10-headless.resc` | Renode emulator script |

## Architecture Notes

The trampoline architecture follows the WASM pattern (C-level dispatch) rather than the JIT pattern (runtime code generation). This is necessary because NuttX builds with `DISABLE_JIT` — no runtime code generation is possible on the read-only flash + limited SDRAM target.

The `.mono_bss` section address changes with each firmware rebuild. After rebuilding, check:
```bash
arm-none-eabi-readelf -S build/dotnet10/nuttx_user.elf | grep mono_bss
```
Then update `mono_bss_zero.bin` size and the address in `meadow-dotnet10-headless.resc`.
The build script (`build-meadow.os-emulated.sh`) does NOT update the Renode script automatically.
