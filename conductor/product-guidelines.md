# Product Guidelines

## Development Approach
- **Interpreter first, JIT second, AOT third**: Get things running interpreted before optimizing
- **Emulator-first validation**: Test in Renode before deploying to hardware
- **Minimal changes to upstream mono**: Use `#ifdef __NuttX__` guards; keep patches isolated
- **Standard P/Invoke**: Do not use mono_add_internal_call; all native calls go through DllImport

## Code Principles
- Keep NuttX platform patches clearly separated from upstream mono code
- Maintain backward compatibility with existing Meadow.Core P/Invoke signatures
- Use the new monovm hosting API (not legacy mono_jit_init)
- Target `net10.0` TFM for all managed code

## Quality Gates
- Each track must pass emulator validation before hardware validation
- Hardware validation is a manual step on real F7FeatherV2 or CCMv2 boards
- Mono test suite should pass at each stage (interpreter, then JIT)
