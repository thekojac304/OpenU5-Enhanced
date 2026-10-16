# T-Deck Native View — Phase 2: renderer integration (prototype)

Status: a development prototype on a feature branch. The shared DOS 320 × 200 screen
painter now builds into a **separate T-Deck prototype image**; the production image is
unchanged and still shows Enhanced View only. Nothing is merged, pushed or published.
**No physical T-Deck testing has been done** (§9). Phase 3 has not begun.

## 1. Starting point

| Item | Value |
| --- | --- |
| Phase 1 | `tdeck/native-view-phase1` at `a37e05c8` (on top of `0ec1b481`), clean, unchanged by this phase |
| `main` = `origin/main` | `db562274…` (unchanged); `v0.5.0` tag object `706c5ef6…` → `355d4a1e…` (unchanged) |
| Phase 2 branch / worktree | `tdeck/native-view-phase2` from `a37e05c8`; `C:\Dev\U5Native\worktrees\u5-tdeck-native-p2` |
| Build outputs | `C:\Dev\U5Native\builds\u5-tdeck-native-p2\phase2-a1\` |

No other branch carries T-Deck Native View work. The worktree was created with
`core.autocrlf=false` (exact committed bytes, avoiding Phase 1's line-ending detour) and
given the same git-ignored inputs as Phase 1 (packs, map fixtures, `loot_names.inc`,
`sdkconfig`, generated `build-dialogue`/`build-shops`, `original`/`node_modules` junctions).

## 2. Approach

Phase 1's model is used as is: **presentation** (`dos_screen`, `dos_panels`) is chosen at
boot; **interaction** is fixed to the handheld's at compile time
(`OPENU5_FIXED_HANDHELD_INTERACTION`), so every 1988 interaction branch is dead code in the
prototype; **journey** rules are untouched. There is one painter: `board_dos_layout.cpp`,
the file Windows Original already uses. No renderer was duplicated.

1. **Platform-neutral build switch.** `OPENU5_DESKTOP_DOS_LAYOUT` → `OPENU5_DOS_SCREEN_LAYOUT`
   (code, Windows CMake, `native/targets/windows/README.md`). Dated records keep the old name.
2. **Prototype image.** `OPENU5_NATIVE_VIEW_PROTOTYPE` (top-level CMake option, default
   **OFF**). ON: adds `board_dos_layout.cpp` and `native_view_prototype.cpp`, defines
   `OPENU5_FIXED_HANDHELD_INTERACTION`, `OPENU5_DOS_SCREEN_LAYOUT` and
   `OPENU5_NATIVE_VIEW_PROTOTYPE` instead of `OPENU5_FIXED_HANDHELD_PROFILE`, and sets
   `PROJECT_VER` to `0.5.0-nativeview-p2-debug` (its own FW line and Launcher file name).
   OFF: exactly v0.5.0's configuration.
3. **Activation (prototype only).** At boot, before any game screen, `main.cpp` reads the
   developer marker `/sd/ultima5/dev/presentation.txt` (`native_view_prototype.cpp`). A first
   word `enhanced` (any case) selects Enhanced; anything else, or no file, selects Native. It
   installs a presentation-only `PlatformProfile` (`dos_screen`, `dos_panels`; T-Deck words,
   no interaction or journey hook) and calls `Board::set_dos_layout`. The firmware never writes
   the file; no Settings row, `settings.json` key, save field or key combination exists.
   Switching presentation takes a reboot, so it never changes mid-play.
4. **Scheduling fix.** `Board::paint_rect()` (every DOS frame, band, panel and console row)
   now pauses with `tft_yield()` every 16 rows like every other row loop. Its batching already
   ended a transfer at those rows; only the yield was missing. Before the fix a full DOS frame
   (240 rows) and the 88-row panel ran with no pause.
5. **Handheld prompts on the DOS screen.** `show_alpha_dos()` drew no context bar: with the
   handheld interaction, Y/N, text, number and direction prompts and list key legends would have
   been invisible. Rows 200–239 stay black, **except while a prompt or list is open**, when
   `draw_dos_context_strip()` shows that bar's question (cyan) and keys (green) under a rule;
   it is cleared to black when the prompt closes. Windows Original composes no bar, so its
   strip never draws. This is the minimum needed to make prompts usable, not a guidance bar.
6. **Shops.** The handheld shop panel has no DOS form. While a shop is open the Board shows
   the handheld screen (whole repaint) and repaints the DOS frame whole when it closes. Only
   reachable with a shop view, which Windows Original never composes.

## 3. Files

| File | Change |
| --- | --- |
| `native/targets/tdeck/main/board_dos_layout.cpp` | `paint_rect` yield; prompt strip; context bar parameter; shared-use comments |
| `native/targets/tdeck/main/tdeck_board.{h,cpp}` | macro rename; `show_alpha_dos`/strip declarations; strip cache; shop fallback with repaint bookkeeping |
| `native/targets/tdeck/main/native_view_prototype.{h,cpp}` (new) | marker parser/reader; presentation-only profile |
| `native/targets/tdeck/main/main.cpp` | prototype-only boot selection (`#if OPENU5_NATIVE_VIEW_PROTOTYPE`) |
| `native/targets/tdeck/CMakeLists.txt`, `main/CMakeLists.txt` | prototype option, version label, sources, definitions |
| `native/targets/windows/CMakeLists.txt`, `README.md` | macro rename and comment only |
| `native/core/CMakeLists.txt` | `openu5_native_handheld_core` (interaction fixed) and `tdeck_native_view_runtime` |
| `native/targets/tdeck/host_tests/tdeck_native_view_runtime_test.cpp` (new) | §6 |
| `native/tools/rc1/verify_capabilities.py` | `--native-prototype`; production must compile no DOS screen |

