// T-Deck Native View Phase 1 (docs/TDECK_NATIVE_VIEW_PHASE1.md): the REAL
// AlphaRuntime on the REAL tdeck_board.cpp over the fake ST7789 (the A4-UI2
// harness), driven by RAW T-Deck input -- keys, the trackball, Mic, Alt+M,
// Alt+S -- under three profiles:
//   T  the T-Deck as shipped (no profile installed)
//   N  the future T-Deck Native presentation: dos_panels and dos_text on, every
//      interaction switch off. (dos_screen stays off: the 320 x 200 geometry
//      is board_dos_layout.cpp's, which no T-Deck build compiles yet -- Phase 2.)
//   O  the Windows Original switches (positive control)
//
//   P1 N's interaction trace equals T's, step for step: modes, requests,
//      prompts, the transcript, the list cursor, the key legends, the context
//      bar, the roster picker, the shop panel, turns, the clock, the journey's
//      Enhanced state (difficulty, cheats) and the saved GAM bytes
//   P2 N's presentation does differ: (R)eady, (U)se and Z-stats compose the
//      1988 panel -- so P1 is not vacuous
//   P3 targeted: (C)ast opens the handheld member LIST (no roster picker, no
//      "Select:"), then the spell LIST (no typed name); Mic cancels the Ready
//      list with no "Item: Done" echo; Look's getdir is cancelled by Mic; the
//      shop is the handheld transactional panel; (Q) is not (Q)uit & Save
//   P4 the positive control O changes the interaction trace
//
// The future configuration is REPRESENTED, not finished: the panel/console
// layout of that combination is Phase 2's, and nothing here claims it.
//
//   tdeck_native_policy_runtime <openu5-alpha1-resources.bin>
#include "a4_ui2_harness.h"
#include "openu5/debug_map_picker.h"
#include "openu5/platform_profile.h"

namespace tdeck {
std::vector<uint8_t> host_memory_save_gam_for_test();
void host_memory_save_forget_for_test();
} // namespace tdeck

using namespace a4_ui2;

