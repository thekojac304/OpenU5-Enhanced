// T-Deck Native View Phase 2 (docs/TDECK_NATIVE_VIEW_PHASE2.md): the
// prototype image's configuration on the host -- the REAL AlphaRuntime, the
// REAL tdeck_board.cpp AND board_dos_layout.cpp (the painter Windows Original
// uses) over the fake ST7789, a core built with only the interaction fixed
// (OPENU5_FIXED_HANDHELD_INTERACTION, as the prototype firmware), the boot
// selector of native_view_prototype.cpp -- driven by RAW T-Deck input.
//
//   P  policy: Native installs the DOS screen and panels, never an
//      interaction switch; the marker file's parser; Enhanced installs none
//   G  geometry: ULTIMA.EXE's frame, the 176 x 176 map at (8,8) pixel for pixel,
//      the IBM.CH roster, the console window, rows 200..239 black
//   Y  scheduling: every window over 16 rows pauses every 16 rows, the full
//      DOS frame included (paint_rect's missing tft_yield, fixed here)
//   D  dirty regions: an unchanged frame writes nothing; a step repaints the
//      map, never the frame
//   S  the handheld prompt strip: a prompt's question and keys under the
//      picture while it is open, black again when it closes
//   T  transitions: handheld -> Native, the System Menu and back, Native ->
//      Enhanced -- each a whole repaint; a shop stays on the Native screen
//      (Phase 3 replaced Phase 2's handheld-screen fallback,
//      docs/TDECK_NATIVE_VIEW_PHASE3.md; tdeck_native_view_phase3_runtime
//      covers the shops themselves)
//   B  bounds: no window outside the panel, no malformed pixel data
//   I  input and gameplay: the same raw script (trackball, its click / WASD,
//      command keys, Confirm / Mic, Mic hold, Symbol+Mic, the member and spell
//      lists, a conversation, Alt+M, Alt+S / Alt+L) under Enhanced and Native:
//      identical interaction traces, transcripts, game state and save bytes
//
//   tdeck_native_view_runtime <openu5-alpha1-resources.bin>
#include "a4_ui2_harness.h"
#include "native_view_prototype.h"
#include "openu5/debug_map_picker.h"
#include "openu5/platform_profile.h"

namespace tdeck {
std::vector<uint8_t> host_memory_save_gam_for_test();
void host_memory_save_forget_for_test();
} // namespace tdeck

using namespace a4_ui2;
using tdeck::PrototypePresentation;
using tdeck::install_prototype_presentation;
using tdeck::parse_prototype_presentation;
using tdeck::read_prototype_presentation;

