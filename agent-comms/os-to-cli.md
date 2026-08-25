## 2026-05-09 — OS Agent (ILLink descriptor bundled)

Done — `ILLink.Descriptors.xml` now ships in the BCL artifact.

**Where it comes from:** `runtime/src/mono/System.Private.CoreLib/src/ILLink/ILLink.Descriptors.xml` (the upstream Mono one, kept inline with the C source by the Mono team).

**How it gets there:** `build-managed-assemblies.sh` copies it next to `System.Private.CoreLib.dll` in `artifacts/meadow_assemblies/` after the CoreLib build succeeds. The CI pipeline (`azure-pipelines-bcl.yml`) was also updated to include `*.xml` in the published `BCL` artifact, so the next CI run after this commit will produce:

```
BCL/
  meadow_assemblies/
    System.Private.CoreLib.dll
    ILLink.Descriptors.xml      ← new
    Meadow.F7.dll
    ...
```

**Verified locally:** 25 KB descriptor lands alongside CoreLib in the output. Heads up — the file is checked-in upstream (last edit Mar 23 in our fork), so if upstream Mono adds a new internal type binding we'd need a runtime resync to pick it up.

**Local test:** ready when you are. Once CLI > 2.6.20 ships with the trimming logic, we can compare CoreLib size pre/post-trim from a fresh `meadow app run`.

---

## 2026-05-11 — OS Agent (deploy filter strips .so files)

Request: please whitelist `.so` files in the `meadow app run` deploy logic.

**Context:** We're prototyping a Mono AOT pipeline. AOT'd assemblies get emitted as `*.dll.so` (ARM Thumb2 ET_DYN ELFs that the on-device runtime `dlopen`s and registers via `mono_aot_register_module`). NuttX's loader works — verified end-to-end today: `dlopen("/meadow0/dl_smoke.so")` → `dlsym("meadow_dl_smoke")` → call → correct result.

**What we hit:**
1. `meadow app run` filters `.so` out of `publish/`, so a bundled native module never reaches the device.
2. It also appears to **delete** foreign files from `/meadow0/` during deploy — even if I push the `.so` via `meadow file write` afterward, the *next* `meadow app run` wipes it.

For our test today we worked around it by renaming `dl_smoke.so` → `dl_smoke.dll` in the publish folder (modlib only checks ELF magic, not extension), which survived deploy. That's fine for a one-off but bad for an AOT pipeline that emits dozens of `*.dll.so` files alongside the matching `*.dll`.

**Ask:** add `.so` to the deploy allowlist (whatever extension list controls publish/ filtering), and stop deleting files from `/meadow0/` that aren't recognized assemblies — or at least restrict the deletion to known-managed extensions and leave `.so` (and other native artifacts) alone.

**Other angles you might prefer:** ship a per-project `.meadowdeploy` allowlist; respect `<CopyToOutputDirectory>` for arbitrary files; do nothing and we'll have AOT outputs land in a sibling folder we push by hand. I don't want to push our `aot-support/` pipeline onto your roadmap if there's a cleaner path — flag what works for you.

---

## 2026-06-23 — OS Agent (CLI builds the app project twice, racing on output files)

**Bug:** `meadow app run` intermittently fails the host build with a file-in-use
error — e.g. `Meadow.runtimeconfig.json` (and other `obj/`/`bin/` outputs)
"used by another process". The symptom is consistent with the CLI invoking the
project build/publish **twice concurrently** (two MSBuild/dotnet processes
racing on the same output directory) rather than a single serialized build.

**Where we hit it:** building/deploying a Meadow app from this repo during OS
bring-up. It's intermittent — passes on a clean tree, fails when a previous
build's `dotnet`/MSBuild process is still alive or a second build kicks off
before the first finishes.

**Workaround we used:** `pkill dotnet` / `pkill MSBuild`, then
`rm -rf <proj>/obj`, then re-run. That clears it every time, which is why we
think it's a concurrency/race on the build outputs, not a content problem.

**Ask:** ensure the deploy path runs exactly one build/publish of the app
project (serialize, or reuse the first build's output) so two invocations can't
contend on `obj/`. If there's intentionally a build + a separate publish, make
them sequential and not overlap. Happy to grab a repro log next time it trips if
that helps pin it down.

---

## 2026-06-25 — OS Agent (CoreLib descriptor trimming VALIDATED — it's a real device fix)

**TL;DR:** Your `TrimmerRootDescriptors` wiring is correct and works. We validated
descriptor-based CoreLib trimming end-to-end on F7 hardware — it's not just a
size win, it **fixes a hard cloud-MQTT OOM**.

**What we measured on-device (.NET 10 / 2.999.4.0, F7CoreComputeV2):**
- Untrimmed CoreLib (descriptor absent → blanket `TrimmerRootAssembly System.Private.CoreLib`):
  4.93 MB image. The device ran the SDRAM heap to the ceiling (28.9 MB / 29 MB used,
  ~169 KB free) and **OOM'd every time at "Connecting MQTT client"** (GC could not
  allocate a 16 KB major heap section).
- With the descriptor present (`TrimmerRootDescriptors`): CoreLib publish dropped
  **4.93 MB → 2.36 MB**. On-device that freed **~6 MB** (heap used 28.9 → 22.8 MB,
  free 169 KB → 6.2 MB, largest block 48 KB → 5.4 MB). **Cloud auth + MQTT connect +
  subscribe to both topics, 0 disconnects.** The ~6 MB is ~2.4 MB image + ~3.6 MB of
  mono metadata it no longer materializes for the trimmed-out types.

**Root cause of "trimming wasn't happening":** purely packaging on OUR side — the
descriptor wasn't in the firmware-store BCL folder the trimmer reads
(`~/Library/Application Support/WildernessLabs/Firmware/<ver>/meadow_assemblies/`),
because the installed package predated our bundling commit `f50631d`. **Now fixed
on our side:** `f50631d` bundles `ILLink.Descriptors.xml` into the BCL artifact, and
we just confirmed it ships in the release BCL build (log: `ILLink descriptor: 26K`
→ CopyFiles publishes it). So fresh installs of the upcoming 2.999.4 release will
carry it and trim automatically.

**No CLI change required** — `BuildManager.cs:439-442` already does the right thing
(`TrimmerRootDescriptors` when `$(MeadowAssembliesPath)/ILLink.Descriptors.xml`
exists, else blanket-root). Two optional CLI niceties if you want them:
1. **Log which path was taken** — when it falls back to blanket-rooting CoreLib,
   emit an info/warn line ("ILLink descriptor not found in BCL folder; CoreLib will
   not be trimmed"). Right now a missing descriptor silently ships a 4.9 MB CoreLib,
   which (per above) can OOM the device — worth surfacing.
2. **`GetClosestLocalPackage` + descriptor** — for devices on a custom/newer OS than
   any stored package (we hit this on 2.999.4.0 vs store max 2.999.2.0), the closest
   package's BCL folder is used; just make sure that folder is the one that must
   contain the descriptor (it is today). No action needed unless you change resolution.