namespace {
enum class Profile { Tdeck, Native, Original };
const char *profile_name(Profile p) {
    return p == Profile::Tdeck ? "T-Deck" : p == Profile::Native ? "Native" : "Original";
}

bool g_on = false;
struct ProfileScope {
    explicit ProfileScope(Profile which) {
        if (which == Profile::Tdeck) return;
        g_on = true;
        PlatformProfile p{};
        p.dos_panels = [](void *) { return g_on; };
        // RUNES.CH 0x01 as every item's class glyph, as win_dos_runtime does.
        p.dos_text = [](void *, uint8_t table, int) -> const char * {
            return table == uint8_t(DosText::EquipmentGlyph) ? "\x01" : nullptr;
        };
        if (which == Profile::Original) {
            p.dos_interface = [](void *) { return g_on; };
            p.typed_spell_names = [](void *) { return g_on; };
            p.quit_save_key = [](void *) { return g_on; };
        }
        set_platform_profile(p);
    }
    ~ProfileScope() {
        g_on = false;
        set_platform_profile(PlatformProfile{});
    }
    ProfileScope(const ProfileScope &) = delete;
    ProfileScope &operator=(const ProfileScope &) = delete;
};

uint64_t fnv(const void *p, size_t n, uint64_t h = 1469598103934665603ull) {
    const auto *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}
std::string text_or(const char *s) { return s ? s : ""; }

// What the interaction model decides -- never how a list or panel is drawn.
struct Trace {
    std::vector<std::string> interaction;
    int dos_panels = 0;       // steps with a composed DeviceDosPanel
    std::string panel_steps;  // ...and their names
    std::string transcript;   // the whole transcript at the end
    std::vector<uint8_t> gam; // the Alt+S save
};

std::string interaction_snapshot(Run &h, const std::string &step, Trace &t) {
    auto *ui = h.rt->ui();
    const auto view = h.rt->selection_view_for_test();
    const auto bar = h.rt->context_bar_for_test();
    const auto hl = h.rt->party_highlight_for_test();
    const auto shop = h.rt->shop_view_for_test();
    if (view.active && view.dos) { ++t.dos_panels; t.panel_steps += step + " "; }
    UiSelectionView sv{};
    const bool sel = ui->selection_view(sv);
    std::string tail;
    const size_t count = ui->transcript_size();
    for (size_t i = count > 3 ? count - 3 : 0; i < count; ++i)
        if (const auto *b = ui->transcript_at(i)) tail += std::string(b->text) + "|";
    std::string shop_rows;
    for (size_t r = 0; r < shop.row_count; ++r) shop_rows += std::string(shop.rows[r].name) + ",";
    const auto &g = h.rt->game();
    char line[1400];
    std::snprintf(line, sizeof(line),
                  "%s: mode=%d req=%d prompt=[%s] sel=%d/%zu/%zu legend=%d[%s|%s] bar=%d[%s|%s] picker=%d/%d "
                  "shop=%d[%s] tail=[%s] turns=%lld clock=%d:%02d hp=%u gold=%u enh=%016llx",
                  step.c_str(), int(ui->mode()), int(ui->request()), text_or(ui->prompt()).c_str(), int(sel),
                  sel ? sv.cursor : size_t(0), sel ? sv.count : size_t(0), int(view.context.active),
                  view.context.status, view.context.actions, int(bar.active), bar.status, bar.actions,
                  int(hl.cursor), int(hl.select_banner), int(shop.active), shop_rows.c_str(), tail.c_str(),
                  (long long)g.turns_since_start, int(g.time.hour), int(g.time.minute),
                  unsigned(g.party.characters[0].current_hp), unsigned(g.gold),
                  (unsigned long long)fnv(&g.enhanced, sizeof(g.enhanced)));
    return line;
}

const std::vector<Member> kParty = {{"Avatar", 'G', 120}, {"Iolo", 'G', 110}, {"Shamino", 'G', 100}};

void stock(Run &h) {
    auto &g = h.rt->game();
    g.equipment_quantities[0] = 0; // Leather Helm: worn by Avatar
    g.equipment_quantities[4] = 2; // two Small Shields in the pack
    g.party.characters[0].helmet = 0;
    g.potion_quantities[0] = 2;
    g.scroll_quantities[1] = 1;
    g.spell_quantities[0] = 3;
    g.party.characters[1].current_mp = 20;
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

Trace walk(Profile which) {
    ProfileScope scope(which);
    tdeck::host_memory_save_forget_for_test();
    Trace t;
    Run h(kParty);
    stock(h);
    h.run(100);
    auto snap = [&](const std::string &s) { t.interaction.push_back(interaction_snapshot(h, s, t)); };
    snap("start");
    // (C)ast: the member, the trackball, RETURN, the spell, Mic twice.
    h.key('c'); snap("cast");
    h.down(); snap("cast-down");
    h.key('\r'); snap("cast-member");
    h.down(); snap("spell-down");
    h.mic(); snap("spell-mic");
    h.mic(); snap("spell-mic2");
    // (R)eady: Avatar, the list, the trackball, Mic.
    h.key('r'); snap("ready");
    h.key('\r'); snap("ready-member");
    h.down(); snap("ready-down");
    h.mic(); snap("ready-mic");
    h.mic(); snap("ready-mic2");
    // (U)se: the list, Mic.
    h.key('u'); snap("use");
    h.down(); snap("use-down");
    h.mic(); snap("use-mic");
    // (Z)-stats: open, page, Mic.
    h.key('z'); snap("z");
    h.key('\r'); snap("z-member");
    h.down(); snap("z-down");
    h.mic(); snap("z-mic");
    h.mic(); snap("z-mic2");
    // (L)ook's getdir, cancelled by Mic; (Q) at the command prompt.
    h.key('l'); snap("look");
    h.mic(); snap("look-mic");
    h.key('q'); snap("q");
    h.mic(); snap("q-mic");
    // The System Menu (Alt+M) and back.
    h.key('m', true); h.run(200); snap("alt-m");
    h.mic(); h.run(200); snap("alt-m-mic");
    // A shop: Britain's blacksmith, Buy.
    h.rt->game().gold = 9999;
    const bool there = teleport_town(h, 2, 15, 30) && h.rt->open_shop_for_test(0);
    h.run(100); snap(there ? "shop" : "shop-unavailable");
    h.key('\r'); h.run(100); snap("shop-greeting");
    h.key('b'); h.run(100); snap("shop-buy");
    h.down(); snap("shop-down");
    h.mic(); h.run(100); snap("shop-mic");
    h.mic(); h.run(100); snap("shop-mic2");
    h.mic(); h.run(100); snap("shop-mic3");
    // Alt+S, then the save's bytes.
    h.key('s', true); h.run(500); snap("alt-s");
    h.key('\r'); h.run(1000); snap("alt-s-confirm");
    t.gam = tdeck::host_memory_save_gam_for_test();
    const size_t count = h.rt->ui()->transcript_size();
    for (size_t i = 0; i < count; ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) t.transcript += std::string(b->text) + "\n";
    tdeck::host_memory_save_forget_for_test();
    return t;
}

// The targeted P3 checks, on a fresh run of each profile.
struct Targeted {
    bool cast_list = false, cast_no_roster = false, spell_list = false, ready_cancel_no_echo = false;
    bool look_cancel = false, q_not_save = false, shop_panel = false, ready_dos_panel = false, legend = false;
};
Targeted targeted(Profile which) {
    ProfileScope scope(which);
    Targeted r;
    Run h(kParty);
    stock(h);
    h.run(100);
    h.key('c');
    auto view = h.rt->selection_view_for_test();
    const auto hl = h.rt->party_highlight_for_test();
    r.cast_list = h.rt->ui()->mode() == UiMode::PartySelection && view.active && !view.dos;
    r.cast_no_roster = hl.cursor < 0 && !hl.select_banner && h.rt->context_bar_for_test().active;
    h.key('\r');
    r.spell_list = h.rt->ui()->mode() == UiMode::SpellSelection;
    h.mic();
    h.mic();
    h.key('r');
    h.key('\r');
    view = h.rt->selection_view_for_test();
    r.ready_dos_panel = h.rt->ui()->mode() == UiMode::EquipmentSelection && view.active && view.dos;
    r.legend = view.context.active;
    h.mic();
    h.mic();
    bool echo = false;
    for (size_t i = 0; i < h.rt->ui()->transcript_size(); ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) echo = echo || std::strstr(b->text, "Item: Done");
    r.ready_cancel_no_echo = h.rt->ui()->mode() == UiMode::Exploration && !echo;
    h.key('l');
    const bool getdir = h.rt->ui()->mode() == UiMode::TargetSelection;
    h.mic();
    r.look_cancel = getdir && h.rt->ui()->mode() == UiMode::Exploration;
    h.key('q');
    r.q_not_save = h.rt->ui()->request() != UiRequestId::QuitSave;
    h.mic();
    h.rt->game().gold = 9999;
    if (teleport_town(h, 2, 15, 30) && h.rt->open_shop_for_test(0)) {
        h.run(100);
        h.key('\r');
        h.run(100);
        h.key('b');
        h.run(100);
        const auto shop = h.rt->shop_view_for_test();
        r.shop_panel = shop.active && shop.row_count > 0 && !h.rt->selection_view_for_test().dos;
    }
    return r;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: tdeck_native_policy_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    std::printf("\nP  T-Deck (as shipped) vs future T-Deck Native presentation vs Windows Original switches\n");
    const Trace tdeck = walk(Profile::Tdeck);
    const Trace native = walk(Profile::Native);
    const Trace original = walk(Profile::Original);
    size_t first = tdeck.interaction.size();
    for (size_t i = 0; i < tdeck.interaction.size() && i < native.interaction.size(); ++i)
        if (tdeck.interaction[i] != native.interaction[i]) { first = i; break; }
    check(native.interaction.size() == tdeck.interaction.size() && first == tdeck.interaction.size(),
          "P1a Native's interaction trace equals the T-Deck's at all " + n(long(tdeck.interaction.size())) +
              " steps (modes, prompts, cursor, legends, context bar, picker, shop, turns, clock, Enhanced state)" +
              (first < tdeck.interaction.size()
                   ? "\n  T: " + tdeck.interaction[first] + "\n  N: " + native.interaction[first]
                   : std::string()));
    check(native.transcript == tdeck.transcript, "P1b the whole transcript is the same");
    check(!tdeck.gam.empty() && native.gam == tdeck.gam,
          "P1c Alt+S saved the same GAM bytes (" + n(long(tdeck.gam.size())) + " bytes, fnv " +
              n((long long)(fnv(tdeck.gam.data(), tdeck.gam.size()) & 0x7fffffff)) + ")");
    check(tdeck.dos_panels == 0 && native.dos_panels >= 3,
          "P2a the presentation changed: Native composed the 1988 panel at " + n(native.dos_panels) +
              " steps [" + native.panel_steps + "] (the T-Deck at " + n(tdeck.dos_panels) + ")");
    for (const Profile p : {Profile::Tdeck, Profile::Native}) {
        const auto r = targeted(p);
        const std::string tag = std::string("[") + profile_name(p) + "] ";
        check(r.cast_list && r.cast_no_roster,
              tag + "P3a (C)ast opens the handheld member LIST with its context bar -- no roster picker, no \"Select:\"");
        check(r.spell_list, tag + "P3b RETURN goes on to the spell LIST, not a typed spell name");
        check(r.ready_dos_panel == (p == Profile::Native) && r.legend,
              tag + "P3c the Ready list is drawn as the 1988 panel only under Native, and keeps its handheld key legend");
        check(r.ready_cancel_no_echo, tag + "P3d Mic cancels the Ready list with no \"Item: Done\" echo");
        check(r.look_cancel, tag + "P3e Mic cancels Look's direction (the 1988 getdir would ignore it)");
        check(r.q_not_save, tag + "P3f Q is not (Q)uit & Save");
        check(r.shop_panel, tag + "P3g the shop is the handheld transactional panel");
    }
    size_t differing = 0;
    for (size_t i = 0; i < tdeck.interaction.size() && i < original.interaction.size(); ++i)
        differing += tdeck.interaction[i] != original.interaction[i];
    check(differing >= 5, "P4 positive control: the Windows Original switches change " + n(long(differing)) + " of " +
                              n(long(tdeck.interaction.size())) + " interaction steps");
    check(platform_profile().dos_panels == nullptr && !platform_dos_panels() && !platform_dos_interface(),
          "Z the default profile is restored");
    std::printf("tdeck_native_policy_runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
