# Development Workflow

## Track Execution Order
Tracks are executed sequentially as listed in `tracks.md`. Each track has phases that are also sequential.

## Building Firmware

**ALWAYS use the emulator build script. NEVER run `make` directly in Meadow/nuttx/.**

```bash
cd Meadow.OS.Emulator/
bash build-meadow.os-emulated.sh
```

The build script handles: correct toolchain (ARM GCC 10.3, NOT homebrew v15), kconfig,
version stamps, multi-pass protected-mode build, artifact extraction, and hook generation.
Output goes to `build/dotnet10/`. See `build-meadow.os-emulated.sh` for details.

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
- **Emulator**: `./run.sh` in Meadow.OS.Emulator, then `meadow config route socket://localhost:4242`
- **App deploy**: `meadow app run` from app project directory (uses Meadow.CLI socket branch)
- **Hardware**: Flash firmware via SWD, deploy app via USB with Meadow.CLI

## Key Branches
- Work on feature branches per track
- Merge to develop/main after track completion and validation
