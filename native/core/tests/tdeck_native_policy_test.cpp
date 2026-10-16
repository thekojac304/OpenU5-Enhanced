// T-Deck Native View Phase 1 (docs/TDECK_NATIVE_VIEW_PHASE1.md): the
// platform profile's presentation, interaction and journey switches are
// decided independently. Built three times (CMakeLists.txt):
//   dynamic          -- the desktop/host build: every switch follows the profile
//   handheld         -- OPENU5_FIXED_HANDHELD_PROFILE, the v0.5.0 firmware: every
//                       1988 switch is a constant false, whatever is installed
//   native_handheld  -- OPENU5_FIXED_HANDHELD_INTERACTION only, the boundary a
//                       future T-Deck Native View build would use: presentation
//                       follows the profile, interaction stays a constant false
//
//   M  the four configurations' capability decisions (Windows Original,
//      Windows Enhanced, the T-Deck as shipped, the future T-Deck Native)
//   U  UiSession's interaction under the future T-Deck Native profile is the
//      T-Deck's, step for step: Look's cancel, Y/N, text entry, the roster
//      picker, the item list, number entry, Q, Z-stats -- with the Windows
//      Original profile as the positive control (it must differ). Dynamic
//      build only: UiSession comes from openu5_core, which is dynamic; the
//      fixed builds' constants make the same guarantee at compile time.
//
// The future configuration is only REPRESENTED here: no DOS renderer is
// built or drawn, and nothing claims the combination is playable yet.
#include "openu5/platform_profile.h"
#if !defined(POLICY_MATRIX_ONLY)
#include "openu5/ui_session.h"
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}

#if defined(EXPECT_FIXED_HANDHELD)
constexpr bool kFixedInteraction = true, kFixedPresentation = true;
const char *kVariant = "handheld";
#elif defined(EXPECT_FIXED_INTERACTION)
constexpr bool kFixedInteraction = true, kFixedPresentation = false;
const char *kVariant = "native_handheld";
#else
constexpr bool kFixedInteraction = false, kFixedPresentation = false;
const char *kVariant = "dynamic";
#endif

// One configuration's switches, as a platform would install them.
struct Config {
    const char *name;
    bool screen, panels, text;          // presentation
    bool interaction, typed, quit_save; // interaction
    bool hide_enhanced_rows;            // journey (Windows Original hides Difficulty/Cheats)
    bool install_nothing;               // the T-Deck installs no profile at all
};
const Config kWindowsOriginal{"Windows Original", true, true, true, true, true, true, true, false};
const Config kWindowsEnhanced{"Windows Enhanced", false, false, false, false, false, false, false, false};
const Config kTdeck{"T-Deck v0.5.0", false, false, false, false, false, false, false, true};
const Config kTdeckNative{"future T-Deck Native", true, true, true, false, false, false, false, false};

const Config *g_config = &kTdeck;
int g_calls = 0;
bool sw(bool v) { ++g_calls; return v; }
bool screen_cb(void *) { return sw(g_config->screen); }
bool panels_cb(void *) { return sw(g_config->panels); }
bool interaction_cb(void *) { return sw(g_config->interaction); }
bool typed_cb(void *) { return sw(g_config->typed); }
bool quit_save_cb(void *) { return sw(g_config->quit_save); }
bool enhanced_cb(void *) { return sw(!g_config->hide_enhanced_rows); }
const char *text_cb(void *, uint8_t, int) { ++g_calls; return g_config->text ? "DOS" : nullptr; }

void install(const Config &c) {
    g_config = &c;
    if (c.install_nothing) { set_platform_profile(PlatformProfile{}); return; }
    PlatformProfile p{};
    p.dos_screen = screen_cb;
    p.dos_panels = panels_cb;
    p.dos_text = text_cb;
    p.dos_interface = interaction_cb;
    p.typed_spell_names = typed_cb;
    p.quit_save_key = quit_save_cb;
    p.enhanced_rows = enhanced_cb;
    set_platform_profile(p);
}