namespace {
constexpr uint16_t kBand = 0x0015;
const std::vector<Member> kParty = {{"Avatar", 'G', 120}, {"Iolo", 'G', 110}, {"Shamino", 'G', 100}};

uint64_t fnv(const void *p, size_t n, uint64_t h = 1469598103934665603ull) {
    const auto *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}
/** Boot as the prototype image does: the selector's profile, then the Board's layout. */
void boot(Run &h, PrototypePresentation p) {
    install_prototype_presentation(p);
    h.board.set_dos_layout(p == PrototypePresentation::Native);
    h.render(true);
    h.run(100);
}
bool rows_black(int y0, int y1) {
    for (int y = y0; y <= y1; ++y)
        for (int x = 0; x < 320; ++x)
            if (px(x, y) != kBlack) return false;
    return true;
}
int ink(int x0, int y0, int x1, int y1) {
    int k = 0;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) k += px(x, y) != kBlack;
    return k;
}
bool white_run(int y, int x0, int x1) {
    for (int x = x0; x <= x1; ++x)
        if (px(x, y) != kWhite) return false;
    return true;
}
bool frame_drawn() {
    return px(3, 100) == kBand && px(7, 100) == kWhite && px(184, 100) == kWhite && px(180, 7) == kWhite &&
           px(180, 184) == kWhite && px(191, 30) == kWhite && px(312, 30) == kWhite && white_run(87, 191, 319);
}
bool full_window_written() {
    for (const auto &w : bus::windows())
        if (w.x0 == 0 && w.y0 == 0 && w.x1 == 319 && w.y1 == 239) return true;
    return false;
}
/** Every window taller than 16 rows paused at least every 16 rows inside it. */
bool windows_paced(std::string &why) {
    for (const auto &w : bus::windows()) {
        if (w.height() <= 16) continue;
        int pauses = 0;
        for (const auto &p : bus::pauses())
            if (p.x0 == w.x0 && p.y0 == w.y0 && p.x1 == w.x1 && p.y1 == w.y1) ++pauses;
        const int want = (w.height() - 1) / 16;
        if (pauses < want) {
            why = "window " + n(w.x0) + "," + n(w.y0) + "-" + n(w.x1) + "," + n(w.y1) + " paused " + n(pauses) +
                  " times, want " + n(want);
            return false;
        }
    }
    return true;
}
bool windows_in_bounds() {
    for (const auto &w : bus::windows())
        if (w.x0 < 0 || w.y0 < 0 || w.x1 > 319 || w.y1 > 239 || w.x1 < w.x0 || w.y1 < w.y0) return false;
    return true;
}
bool teleport_town(Run &h, uint8_t location, int x, int y) {
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::SmallMap;
    r.location = location;
    r.x = x;
    r.y = y;
    const bool ok = apply_debug_teleport(h.rt->command_context_for_test(), r).status == DebugTeleportStatus::Applied;
    h.rt->render(h.board, true);
    return ok;
}
void raw_key(Run &h, tdeck::RawInputEvent e) {
    openu5_host_virtual_clock_us() += 100000;
    h.raw(e);
    h.render();
}

// ---------------------------------------------------------------------------
void test_policy() {
    std::printf("\nP  the prototype's policy\n");
    check(!kPlatformSupportsDosInteraction && kPlatformSupportsDosPresentation,
          "P0 this build fixes the interaction only (the prototype firmware's configuration)");
    install_prototype_presentation(PrototypePresentation::Native);
    check(platform_dos_screen() && platform_dos_panels() && !platform_dos_interface() && !platform_typed_spell_names() &&
              !platform_quit_save_key() && platform_enhanced_rows() && !platform_dos_text(DosText::Literal, 0),
          "P1 Native: the DOS screen and panels; the handheld interaction; Difficulty/Cheats kept; no DOS string at Literal 0");
    check(platform_profile().dos_interface == nullptr && platform_profile().typed_spell_names == nullptr &&
              platform_profile().quit_save_key == nullptr && platform_profile().enhanced_rows == nullptr &&
              std::strcmp(platform_profile().back_key, "Mic") == 0,
          "P2 the selector installs no interaction or journey switch and keeps the T-Deck's words");
    install_prototype_presentation(PrototypePresentation::Enhanced);
    check(!platform_dos_screen() && !platform_dos_panels() && !platform_dos_interface(),
          "P3 Enhanced: the handheld screen and panels");
    using P = PrototypePresentation;
    check(parse_prototype_presentation(nullptr) == P::Native && parse_prototype_presentation("") == P::Native &&
              parse_prototype_presentation("native") == P::Native && parse_prototype_presentation("enhanced") == P::Enhanced &&
              parse_prototype_presentation("  ENHANCED\r\n") == P::Enhanced &&
              parse_prototype_presentation("Enhanced view") == P::Enhanced &&
              parse_prototype_presentation("enhancedx") == P::Native && parse_prototype_presentation("enhance") == P::Native,
          "P4 the marker: a first word \"enhanced\" (any case) is Enhanced; anything else is Native");
    check(read_prototype_presentation("no/such/marker.txt") == P::Native, "P5 no marker file: Native");
    set_platform_profile(PlatformProfile{});
}

