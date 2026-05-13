# Implementation Plan: Meadow 3.0 Stabilization

## Phase 1: Decisions

Land committed defaults for the toggles still in flux. Each item is a
small decision + commit, not real engineering work.

- [ ] `DISABLE_AOT` default — keep 0 (carry +2MB for future AOT) or
      revert to 1 (smaller firmware, no AOT loader). Recommend: keep 0
      since Track 12 is ongoing and rebuild cost to flip later is low.
- [ ] Explicit-null-checks final policy. Two options:
      (a) ON (current default in `mini-arm.h`) — safe, costs ~3% Pi.
      (b) OFF with MPU + MemFault → mono dispatch path landed. Recommend:
      ON for 3.0; MPU path is a follow-up.
- [ ] Keep or remove `arm-codegen.h` forced `__thumb2__` define. Recommend:
      keep with comment until proper triple-driven path lands.
- [ ] JIT-opt scaffolding in `mono_main.c` — keep dormant (already done) or
      strip. Recommend: keep, costs nothing, useful for future tuning.

## Phase 2: Fresh-Checkout Verification

- [ ] Branch off meadow-3.0 to a clean throwaway worktree.
- [ ] Run `./build.sh` end-to-end. Capture any "missing piece" errors.
- [ ] Run emulator boot. Confirm App Up.
- [ ] Flash hardware, run bench. Confirm App Up + Pi numbers.

## Phase 3: Hardware Re-Validation

Replay the Track 08 validation matrix against the final firmware bits.

- [ ] Boot timing (Enable → App Up). Compare to historical (~39s).
- [ ] Bench suite: list ops, GPIO writes, Pi calc, SoftPWM. Capture
      numbers, compare to committed README values.
- [ ] Network: DNS, HTTP/1.0, HTTPS x2, NetworkInterface.

## Phase 4: Document + Release Notes

- [ ] Update `conductor/tracks.md` status lines.
- [ ] Draft 3.0 release notes covering: .NET 10 BCL, JIT, networking, TLS,
      GPIO fast path, performance summary, known limitations (no AOT
      execution, etc.).
- [ ] Update `README.md` top-of-tree if needed.

## Phase 5: CI Green

- [ ] Verify Azure pipeline succeeds on meadow-3.0 tip.
- [ ] Verify BCL artifact pipeline produces expected output.
- [ ] Verify emulator tests pass.

## Sequencing

Phases 1 -> 2 -> 3 -> 4 in order. Phase 5 can run in parallel with 3/4
once Phase 1 lands and CI sees the resulting commits.