// ---------------------------------------------------------------------------
void test_matrix() {
    std::printf("\nM  the capability matrix (%s build)\n", kVariant);
    check(kPlatformSupportsDosInteraction == !kFixedInteraction &&
              kPlatformSupportsDosPresentation == !kFixedPresentation,
          "M0 the build fixes exactly its groups: interaction " + std::to_string(kFixedInteraction) +
              ", presentation " + std::to_string(kFixedPresentation));
    for (const Config *c : {&kWindowsOriginal, &kWindowsEnhanced, &kTdeck, &kTdeckNative}) {
        install(*c);
        const std::string t = std::string("[") + c->name + "] ";
        const bool screen = !kFixedPresentation && c->screen, panels = !kFixedPresentation && c->panels;
        const bool text = !kFixedPresentation && c->text;
        const bool interaction = !kFixedInteraction && c->interaction, typed = !kFixedInteraction && c->typed;
        const bool quit_save = !kFixedInteraction && c->quit_save;
        check(platform_dos_screen() == screen && platform_dos_panels() == panels &&
                  (platform_dos_text(DosText::Literal, 0) != nullptr) == text,
              t + "M1 presentation: screen " + std::to_string(screen) + ", panels " + std::to_string(panels) +
                  ", DOS text " + std::to_string(text));
        check(platform_dos_interface() == interaction && platform_typed_spell_names() == typed &&
                  platform_quit_save_key() == quit_save,
              t + "M2 interaction: 1988 model " + std::to_string(interaction) + ", typed spells " +
                  std::to_string(typed) + ", (Q)uit & Save " + std::to_string(quit_save));
        check(platform_enhanced_rows() == !c->hide_enhanced_rows,
              t + "M3 journey: Difficulty/Cheats rows " + std::to_string(!c->hide_enhanced_rows) +
                  " (never fixed, never derived from presentation)");
    }
    // The future T-Deck Native configuration specifically: DOS presentation
    // must not drag in any 1988 interaction rule, nor hide the journey rows.
    install(kTdeckNative);
    check(!platform_dos_interface() && !platform_typed_spell_names() && !platform_quit_save_key() &&
              platform_enhanced_rows(),
          "M4 future T-Deck Native: handheld cancel/confirm, spell list, no Q save, Difficulty and Cheats kept");
    check(platform_dos_panels() == !kFixedPresentation && platform_dos_screen() == !kFixedPresentation,
          "M5 future T-Deck Native: the DOS screen and panels are representable where presentation is not fixed");
    // And the converse: a profile that asks for the 1988 interaction cannot
    // get it where interaction is fixed (the firmware's guarantee).
    install(kWindowsOriginal);
    check(platform_dos_interface() == !kFixedInteraction,
          "M6 a Windows Original profile gets the 1988 interaction only where interaction is not fixed");
    // Fixed groups never call their callbacks: their code is dead in that build.
    install(kWindowsOriginal);
    g_calls = 0;
    (void)platform_dos_interface();
    (void)platform_typed_spell_names();
    (void)platform_quit_save_key();
    const int interaction_calls = g_calls;
    g_calls = 0;
    (void)platform_dos_screen();
    (void)platform_dos_panels();
    (void)platform_dos_text(DosText::Literal, 0);
    const int presentation_calls = g_calls;
    check(interaction_calls == (kFixedInteraction ? 0 : 3) && presentation_calls == (kFixedPresentation ? 0 : 3),
          "M7 callbacks run only for the groups the build leaves dynamic (interaction " +
              std::to_string(interaction_calls) + ", presentation " + std::to_string(presentation_calls) + ")");
    set_platform_profile(PlatformProfile{});
}

#if !defined(POLICY_MATRIX_ONLY)
// ---------------------------------------------------------------------------
// U: a scripted UiSession run, traced after every step.
UiAction ch(char16_t c) { UiAction a; a.kind = UiActionKind::Character; a.character = c; return a; }
UiAction kind(UiActionKind k) { UiAction a; a.kind = k; return a; }
UiAction dir(Direction d) { UiAction a; a.kind = UiActionKind::Direction; a.direction = d; return a; }

struct Spy {
    std::vector<std::string> seen;
    static void send(void *p, const UiIntent &i) {
        char line[160];
        std::snprintf(line, sizeof(line), "intent kind=%d req=%d cmd=%d acc=%d yes=%d num=%ld idx=%ld text=%zu",
                      int(i.kind), int(i.request), int(i.command.kind), int(i.value.accepted), int(i.value.yes),
                      long(i.value.number), long(i.value.index),
                      i.kind == UiIntentKind::Command ? i.command.text_length : i.value.text_length);
        static_cast<Spy *>(p)->seen.emplace_back(line);
    }
};

const char *kNames[] = {"Avatar", "Iolo", "Shamino"};
size_t three(void *) { return 3; }
UiSelectionItem three_item(void *, size_t i) { return {kNames[i % 3], true}; }
size_t twenty(void *) { return 20; }
UiSelectionItem twenty_item(void *, size_t) { return {"Item", true}; }
const UiSelectionSource kThree{nullptr, three, three_item};
const UiSelectionSource kTwenty{nullptr, twenty, twenty_item};