// ---------------------------------------------------------------------------
void test_screen() {
    std::printf("\nG/Y/D/T/S/B  the Native screen on the real Board\n");
    Run h(kParty);
    h.run(100);
    bus::reset_stats();
    boot(h, PrototypePresentation::Native);
    std::string why;
    // T1 / G: the switch from the handheld screen is a whole repaint.
    check(h.board.dos_layout() && full_window_written() && frame_drawn(),
          "T1 handheld -> Native: one full-panel window, ULTIMA.EXE's frame (band, map box, roster box, console rules)");
    check(rows_black(200, 239), "G1 rows 200..239 are black: nothing of the handheld screen is left below the picture");
    const uint16_t *vp = h.rt->composed_viewport();
    bool map_exact = vp != nullptr;
    for (int y = 0; map_exact && y < 176; ++y)
        for (int x = 0; x < 176; ++x)
            if (px(8 + x, 8 + y) != vp[y * 176 + x]) { map_exact = false; break; }
    check(map_exact, "G2 the 176 x 176 map is at (8,8), every pixel the composed viewport's (no strip, no scaling)");
    bool glyph = true;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if ((px(192 + c, 8 + r) == kWhite) != ibm_bit('A', r, c)) glyph = false;
    check(glyph, "G3 the roster's first cell (192,8) is IBM.CH 'A' of \"Avatar\", white on black");
    check(px(191, 150) == kWhite && px(190, 150) == kBand && px(192, 87) == kWhite && px(192, 86) == kBand,
          "G4 the console window is x 192..319 x y 88..191, ruled at x 191 against the band");
    // Y: the full frame's pacing.
    check(windows_paced(why), "Y1 every window over 16 rows (the 320 x 240 frame, the map, the panel) pauses every 16 rows" +
                                  (why.empty() ? std::string() : ": " + why));
    // D: nothing changed, nothing written; a step repaints the map only.
    bus::reset_stats();
    h.render();
    const size_t idle_windows = bus::windows().size();
    h.rt->game().position.xy.x = uint8_t(h.rt->game().position.xy.x + 1); // one tile east: a new map picture
    h.render(true);
    h.run(100);
    bool frame_again = full_window_written(), map_window = false;
    std::string written;
    for (const auto &w : bus::windows()) {
        map_window = map_window || (w.x0 == 8 && w.y0 == 8 && w.x1 == 183 && w.y1 == 183);
        if (written.size() < 400) written += n(w.x0) + "," + n(w.y0) + "-" + n(w.x1) + "," + n(w.y1) + " ";
    }
    check(idle_windows == 0, "D1 an unchanged frame writes no window (" + n(long(idle_windows)) + ")");
    check(map_window && !frame_again && frame_drawn() && rows_black(200, 239),
          "D2 one tile east repaints the map window; the frame is not redrawn [" + written + "]");
    // S: Look's direction prompt, on the strip.
    h.key('l');
    const bool strip_open = h.rt->context_bar_for_test().active && white_run(203, 4, 315) && ink(4, 208, 315, 216) > 0;
    check(h.rt->ui()->mode() == UiMode::TargetSelection && strip_open,
          "S1 (L)ook's direction prompt shows on the strip: its rule at y 203 and the question's ink");
    h.mic();
    check(h.rt->ui()->mode() == UiMode::Exploration && rows_black(200, 239), "S2 Mic cancels; the strip is black again");
    h.key('c');
    check(h.rt->ui()->mode() == UiMode::PartySelection && white_run(203, 4, 315) && ink(4, 224, 315, 232) > 0 &&
              h.rt->selection_view_for_test().active,
          "S3 the (C)ast member list: its rows in the DOS list frame, its key legend on the strip");
    h.mic();
    check(h.rt->ui()->mode() == UiMode::Exploration && rows_black(200, 239) && frame_drawn(),
          "S4 Mic closes the list: the roster is back, the strip black");
    // T2: the System Menu (the handheld's own screen) and back.
    h.key('m', true);
    h.run(200);
    const bool menu = !frame_drawn();
    bus::reset_stats();
    h.mic();
    h.run(200);
    check(menu && full_window_written() && frame_drawn() && rows_black(200, 239),
          "T2 Alt+M draws the handheld System Menu; Mic returns to a whole repaint of the Native frame");
    // T3: a shop stays on the Native screen (Phase 3), and closes back to the roster.
    h.rt->game().gold = 9999;
    const bool shop_open = teleport_town(h, 2, 15, 30) && h.rt->open_shop_for_test(0);
    h.run(100);
    h.key('\r');
    h.run(100);
    h.key('b');
    h.run(100);
    const auto shop = h.rt->shop_view_for_test();
    const bool native_shop = shop.active && shop.row_count > 0 && !white_run(2, 182, 318) && frame_drawn() &&
                             h.board.handheld_fallbacks() == 0;
    check(shop_open && native_shop, "T3a a shop's Buy list stays on the Native screen: the DOS frame, no handheld shop panel");
    bus::reset_stats();
    for (int i = 0; i < 4 && h.rt->ui()->mode() == UiMode::Shop; ++i) {
        h.mic();
        h.run(100);
    }
    check(h.rt->ui()->mode() != UiMode::Shop && !full_window_written() && frame_drawn() && rows_black(200, 239) &&
              h.board.handheld_fallbacks() == 0,
          "T3b leaving the shop redraws the panel and strip only (the frame never left); rows 200..239 black");
    // T4: Native -> Enhanced (the selector's other choice).
    bus::reset_stats();
    boot(h, PrototypePresentation::Enhanced);
    check(!h.board.dos_layout() && full_window_written() && !frame_drawn(),
          "T4 Native -> Enhanced: a whole repaint of the handheld screen");
    check(bus::stats().malformed == 0 && windows_in_bounds(), "B1 no malformed pixel data, no window outside the panel");
    set_platform_profile(PlatformProfile{});
}

