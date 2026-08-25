# Tracks Registry: Mono Runtime Upgrade to .NET 10

---
- [x] **Track 01: Build System — CMake + NuttX Cross-Compilation Target** *(archived)*
*Link: [./tracks/_archive/mono-upgrade-01-build/](./tracks/_archive/mono-upgrade-01-build/)*

---
- [x] **Track 02: NuttX Platform Port — Threading, Memory, Signals** *(archived)*
*Link: [./tracks/_archive/mono-upgrade-02-nuttx/](./tracks/_archive/mono-upgrade-02-nuttx/)*

---
- [x] **Track 03: Emulator Bring-Up — Build Pipeline + Renode Boot** *(archived)*
*Link: [./tracks/_archive/mono-upgrade-03-emulator/](./tracks/_archive/mono-upgrade-03-emulator/)*

---
- [x] **Track 04: Initialization — monovm Hosting API Integration** *(complete)*
*Link: [./tracks/mono-upgrade-04-init/](./tracks/mono-upgrade-04-init/)*

---
- [x] **Track 05: BCL Deployment — Interp-to-Native + Native-to-Interp Trampolines** *(complete)*
*Link: [./tracks/mono-upgrade-05-bcl/](./tracks/mono-upgrade-05-bcl/)*

---
- [x] **Track 06: System.Native PAL + Hello World Execution** *(complete)*
*Link: [./tracks/mono-upgrade-06-interp/](./tracks/mono-upgrade-06-interp/)*

---
- [x] **Track 07: Blinky on Interpreter — End-to-End App Validation** *(complete)*
*Link: [./tracks/mono-upgrade-07-blinky/](./tracks/mono-upgrade-07-blinky/)*

---
- [x] **Track 08-PAL: System.Native PAL Port — Upstream pal_*.c for NuttX** *(complete)*
*Link: [./tracks/mono-upgrade-08-pal-port/](./tracks/mono-upgrade-08-pal-port/)*

---
- [x] **Track 08: Hardware Validation (Interpreter) — Physical Board Testing** *(complete)*
*Link: [./tracks/mono-upgrade-08-hw-validation/](./tracks/mono-upgrade-08-hw-validation/)*

---
- [x] **Track 09: Mono Tests (Interpreter) — Test Suite Validation** *(735 tests, 732 pass, 99.6%)*
*Link: [./tracks/mono-upgrade-09-tests/](./tracks/mono-upgrade-09-tests/)*

---
- [x] **Track 10: Thumb2 JIT — Port JIT Backend to .NET 10 Mono** *(complete)*
*Link: [./tracks/mono-upgrade-10-thumb2/](./tracks/mono-upgrade-10-thumb2/)*

---
- [x] **Track 11: TLS / Networking — Mbed TLS Integration** *(complete)*
*Link: [./tracks/mono-upgrade-11-tls/](./tracks/mono-upgrade-11-tls/)*

---
- [x] **Track 12: WiFi + TCP/HTTP Networking — Full Stack Validation** *(complete)*
*DNS, sync/async sockets, HttpClient HTTP/1.0 — all passing on F7CoreComputeV2 hardware*

---
- [~] **Track 13: AOT Compilation — Ahead-of-Time via LLVM** *(in progress)*
*Cross-AOT compiler + dlopen + `mono_aot_register_module` working end-to-end; CoreLib AOT execution still memfaults. See [handoff.md](./tracks/mono-upgrade-12-aot/handoff.md).*
*Link: [./tracks/mono-upgrade-12-aot/](./tracks/mono-upgrade-12-aot/)*

---
- [~] **Track 14: Meadow 3.0 Stabilization** *(in progress)*
*Pre-release cleanup: decide lingering toggles, re-validate on hardware, draft release notes.*
*Link: [./tracks/meadow-3.0-stabilize/](./tracks/meadow-3.0-stabilize/)*

---
- [~] **Track 15: Golden 3.0 Release** *(in progress)*
*Road to the golden (stable) Meadow OS 3.0 release. **Preview 2 (`2.999.4.0`) SHIPPED** — cloud auth + MQTT working, CoreLib-trimming OOM fix; OS Release-233 + coupled ESP Release-167 on S3, validated end-to-end (download→flash→deploy→cloud delivery). Now driving to GA.*
*Link: [./tracks/golden-3.0-release/](./tracks/golden-3.0-release/)*

---
