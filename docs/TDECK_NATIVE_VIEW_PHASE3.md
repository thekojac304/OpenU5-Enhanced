# T-Deck Native View — Phase 3: functional UI parity (prototype)

Status: implemented on a feature branch (`d09e0f41`; development package recorded in §9). Native View now keeps
ordinary play — shops, lists, the member picker, prompts — on the DOS screen; the production image is
unchanged. Nothing is merged, pushed or published. **No physical T-Deck testing has been done for this
phase** (§10). Phase 4 (mode switching) has not begun.

## 1. Starting point

| Item | Value |
| --- | --- |
| Phase 2 | `tdeck/native-view-phase2` at `97c6278a` (package record) on `6f40c96f` (renderer), clean, unchanged by this phase |
| Phase 1 | `a37e05c8` on `0ec1b481`, an ancestor of Phase 2 |
| `v0.5.0` | tag → `355d4a1e…`, an ancestor of Phase 2 (verified with `git merge-base --is-ancestor`) |
| Phase 3 branch / worktree | `tdeck/native-view-phase3` from `97c6278a`; `C:\Dev\U5Native\worktrees\u5-tdeck-native-p3` |
| Build outputs | `C:\Dev\U5Native\builds\u5-tdeck-native-p3\phase3-a1\` |

The worktree was created from `97c6278a`, then every tracked file was re-checked out with
`core.autocrlf=false` (the system Git config sets it `true`; the Phase 1/2 records explain why exact
bytes matter). The git-ignored inputs were copied from the Phase 2 worktree (packs, map fixtures,
`loot_names.inc`, `sdkconfig`, `build-dialogue`, `build-shops`, `game/assets`) and `original/` and
`node_modules/` are junctions to `source`, as Phase 1 and 2 did.

## 2. UI parity inventory (before implementation)

Columns: **E** Enhanced View (T-Deck v0.5.0 handheld screen); **W** Windows Original (1988
interaction and presentation); **P2** the Phase 2 Native prototype (DOS 320 × 200 screen with the
handheld interaction); **Gap** what P2 loses or hides; **Keep** the handheld interaction that must
not change; **Change** what Phase 3 does. *Adaptation* marks an intentional handheld difference, not
a defect.

| # | Interface | E | W | P2 | Gap | Keep | Change |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Overworld / town movement | 176² viewport clipped by 9-px sky/wind strips, roster, status box with location and `MOVE` | DOS frame, map whole at (8,8), bands outside it | as W | no Movement (WASD) Mode indicator | trackball steps, click / Mic-hold WASD toggle | `MOVE` on the strip while WASD Mode is active and nothing else is shown there (§4) |
| 2 | Dungeon navigation | level / facing in the two strips | `L#` and `Dir:` bands | as W | none | trackball turns/steps | none |
| 3 | Combat | actor tinted, target reticle, roster | actor row in reverse video | as W (dos_panels) | none | trackball target, Confirm, Mic | none |
| 4 | Combat targeting / party member choice | compact selector list with HP/MP detail | roster picker, `Select:` banner | generic list in the DOS list frame, rows cut to 13, no detail, only 7 of the 8 rows of each page | detail hidden; row 8 of each page invisible | Up/Down, Confirm, Mic, CampGuard digits | **roster picker presentation**: the roster box with the cursor row in reverse video under `Select:`; the question, the member's HP/MP/status on the strip |
| 5 | Conversation / text entry | transcript + context bar `prompt >input` | console, typed | console (16 × 13) + strip | none | keyboard text entry | none (verified) |
| 6 | Inventory / Use | compact selector, long names, `xN` | `Items:` DOS panel, DATA.OVL (U)se names, sigils | DOS panel with **device long names** (no `dos_text` on the T-Deck), cut at 13 | names cut (`Yellow Potion x` …), indistinguishable cases | list order and keys | DATA.OVL names on the T-Deck (§3, item 3); highlighted entry's full name and count on the strip |
| 7 | Ready / equipment | selector + `Slot:` / `Current:` detail | member-name banner, `NN`/`--`, class glyph, DS 0x1962 short names | DOS panel, device names cut at 13, no slot detail | names cut; slot / current item hidden | member pick then item pick | short names (§3, item 3); full name, slot and the slot's current item on the strip |
| 8 | Ready / Use member choice | selector list | roster picker | generic list | as #4 | as #4 | as #4 |
| 9 | Cast — member, spell list | member list; spell list with effect / target / MP detail | typed spell name | generic list, 13-cell rows (`In Vas Por Yl…`), no detail | effect, target, MP hidden; names cut | spell list (not typed), availability rules | spell list as a framed DOS list: `NN-` count + DATA.OVL short spell name; full name, effect, target, MP on the strip |
| 10 | Mix — spell, reagents, quantity | spell list; reagent list with marks | typed name; `Reagents:` panel | spell: generic list; reagents: DOS panel with device names | spell list as #9; reagent names long | Enter marks, `M` mixes, Mic cancels; NumericEntry quantity | as #9; DATA.OVL reagent names; the spell being mixed and the highlighted reagent's full name on the strip |
| 11 | Reagent lists / quantities (Z-stats) | Z-stats page list | DOS list, `NN-Name` | DOS panel, device names | names | page keys | DATA.OVL names |
| 12 | Z-stats / character details | selector: stats rows | 1988 pages (♂/♀, `Str=`, Arms short names, lists) | 1988 pages, device names for Arms / lists | names | Left/Right page, Up/Down scroll, digits jump, Mic/Space close | DATA.OVL names; legend unchanged |
| 13 | Z member pick | detail panel (Class, HP, MP, STR…) | roster picker | generic list (no detail) | detail hidden | as #4 | roster picker + member detail on the strip |
| 14 | New Order (move from / to) | selector | roster picker (`Swap` / `with`) | generic list | as #4 | as #4 | roster picker |
| 15 | Shops — buy / sell / reagents / guild / ships / wine | handheld shop panel: title, keeper, phase/page, 6 priced rows, gold, log, legend | console shops (letters), `Arms` window | **falls back to the handheld (Enhanced) screen** | not Native | Up/Down, Confirm, letters a–z, Mic back, B/S etc. | **Native shop panel** in the DOS panel: phase banner, priced rows (DOS short names when long), gold, scroll arrow; full name / count / price and the keys on the strip (§4) |
| 16 | Shops — deal Y/N, insufficient gold, full, declined, cancelled | shop panel + `Y Yes|N No` | console questions | fallback | as #15 | Y/N/Confirm/Mic | the panel keeps the item row; the question and keys on the strip; results on the console |
| 17 | Healer / inn member choice | shop panel rows of companions | roster picker | fallback | as #15 | as #15 | roster picker with the service price on the strip |
| 18 | Inn pick-up (guest register) | shop panel | `GUEST REGISTER` DOS panel | fallback | as #15 | as #15 | the 1988 `GUEST REGISTER` panel (presentation reused) |
| 19 | Shop menus (services, tavern, healer need, pause) | world screen + keeper / keys bar | console | DOS screen + strip | none | menu letters | none (verified) |
| 20 | Rations / rest hours / numbers | context bar `prompt >digits` | console | strip | none | NumericEntry | none (verified) |
| 21 | Camp and rest | camp viewport, guard choice | camp viewport, roster picker | camp viewport; guard as generic list | as #4 | digits 1–9 choose the guard | roster picker |
| 22 | Food, gold, date, torches | status box | status box | status box (F/G/date) | clock not shown (DOS has none) | — | adaptation: none |
| 23 | Quest items / special inventory | Use list, Z-stats lists | DOS (U)se sigils (scroll runes, potion colours, `Moonstone N`) | device names | sigils absent | — | DATA.OVL (U)se entries with their sigils |
| 24 | Gem / zodiac view | full-square viewport | whole map | whole map | none | — | none |
| 25 | Save / load (Alt+S / Alt+L) | prompt bar | (Q)uit & Save | strip | none | Alt+S/L + confirm | none (verified) |
| 26 | System Menu, settings, Developer | handheld screens | handheld screens | handheld screens | — | — | adaptation: none |
| 27 | Intro, character creation, title | handheld screens | handheld screens | handheld screens | — | — | adaptation: none |
| 28 | Death / defeat, Lord British, victory, ending | narrative scenes on the game screen; ending pages at y 20 | DOS screen; pages at y 0 | shared code path (`top = 0`), not exercised | not tested | — | exercised by a test (§8.1, E1) |
| 29 | Text Size setting | changes the console | n/a | no effect (fixed 16 × 13) | — | — | adaptation: none |
| 30 | Shift+Up/Down transcript paging | by the console on screen | DOS console | DOS console geometry | none | — | none |