// ---------------------------------------------------------------------------
// I: one raw-input script under both presentations.
struct Trace {
    std::vector<std::string> steps;
    std::string transcript;
    std::vector<uint8_t> gam;
    bool native_frame_at_end = false;
};
std::string snapshot(Run &h, const std::string &label) {
    auto *ui = h.rt->ui();
    UiSelectionView sv{};
    const bool sel = ui->selection_view(sv);
    const auto bar = h.rt->context_bar_for_test();
    const auto view = h.rt->selection_view_for_test();
    std::string tail;
    const size_t count = ui->transcript_size();
    for (size_t i = count > 3 ? count - 3 : 0; i < count; ++i)
        if (const auto *b = ui->transcript_at(i)) tail += std::string(b->text) + "|";
    const auto &g = h.rt->game();
    char line[1400];
    std::snprintf(line, sizeof(line),
                  "%s: mode=%d req=%d prompt=[%s] input=%zu sel=%d/%zu/%zu legend=%d[%s] bar=%d[%s|%s] tail=[%s] "
                  "loc=%u/%u xy=%u,%u turns=%lld clock=%d:%02d hp=%u mp=%u gold=%u active=%u move=%d enh=%016llx",
                  label.c_str(), int(ui->mode()), int(ui->request()), ui->prompt() ? ui->prompt() : "", ui->input_length(),
                  int(sel), sel ? sv.cursor : size_t(0), sel ? sv.count : size_t(0), int(view.context.active),
                  view.context.actions, int(bar.active), bar.status, bar.actions, tail.c_str(),
                  unsigned(g.position.map.location), unsigned(g.position.map.floor), unsigned(g.position.xy.x),
                  unsigned(g.position.xy.y), (long long)g.turns_since_start, int(g.time.hour), int(g.time.minute),
                  unsigned(g.party.characters[0].current_hp), unsigned(g.party.characters[1].current_mp),
                  unsigned(g.gold), unsigned(g.party.active_character),
                  int(h.rt->ui() && h.rt->device_settings().movement_mode),
                  (unsigned long long)fnv(&g.enhanced, sizeof(g.enhanced)));
    return line;
}
const NpcActor *actor_with_dialog(Run &h, uint8_t dialog) {
    for (size_t i = 0; i < h.rt->actors().count; ++i) {
        const auto &a = h.rt->actors().actors[i];
        if (a.location == h.rt->game().position.map.location && a.schedule.dialog == dialog) return &a;
    }
    return nullptr;
}
/** Lord British's castle, beside Chuckles (dialog 9), (T)alk toward him: a3_hf5's route. */
bool talk_to_chuckles(Run &h) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {pack->location_x[16], pack->location_y[16]};
    h.key('e');
    h.run(200);
    const auto *a = actor_with_dialog(h, 9);
    if (g.position.map.location != 17 || !a) return false;
    auto &ctx = h.rt->command_context_for_test();
    g.position.map.floor = a->z;
    constexpr Direction dirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
    for (auto d : dirs) {
        const auto dd = direction_delta(d);
        const int x = a->x + dd.dx, y = a->y + dd.dy;
        if (!is_passable(ctx.terrain->effective(ctx.world, g.position.map, x, y), TransportMode::Foot).value) continue;
        g.position.xy = {uint8_t(x), uint8_t(y)};
        h.key('t');
        h.ball(d == Direction::South ? RawInputKind::TrackballUp : d == Direction::North ? RawInputKind::TrackballDown
               : d == Direction::East ? RawInputKind::TrackballLeft : RawInputKind::TrackballRight);
        h.run(1500);
        return h.rt->ui()->mode() == UiMode::TextEntry;
    }
    return false;
}