No save format, `settings.json` key, Windows `Original`/`Enhanced` value, asset, font, key
mapping, input routing or gameplay code changed. `KeyboardMatrix → RawInputEvent →
UiInputAdapter → UiAction/DeviceShortcut → AlphaRuntime` is untouched.

## 4. Incomplete interfaces (Phase 3)

Every item below is usable (opens, moves, confirms, cancels) but not yet original-looking:

| Area | Phase 2 behaviour on the Native screen |
| --- | --- |
| Member and spell lists (Cast, Ready/Use member, New Order, Camp guard, Look/Search/Talk member, spell list) | the handheld list in the DOS list frame: title, rows cut to 13 cells, cursor bar, scroll arrow; legend on the strip. The detail lines (member HP/MP, spell effect/target/MP) are not shown. |
| Ready, Use, Mix, Z-stats | the 1988 panels (`dos_panels`), but with the device's own names: the T-Deck has no DATA.OVL short-name tables (`dos_text` is null) |
| Shops | the handheld screen while open |
| Prompts and key legends | the strip under the picture, in the handheld's 6 × 8 font |
| Movement Mode (`MOVE`) indicator | not shown on the Native screen (the handheld status caption has it) |
| Text Size setting | no effect on the Native screen (fixed 16 × 13 console) |
| Shift+Up/Down paging while a list is open | page size from the handheld selector geometry (pre-existing on Windows Original) |
| System Menu, frontend, Developer screen | the handheld's own full screens, as on Windows (by design) |
| Endgame pages and dissolve | shared code path (`top = 0`), not exercised on the T-Deck yet |

## 5. Validation

