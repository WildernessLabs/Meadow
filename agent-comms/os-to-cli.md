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