Trace walk(PrototypePresentation p) {
    tdeck::host_memory_save_forget_for_test();
    Trace t;
    Run h(kParty);
    auto &g = h.rt->game();
    g.spell_quantities[0] = 3;
    g.party.characters[1].current_mp = 20;
    g.potion_quantities[0] = 2;
    boot(h, p);
    auto snap = [&](const std::string &s) { t.steps.push_back(snapshot(h, s)); };
    snap("boot");
    // Movement: the trackball, then its click (WASD mode) and W/A/S/D, the click back.
    h.ball(RawInputKind::TrackballUp); h.run(200); snap("ball-up");
    h.ball(RawInputKind::TrackballLeft); h.run(200); snap("ball-left");
    h.ball(RawInputKind::TrackballDown); h.run(200); snap("ball-down");
    tdeck::RawInputEvent click{};
    click.kind = RawInputKind::TrackballClick;
    click.transition = tdeck::KeyTransition::Pressed;
    raw_key(h, click);
    click.transition = tdeck::KeyTransition::Released;
    raw_key(h, click); h.run(200); snap("click");
    h.key('d'); h.run(200); snap("wasd-d");
    h.key('w'); h.run(200); snap("wasd-w");
    click.transition = tdeck::KeyTransition::Pressed;
    raw_key(h, click);
    click.transition = tdeck::KeyTransition::Released;
    raw_key(h, click); h.run(200); snap("click-back");
    // Mic held: the movement-mode toggle (a long press), and back.
    for (int pass = 0; pass < 2; ++pass) {
        tdeck::RawInputEvent mic{};
        mic.kind = RawInputKind::Keyboard;
        mic.column = tdeck::kMicrophoneKeyColumn;
        mic.row = tdeck::kMicrophoneKeyRow;
        mic.transition = tdeck::KeyTransition::Pressed;
        raw_key(h, mic);
        h.run(1500);
        mic.transition = tdeck::KeyTransition::Released;
        raw_key(h, mic); h.run(200); snap(pass ? "mic-hold-back" : "mic-hold");
    }
    // Command keys with Confirm / Cancel: Look and its direction; Look cancelled by Mic.
    h.key('l'); snap("look");
    h.ball(RawInputKind::TrackballRight); h.run(500); snap("look-east");
    h.key('l'); h.mic(); snap("look-mic");
    // The member list and the spell list: (C)ast, the trackball, Enter, Mic.
    h.key('c'); snap("cast");
    h.down(); snap("cast-down");
    h.key('\r'); snap("cast-member");
    h.down(); snap("spell-down");
    h.mic(); h.mic(); snap("cast-mic");
    // Numbers on the symbol layer: (Z)-stats' member list, Symbol+Mic '0'.
    h.key('z'); snap("zstats");
    tdeck::RawInputEvent zero{};
    zero.kind = RawInputKind::Keyboard;
    zero.column = tdeck::kMicrophoneKeyColumn;
    zero.row = tdeck::kMicrophoneKeyRow;
    zero.modifiers.symbol = true;
    zero.transition = tdeck::KeyTransition::Pressed;
    raw_key(h, zero);
    zero.transition = tdeck::KeyTransition::Released;
    raw_key(h, zero); snap("symbol-mic-0");
    h.mic(); h.mic(); snap("zstats-mic");
    // The System Menu and back.
    h.key('m', true); h.run(200); snap("alt-m");
    h.mic(); h.run(200); snap("alt-m-mic");
    // Alt+S, Alt+L.
    h.key('s', true); h.run(500); snap("alt-s");
    h.key('\r'); h.run(1000); snap("alt-s-confirm");
    t.gam = tdeck::host_memory_save_gam_for_test();
    h.key('l', true); h.run(500); snap("alt-l");
    h.key('\r'); h.run(1500); snap("alt-l-confirm");
    // A conversation: Chuckles, a keyword, BYE.
    const bool talking = talk_to_chuckles(h);
    snap(talking ? "talk" : "talk-unavailable");
    for (const char *c = "job"; *c; ++c) h.key(uint8_t(*c));
    h.key('\r'); h.run(3000); snap("talk-job");
    for (const char *c = "bye"; *c; ++c) h.key(uint8_t(*c));
    h.key('\r'); h.run(3000); snap("talk-bye");
    t.transcript = h.transcript();
    t.native_frame_at_end = h.board.dos_layout() && frame_drawn() && rows_black(200, 239);
    tdeck::host_memory_save_forget_for_test();
    set_platform_profile(PlatformProfile{});
    return t;
}