Defects found while taking the inventory (Phase 2 code, fixed in Phase 3):

- **D1** The generic DOS list (`show_alpha_dos`) shows at most 7 of the selector's 8-row page; with the
  cursor on row 8 of a page the highlight is invisible.
- **D2** `compose_selection_view()` clears the Ready slot / current-item detail under
  `platform_dos_panels()`; on the T-Deck nothing else shows it.
- **D3** `dos_text` is null on the T-Deck, although the resource pack already carries `data.json`
  (the player's DATA.OVL dump the Windows port reads).

## 3. Approach

Phase 1's three groups stand: **presentation** chosen at boot by the developer marker, **interaction**
fixed to the handheld's at compile time, **journey** untouched. Phase 3 adds no interaction code path
and no gameplay code. Every new form is presentation of state the handheld screen already shows.

1. **The Native frame** (`DeviceNativeFrame`, `device_ui_views.h`). For the DOS presentation with the
   handheld interaction — `platform_dos_panels() && !platform_dos_interface()`, true only on the
   T-Deck prototype's Native View — `AlphaRuntime::compose_native_frame()` composes, every gameplay
   frame and from the **same** three views the handheld screen gets (`compose_shop_view()`,
   `compose_selection_view()`, `compose_context_bar()`) and the same runtime state:
   - `panel`: the right panel's DOS form (a `DeviceDosPanel`), or null for the roster and status
     boxes, which with the party highlight's cursor and `Select:` are the member picker;
   - `strip`: the 40 rows under the picture — a rule and three rows, *question* (cyan), *detail*
     (white), *keys* (green) — and Movement (WASD) Mode's `MOVE`.

   `render()` composes the three views once into locals and hands the frame to the Board
   (`Board::set_native_frame`) before `show_alpha`. Windows Original and Enhanced get none
   (`native_presentation()` is false there; checked by `tdeck_native_view_phase3_desktop`).
   All of it is compiled only where `OPENU5_DOS_SCREEN_LAYOUT` is defined (Windows and the
   prototype); the production firmware compiles none of it.
2. **No shop fallback.** `Board::show_alpha` leaves the DOS screen for a shop only when no Native
   frame was composed (an inconsistent configuration). The count is `Board::handheld_fallbacks()`;
   every test asserts it stays 0.
3. **DOS names on the T-Deck** (`openu5/dos_text_tables.{h,cpp}`, `native_view_prototype.cpp`). At
   boot, while the resource pack is still open, the prototype reads `data.json` (the extractor's dump
   of the player's DATA.OVL, already in the pack and identity-locked with it) into a 54 KB PSRAM
   buffer, reduces it with a single-pass scanner — no tree, no `std::map`, no per-string allocation
   — to one fixed **1,762-byte** `DosTextBlock` (859 of 1,536 arena bytes used with the player's
   data) in PSRAM, and frees the document. The block keeps the potion colours, reagents, spells, the
   38 (U)se entries (assembled as Windows assembles them) and the nine DGROUP literals the panels
   print. The two equipment tables (DS 0x1962 names, DS 0x1ae8 glyphs) come from the copies the
   repository tracks for the web port, generated at build time by the same `cmake/DosTables.cmake`
   Windows uses. Nothing new of the game's text is tracked or compiled in. The profile's `dos_text`
   hook is installed whichever presentation boots (only DOS panels ask), so a later switch finds the
   names ready. Without `data.json` the panels fall back to the device's own names, as in Phase 2.
4. **Presentation changes of a running game** (Phase 4 preparation). `render()` (prototype builds
   only) notices that `platform_dos_screen()` changed since the last gameplay frame and makes the
   next frame a whole repaint, refreshing the console's paging geometry at once. With
   `Board::set_dos_layout` (which already invalidated both screens' caches) the switch is two calls
   between frames; no game state is touched (tested at the command prompt, with a list open and
   with a shop open).

## 4. Interfaces implemented or corrected

| Interface | Native presentation now | Strip |
| --- | --- | --- |
| Shop lists: Buy, Sell, Reagents, Guild, Ships, Drinks | the panel's box: banner (`Buy`, `Sell`, `Reagents`, …), seven rows of `name (10 cells)` and the price right-aligned to cell 14, the bar in reverse video over cols 0–14, `Gold` and the party's gold on the window's last row, the scroll arrow in the band below; a name longer than 10 cells shows the DOS short name (DS 0x1962 for arms, DATA.OVL's reagents), else the device's name cut with a `.` | `<shop> - <Phase> n/N`; the highlighted row's **whole** name and price (Sell: and how many the party has); the handheld keys |
| Shop deals (Y/N): buy, sell, services, horses, ships, rest, rumours, rounds | the same panel with the deal's one row under the bar | the deal's item and price; `Y Yes|N No|Mic Back` |
| Not enough gold, declined, cancelled, sold out | unchanged shop rules; the keeper's words on the console; the panel's gold row | — |
| Healer / inn member choice | the roster with the cursor's row in reverse video under `Select:` (the 1988 roster picker) | member HP and status |
| Inn pick-up (two or more guests) | the 1988 GUEST REGISTER panel (presentation now shared with Windows Original's) | the highlighted guest |
| Shop menus, rations / rumour entry | the world panel (unchanged) | the keeper and the menu keys; `prompt >digits` |
| Member choices (Cast, Ready, Use on, Look/Search, Camp guard, New Order, Z member, spell target) | the roster picker | the list's question; name, HP/max, MP, level, status |
| Ready | the 1988 item page with DS 0x1962 names and class glyphs | the whole label (`Leather Armour x2 [equipped]`); `Slot: … Current: …` (D2 fixed) |
| Use | the 1988 `Items:` page with DATA.OVL's (U)se entries and sigils (scroll runes, potion colours, moonstone phases) | the whole label (`Blue Potion x2`) |
| Cast / Mix spell list | the framed list of the Z-stats spell page: `NN-` (or `--`) and DATA.OVL's short spell name; minimal scroll and arrow | the whole name and count; effect; target; MP |
| Mix reagents | the 1988 `Reagents:` panel with DATA.OVL's names | `Mix: <spell>`; the reagent's whole name, count, `(marked)` |
| Z-stats | the 1988 pages, now with DS 0x1962 / DATA.OVL names (Arms, lists) | `Z-stats: <banner>`; page position |
| Any other list | the framed list of the device's labels (fit with a `.`), minimal scroll around the cursor | the whole label |
| Prompts (direction, Y/N, text, number, conversation) | — | question and keys (Phase 2's content, now in the 3-row geometry) |
| Movement (WASD) Mode | — | `MOVE` (green) at the right end of the keys' row, alone when nothing else is open; the keys stop short of it |
| Phase 2's generic list (no Native frame) | keeps the highlighted row in view (D1 fixed for that path too) | — |

## 5. Remaining intentional handheld differences

- **Interaction**: lists, shops and prompts keep the handheld's keys, cursors, letters, Confirm /
  Cancel and digits. No typed spell names, no console shops, no 1988 `Player:` key rules.
- **The strip** (rows 200–239) exists only on the T-Deck's taller panel; the DOS picture is 1:1 and
  untouched above it. It is black whenever nothing is open and WASD Mode is off.
- **Shop panels** are new forms in the DOS panel's geometry: the 1988 shops print their lists on
  the console with letters and no panel (except `Arms` and the guest register). The Native shop
  shows the handheld's priced list instead, because the handheld interaction selects with a cursor.
  Prices and the gold row are on screen; the keeper's words still print on the console.
- **Sell** shows priced rows with DS 0x1962 names rather than the 1988 `Arms` window (`qq-Name`, no
  prices): the count moves to the strip.
- **Member choices** use the 1988 roster picker's look; the cursor still moves with the trackball
  and the choice is the handheld list's.
- The System Menu, title, character creation and Developer screen stay the handheld's own screens.
- The Text Size setting has no effect on the Native screen (the DOS console is 16 × 13).
- The roster's status box shows no clock (DOS has none); the handheld's location caption is not shown.

## 6. Incomplete or deferred

- **Hardware**: nothing in this phase has run on a T-Deck (§10).
- The strip uses the handheld's 5 × 7 face in 6-px cells, as the handheld's own context bar does;
  its legibility on the glass is a hardware check (§10 item 7), not proven here.
- Shop rows longer than 10 cells with no DOS short name (guild items, ships, drinks are all short)
  would be cut with a `.`; the strip always carries the whole name.
- Combat target choice on the map and the conversation screens are unchanged from Phase 2 (already
  usable; inventory rows 3, 5).
- Windows' own `data.json` reader counts a UTF-8-encoded byte as two, which misses DS 0x48b8
  `Items:` (its fallback is the same text, so nothing visible). Recorded, not changed: Windows is out
  of scope. The T-Deck extractor decodes them (test N4/N6).
- Pre-existing, shared by both presentations and unchanged (shop rules are out of scope): at the
  healer's "Anything else?" gate the Mic is no key (the strip says `Mic: back`); N leaves.


## 7. Resources

`firmware_report.py` on the validation builds (`phase3-a1\tdeck-a1-production`,
`tdeck-a2-prototype`; the `a1` prototype stopped on a real `-Werror=format-truncation` diagnostic,
fixed before `a2`, logs kept). Phase 2 figures are its post-commit builds.

| Bytes | Phase 2 production | Phase 3 production | Phase 2 prototype | Phase 3 prototype | Δ prototype |
| --- | ---: | ---: | ---: | ---: | ---: |
| App image | 1,075,520 | **1,075,520** | 1,092,176 | **1,104,080** | +11,904 |
| App partition | 1,310,720 | 1,310,720 | 1,310,720 | 1,310,720 | 0 |
| Partition free | 235,200 | 235,200 | 218,544 | 206,640 | −11,904 |
| Launcher allocation | 1,114,112 | 1,114,112 | 1,114,112 | 1,114,112 | 0 (no 64 KiB step crossed) |
| Launcher allocation free | 38,592 | 38,592 | 21,936 | 10,032 | −11,904 |
| `.flash.text` | 738,902 | 738,902 | 754,306 | 764,806 | +10,500 |
| `.flash.rodata` | 237,020 | 237,020 | 238,268 | 239,660 | +1,392 |
| `.iram0.text` | 76,039 | 76,039 | 76,039 | 76,039 | 0 |
| `.dram0.data` | 22,107 | 22,107 | 22,107 | 22,123 | +16 |
| `.dram0.bss` | 54,096 | 54,096 | 54,096 | 54,208 | +112 |

- **Production is unchanged**: the same image size, every section and every compiler stack frame
  identical to Phase 2's. All Phase 3 code is behind `OPENU5_DOS_SCREEN_LAYOUT` or a constant-false
  presentation predicate; `verify_capabilities.py` now also requires that the production image builds
  neither `native_view_prototype.cpp` nor `dos_text_tables.cpp`.
- Both images pass `check_app_budget.py` (warn below 128 KiB free, fail below 64 KiB; unchanged).
  The prototype's next 64 KiB Launcher step is 10,032 bytes away: a later phase that adds more than
  that crosses it (an allocation of 1,179,648 bytes), which is not by itself a failure.
- **Memory** (no framebuffer added; rows still stream through the 640-byte `transfer_row_`):
  - PSRAM, at boot only: the 54,874-byte `data.json` while it is scanned, freed at once.
  - PSRAM, kept: the 1,762-byte `DosTextBlock`.
  - Internal heap, on the first Native frame: `NativeFrameState` (a strip, a frame and a
    `DeviceDosPanel`, about 380 bytes).
  - Internal heap: the Board's `DosScreenCache` grows by 80 bytes (the 3-row strip cache).
  - `.bss`: the 48 one-character equipment glyph strings (96 bytes) and two pointers.
  - Flash: the 48 DS 0x1962 names and glyphs (generated table), and the code.
- **Stack** (compiler frames, `-fstack-usage`; not whole-chain bounds): `show_alpha_dos` 336 → 496 B,
  `draw_dos_context_strip` 48 → 224 B, `compose_party_highlight` 64 → 80 B; new
  `compose_native_frame` 208 B, `compose_native_list` 336 B, `compose_native_shop` 128 B. The
  deepest draw chain (`render` 2,720 → `show_alpha` 2,688 → `show_alpha_dos` 496 →
  `draw_dos_console` 2,752 → `paint_rect` 144) is about 8.8 KB of the 24 KB main task stack by these
  estimates. The `a2` build showed `dos_text_extract` at 1,824 B (a `DosTextBlock{}` temporary at
  boot); it now clears the block in place (post-commit frame in §9). The largest frame is still
  `DeviceSmokeTests::run` (5,920 B).
- **CPU**: the Native frame is recomposed with each drawn gameplay frame from state already
  composed for the views (a list's or shop's seven rows); the Board redraws only a strip row or a
  panel that changed (`cache.strip == now` and the panel `memcmp`), so an idle frame still writes no
  window (R2). No cooperative yield was removed; every window over 16 rows is paced (S16, Y1).
- Runtime heap and stack high-water marks need the device's own `METRICS` lines: **not measured**.

## 8. Regression results

All from this branch's source in `C:\Dev\U5Native\builds\u5-tdeck-native-p3\phase3-a1`
(logs and JSON in `evidence\`).

| Suite | Result |
| --- | --- |
| Host baseline, unmodified Phase 2 tree in the new worktree | **218/218** (`baseline-ctest.log`) |
| Host, Phase 3 (GCC 16.2.0, developer tools and RC1 differential on) | **220/220** = 218 + `tdeck_native_view_phase3_runtime` (62 checks) + `tdeck_native_view_phase3_desktop` (2 checks); `tdeck_native_view_runtime` **28/28** (T3a/T3b rewritten, §8.1); no warnings |
| Windows (MSVC 19.51, SDL 3.4.16 offline source, `ctest -j 1`) | **40/40**, no warnings, including the Original DOS-screen framebuffer scenarios (`presentation_dos`, `original_dos_panels`, `original_dos_panels_handheld`, `original_arena`, `original_camp`, `original_keys`, `original_interface`); no baseline regenerated |
| Release tooling (`test_package_smoke`, `test_release_manifest`) | **7/7** (`test_cleanup_builds` 16, 1 skipped) |
| `differential_check.py` vs the v0.5.0 final traces | **byte-identical**, unpaced and paced, 1,065 walks / 8,514 snapshots each (SHA-256 `8189acce…` / `2efc4253…`, as Phase 2); the DOS positive control differs in 1,054 walks |
| `mutation_check.py` (production + Windows compile commands) | **10/10** boundary mutants killed, header restored |
| Phase 3 mutants (`phase3-a1\phase3_mutations.py`) | **15/15** killed (§8.2) |
| `verify_capabilities.py` matrix | production `--handheld` PASS (101 TUs), prototype `--native-prototype` PASS (104 TUs), Windows PASS (98 TUs); each rejected under the other two audits (`verify-matrix.log`); the two new rules' 4 synthetic mutants killed (`verify-new-rules.log`) |
| Firmware (ESP-IDF 6.1, Xtensa GCC 15.2.0, `--no-ccache`, v0.5.0 `sdkconfig`, `-j 2`, `-Werror`, `-fstack-usage`) | production and prototype PASS, both pass `check_app_budget.py` |

### 8.1 Tests

`tdeck_native_view_phase3_runtime` builds the prototype's configuration on the host (the real
`AlphaRuntime`, `tdeck_board.cpp`, `board_dos_layout.cpp`, `native_view_prototype.cpp` and
`dos_text_tables.cpp` over the fake ST7789, the core with only the interaction fixed), loads the
pack's `data.json` as the device does, and drives raw T-Deck input. Screens are read back from the
fake panel's GRAM (the panel's cells decoded against IBM.CH; `--dump <dir>` writes PNGs).

| Group | Checks |
| --- | --- |
| N names | the pack's `data.json` → one block (859 / 1,536 arena bytes); 48 spells and 8 reagents ≤ 10 cells; the 38 (U)se entries with their sigils in order; the DGROUP literals by address; a synthetic document's escapes and UTF-8; 47 spells, truncation and arena overflow refused; the profile's hook serves DS 0x1962 (`Leath Helm`) and the glyphs |
| S shops | the blacksmith's menu and Buy list on the Native screen; banner, priced rows ending in cell 14, bar, gold row; the strip's shop / position / whole name / price; the bar follows the trackball; buy (gold −price), decline (unchanged), 1 gold refused by the smith's own words; Mic out to a black strip; Sell with distinct look-alike rings and `(have n)`; a 10-row list scrolled end to end with the arrows; a sale; Reagents and Guild panels; the healer's roster picker (cursor, `Select:`, the member on the strip) and its exit; the inn's Leave on the roster and the GUEST REGISTER with two guests; no malformed data, bounds, every tall window paced; `handheld_fallbacks() == 0` throughout |
| L lists | Ready's member on the roster with HP/MP; the Ready page's `Leath Helm`, the whole name and `Slot: … Current: …`; Use's `Items:` page with RUNES.CH sigils; Cast's member cursor and `MP 20`; the framed spell list (`NN-name`) with effect / target / MP; Mix's 48 spells end to end (bar in view, arrows, 48 distinct names); Mix's reagents and `(marked)`; Z-stats pages with DS 0x1962 names; every close back to the roster and a black strip |
| M/P strip | WASD off: black; the trackball click: `MOVE` alone, the DOS picture untouched; `d` moves; Look's prompt with `MOVE` beside its keys; Mic back to `MOVE` alone; Mic held: black; a 52-cell legend's window stops short of `MOVE`'s; a text prompt with what is typed, then black; Cast → spells → Mic, Mic: no stale detail row |
| R/X/E | Alt+M over the Ready page and back: whole repaint, page and strip as they were; an idle frame writes nothing; Native ↔ Enhanced at the prompt, with Cast's list and with a shop open: whole repaints, the right form each way, game state untouched; an ending page at rows 0..199 (y 20 on Enhanced), then a whole repaint |
| I identity | one raw script (blacksmith buy / decline / sell, healer, reagents, Ready, Cast, Mix, Z-stats, Alt+S) under Enhanced and Native: the **31-step interaction trace** (modes, phases, cursors, prompts, legends, gold, items, spells, reagents, HP), the transcript, the whole `GameState` and the Alt+S GAM bytes are identical; no fallback |
| W (`tdeck_native_view_phase3_desktop`, dynamic core as Windows) | the Windows Original profile (Ready, the roster picker, a console shop) and Enhanced: no Native frame, no strip, no fallback |

`tdeck_native_view_runtime` (Phase 2): T3a/T3b asserted the handheld-screen shop fallback that this
phase removes; they now assert the shop stays on the Native screen and closes to the roster without
leaving the frame. P1's wording no longer says "no DOS tables". No other expectation changed; its
30-step Enhanced/Native trace (I1–I5) is identical as before.

### 8.2 Mutants

`phase3_mutations.py` changes one fragment, rebuilds the test(s) it names and restores exact bytes:
no Native frame; the shop fallback back; no roster cursor; no strip detail; no `MOVE`; the keys under
`MOVE`; no `dos_text` hook; UTF-8 counted as raw bytes; no presentation-change detection; a shop
list or the spell list pinned at its top; the strip never cleared; the guest register for the 1988
shops only; Native frames for Windows Original (killed by `tdeck_native_view_phase3_desktop`); the
Sell count dropped from the strip. The first run killed 14; *the keys under `MOVE`* survived because
`MOVE`'s own box covers the cut on screen — the assertion (M4) was weak. It was rewritten (M4) and a
Board-level check of the windows written (M4b, no overdraw) added; the final run of all 15 against
the final tests killed **15/15** (`phase3-mutations-final.log`).

## 9. Development package

Built fresh from the clean implementation commit `d09e0f414df4d70f6a4400d10025addcfe47d617` (`d09e0f41`; the image embeds
`d09e0f414df4`, no `-dirty`), then packaged by `native/targets/tdeck/package_launcher.py`.

| Item | Value |
| --- | --- |
| File | `OpenU5-TDeck-Alpha0.5.0-nativeview-p3-Debug-Launcher.bin` |
| Location | `C:\Dev\U5Native\builds\u5-tdeck-native-p3\package-d09e0f41\` (with `SHA256SUMS.txt` and the `sdkconfig` used); built in `phase3-a1\tdeck-postcommit-prototype` |
| SHA-256 | `19d5984479950be495e501e538ca6741b543a742c2b6260da698ec00bb6e82d1` |
| Size | 1,104,128 bytes (Launcher allocation 1,114,112; 9,985 bytes to the next 64 KiB step; 206,592 bytes of the app partition free) |
| Version / FW line | `0.5.0-nativeview-p3-debug` (a development build, not a release) |
| Configuration | ESP-IDF 6.1, Xtensa GCC 15.2.0, v0.5.0 `sdkconfig`, `--no-ccache`, `-D OPENU5_NATIVE_VIEW_PROTOTYPE=ON`, developer tools ON (as production) |
| Activation | flash with Launcher; Native by default; `enhanced` as the first word of `/sd/ultima5/dev/presentation.txt` selects Enhanced (reboot to change); the serial line `NATIVE_VIEW_PROTOTYPE presentation=… dos_text=loaded` confirms both |

The post-commit images: production `0.5.0-rc2` **1,075,520** bytes, every section and stack frame
identical to Phase 2's (audit PASS, 101 TUs); prototype **1,104,128** bytes (`.flash.text` 764,874,
`.flash.rodata` 239,644, `.iram0.text` 76,039, `.dram0.data` 22,123, `.dram0.bss` 54,208; audit PASS,
104 TUs) — 48 bytes over the `a2` validation image (§7) for the two fixes made after it (the bounded
strip format and the in-place block clear; `dos_text_extract`'s frame is now 48 B). The Windows
build of the same commit passes **40/40** with no warnings (`windows-postcommit-*.log`). Nothing
under `C:\Dev\U5Native\releases` was written; `v0.5.0` (`355d4a1e…`), `main` and `origin/main`
(`db562274…`) are unchanged.

## 10. Hardware acceptance checklist (all **PENDING**)

Flash the Phase 3 package with Launcher (§9). Leave `/ultima5/dev/presentation.txt` absent for
Native; write `enhanced` in it for Enhanced (reboot to change). The serial log's
`NATIVE_VIEW_PROTOTYPE presentation=… dos_text=loaded` line confirms the selection and the names.
Phase 2's user report (Native gameplay looked and played normally; photographs of the dialogue and
Mix Spells layouts) is limited evidence for **Phase 2**; it does not cover any of the items below.

| # | Test | Expected | Result |
| --- | --- | --- | --- |
| 1 | Boot | identity screen `FW 0.5.0-nativeview-p3-debug`; serial `dos_text=loaded` | PENDING |
| 2 | Blacksmith (Britain, Iolo's Bows): Buy | the DOS frame stays; panel `Buy`, priced rows, `Gold`; strip: shop, `Buy n/N`, the whole item name and price, keys | PENDING |
| 3 | Buy with Y; decline with N; too little gold | gold changes / unchanged; the keeper's words on the console; never the handheld shop screen | PENDING |
| 4 | Sell a long-named item (a ring) | rows distinct (`Inv. Ring`, `Prot. Ring`, `Regen Ring`); the strip names it whole with `(have n)`; the list scrolls past 7 rows with the arrow | PENDING |
| 5 | Reagents, guild, healer, inn (leave a member; pick up with two guests: GUEST REGISTER), tavern, shipwright, horses | each usable on the Native screen; member choices on the roster under `Select:` | PENDING |
| 6 | Ready / Use / Z-stats | DS 0x1962 names (`Leath Helm`), (U)se sigils, Z-stats Arms short names; slot and current item on the strip | PENDING |
| 7 | Strip readability | question, detail and keys readable at arm's length; no overlap with the picture | PENDING |
| 8 | Cast (member, spell list) and Mix (48 spells, reagents, marks, quantity) | roster picker; framed spell rows `NN-name`; effect / target / MP on the strip; marks show `(marked)` | PENDING |
| 9 | WASD Mode (trackball click; Mic hold 1.1 s) | `MOVE` at the strip's bottom right, gone when off; never over a prompt's keys | PENDING |
| 10 | Prompt transitions | Look, Yell, Y/N, numbers: the strip opens and closes with no stale row | PENDING |
| 11 | Redraw stability | System Menu over a list and back; shop in and out; no tearing, no stale panel or strip | PENDING |
| 12 | Audio continuity | music and SFX do not stutter during shop panels, list scrolling, full repaints | PENDING |
| 13 | Save / load | Alt+S / Alt+L in Native; the save loads in Enhanced and in the v0.5.0 production image | PENDING |
| 14 | Boot selection Native ↔ Enhanced | marker `enhanced` → handheld screen unchanged from v0.5.0; remove it → Native | PENDING |
| 15 | Extended play | 30+ minutes with shops, combat, a dungeon, conversations: no freeze or watchdog reset (serial); heap / stack lines in the METRICS logs recorded | PENDING |

## 11. Phase 4 (mode switching) prerequisites

Reviewed for a later switch at a safe boundary:

| Area | State after Phase 3 |
| --- | --- |
| Board layout | `Board::set_dos_layout(on)` clears both screens' "drawn" flags; the next frame repaints whole either way (tested X1–X4) |
| Render caches | the DOS screen's cache resets on its full frame; the handheld screen's (`alpha_ui`, viewport, sky, shop, selection, DOS panel, context) are invalidated by its own full repaint; the strip cache resets with the DOS frame |
| Runtime | (prototype builds) a change of `platform_dos_screen()` forces a whole repaint and refreshes the console's paging geometry (`NATIVE_VIEW_PRESENTATION` log line) |
| Modal state | the Native frame is recomposed every frame from live state; the only Native-only state is a list's scroll position (recomputed by the minimal scroll), so a switch with a list or a shop open shows the right panel and strip at once (X3, X4) |
| DOS names | loaded at boot whichever presentation boots |
| Saves / journey | no presentation state is saved; a switch changes no game byte (X1, X2, I3, I4) |

Restrictions and work left for Phase 4:

1. The switch must call `install_prototype_presentation()` (or the future Settings equivalent) and
   `Board::set_dos_layout()` **together**, on the main task, between frames. Doing it while the
   System Menu is open is the natural boundary: its exit repaints whole anyway.
2. The change detection in `render()` is compiled only into prototype builds
   (`OPENU5_NATIVE_VIEW_PROTOTYPE`); a production Native View needs it (or an explicit call).
3. A Settings row and its persistence must stay readable by v0.5.0's `settings.json` reader.
4. Switching during the endgame's dissolve (its PSRAM capture of the panel) is untested; Phase 4
   should refuse a switch while `endgame` is active.
5. If Native View enters the production image, its size cost (§7) moves there too.

## 12. Known risks

- **No hardware run.** Pacing is proven on the host (every window over 16 rows yields every 16
  rows, S16 and Phase 2's Y1); audio and watchdog behaviour under real SPI timing are not.
- **Heap and stack** are compiler estimates and code reading (§7), not measured high-water marks.
- **The data.json scan** runs once at boot (54 KB PSRAM, freed); a pack without it, or a malformed
  one, leaves the device's names (tested N7) and logs `dos_text=none`.
- **Strip legibility** of the 5 × 7 face in three rows is unproven on the glass.
- The healer gate's Mic (pre-existing) may confuse a player reading `Mic: back`.

## 13. Files

| File | Change |
| --- | --- |
| `native/core/include/openu5/dos_text_tables.h`, `native/core/src/dos_text_tables.cpp` (new) | the compact `data.json` extractor and lookup (`DosTextBlock`) |
| `native/targets/tdeck/main/native_view_prototype.{h,cpp}` | DOS-name loading at boot, the `dos_text` hook, the generated equipment tables |
| `native/targets/tdeck/main/main.cpp` | the prototype loads the names while the pack is open (`dos_text=` in the boot log line) |
| `native/targets/tdeck/main/device_ui_views.h` | `DeviceNativeStrip`, `DeviceNativeFrame` (only where `OPENU5_DOS_SCREEN_LAYOUT`) |
| `native/targets/tdeck/main/alpha_runtime.{h,cpp}` | `compose_native_frame/shop/list`; the roster picker highlight; the guest register for the Native inn; views composed once and handed to the Board; presentation-change detection (prototype); `native_frame_for_test()` |
| `native/targets/tdeck/main/tdeck_board.{h,cpp}` | `set_native_frame`, `handheld_fallbacks()`; the shop fallback only without a Native frame |
| `native/targets/tdeck/main/board_dos_layout.cpp` | the 3-row strip with `MOVE`; the Native frame's panel; Phase 2's generic list keeps its cursor in view |
| `native/targets/tdeck/CMakeLists.txt` | prototype version `0.5.0-nativeview-p3-debug` |
| `native/targets/tdeck/main/CMakeLists.txt`, `native/core/CMakeLists.txt` | the extractor and generated tables for the prototype; two new tests |
| `native/targets/windows/cmake/DosTables.cmake` | comment only (the prototype uses it too) |
| `native/tools/rc1/verify_capabilities.py` | prototype must build, production must not, the selector and the extractor |
| `native/targets/tdeck/host_tests/tdeck_native_view_phase3_runtime_test.cpp` (new) | §8.1 |
| `native/targets/tdeck/host_tests/tdeck_native_view_runtime_test.cpp` | T3a/T3b for the removed fallback |

No save format, save slot, settings key, player-visible setting, asset, font, key mapping, input
routing, shop rule, spell rule or Windows behaviour changed. `KeyboardMatrix → RawInputEvent →
UiInputAdapter → UiAction/DeviceShortcut → AlphaRuntime` is untouched. The developer marker
`/sd/ultima5/dev/presentation.txt` still selects the presentation at boot.
