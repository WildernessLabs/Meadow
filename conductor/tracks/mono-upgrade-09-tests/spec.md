# Track 9: Mono Tests (Interpreter) — Test Suite Validation

## Overview
Run the Mono test suite in interpreter mode to validate runtime correctness before porting the Thumb2 JIT. This catches runtime/platform bugs early, ensuring the foundation is solid before adding codegen complexity.

## Background
The .NET 10 mono test infrastructure has evolved:
- `src/mono/mono/tests/` — Native C-level unit tests
- `src/mono/mono/unit-tests/` — Granular unit tests
- Most managed tests have migrated to `src/tests/` in dotnet/runtime (shared with CoreCLR)
- Old standalone mono had `mono/tests/` with hundreds of C# test programs

### Test Categories Relevant to Meadow
- **Basic runtime**: type system, generics, exceptions, threading
- **P/Invoke**: native interop, marshaling, struct layout
- **GC**: collection, finalization, weak references
- **Interpreter-specific**: IL interpretation correctness
- **ARM-specific**: calling conventions, float handling, alignment

## Functional Requirements
1. Identify which mono tests can run on NuttX/ARM interpreter
2. Build and deploy test assemblies to emulator
3. Run tests and collect pass/fail results
4. Fix any runtime failures found
5. Establish a baseline pass rate for interpreter mode

## Acceptance Criteria
- [ ] Test runner infrastructure works on NuttX (test execution + result collection)
- [ ] Core runtime tests pass (type system, exceptions, generics)
- [ ] P/Invoke marshaling tests pass
- [ ] GC tests pass (basic collection cycles)
- [ ] Threading tests pass (pthread creation, synchronization)
- [ ] Float/double arithmetic tests pass (VFP hard float)
- [ ] Pass rate documented as interpreter baseline

## Out of Scope
- JIT-mode tests (run again after Track 10)
- Performance benchmarks
- Networking tests (Track 11)