void test_input() {
    std::printf("\nI  the same raw input under Enhanced and Native\n");
    const Trace enhanced = walk(PrototypePresentation::Enhanced);
    const Trace native = walk(PrototypePresentation::Native);
    size_t first = enhanced.steps.size();
    for (size_t i = 0; i < enhanced.steps.size() && i < native.steps.size(); ++i)
        if (enhanced.steps[i] != native.steps[i]) { first = i; break; }
    check(native.steps.size() == enhanced.steps.size() && first == enhanced.steps.size(),
          "I1 the interaction trace is identical at all " + n(long(enhanced.steps.size())) +
              " steps (modes, prompts, lists, legends, position, turns, clock, party, Enhanced state)" +
              (first < enhanced.steps.size() ? "\n  E: " + enhanced.steps[first] + "\n  N: " + native.steps[first]
                                             : std::string()));
    check(native.transcript == enhanced.transcript, "I2 the whole transcript is the same");
    check(!enhanced.gam.empty() && native.gam == enhanced.gam,
          "I3 Alt+S saved the same GAM bytes (" + n(long(enhanced.gam.size())) + " bytes)");
    bool talked = false, moved = false, wasd = false;
    for (const auto &s : native.steps) {
        talked = talked || (s.rfind("talk:", 0) == 0);
        wasd = wasd || (s.rfind("wasd-d:", 0) == 0);
    }
    moved = enhanced.steps.size() > 3 && enhanced.steps[1] != enhanced.steps[0];
    check(talked && moved && wasd, "I4 the script reached a conversation, moved the party, and ran WASD");
    check(native.native_frame_at_end, "I5 Native still shows its frame at the end, rows 200..239 black");
    for (const auto &s : native.steps)
        if (s.find("-unavailable") != std::string::npos) check(false, "I6 a scripted scene was not reachable: " + s);
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: tdeck_native_view_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    test_policy();
    test_screen();
    test_input();
    std::printf("tdeck_native_view_runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
