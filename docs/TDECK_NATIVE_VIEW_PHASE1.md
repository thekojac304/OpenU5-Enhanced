# T-Deck Native View — Phase 1: presentation/interaction separation

Status: Phase 1 implemented on a feature branch. Native View is **not** enabled on the
T-Deck; no T-Deck build presents the DOS screen, and no Settings toggle exists. No
hardware testing was performed. Phase 2 has not begun.

## 1. Starting point

| Item | Value |
| --- | --- |
| Repository | `C:\Dev\U5Native\source` (common Git store) |
| `main` = `origin/main` (verified) | `db562274dec8f599e3d69d8292b4b7729df9f6da` |
| `v0.5.0` | annotated tag object `706c5ef6…`, peeled `355d4a1e564cef4c6a6d422b47dd0b6d9334cfe9`, an ancestor of `main`; `355d4a1e..main` changes documentation and `native/tools/cleanup_builds.py` only |
| Branch | `tdeck/native-view-phase1`, created from `db562274` |
| Worktree | `C:\Dev\U5Native\worktrees\u5-tdeck-native-p1` |
| Build outputs | `C:\Dev\U5Native\builds\u5-tdeck-native-p1\` (`baseline-main-db562274`, `phase1-a1`) |

The earlier feasibility audit is not stored in the repository or workspace; each of its
findings was re-checked against the code before use. Private, git-ignored inputs were
provided to the new worktree without changing their sources: the three `native/assets`
packs and four map fixtures copied from `source` (SHA-256 identical), `original/` and
`node_modules/` as junctions to `source` (the ui-polish worktree's convention), and
`native/core/src/loot_names.inc` and `native/targets/tdeck/sdkconfig` copied (identical to
`source`). `build-dialogue/dialogue.bin` and `build-shops/*`
were regenerated with the repository's generators and are byte-identical to the v0.5.0
ui-polish worktree's.

## 2. The original coupling

`PlatformProfile` already separated `dos_screen` (the 320 × 200 geometry) from
`dos_interface` (the 1988 interaction model), but:

1. **Presentation read the interaction switch.** `AlphaRuntime` drew the right panel the
   1988 way — `compose_dos_panel()` (Ready/Use/Mix lists, Z-stats pages), DOS banners,
   `%.22s` list-row cuts, the reverse-video arena actor — only when
   `platform_dos_interface()` was true. A platform could not have the DOS look without
   the 1988 prompts, ESC/getdir rules, roster picker, typed spell names and console shops.
2. **The firmware fixed everything at once.** `OPENU5_FIXED_HANDHELD_PROFILE` folded every
   1988 predicate to `false` together, so no build could keep the handheld interaction
   compiled down to constants while enabling a DOS presentation path.
3. Windows `DesktopProfile::install()` maps typed spells, (Q)uit & Save and the 1988
   interaction to "Interface: Original", and the screen to Original + `originalScreen`.
   That is a Windows product decision and is retained.

`UiSession` (40 `platform_dos_interface()` and 2 `platform_quit_save_key()` reads; no
presentation reads), `shop_orchestration.cpp` and the T-Deck input path
(`input_controller.cpp`, `ui_input_adapter.cpp`, `keyboard_matrix.cpp`: no capability
reads) were audited: every `UiSession` read is interaction (key rules, prompt/echo
strings, the live prompt row), so none moved.

## 3. The new policy model

| Group | Switch / predicate | Null default | Win Original | Win Enhanced | T-Deck v0.5.0 | Future T-Deck Native |
| --- | --- | --- | --- | --- | --- | --- |
| Presentation | `dos_screen` / `platform_dos_screen()` | off | on (DOS screen setting) | off | off (constant) | on (Phase 2) |
| Presentation | **`dos_panels` / `platform_dos_panels()` (new)** | off | on | off | off (constant) | on |
| Presentation | `dos_text` / `platform_dos_text()` | none | game data | game data | none (constant) | game data |
| Interaction | `dos_interface` / `platform_dos_interface()` | off | on | off | off (constant) | off (constant) |
| Interaction | `typed_spell_names`, `quit_save_key` | off | on | off | off (constant) | off (constant) |
| Journey | `enhanced_rows` / `platform_enhanced_rows()` | shown | hidden | shown | shown | shown |

`dos_panels` decides only how the right panel looks; which list it shows, its cursor rules,
prompts and legends stay the interaction model's. Difficulty, cheats and saved Enhanced
flags live in `GameState::enhanced` and the save; no presentation or interaction switch
reads or writes them. `enhanced_rows` only decides whether the System Menu lists them.

**Compile-time boundary** (`platform_profile.h`):

| Macro | Effect |
| --- | --- |
| `OPENU5_FIXED_HANDHELD_INTERACTION` | interaction predicates are `constexpr false`; `kPlatformSupportsDosInteraction = false` |
| `OPENU5_FIXED_HANDHELD_PRESENTATION` | `dos_screen`, `dos_panels` `constexpr false`, `dos_text` `nullptr`; `kPlatformSupportsDosPresentation = false` |
| `OPENU5_FIXED_HANDHELD_PROFILE` | both (unchanged meaning; still what the firmware defines) |

Phase 2 can switch the firmware from `…_PROFILE` to `…_INTERACTION` alone: presentation
becomes selectable while every 1988 interaction branch stays dead code.

**Call sites moved to presentation** (`alpha_runtime.cpp`): `compose_dos_panel()` gate;
Z-stats panel composition; list-row cut and `Current:` detail format; DOS list banners and
cleared detail lines; the reverse-video arena actor in `compose_party_highlight()`.
**Split** at the same sites: the cleared key legend (`context = {}`) and the roster
picker's cursor/`Select:` banner stay on `platform_dos_interface()`; the console-shop
windows ("Arms", GUEST REGISTER) inside `compose_dos_panel()` now require both.
**Unchanged (interaction):** all other `alpha_runtime.cpp` reads — banners, picker
prompts/echoes, list sort/empty rules, yields, typed spells, context bar, transcript
paging geometry, console shops (`sync_shop_profile`, `compose_shop_view`).

## 4. Files

| File | Change |
| --- | --- |
| `native/core/include/openu5/platform_profile.h`, `src/platform_profile.cpp` | grouped documentation; `dos_panels` hook and predicate; per-group fixing macros; `kPlatformSupportsDosInteraction` |
| `native/targets/tdeck/main/alpha_runtime.cpp` | presentation sites → `platform_dos_panels()`; interaction parts split out |
| `native/targets/tdeck/main/device_ui_views.h`, `tdeck_board.cpp` | comments only |
| `native/targets/windows/src/app/desktop_profile.cpp` | installs `dos_panels = original_mode` |
| `native/targets/windows/ORIGINAL_MODE_FIDELITY.md` | one pointer paragraph |
| `native/core/CMakeLists.txt` | four new tests |
| `native/core/tests/tdeck_native_policy_test.cpp` (new), `targets/tdeck/host_tests/tdeck_native_policy_runtime_test.cpp` (new) | policy tests |
| `native/core/tests/rc1_platform_profile_test.cpp`, `targets/windows/tests/presentation_test.cpp` | extended |
| `targets/tdeck/host_tests/{win_dos,spp1,spp2}_runtime_test.cpp`, `core/tools/lookpolish_differential/tdeck_diff_probe_test.cpp` | their "Windows Original" fixture profiles also install `dos_panels` (fixtures only; no expectation changed) |
| `native/tools/rc1/mutation_check.py`, `verify_capabilities.py`, `README.md` | `dos_panels` / interaction-constant mutants; per-group macro leak check |

No public function was removed or renamed. `PlatformProfile` gained two fields (it is not
serialized). No save format, settings key, asset, font, geometry or key mapping changed.

## 5. Behavioural invariants

1. The T-Deck firmware installs no profile and is still built with
   `OPENU5_FIXED_HANDHELD_PROFILE`: every 1988 predicate, including `platform_dos_panels()`,
   is `constexpr false` there.
2. Windows Original sets `dos_panels` with the same predicate as `dos_interface`, so every
   moved call site evaluates exactly as before; Windows Enhanced and the T-Deck have both off.
3. No presentation switch can enable a 1988 interaction rule, hide Difficulty/Cheats, or
   change `GameState::enhanced` or save bytes (tested, §6).
4. Under fixed interaction, a profile requesting the 1988 interaction gets the handheld one.

## 6. Tests

| Test | What it proves |
| --- | --- |
| `tdeck_native_policy_dynamic` / `_handheld` / `_native_handheld` (new) | the four configurations' 9 decisions each, in the dynamic, v0.5.0-firmware and interaction-only-fixed builds; callbacks of fixed groups never run; (dynamic) a 24-step UiSession trace — Look cancel, Y/N, text entry, roster picker, item paging, numbers, Q, Z-stats — is identical for T-Deck, Windows Enhanced and future Native, and differs at 22/24 steps for Windows Original |
| `tdeck_native_policy_runtime` (new) | real `AlphaRuntime` + Board, **raw T-Deck input** (keys, trackball, Mic, Alt+M, Alt+S): 35-step interaction trace, full transcript, `GameState::enhanced` and the Alt+S GAM bytes identical between T-Deck and future Native; Native composes the 1988 panel at 6 steps (Ready, Use, Z-stats) where the T-Deck composes none; targeted: member list not roster picker, spell list not typed name, Mic cancels Ready (no `Item: Done`) and Look, Q is not Quit & Save, handheld shop panel; Windows Original changes 34/35 steps |
| `rc1_profile_dynamic` / `_handheld` (extended) | `dos_panels` and `kPlatformSupportsDosInteraction` in both builds |
| Windows `uv_presentation_test` K1–K5 (new) | Original/DOS screen, Original/handheld screen (only `dos_screen` moves), Enhanced × both screen settings, separate callbacks |

The future Native combination is represented with `dos_screen` off in the runtime test:
the 320 × 200 Board path is `board_dos_layout.cpp`, not compiled into any T-Deck build.

## 7. Validation

All builds are fresh, from this branch's source, under
`C:\Dev\U5Native\builds\u5-tdeck-native-p1\phase1-a1` (logs and JSON in `evidence\`).

| Suite | Result |
| --- | --- |
| Host baseline, unmodified `main` (`baseline-main-db562274`, run in `source`) | 211/213: `dialogue_parity` and `typescript_dialogue_fixture_drift` fail on `source`'s stale, git-ignored `build-dialogue/dialogue.bin` (Sep 15). The same baseline executable passes against a freshly generated fixture (0 mismatches). Not a code failure. |
| Host, Phase 1 (GCC 16.2.0, developer tools and RC1 differential on) | **217/217** = 213 existing + 4 new |
| Windows (MSVC, SDL 3.4.16 offline source, `ctest -j 1`) | **40/40**; `uv_presentation_test` gains 6 checks (K1–K5) |
| Release tooling (`test_package_smoke`, `test_release_manifest`) | **7/7** |
| Firmware (ESP-IDF 6.1, `--no-ccache`, v0.5.0 `sdkconfig`, `-j 2`) | build, `check_app_budget.py`, `verify_capabilities.py --handheld` (101 TUs) PASS; the Windows commands (98 TUs) carry no handheld macro |
| `mutation_check.py` | **10/10** boundary mutants killed (new: `dos_panels`, `kPlatformSupportsDosInteraction`); header restored byte-exact |
| Call-site mutants (`phase1-a1\callsite_mutations.py`) | **4/4** killed by `tdeck_native_policy_runtime`: the panel gate put back on interaction; the roster picker, list legend and console shop moved onto presentation |
| `differential_check.py` against the v0.5.0 final traces (`builds\ui-polish\evidence\final-differential`) | **byte-identical**, unpaced and paced, 1,065 walks / 8,514 snapshots each, for both the dynamic (`rc1_tdeck_reference`) and fixed-handheld (`rc1_tdeck_handheld`) runtimes. The Windows Original positive control (now `dos_interface` + `dos_panels`) is byte-identical to v0.5.0's control trace |

Environment notes, all preserved as separate logs: the first host build lacked the
git-ignored `build-shops/` fixtures (regenerated); the first firmware build was launched
from Git Bash, whose MSYS environment could not find ESP-IDF's Ninja (retried from
PowerShell; that attempt is kept in `tdeck-attempt1-msys`); the system Git config's
`core.autocrlf=true` had checked out 959 attribute-less files with other line endings than
`source`, failing six TypeScript fixture-drift tests, so those files were re-checked out as
their exact committed blobs (no tracked change; no C/C++/CMake source among them); and
`a3_04e_pacing` reads the git-ignored `native/targets/tdeck/sdkconfig`, copied from
`source` (identical to the release build's). No assertion, flag or test was weakened.

## 8. Firmware size

`firmware_report.py` on the v0.5.0 release build (`builds\ui-polish\candidate-tdeck`,
image SHA-256 `62f90df1…`) and on this phase's build:

| Bytes | v0.5.0 | Phase 1 | Δ |
| --- | ---: | ---: | ---: |
| App image (raw firmware) | 1,075,504 | 1,075,520 | +16 |
| App partition | 1,310,720 | 1,310,720 | 0 |
| Partition free | 235,216 | 235,200 | −16 |
| Launcher allocation (rounded) | 1,114,112 | 1,114,112 | 0 |
| Launcher allocation free | 38,608 | 38,592 | −16 |
| `.flash.text` | 738,902 | 738,902 | 0 |
| `.flash.rodata` | 237,020 | 237,020 | 0 |
| `.iram0.text` | 76,039 | 76,039 | 0 |
| `.dram0.data` | 22,091 | 22,107 | +16 |
| `.dram0.bss` | 54,096 | 54,096 | 0 |

The whole delta is the `PlatformProfile` instance `g_profile`: 0x88 → 0x90 bytes (the two
new pointer fields) plus 8 bytes of data alignment. No code was added: `.flash.text` is
identical, the 20 linked desktop-candidate symbols are the same set, and every compiler
stack frame is unchanged. Removing the two fields from fixed builds would make the
struct's layout depend on build flags, so it was not done.

## 9. Known limitations

- The future configuration is representable and its decisions are tested; it is not a
  working Native View. With `dos_panels` on and handheld interaction, the 1988 panel and
  the handheld context bar/legend are both composed; how they share the screen is Phase 2.
- `refresh_session_context()` still sizes transcript paging for the handheld selector
  while a list is open, also under a DOS panel (pre-existing on Windows Original; affects
  Shift+Up/Down page size only).
- The roster picker's highlight style under a DOS presentation with handheld interaction
  is undecided: the member choice stays the handheld list (as tested).
- The Windows Interface row still switches presentation and interaction together, by design.
- Not hardware-tested.

## 10. Phase 2 readiness criteria

Phase 2 may start when all hold (all met on this branch, subject to the user's review):

1. This branch's host, Windows, tooling and firmware validation pass (§7) and it is
   reviewed/merged as the user decides.
2. Firmware switches from `OPENU5_FIXED_HANDHELD_PROFILE` to
   `OPENU5_FIXED_HANDHELD_INTERACTION`; `verify_capabilities.py` is updated to require that
   macro on every firmware TU; the `rc1_profile`/`tdeck_native_policy_native_handheld`
   tests already cover that build's predicates.
3. A size budget is set for compiling `board_dos_layout.cpp` and the `DeviceDosPanel` paths
   into the firmware (`OPENU5_DESKTOP_DOS_LAYOUT` is still Windows-only), measured against
   this phase's image.
4. A design is agreed for the bottom 40-pixel guidance strip that carries the handheld
   prompts/legends under the 200-row DOS screen, and for the roster picker style.
5. A T-Deck Settings row (and its persistence) is designed; T-Deck saves and Settings
   files must stay readable by v0.5.0.
6. `tdeck_native_policy_runtime` is extended with `dos_screen` on once the Board path
   exists in the harness, and the RC1 differential remains exact for the default profile.
