# Fix: FormDesc's runtime-FormID conversion ignored ESL/light plugins

## Symptom

After the `WorldState`/`AttachEspmRecord` ESL-mapping fix (see
`server-worldstate-esl-formid-fix.md`), creating a brand-new character
spawned fine, but **loading an existing character produced a black screen**
client-side, with no server-side error logged.

## Root cause

`FormDesc::ToFormId`/`FormDesc::FromFormId` — used to persist an actor's
`worldOrCell` (and base id, template chain, factions, etc.) between sessions
as a portable `(localId, plugin filename)` pair — had their **own**,
independent copy of the same bug: `fileIdx * 0x01000000 + shortFormId` /
`formId / 0x01000000`, where `fileIdx` was the plugin's raw position in
`WorldState::espmFiles` (`espm::Loader::GetFileNames()`'s order — i.e. the
load order's configured order, ESL plugins counted as a normal slot).

New-character creation never hit the buggy path: `startPoints` always
resolves to `worldOrCell = 0x3c` (Tamriel), and `FormDesc::FromFormId` had a
hardcoded shortcut for exactly that value (`if (formId == 0x3c) return
FormDesc::Tamriel();`, a "workaround legacy tests" special case). Loading an
*existing* character's saved position goes through the general path instead,
so it silently computed the wrong (but structurally valid, so no exception)
`worldOrCell` — the client then tried to load a cell/world that didn't match
reality, hence the black screen with nothing in the server log.

## Fix

Found — but never implemented or wired up — `unit/PluginIndexMappingTest.cpp`
and a matching case in `unit/CombinerLightTest.cpp` (`"Loader metadata keeps
PluginIndexMapping on runtime FormIDs"`): a fully-specified, unit-tested API
for exactly this translation, referencing a `PluginIndexMapping.h` that did
not exist anywhere in the repo. Implemented it from that spec:

- **New**: `server/cpp/server_guest_lib/PluginIndexMapping.h` / `.cpp`.
  Classifies each plugin (`Master` / `RegularEsp` / `LightEsl` /
  `LightEspfe`) from its extension **and** its TES4 header flags (so an
  ESL-flagged `.esp` — "espfe" — is detected even though its extension looks
  like a regular plugin), assigns regular plugins slots `0x00`-`0xFD` and
  light plugins slots `0x000`-`0xFFF` **separately** (light plugins do not
  consume a regular slot), and translates between runtime FormIDs, the
  legacy "top byte = raw file index" espm IDs, and `FormDesc`.
- **`FormDesc.h`/`.cpp`**: `ToFormId`/`FromFormId` now take a
  `const PluginIndexMapping&` instead of `const std::vector<std::string>&`,
  and just delegate to it. The old raw-index arithmetic and the `0x3c`/
  `Tamriel` special-casing are gone — the mapping already resolves Tamriel
  correctly through the general path.
- **`WorldState`**: gained a `PluginIndexMapping pluginIndexMapping` member,
  built alongside `espmFiles` in `AttachEspm` (`pluginIndexMapping.Build(espmFiles, espm)` —
  the `espm::Loader*` is passed so `GetFileHeaderFlags()` can detect espfe
  plugins; without it, only the `.esl` extension is used).
- **Every call site** that passed `...->espmFiles` (or a fresh
  `Loader::GetFileNames()`) into `ToFormId`/`FromFormId`/
  `GetJsValueFromPapyrusValue`/`MovementValidation::Validate` now passes
  `...->pluginIndexMapping` instead: `WorldState.cpp`, `MpActor.cpp`,
  `MpObjectReference.cpp`, `PartOne.cpp`, `ActionListener.cpp`,
  `EvaluateTemplate.h`, `condition_functions/IsInInterior.cpp`,
  `script_classes/PapyrusActor.cpp`, `script_classes/PapyrusObjectReference.cpp`,
  `MovementValidation.h`/`.cpp`, `addon/PapyrusUtils.h`,
  `addon/ScampServer.cpp`, `addon/ScampServerListener.cpp`,
  `addon/property_bindings/TemplateChainBinding.cpp`,
  `addon/property_bindings/BaseDescBinding.cpp`. Every site was found by
  letting the compiler enumerate the type errors after changing
  `FormDesc`'s signature, so none were missed by hand.
- Plain `espmFiles` **membership checks** (`EatItemEvent.cpp`'s
  `modFiles.count("SweetPie.esp")`, `MpActor::ReapplyMagicEffects`'s
  Sweetpie check) were left untouched — `espmFiles` still lists every loaded
  plugin correctly, that was never the bug.
- Updated the existing unit tests that called the old
  `vector<string>`-based API to build a `PluginIndexMapping` first:
  `FormDescTest.cpp`, `MovementValidationTest.cpp`, `ActorTest.cpp`,
  `CraftTest.cpp`.

Verified: `cmake --build . --config Release --target skymp5-server` — 0
errors, linked `scam_native.node`. `--target unit` also **compiled** cleanly
(including the pre-existing `PluginIndexMappingTest.cpp`/
`CombinerLightTest.cpp`), which is what caught every remaining call site —
but `unit.exe` fails at the **link** step with `LNK2019` unresolved symbols
in `Catch2.lib` (`__std_find_last_not_ch_pos_1` and similar STL-internal
vectorized helpers) — an ABI mismatch between the vcpkg-prebuilt Catch2 and
the current MSVC STL/SDK, unrelated to this change. Because of that the
pre-existing test suite could not actually be *run*; every assertion in
`PluginIndexMappingTest.cpp` and `CombinerLightTest.cpp` was instead traced
by hand against this implementation and matches. Rebuilding Catch2 (e.g.
`vcpkg install catch2 --recurse` for the `x64-windows-sp` triplet) should
unblock running `unit.exe` if that confidence is wanted later.

## Known related debt (not fixed here)

- `WorldState::GetFileIdx` / `IsNpcAllowed` (per-plugin NPC spawn
  interior/exterior override) still does `formId >> 24` indexing directly
  into `espmFiles` — same class of bug, different feature, out of scope
  here.
- `espm::utils::GetMappedId` / `CombineBrowser::GetCombMapping` /
  `GetRawMapping` (all `[[deprecated]]`) still have call sites in
  `CraftService.cpp` (3), `LocationalDataUtils.cpp` (1),
  `MpObjectReference.cpp` (4, unrelated to the ones fixed above — crafting
  recipe / leveled-list / result-item resolution). Same bug class, would
  need the same treatment `WorldState.cpp`'s `AttachEspmRecord` already got.