std::vector<std::string> run_script(const Config &c) {
    install(c);
    UiTextBlock blocks[64]{};
    Spy spy;
    UiSession ui{{blocks, 64}, {&spy, Spy::send}, {38, 40, 63}};
    ui.set_console_layout(true);
    ui.set_console_ready(true);
    ui.append(UiTextChannel::Message, "Welcome");
    std::vector<std::string> trace;
    auto snap = [&](const char *step) {
        char live[256]{};
        ui.live_prompt_text(live, sizeof(live));
        UiSelectionView v{};
        const bool sel = ui.selection_view(v);
        const size_t n = ui.transcript_size();
        const auto *last = n ? ui.transcript_at(n - 1) : nullptr;
        char line[512];
        std::snprintf(line, sizeof(line),
                      "%s: mode=%d req=%d prompt=[%s] live=[%s] cursor=%d sel=%d/%zu input=%zu log=%zu last=[%s] "
                      "intents=%zu %s",
                      step, int(ui.mode()), int(ui.request()), ui.prompt() ? ui.prompt() : "", live,
                      int(ui.console_cursor()), int(sel), sel ? v.cursor : size_t(0), ui.input_length(), n,
                      last ? last->text : "", spy.seen.size(), spy.seen.empty() ? "" : spy.seen.back().c_str());
        trace.emplace_back(line);
    };
    // Look's getdir, cancelled with ESC (the handheld cancels; the 1988 rules ignore it).
    ui.handle_input(ch('l')); snap("look");
    ui.handle_input(kind(UiActionKind::Cancel)); snap("look-esc");
    ui.cancel_modal(); snap("look-closed");
    // Y/N: RETURN, a mapped ESC, an unmapped ESC, then 'n'.
    ui.begin_yes_no(UiRequestId::Custom, "Sure?", true); snap("yn");
    ui.handle_input(kind(UiActionKind::Confirm)); snap("yn-return");
    ui.begin_yes_no(UiRequestId::Custom, "Sure?", true);
    ui.handle_input(kind(UiActionKind::Cancel)); snap("yn-esc-mapped");
    ui.begin_yes_no(UiRequestId::Custom, "Sure?", false);
    ui.handle_input(kind(UiActionKind::Cancel)); snap("yn-esc");
    ui.handle_input(ch('n')); snap("yn-n");
    // Text entry: case kept, ESC cancels.
    ui.begin_text(UiRequestId::Custom, "Name:", 8);
    ui.handle_input(ch('a')); ui.handle_input(ch('B')); snap("text-typed");
    ui.handle_input(kind(UiActionKind::Cancel)); snap("text-esc");
    ui.cancel_modal();
    // The member picker: a digit, wrapping, Space, RETURN.
    ui.begin_selection(UiMode::PartySelection, UiRequestId::Party, "Player: ", kThree); snap("party");
    ui.handle_input(ch('2')); snap("party-digit");
    ui.handle_input(dir(Direction::North)); snap("party-up");
    ui.handle_input(ch(' ')); snap("party-space");
    ui.handle_input(kind(UiActionKind::Confirm)); snap("party-return");
    ui.cancel_modal();
    // An item list: paging, then ESC.
    ui.begin_selection(UiMode::InventorySelection, UiRequestId::Inventory, "Item: ", kTwenty); snap("items");
    ui.handle_input(kind(UiActionKind::PageDown)); snap("items-pgdn");
    ui.handle_input(kind(UiActionKind::Cancel)); snap("items-esc");
    ui.cancel_modal();
    // Number entry.
    ui.begin_number(UiRequestId::Custom, "How many?", 0, 99, 2);
    ui.handle_input(ch('1')); ui.handle_input(ch('2')); snap("number-typed");
    ui.handle_input(kind(UiActionKind::Confirm)); snap("number-return");
    // Q at the command prompt (the 1988 (Q)uit & Save on Windows Original only).
    ui.handle_input(ch('q')); snap("q");
    ui.cancel_modal();
    // Z-stats' member picker and its '0'.
    ui.handle_input(ch('z')); snap("z");
    ui.handle_input(ch('0')); snap("z-0");
    ui.cancel_modal(); snap("end");
    set_platform_profile(PlatformProfile{});
    return trace;
}

void test_ui_session() {
    std::printf("\nU  UiSession interaction under each profile (%s build)\n", kVariant);
    const auto tdeck = run_script(kTdeck);
    const auto enhanced = run_script(kWindowsEnhanced);
    const auto native = run_script(kTdeckNative);
    const auto original = run_script(kWindowsOriginal);
    size_t first_diff = tdeck.size();
    for (size_t i = 0; i < tdeck.size() && i < native.size(); ++i)
        if (tdeck[i] != native[i]) { first_diff = i; break; }
    check(native.size() == tdeck.size() && first_diff == tdeck.size(),
          "U1 future T-Deck Native drives UiSession exactly as the T-Deck: " + std::to_string(tdeck.size()) +
              " steps identical" +
              (first_diff < tdeck.size() ? " -- first difference:\n  " + tdeck[first_diff] + "\n  " + native[first_diff]
                                         : std::string()));
    check(enhanced == tdeck, "U2 Windows Enhanced drives UiSession exactly as the T-Deck");
    size_t differing = 0;
    for (size_t i = 0; i < tdeck.size() && i < original.size(); ++i) differing += tdeck[i] != original[i];
    check(differing > tdeck.size() / 2,
          "U3 positive control: the Windows Original profile changes " + std::to_string(differing) + " of " +
              std::to_string(tdeck.size()) + " steps (the trace sees the 1988 rules)");
}
#endif
} // namespace

int main() {
    test_matrix();
#if !defined(POLICY_MATRIX_ONLY)
    test_ui_session();
#endif
    check(platform_profile().dos_interface == nullptr && platform_profile().dos_panels == nullptr,
          "Z the default profile is restored");
    std::printf("tdeck_native_policy (%s): %d/%d checks\n", kVariant, checks - failures, checks);
    return failures ? 1 : 0;
}
