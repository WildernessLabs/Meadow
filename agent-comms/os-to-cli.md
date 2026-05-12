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
