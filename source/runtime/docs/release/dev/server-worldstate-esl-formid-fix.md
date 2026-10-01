# Fix: wrong FormID resolution in WorldState when ESL/light plugins are present

## Symptom

New-character spawn crashed with:

```
Error: Record 0x1c047d70 doesn't exist
    at EventEmitter.listenerFn (C:\server\ts\systems\spawn.ts:35:17)
```

Reproduced only with a modded load order containing ESL-flagged plugins (`.esl`
files and ESL-flagged `.esp`/espfe files) mixed with regular plugins; a
vanilla/short load order did not trigger it. Disabling the JS gamemode did not
fix it, which pointed at the native server core rather than gamemode code.

## Root cause

`libespm`'s `Combiner` builds **two** parallel FormID-resolution systems:

- The current, ESL-aware one (`RuntimePlacement`, `regularSourceBySlot` /
  `lightSourceBySlot`, and the `ResolveReferencedFormId` / `ResolveRecordFormId`
  / `ToRuntimeFormId` / `ToFileFormId` API on `CombineBrowser`).
- A **legacy compatibility** mapping (`Source::toComb` / `Source::toRaw`,
  consumed via `espm::utils::GetMappedId` and `CombineBrowser::GetCombMapping`
  / `GetRawMapping`), built from each plugin's **raw insertion order**
  (`SourceId`) into the loader's source list — i.e. it does **not** exclude
  ESL-flagged plugins from the index count. Both accessors are already marked
  `[[deprecated("use source-aware runtime FormID APIs")]]` in
  `libespm/include/libespm/CombineBrowser.h`.

`server/cpp/server_guest_lib/WorldState.cpp` (`AttachEspmRecord`, called from
`LoadForm`, plus `GetNeighborsByPosition`) was never migrated off the legacy
mapping. So any REFR whose base/world-or-cell field pointed at a *regular*
plugin sitting **after** ESL-flagged plugins in the load order got the wrong
master-index byte: e.g. `HammetDungeon02.esm` is really at load-order index
`0x0c` (verified against the live load order with houseCARL), but the legacy
mapping computed `0x1c` (its raw position in the plugin list, ESLs counted as
if they consumed a normal slot) — an off-by-16 caused by 16 ESL/light plugins
sitting earlier in that particular load order.

Confirmed via the exception origin: `libespm/include/libespm/Loader.h:117`
(`GetData<RecordT>`), and the log line `WorldState.cpp` prints on a failed
base-record lookup (`logger->info("baseId {} {}", ...)`), which showed the
*correctly* mapped id (`0x0c047d70`, decimal `201620848`) right next to the
wrongly-mapped one that crashed (`0x1c047d70`).

## Fix

Migrated `WorldState.cpp` off the deprecated legacy mapping, onto the
source-aware API:

- `WorldState.h` / `WorldState.cpp` — `AttachEspmRecord(...)` now takes
  `espm::SourceId owningSource` instead of `const espm::IdMapping& mapping`.
- `AttachEspmRecord`:
  - `baseId` — `br.TryResolveReferencedFormId(owningSource, data.baseId)`
    (was `GetMappedId(data.baseId, mapping)`). On failure, logs and returns
    `false` (same "skip this reference" behavior the function already had for
    other failure cases).
  - `formId` (the record's own id) — `br.TryResolveReferencedFormId(owningSource, record->GetId())`
    (was `GetMappedId(record->GetId(), mapping)`).
  - `worldOrCell` — `br.TryResolveReferencedFormId(owningSource, GetWorldOrCell(br, record)).value_or(0)`
    (was `GetMappedId(GetWorldOrCell(br, record), mapping)`). This was the
    field that actually crashed spawn.
- `LoadForm` — stopped calling the deprecated `br.GetCombMapping(lookupRes.fileIdx)`
  and just passes `lookupRes.fileIdx` (already a `SourceId`) straight through.
- `GetNeighborsByPosition` — replaced `GetCombMapping`/`GetRawMapping` +
  `GetMappedId` (bidirectional legacy lookup per source file) with
  `br.ToFileFormId(source, cellOrWorld)` and `br.ResolveRecordFormId(source, rec->GetId())`,
  wrapped in `try { } catch (const espm::ResolveError&) { continue; }` to
  preserve the old "this plugin doesn't know about that cell/world, skip it"
  behavior (the new API throws instead of silently returning a sentinel/garbage
  value).

Verified with an incremental rebuild (`cmake --build . --config Release
--target skymp5-server`): recompiled `WorldState.cpp` and everything
depending on `WorldState.h`, 0 errors/warnings, linked
`skymp5-server.vcxproj -> scam_native.node` successfully.

## Known related debt (not fixed here, same underlying pattern)

Left untouched, out of scope for this crash:

- `WorldState.cpp` — `GetFileIdx(uint32_t formId) { return formId >> 24; }`,
  used by `IsNpcAllowed` for the per-plugin NPC spawn interior/exterior
  override feature. Same "raw top byte as index" assumption; worth checking
  once ESL plugins are involved in that feature specifically.
- Other files still calling the deprecated `espm::utils::GetMappedId` /
  `GetCombMapping` / `GetRawMapping`: `CraftService.cpp` (3 call sites),
  `LocationalDataUtils.cpp` (1), `MpObjectReference.cpp` (4). Same class of
  bug could surface there (crafting recipes / leveled-list result items /
  object-reference result items resolving to the wrong plugin) once a regular
  plugin they reference sits after enough ESL plugins in the load order.

## Diagnosis references

- houseCARL (MO2 profile `AETHERIUS - PERFORMANCE`) confirmed the real load
  order: index `0x1c` = `Nordic Ruins of Skyrim.esp` (does not define record
  `047D70`), index `0x0c` = `HammetDungeon02.esm` (defines `047D70`, a
  `PlacedObject` — a plain clutter item, unrelated to spawn logic; it's simply
  the record whose reference happened to be mis-mapped in this repro).
- `housecarl_check` (`errors` family) on `HammetDungeon02.esm` +
  `Nordic Ruins of Skyrim.esp` found 0 dangling refs / missing masters — this
  was never a mod-conflict or missing-patch issue, purely a load-order-index
  computation bug in the server.
