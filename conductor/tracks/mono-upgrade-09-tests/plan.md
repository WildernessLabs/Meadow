# Implementation Plan: Mono Tests (Interpreter)

## Phase 1: Test Infrastructure
- [ ] Survey available tests in `src/mono/mono/tests/` and `src/mono/mono/unit-tests/`
- [ ] Determine how to build test assemblies for the Mono ARM target
- [ ] Create a simple test runner that can execute on NuttX (or via HCOM)
- [ ] Design result collection: test output via syslog/HCOM, parse for pass/fail

## Phase 2: Core Runtime Tests
- [ ] Run type system tests (class loading, interfaces, generics, valuetypes)
- [ ] Run exception handling tests (try/catch/finally, nested, filters)
- [ ] Run string and array tests
- [ ] Run delegate and event tests
- [ ] Run reflection tests (basic Type queries)
- [ ] Document failures, triage: runtime bug vs. platform issue vs. test infrastructure

## Phase 3: Interop Tests
- [ ] Run P/Invoke tests (basic DllImport, struct marshaling)
- [ ] Run blittable struct layout tests
- [ ] Run IntPtr / pointer tests
- [ ] Verify calling convention correctness on ARM

## Phase 4: GC and Threading Tests
- [ ] Run GC allocation/collection tests
- [ ] Run finalization tests
- [ ] Run weak reference tests
- [ ] Run threading tests (Thread, ThreadPool, Mutex, ManualResetEvent)
- [ ] Run async/await tests (Task completion, cancellation)

## Phase 5: ARM-Specific Tests
- [ ] Run floating-point tests (float, double arithmetic)
- [ ] Run VFP register tests
- [ ] Run alignment-sensitive tests (unaligned access with NO_UNALIGNED_ACCESS)
- [ ] Document final pass rate and known failures
