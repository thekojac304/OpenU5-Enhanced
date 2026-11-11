> **About this repository.** OpenU5-Enhanced is the development history behind
> [Ultima V Native](https://github.com/thekojac304/ultima-v-native), which is where releases, downloads and current
> source live ([latest release](https://github.com/thekojac304/ultima-v-native/releases/latest)). `main` here is the OpenU5 browser-port line as of August 2026 (described below). The other branches
> (`enhanced-mode`, `esp32-tdeck-port`, `tdeck/native-view-phase1`–`3`, `windows/original-dos-shops`,
> `preserve/a4-parity1-wip`) and the 42 `alpha2-*`, `alpha3-*` and `alpha4-*` tags preserve work-in-progress lines
> and milestone snapshots that aren't in `ultima-v-native`. GPL-3.0 ([LICENSE](LICENSE), [NOTICE](NOTICE)). No
> original game files are included; you need your own copy of Ultima V.

# OpenU5

**A byte-exact browser port of *Ultima V: Warriors of Destiny* (Origin Systems, 1988).**
TypeScript + PixiJS. Requires your own copy of the original game
([~€5 on GOG](https://www.gog.com/en/game/ultima_456), the *Ultima 4+5+6* pack).

*(hero GIF: walking through Britannia, tactical combat, 3D dungeon)*

## What makes this port different

Every other recreation of Ultima V reinterprets the game. This project **ports the
original engine's rules, re-derived from the binary itself**:

- The `ULTIMA.EXE` kernel and its 24 code overlays were disassembled and every
  gameplay rule was re-derived from the machine code, **with the assembly cited
  next to the code that implements it** (file + offset, e.g. `COMBAT:0x194A`).
- **Coverage ledger: 202,800 / 202,800 bytes** of executable + data justified one
  by one (code / data / inert, each with a note).
- **276 automated parity tests** run identical scenarios in the re-derived model
  and compare outputs — including a full gypsy-to-endgame critical path threaded
  through a single RNG seed.
- **Runtime verification against the real binary**: a custom harness boots the
  original game in headless DOSBox-X, seeds RAM, sets code breakpoints and
  compares state byte-for-byte with the port (movement, combat rolls, RNG orbit,
  wind, moon phases, character creation).
- The original RNG is **not** a textbook LCG — it's an add/rotate/xor routine
  (`((seed+0x9248) ror 3) ^ 0x9248 + 0x11`), reimplemented bit-exactly.

What is *deliberately* not identical is documented too: see
[`deliberate-divergences.md`](re/deliberate-divergences.md). Honesty over vibes.

## Play

You need the original game files (not included — they belong to EA):

1. Buy [Ultima 4+5+6 on GOG](https://www.gog.com/en/game/ultima_456) (~€5).
2. Extract the Ultima V files (`innoextract` works on the offline installer).
3. Drop them into `original/u5/ultima5/` inside the repo (the extractor's
   default location — `original/` is gitignored), or point `--src` anywhere:
   ```bash
   npm install
   npm run extract                            # reads original/u5/ultima5/
   # …or: npm run extract -- --src /path/to/your/ultima5
   npm run dev                                # → "Journey Onward"
   ```

Keys: arrows to move · **T**alk **O**pen **L**ook **G**et **K**limb **S**earch ·
**M**ix **C**ast **R**eady **U**se · **z** stats · **Tab** map · **F5** save.
Full list in [docs/controls.md].

Or skip the local setup: the browser "bring your own files" demo at
[openu5.org](https://openu5.org) asks for your game folder and extracts it
client-side — nothing is uploaded.

## Status

| Area | State |
|---|---|
| Full game completable start-to-finish | ✅ |
| Binary-exact rule migration (ledger 100%) | ✅ |
| Unit + parity + E2E suites (`verify:all`) | ✅ |
| Live game on the unified original RNG stream | ✅ |
| "Grand Tour" — a purist E2E playthrough of ALL content (112/112 dungeon rooms sealed, main line closed end-to-end, re-run byte-identically) | ✅ |
| 1988-faithful skin + optional shader skin + mobile touch deck (gameplay untouched, suite-locked) | ✅ |

## Methodology

The reverse-engineering method (coverage ledger, DOSBox-X oracle, parity harness,
independent re-derivation review) is documented in [docs/methodology.md]. The
golden rule of the project: **no gameplay change is accepted without an assembly
citation or a parity scenario.** If the port contradicts the derived rule, the
port is wrong — never the other way around.

## Contributing

See [CONTRIBUTING.md]. Short version: bug reports with an exported save + seed
are gold; gameplay PRs need asm citations; UI PRs must keep the entire E2E suite
green without touching a single assert.

## Legal

This project contains **no game data, art, music, text or maps** from Ultima V.
It is a clean engine that reads the data files from your legally-owned copy.
Ultima is a registered trademark of Electronic Arts. This project is not
affiliated with or endorsed by EA. If you are a rights holder and have concerns,
please open an issue — we will respond cooperatively.

Code licensed under GPL-3.0. Tile metadata derived from
[Ultima5Redux](https://github.com/bradhannah/Ultima5Redux) (MIT) — see NOTICE.