All builds are fresh, from this branch's source, under `builds\u5-tdeck-native-p2\phase2-a1`
(logs and JSON in `evidence\`). Phase 1 figures in brackets.

| Suite | Result |
| --- | --- |
| Host (GCC 16.2.0, developer tools and RC1 differential on) | **218/218** [217]: all existing tests + `tdeck_native_view_runtime` (28 checks) |
| Windows (MSVC, SDL 3.4.16 offline source, `ctest -j 1`) | **40/40** [40], including the Original DOS-screen framebuffer scenarios (`presentation_dos`, `original_dos_panels`, `original_dos_panels_handheld`, `original_arena`, `original_camp`, `original_keys`, `original_interface`) |
| Release tooling | **7/7** [7] |
| `differential_check.py` vs v0.5.0 final traces | **byte-identical**, unpaced and paced, 1,065 walks / 8,514 snapshots each, dynamic and fixed-handheld runtimes; the Windows Original control trace is byte-identical to v0.5.0's |
| `mutation_check.py` (production firmware + Windows commands) | **10/10** killed, header restored |
| Phase 2 renderer mutants (`phase2-a1\phase2_mutations.py`) | **7/7** killed by `tdeck_native_view_runtime`: no `paint_rect` yield (Y1), no strip (S1), strip never cleared (S2), no shop fallback (T3a), fallback without repaint (T3b), selector installing an interaction hook (P2), case-sensitive marker (P4); sources restored |
| `verify_capabilities.py` | production `--handheld` PASS (101 TUs); prototype `--native-prototype` PASS (103 TUs); Windows PASS (98 TUs); each configuration rejected under the other two audits (`evidence\verify-negative.log`) |
| Firmware builds (ESP-IDF 6.1, Xtensa GCC 15.2.0, `--no-ccache`, v0.5.0 `sdkconfig`, `-j 2`, `-Werror`, `-fstack-usage`) | production and prototype PASS, both pass `check_app_budget.py` (warn < 128 KiB, fail < 64 KiB free; unchanged) |

## 6. New tests

`tdeck_native_view_runtime` builds the prototype's configuration on the host: the real
`AlphaRuntime`, `tdeck_board.cpp` **and** `board_dos_layout.cpp` over the fake ST7789, a core
with only the interaction fixed, and `native_view_prototype.cpp`, driven by raw T-Deck input.

| Group | Checks |
| --- | --- |
| P policy | interaction fixed / presentation dynamic; Native = screen + panels, handheld interaction, Difficulty/Cheats kept, no DOS tables; no interaction or journey hook installed; Enhanced; marker parser (8 cases); no file → Native |
| G geometry | frame (band, map box, roster box, console rules); the 176 × 176 map at (8,8) equal pixel for pixel to the composed viewport; IBM.CH roster glyph at (192,8); console window x 192–319, y 88–191; rows 200–239 black |
| Y pacing | every window over 16 rows (the 320 × 240 frame, map, panel) pauses at least every 16 rows |
| D dirty regions | an unchanged frame writes nothing; a one-tile move repaints the map window, not the frame |
| S strip | Look's prompt and the Cast list's legend appear; Mic clears the strip to black |
| T transitions | handheld → Native, System Menu and back, shop on the handheld screen and back, Native → Enhanced: each one full-panel window |
| B bounds | no window outside 320 × 240, no malformed pixel data |
| I input | one raw script under Enhanced and Native — trackball, trackball click (WASD) + `d`/`w`, Mic long hold ×2, Look + direction, Mic cancel, Cast → member → spell list, Z-stats + Symbol+Mic `0`, Alt+M, Alt+S + confirm, Alt+L + confirm, a conversation with Chuckles (`job`, `bye`): identical 30-step interaction trace, transcript and Alt+S GAM bytes |

## 7. Firmware size

`firmware_report.py` (compiler-section sizes; frames are per-function compiler estimates, not
whole-stack bounds):

| Bytes | v0.5.0 | Phase 1 | Phase 2 production | Phase 2 prototype | prototype − Phase 1 |
| --- | ---: | ---: | ---: | ---: | ---: |
| App image | 1,075,504 | 1,075,520 | 1,075,520 | 1,092,176 | +16,656 |
| App partition | 1,310,720 | 1,310,720 | 1,310,720 | 1,310,720 | 0 |
| Partition free | 235,216 | 235,200 | 235,200 | 218,544 | −16,656 |
| Launcher allocation | 1,114,112 | 1,114,112 | 1,114,112 | 1,114,112 | 0 (no boundary crossed) |
| Launcher allocation free | 38,608 | 38,592 | 38,592 | 21,936 | −16,656 |
| `.flash.text` | 738,902 | 738,902 | 738,902 | 754,306 | +15,404 |
| `.flash.rodata` | 237,020 | 237,020 | 237,020 | 238,268 | +1,248 |
| `.iram0.text` | 76,039 | 76,039 | 76,039 | 76,039 | 0 |
| `.dram0.data` | 22,091 | 22,107 | 22,107 | 22,107 | 0 |
| `.dram0.bss` | 54,096 | 54,096 | 54,096 | 54,096 | 0 |

The production image is the same size as Phase 1 in every section, and every stack frame is
the same. The prototype's 16.3 KiB is far below the feasibility audit's 32–96 KiB estimate:
the painter, the `paint_rect` instantiations, `compose_dos_panel()` (now live: frame
32 → 1,152 B) and the selector. No framebuffer is added: the DOS screen streams rows through
the existing `transfer_row_` buffer. Its caches are heap objects created on the first DOS
frame (`DosScreenCache`, about 2 KiB with the strip; `DeviceDosPanel`); no JSON is loaded.
New compiler frames: `draw_dos_console` 2,752 B (13 rows of text plus the rendered lines),
`show_alpha_dos` 336 B, `paint_rect` instances 144 B; `show_alpha` grows 2,640 → 2,688 B. The
deepest new chain (`show_alpha` → `show_alpha_dos` → `draw_dos_console` → `paint_rect`) is
about 6 KiB of the 24 KiB main task stack by these estimates; the largest frame is still
`DeviceSmokeTests::run` (5,920 B). Runtime heap and stack high-water marks need the device's
own logs (`stack_margin`, `internal`, `psram` in the render logs): **not measured**.

## 8. Development package

Built from the clean commit after this record; its identity is recorded in the follow-up commit below this line.

## 9. Hardware checklist (all **PENDING**)

Flash the prototype package with Launcher. Delete `/ultima5/dev/presentation.txt` (or
leave it absent) for Native; put `enhanced` in it for Enhanced. Watch the serial log for
`NATIVE_VIEW_PROTOTYPE presentation=…`.

| # | Test | Expected | Result |
| --- | --- | --- | --- |
| 1 | Boot and title | identity screen shows `0.5.0-nativeview-p2-debug`; title, intro and menus are the handheld's screens | PENDING |
| 2 | Enter Native View | New/Continue a journey: the DOS frame, map at top-left, roster/status boxes, console; bottom 40 rows black | PENDING |
| 3 | Overworld rendering | sky/moons band, wind band, animated water, no tearing or stale pixels | PENDING |
| 4 | Trackball movement | steps as on Enhanced; map redraws, frame does not flicker | PENDING |
| 5 | WASD | trackball click toggles; `W/A/S/D` move; click back | PENDING |
| 6 | Command keys | Look/Talk/Get/Enter/Klimb etc. echo on the 16-column console | PENDING |
| 7 | DOS console | text wraps at 16 columns, newest at bottom, cursor blinks | PENDING |
| 8 | Status panel | roster names/HP/status, F/G, date update after actions; Ready/Use/Z-stats show the 1988 panel | PENDING |
| 9 | Alt+M | System Menu opens; Mic returns to a complete Native frame | PENDING |
| 10 | Confirm/Cancel | Enter confirms; Mic short release cancels; a prompt's question and keys appear in the bottom strip and vanish when it closes; Mic hold toggles Movement Mode; Symbol+Mic gives `0` | PENDING |
| 11 | Enhanced View | write `enhanced` to the marker, reboot: the handheld screen, unchanged | PENDING |
| 12 | Save/load | Alt+S / Alt+L; the save loads in the production v0.5.0 image too | PENDING |
| 13 | Audio during redraws | music and SFX do not stutter on full Native repaints (System Menu exit, shop exit) | PENDING |
| 14 | Extended play | 30+ minutes including a shop, combat, a dungeon: no freeze, no watchdog reset (serial log) | PENDING |

## 10. Phase 3 prerequisites

1. The user's review of this branch and the §9 results, especially 13 and 14 (pacing and
   watchdog behaviour cannot be proven on the host).
2. DATA.OVL short-name tables on the T-Deck (a pack field or extraction) for `dos_text`.
3. Native forms for: member/spell lists (with their details), shops, the guidance strip
   (replacing the interim prompt strip), the Movement Mode indicator.
4. A design for a player-facing selector (Settings row and persistence), which must stay
   readable by v0.5.0's `settings.json` reader.
5. A size budget for the production image if Native View ships there (§7 measures the cost).
