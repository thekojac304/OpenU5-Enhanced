// T-Deck Native View Phase 3 (docs/TDECK_NATIVE_VIEW_PHASE3.md): functional
// UI parity on the prototype image's configuration -- the REAL AlphaRuntime,
// tdeck_board.cpp and board_dos_layout.cpp over the fake ST7789, a core with
// only the interaction fixed, the boot selector with the DOS names loaded from
// the pack's data.json -- driven by RAW T-Deck input.
//
//   N  the DOS names: data.json reduced to one block (and refused when broken)
//   S  shops in Native: buy, sell, reagents, guild, healer and inn members,
//      the guest register; deals accepted, declined, refused for gold,
//      cancelled -- never the handheld screen
//   L  lists: Ready, Use, the member choice as the roster picker, the spell
//      list, Mix's reagents, Z-stats, a long list scrolled end to end, long
//      and look-alike names
//   M  Movement (WASD) Mode: MOVE on the strip, and not over a prompt
//   P  the strip's transitions: prompts in and out, no stale rows
//   R  redraws: modal exits and returns, the System Menu over a list
//   X  presentation switches at a running game (Native <-> Enhanced): whole
//      repaints, the right panel and strip for what is open, no game change
//   I  the same raw scripts (shops, lists) under Enhanced and Native:
//      identical interaction traces, transcripts, game state and save bytes
//
//   tdeck_native_view_phase3_runtime <openu5-alpha1-resources.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "native_view_prototype.h"
#include "openu5/debug_map_picker.h"
#include "openu5/dos_text_tables.h"
#include "openu5/platform_profile.h"
#include "openu5/shop_orchestration.h"

#include <set>

namespace tdeck {
std::vector<uint8_t> host_memory_save_gam_for_test();
void host_memory_save_forget_for_test();
} // namespace tdeck

using namespace a4_ui2;
using tdeck::DeviceNativeFrame;
using tdeck::PrototypePresentation;
using tdeck::kDosArrowDown;
using tdeck::kDosArrowUp;

namespace {
constexpr uint16_t kBand = 0x0015, kCyan = 0x07FF, kGreen = 0x07E0;
const std::vector<Member> kParty = {{"Avatar", 'G', 120}, {"Iolo", 'G', 110}, {"Shamino", 'G', 100}};
std::string g_data_json;

uint64_t fnv(const void *p, size_t n, uint64_t h = 1469598103934665603ull) {
    const auto *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}
void select(Run &h, PrototypePresentation p) {
    tdeck::install_prototype_presentation(p);
    h.board.set_dos_layout(p == PrototypePresentation::Native);
}
void boot(Run &h, PrototypePresentation p) {
    select(h, p);
    h.render(true);
    h.run(100);
}
bool rows_black(int y0, int y1, int x0 = 0, int x1 = 319) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (px(x, y) != kBlack) return false;
    return true;
}
int ink(int x0, int y0, int x1, int y1, uint16_t colour = 0) {
    int k = 0;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) k += colour ? px(x, y) == colour : px(x, y) != kBlack;
    return k;
}
bool white_run(int y, int x0, int x1) {
    for (int x = x0; x <= x1; ++x)
        if (px(x, y) != kWhite) return false;
    return true;
}
/** ULTIMA.EXE's frame; `console_rule` also wants the console's top rule whole (no list arrow notched in it). */
bool frame_drawn(bool console_rule = true) {
    return px(3, 100) == kBand && px(7, 100) == kWhite && px(184, 100) == kWhite && px(180, 7) == kWhite &&
           px(180, 184) == kWhite && px(191, 150) == kWhite && (!console_rule || white_run(87, 191, 319));
}
bool full_window_written() {
    for (const auto &w : bus::windows())
        if (w.x0 == 0 && w.y0 == 0 && w.x1 == 319 && w.y1 == 239) return true;
    return false;
}
bool windows_in_bounds() {
    for (const auto &w : bus::windows())
        if (w.x0 < 0 || w.y0 < 0 || w.x1 > 319 || w.y1 > 239 || w.x1 < w.x0 || w.y1 < w.y0) return false;
    return true;
}
/** Every window taller than 16 rows paused at least every 16 rows inside it (A3-04E). */
bool windows_paced(std::string &why) {
    for (const auto &w : bus::windows()) {
        if (w.height() <= 16) continue;
        int pauses = 0;
        for (const auto &p : bus::pauses())
            if (p.x0 == w.x0 && p.y0 == w.y0 && p.x1 == w.x1 && p.y1 == w.y1) ++pauses;
        if (pauses < (w.height() - 1) / 16) {
            why = n(w.x0) + "," + n(w.y0) + "-" + n(w.x1) + "," + n(w.y1) + " paused " + n(pauses);
            return false;
        }
    }
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
/** A town whose `type` shop opens (the first of locations 1..32), opened; false if none. */
bool open_shop(Run &h, uint8_t type, uint8_t &town) {
    for (uint8_t location = 1; location <= 32; ++location) {
        if (!teleport_town(h, location, 15, 30)) continue;
        if (h.rt->open_shop_for_test(type)) {
            town = location;
            h.run(100);
            return true;
        }
    }
    return false;
}
void raw_key(Run &h, tdeck::RawInputEvent e) {
    openu5_host_virtual_clock_us() += 100000;
    h.raw(e);
    h.render();
}
void trackball_click(Run &h) {
    tdeck::RawInputEvent click{};
    click.kind = RawInputKind::TrackballClick;
    click.transition = tdeck::KeyTransition::Pressed;
    raw_key(h, click);
    click.transition = tdeck::KeyTransition::Released;
    raw_key(h, click);
    h.run(100);
}

/**
 * Out of a shop as a player would: N at a gate or a deal ("Anything else?",
 * "Deal?" -- Mic is no key at a gate), Mic everywhere else.
 */
void leave_shop(Run &h) {
    for (int i = 0; i < 12 && h.rt->ui()->mode() == UiMode::Shop; ++i) {
        const auto phase = h.rt->ui()->shop_phase();
        const bool gate = phase == ShopPhase::Greeting || phase == ShopPhase::HealerAgain || phase == ShopPhase::InnAgain ||
                          phase == ShopPhase::TavernAgain || phase == ShopPhase::ShipAgain || phase == ShopPhase::BuyDeal ||
                          phase == ShopPhase::SellDeal || phase == ShopPhase::ReagentDeal || phase == ShopPhase::GuildDeal ||
                          phase == ShopPhase::HorseDeal || phase == ShopPhase::ShipDeal || phase == ShopPhase::HealerDeal ||
                          phase == ShopPhase::InnRestDeal || phase == ShopPhase::InnLeaveDeal || phase == ShopPhase::RumorDeal ||
                          phase == ShopPhase::TavernRoundDeal || phase == ShopPhase::TavernDrinkDeal;
        if (gate) h.key('n');
        else h.mic();
        h.run(100);
    }
}

// --- the DOS panel's cells, read back from the screen -------------------------
// Window 1's cell (col, row): x 192 + 8 col, y 8 + 8 row. Its IBM.CH code, by
// matching the 8 x 8 pixels against the font (either polarity); -1 if none.
int cell_code(int col, int row, bool *inverse = nullptr) {
    const int x0 = 192 + col * 8, y0 = 8 + row * 8;
    // Where two codes share a glyph (the blank, 'O'/'0'), the space and the
    // letters are tried first.
    static const std::vector<int> order = [] {
        std::vector<int> o{0x20};
        for (int c = 'A'; c <= 'Z'; ++c) o.push_back(c);
        for (int c = 'a'; c <= 'z'; ++c) o.push_back(c);
        for (int c = 0; c < 128; ++c)
            if (std::find(o.begin(), o.end(), c) == o.end()) o.push_back(c);
        return o;
    }();
    for (const int code : order)
        for (int inv = 0; inv < 2; ++inv) {
            bool same = true;
            for (int r = 0; r < 8 && same; ++r)
                for (int c = 0; c < 8 && same; ++c)
                    same = (px(x0 + c, y0 + r) == kWhite) == (ibm_bit(char(code), r, c) != (inv != 0));
            if (same) {
                if (inverse) *inverse = inv != 0;
                return code;
            }
        }
    return -1;
}
/** Window 1's row as text: printable IBM.CH codes, '#' for anything else (a RUNES.CH cell, a box glyph). */
std::string panel_row(int row, int from = 0, int to = 15, bool *inverse = nullptr) {
    std::string s;
    bool inv = false, any = false;
    for (int col = from; col < to; ++col) {
        bool cell_inverse = false;
        const int code = cell_code(col, row, &cell_inverse);
        s += code >= 0x20 && code < 0x7f ? char(code) : '#';
        any = any || cell_inverse;
        inv = inv || cell_inverse;
    }
    if (inverse) *inverse = any;
    return s;
}
std::string trim(std::string s) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}
const DeviceNativeFrame *frame(Run &h) { return h.rt->native_frame_for_test(); }
std::string strip_question(Run &h) { const auto *f = frame(h); return f ? f->strip.question : ""; }
std::string strip_detail(Run &h) { const auto *f = frame(h); return f ? f->strip.detail : ""; }
// IBM.CH draws '0' and 'O' alike: compare with the two folded together.
std::string fold(std::string s) {
    for (auto &c : s)
        if (c == '0') c = 'O';
    return s;
}
bool has(const std::string &s, const std::string &what) { return fold(s).find(fold(what)) != std::string::npos; }

// Strip rows on the screen (rule 203; question 206..214, detail 217..225, keys 228..236).
bool strip_rule() { return white_run(203, 4, 315); }
int strip_ink(int y0, int y1, uint16_t colour) { return ink(4, y0, 315, y1, colour); }

// ---------------------------------------------------------------------------
void test_names() {
    std::printf("\nN  the DOS names (data.json -> one block)\n");
    DosTextBlock block{};
    const bool ok = dos_text_extract(g_data_json.data(), g_data_json.size(), block);
    bool spells = ok, reagents = ok, sigils = ok;
    for (int i = 0; ok && i < 48; ++i) {
        const char *s = dos_text_lookup(block, DosText::Spell, i);
        spells = spells && s && *s && std::strlen(s) <= 10;
    }
    for (int i = 0; ok && i < 8; ++i) {
        const char *r = dos_text_lookup(block, DosText::Reagent, i);
        reagents = reagents && r && *r && std::strlen(r) <= 10 && dos_text_lookup(block, DosText::PotionColour, i);
        const char *scroll = dos_text_lookup(block, DosText::UseItem, i);
        const char *potion = dos_text_lookup(block, DosText::UseItem, 8 + i);
        const char *phase = dos_text_lookup(block, DosText::UseItem, 21 + i);
        sigils = sigils && scroll && scroll[0] == '*' && potion && std::string(potion) == "!" && phase && phase[0] == '(' &&
                 phase[1] == char('0' + i);
    }
    check(ok && block.loaded && block.used < DosTextBlock::kArenaBytes,
          "N1 the pack's data.json reduces to one block: " + n(long(block.used)) + " of " +
              n(long(DosTextBlock::kArenaBytes)) + " arena bytes, sizeof " + n(long(sizeof(DosTextBlock))));
    check(spells && reagents, "N2 48 spell and 8 reagent short names (at most 10 cells), 8 potion colours");
    check(sigils && dos_text_lookup(block, DosText::UseItem, 37) && !dos_text_lookup(block, DosText::UseItem, 38),
          "N3 the 38 (U)se entries in DS 0x1916's order: '*' scrolls, '!' potions, '(' moonstone phases 0..7");
    auto literal = [&](int at) { const char *s = dos_text_lookup(block, DosText::Literal, at); return std::string(s ? s : "<none>"); };
    check(literal(0x9778) == "--" && literal(0x9788) == "Moonstone " && literal(0x8f64) == "Reagents:" &&
              literal(0x48b8) == "Items:" && literal(0x9794) == "(None owned!)" && literal(0x4fc0) == "    GUEST" &&
              literal(0x977c).size() == 4 && literal(0x977c)[0] == '\x1c' && !dos_text_lookup(block, DosText::Literal, 0x1234),
          "N4 the DGROUP literals by DS address (UTF-8 decoded, so 0x48b8 is \"Items:\"): " + literal(0x48b8));
    check(!dos_text_lookup(block, DosText::EquipmentShort, 0),
          "N5 the equipment tables are not data.json's (the platform's generated tables serve them)");
    // Synthetic documents: escapes, UTF-8, the counts, the arena.
    auto doc = [](const std::string &spells_json, const std::string &pool) {
        std::string s = "{\"x\":{\"y\":[1,2.5e3,true,null,\"\\\"q\\\"\"]},\"potions\":[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\"],"
                        "\"reagents\":[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\"],\"spells\":" + spells_json +
                        ",\"specialItemNames\":[\"1\",\"2\",\"3\",\"4\",\"5\"],\"shards\":[\"1\",\"2\",\"3\"],"
                        "\"specialItemNames2\":[\"1\",\"2\",\"3\",\"4\",\"5\",\"6\"],\"stringPools\":[{\"strings\":[\"*a\",\"*b\","
                        "\"*c\",\"*d\",\"*e\",\"*f\",\"*g\",\"*h\"],\"name\":\"scrollSpellCodes\",\"offset\":16},{\"name\":"
                        "\"parenDigitStrings\",\"offset\":100,\"strings\":[\"(0\",\"(1\",\"(2\",\"(3\",\"(4\",\"(5\",\"(6\",\"(7\"]}" +
                        pool + "]}";
        return s;
    };
    std::string spells48 = "[";
    for (int i = 0; i < 48; ++i) spells48 += std::string(i ? "," : "") + "\"S" + std::to_string(i) + "\"";
    spells48 += "]";
    // A pool at DS 0x9778 - 4: "é\u0001" (2 bytes) then "Ã©" as raw UTF-8 (2 bytes), then "--".
    const std::string pool = ",{\"name\":\"p\",\"offset\":" + std::to_string(0x9778 + 0x10 - 6) +
                             ",\"strings\":[\"\\u00e9\\u0001\",\"\xc3\xa9\xc3\xa9\",\"--\"]}";
    DosTextBlock synthetic{};
    const bool parsed = dos_text_extract(doc(spells48, pool).data(), doc(spells48, pool).size(), synthetic);
    const char *dashes = dos_text_lookup(synthetic, DosText::Literal, 0x9778);
    check(parsed && dashes && std::string(dashes) == "--" && std::string(dos_text_lookup(synthetic, DosText::Spell, 47)) == "S47" &&
              std::string(dos_text_lookup(synthetic, DosText::UseItem, 0)) == "*a" &&
              std::string(dos_text_lookup(synthetic, DosText::UseItem, 21)) == "(0",
          "N6 a synthetic document: escapes and raw UTF-8 count one byte each, so the DS address after them is exact");
    std::string spells47 = spells48.substr(0, spells48.rfind(',')) + "]";
    DosTextBlock refused{};
    const std::string short_doc = doc(spells47, pool), whole = doc(spells48, pool);
    const bool short_spells = dos_text_extract(short_doc.data(), short_doc.size(), refused);
    const bool truncated = dos_text_extract(whole.data(), whole.size() / 2, refused);
    std::string huge = "[";
    for (int i = 0; i < 48; ++i) huge += std::string(i ? "," : "") + "\"" + std::string(60, 'x') + "\"";
    huge += "]";
    const std::string overflow_doc = doc(huge, pool);
    const bool overflow = dos_text_extract(overflow_doc.data(), overflow_doc.size(), refused);
    check(!short_spells && !truncated && !overflow && !refused.loaded && !dos_text_lookup(refused, DosText::Spell, 0),
          "N7 47 spells, a truncated document, an arena overflow: refused, nothing served");
    // The prototype's hook.
    check(tdeck::load_prototype_dos_text_json(g_data_json.data(), g_data_json.size()) && tdeck::prototype_dos_text_loaded(),
          "N8 the prototype loads the tables (as at boot, from the open pack)");
    tdeck::install_prototype_presentation(PrototypePresentation::Native);
    const char *leath = platform_dos_text(DosText::EquipmentShort, 0);
    const char *glyph = platform_dos_text(DosText::EquipmentGlyph, 0);
    check(leath && std::string(leath) == "Leath Helm" && glyph && std::strlen(glyph) == 1 &&
              platform_dos_text(DosText::Spell, 0) && !platform_dos_text(DosText::EquipmentShort, 48),
          "N9 the profile's dos_text: DS 0x1962 (\"Leath Helm\"), its glyphs, and the data.json tables");
    set_platform_profile(PlatformProfile{});
}

// ---------------------------------------------------------------------------
// S: shops on the Native screen.
struct ShopProbe {
    bool native_panel = false, frame = false, strip = false;
    std::string banner, rows[7], detail, question;
    int bar_row = -1;
};
ShopProbe probe_shop(Run &h) {
    ShopProbe p;
    const auto *f = frame(h);
    p.native_panel = f && f->panel;
    p.frame = frame_drawn();
    p.strip = strip_rule() && strip_ink(206, 214, kCyan) > 0;
    if (f && f->panel) p.banner = f->panel->banner;
    for (int r = 0; r < 7; ++r) {
        bool inv = false;
        p.rows[r] = panel_row(r, 0, 15, &inv);
        if (inv && p.bar_row < 0) p.bar_row = r;
    }
    p.detail = strip_detail(h);
    p.question = strip_question(h);
    return p;
}

void test_shops() {
    std::printf("\nS  shops on the Native screen\n");
    Run h(kParty);
    boot(h, PrototypePresentation::Native);
    auto &g = h.rt->game();
    g.gold = 9999;
    uint8_t town = 0;
    const bool open = open_shop(h, 0, town);
    h.key('\r');
    h.run(100);
    dump("p3-shop-menu");
    const bool menu_native = frame_drawn() && strip_rule() && h.board.handheld_fallbacks() == 0;
    h.key('b');
    h.run(100);
    auto view = h.rt->shop_view_for_test();
    auto p = probe_shop(h);
    dump("p3-shop-buy");
    check(open && menu_native && view.active && p.native_panel && p.frame && h.board.handheld_fallbacks() == 0,
          "S1 the blacksmith's menu and Buy list stay on the Native screen (no handheld fallback)");
    bool priced = true;
    std::string listed;
    for (size_t r = 0; r < std::min<size_t>(7, h.rt->shop_view_for_test().total); ++r) {
        const auto row = trim(p.rows[r]);
        listed += row + "|";
        // name (<= 10 cells) ... price right-aligned to cell 14
        priced = priced && row.size() >= 3 && ((p.rows[r][14] >= '0' && p.rows[r][14] <= '9') || p.rows[r][14] == 'O');
    }
    check(p.banner == "Buy" && priced && p.bar_row == 0 && trim(panel_row(8)).rfind("Gold", 0) == 0 &&
              has(panel_row(8), "9999"),
          "S2 the Buy panel: banner 'Buy', a price ending in cell 14 on every row, the bar on row 0, 'Gold 9999': " + listed);
    check(has(p.question, view.title) && has(p.question, "Buy 1/") && has(p.detail, view.rows[0].name) &&
              has(p.detail, n(view.rows[0].price) + "gp") && strip_ink(217, 225, kWhite) > 0 && strip_ink(228, 236, kGreen) > 0,
          "S3 the strip: '" + p.question + "' / '" + p.detail + "' and the keys");
    h.down();
    h.run(50);
    auto p2 = probe_shop(h);
    const auto view2 = h.rt->shop_view_for_test();
    check(p2.bar_row == 1 && has(p2.detail, view2.rows[view2.selected_row].name) && has(p2.question, "Buy 2/"),
          "S4 the trackball moves the bar to row 1 and the strip follows: " + p2.detail);
    // Buy it: Enter, then Y.
    const int32_t gold_before = g.gold;
    const int32_t price = view2.rows[view2.selected_row].price;
    h.key('\r');
    h.run(100);
    dump("p3-shop-deal");
    const auto deal = probe_shop(h);
    const bool deal_native = h.rt->ui()->mode() == UiMode::Shop && deal.native_panel && deal.bar_row == 0 && deal.frame;
    h.key('y');
    h.run(300);
    check(deal_native && g.gold == gold_before - price && h.board.handheld_fallbacks() == 0,
          "S5 Enter then Y buys it on the Native screen: gold " + n(gold_before) + " -> " + n(g.gold) + " (price " + n(price) + ")");
    dump("p3-shop-bought");
    // Declined: Enter then N.
    for (int i = 0; i < 4 && h.rt->ui()->shop_phase() != ShopPhase::Buy; ++i) { h.key('\r'); h.run(100); }
    if (h.rt->ui()->shop_phase() != ShopPhase::Buy) { h.key('b'); h.run(100); }
    const int32_t gold_declined = g.gold;
    h.key('\r');
    h.run(100);
    h.key('n');
    h.run(300);
    check(g.gold == gold_declined && frame_drawn(), "S6 Enter then N declines: gold unchanged on the Native screen");
    // Not enough gold.
    for (int i = 0; i < 4 && h.rt->ui()->shop_phase() != ShopPhase::Buy; ++i) { h.key('\r'); h.run(100); }
    if (h.rt->ui()->shop_phase() != ShopPhase::Buy) { h.key('b'); h.run(100); }
    g.gold = 1;
    h.down(); // the Sling, 14 gp
    h.run(50);
    const auto poor = probe_shop(h);
    const std::string poor_gold = trim(panel_row(8));
    h.key('\r');
    h.run(100);
    h.key('y');
    h.run(300);
    dump("p3-shop-poor");
    const std::string said = h.transcript().substr(h.transcript().size() > 160 ? h.transcript().size() - 160 : 0);
    check(g.gold == 1 && poor.native_panel && poor.bar_row == 1 && poor_gold.rfind("Gold", 0) == 0 &&
              poor_gold.back() == '1' && has(said, "pay") && frame_drawn() && h.board.handheld_fallbacks() == 0,
          "S7 with 1 gold ('" + poor_gold + "') the Sling is refused -- the smith's own words on the console -- gold stays 1");
    // Cancel out of everything with Mic: the frame repaints whole, the strip goes black.
    bus::reset_stats();
    leave_shop(h);
    check(h.rt->ui()->mode() != UiMode::Shop && frame_drawn() && rows_black(200, 239) && h.board.handheld_fallbacks() == 0,
          "S8 Mic cancels back out of the shop: the roster returns, the strip is black");
    // Sell: the party's count and the sell price on the strip; look-alike rings distinct.
    g.gold = 100;
    g.equipment_quantities[0] = 2;  // Leather Helm
    g.equipment_quantities[42] = 1; // three rings with long, look-alike names
    g.equipment_quantities[43] = 1;
    g.equipment_quantities[44] = 1;
    open_shop(h, 0, town);
    h.key('\r');
    h.run(100);
    h.key('s');
    h.run(100);
    const auto sell = probe_shop(h);
    dump("p3-shop-sell");
    const auto sv = h.rt->shop_view_for_test();
    std::set<std::string> distinct;
    std::string sold;
    for (size_t r = 0; r < std::min<size_t>(7, sv.total); ++r) {
        distinct.insert(trim(sell.rows[r].substr(0, 10)));
        sold += trim(sell.rows[r]) + "|";
    }
    check(sell.banner == "Sell" && sv.total >= 4 && distinct.size() == std::min<size_t>(7, sv.total) &&
              has(sell.detail, "(have 2)") && has(sell.detail, "Leather Helm"),
          "S9 Sell: every row distinct (long names as DS 0x1962's), the count and whole name on the strip: " + sold +
              " / " + sell.detail);
    // S9b: past the seventh row the list scrolls (minimal scroll): the bar stays in view.
    bool bar_in_view = sv.total > 7;
    for (size_t i = 1; bar_in_view && i < sv.total; ++i) {
        h.down();
        h.run(20);
        const auto q = probe_shop(h);
        bar_in_view = q.bar_row >= 0 && q.bar_row < 7 && has(q.question, "Sell " + n(long(i + 1)) + "/" + n(long(sv.total)));
    }
    const auto *bottom = frame(h);
    const bool arrow_up = bottom && bottom->panel && bottom->panel->arrow == kDosArrowUp;
    for (size_t i = 1; i < sv.total; ++i) {
        h.up();
        h.run(20);
    }
    const auto *top = frame(h);
    check(bar_in_view && arrow_up && top && top->panel && top->panel->arrow == kDosArrowDown && probe_shop(h).bar_row == 0,
          "S9b the " + n(long(sv.total)) + "-row Sell list scrolls end to end: the bar always among the seven rows, the arrow up at the end, down at the top");
    const int32_t gold_sell = g.gold;
    h.key('\r');
    h.run(100);
    h.key('y');
    h.run(300);
    check(g.gold > gold_sell && g.equipment_quantities[0] == 1, "S10 selling one Leather Helm: gold " + n(gold_sell) + " -> " + n(g.gold));
    leave_shop(h);
    // The other merchants and services.
    struct Kind { uint8_t type; const char *key; const char *banner; };
    const Kind kinds[] = {{4, "\r", "Reagents"}, {5, "\r", "Guild"}};
    for (const auto &k : kinds) {
        g.gold = 9999;
        const bool opened = open_shop(h, k.type, town);
        for (int i = 0; i < 3 && h.rt->ui()->mode() == UiMode::Shop && !h.rt->shop_view_for_test().active; ++i) {
            h.key(uint8_t(*k.key));
            h.run(100);
        }
        const auto q = probe_shop(h);
        dump((std::string("p3-shop-") + k.banner).c_str());
        bool rows = true;
        for (size_t r = 0; r < std::min<size_t>(7, h.rt->shop_view_for_test().total); ++r) rows = rows && trim(q.rows[r]).size() >= 3;
        check(opened && q.native_panel && q.banner == k.banner && rows && q.frame && h.board.handheld_fallbacks() == 0,
              std::string("S11 the ") + k.banner + " list in town " + n(town) + " is a Native panel: " + trim(q.rows[0]) +
                  " / " + q.detail);
        leave_shop(h);
    }
    // The healer: the member choice is the roster picker.
    g.gold = 9999;
    g.party.characters[1].current_hp = 10;
    const bool healer = open_shop(h, 6, town);
    h.key('\r');
    h.run(100);
    h.key('h');
    h.run(100);
    const auto hp = h.rt->party_highlight_for_test();
    const auto *hf = frame(h);
    dump("p3-shop-healer");
    bool banner = ibm_mismatch(192 + 8 * 4, 0, "Select:", kWhite, kBlack) == 0 || ink(200, 0, 300, 7, kWhite) > 0;
    bool cursor_row = false;
    panel_row(int(hp.cursor >= 0 ? hp.cursor : 0), 0, 15, &cursor_row);
    check(healer && hf && !hf->panel && hp.select_banner && hp.cursor >= 0 && cursor_row && banner &&
              has(strip_detail(h), g.party.characters[hp.cursor].name) && h.board.handheld_fallbacks() == 0,
          "S12 the healer's 'whom' is the roster picker: the cursor's row in reverse video, Select:, the member on the strip: " +
              strip_detail(h));
    h.down();
    h.run(50);
    const auto hp2 = h.rt->party_highlight_for_test();
    check(hp2.cursor >= 0 && hp2.cursor != hp.cursor, "S13 the trackball moves the roster cursor (" + n(hp.cursor) + " -> " + n(hp2.cursor) + ")");
    leave_shop(h);
    check(frame_drawn() && rows_black(200, 239) && !h.rt->party_highlight_for_test().select_banner,
          "S14 leaving the healer: no cursor, no Select:, strip black");
    // The inn: Leave's member choice is the roster picker; with two guests in
    // the register, Pick up is the 1988 GUEST REGISTER panel.
    const bool inn = open_shop(h, 7, town);
    h.key('\r');
    h.run(100);
    dump("p3-inn-menu");
    h.key('l');
    h.run(100);
    const auto leave = h.rt->party_highlight_for_test();
    dump("p3-inn-leave");
    const bool leave_roster = h.rt->ui()->shop_phase() == ShopPhase::InnLeaveMember && leave.select_banner &&
                              leave.cursor >= 0 && frame(h) && !frame(h)->panel;
    leave_shop(h);
    auto &party = h.rt->game().party;
    const char *guests[] = {"Dupre", "Geoffrey"};
    for (int i = 0; i < 2; ++i) {
        auto &m = party.characters[party.character_count + i];
        m = party.characters[1];
        std::snprintf(m.name, sizeof(m.name), "%s", guests[i]);
        m.party_status = town;
    }
    party.character_count = uint8_t(party.character_count + 2);
    open_shop(h, 7, town);
    h.key('\r');
    h.run(100);
    h.key('p');
    h.run(100);
    const auto *rf = frame(h);
    dump("p3-inn-register");
    std::string names = trim(panel_row(4, 1, 14)) + "|" + trim(panel_row(5, 1, 14));
    bool bar = false;
    panel_row(4, 1, 14, &bar);
    const bool register_panel = h.rt->ui()->shop_phase() == ShopPhase::InnPickupMember && rf && rf->panel &&
                                has(panel_row(1, 1, 14), "GUEST") && has(panel_row(2, 1, 14), "REGISTER") &&
                                has(names, "Dupre") && has(names, "Geoffrey") && bar && has(strip_detail(h), "Dupre");
    h.down();
    h.run(50);
    check(inn && leave_roster && register_panel && has(strip_detail(h), "Geoffrey") && h.board.handheld_fallbacks() == 0,
          "S15 the inn: Leave is the roster picker; Pick up with two guests is the 1988 GUEST REGISTER (" + names +
              "), the bar and the strip follow the cursor");
    leave_shop(h);
    std::string why;
    check(bus::stats().malformed == 0 && windows_in_bounds() && windows_paced(why),
          "S16 no malformed pixel data, no window outside the panel, every window over 16 rows paced" + (why.empty() ? std::string() : ": " + why));
    set_platform_profile(PlatformProfile{});
}

// ---------------------------------------------------------------------------
void test_lists() {
    std::printf("\nL  lists, panels and the member picker\n");
    Run h(kParty);
    auto &g = h.rt->game();
    auto &avatar = g.party.characters[0];
    avatar.armor = 10;  // Leather Armour, worn
    avatar.helmet = 0;  // Leather Helm, worn ("Leath Helm" in DS 0x1962)
    g.equipment_quantities[10] = 1;
    g.equipment_quantities[0] = 1;
    g.equipment_quantities[44] = 1; // Ring of Regeneration
    g.potion_quantities[0] = 2;
    g.scroll_quantities[0] = 1;
    for (int i = 0; i < 8; ++i) g.reagent_quantities[i] = int16_t(3 + i);
    g.spell_quantities[0] = 3;
    g.spell_quantities[5] = 1;
    g.party.characters[1].current_mp = 20;
    boot(h, PrototypePresentation::Native);
    // Ready: who, then the 1988 item page with DS 0x1962 names; slot detail on the strip.
    h.key('r');
    h.run(100);
    const auto who = h.rt->party_highlight_for_test();
    const auto *wf = frame(h);
    dump("p3-ready-who");
    check(h.rt->ui()->mode() == UiMode::PartySelection && wf && !wf->panel && who.select_banner && who.cursor == 0 &&
              has(strip_detail(h), "Avatar") && has(strip_detail(h), "HP"),
          "L1 Ready's 'whom' is the roster picker: Avatar's row under Select:, HP/MP on the strip: " + strip_detail(h));
    h.key('\r');
    h.run(100);
    dump("p3-ready-items");
    std::string ready_rows;
    for (int r = 1; r <= 3; ++r) ready_rows += trim(panel_row(r, 1, 14)) + "|";
    check(h.rt->ui()->mode() == UiMode::EquipmentSelection && has(ready_rows, "Leath Helm") && has(strip_question(h), "Leather") &&
              has(strip_detail(h), "Slot:") && has(strip_detail(h), "Current:"),
          "L2 the Ready page names DS 0x1962's 'Leath Helm'; the whole name and the slot's current item on the strip: " +
              ready_rows + " / " + strip_detail(h));
    h.down();
    h.run(50);
    const std::string moved = strip_question(h);
    check(!moved.empty() && moved != "" && has(strip_detail(h), "Slot:"), "L3 moving the bar updates the strip: " + moved);
    h.mic();
    h.mic();
    h.run(100);
    check(h.rt->ui()->mode() == UiMode::Exploration && frame_drawn() && rows_black(200, 239), "L4 Mic, Mic: back to the roster, strip black");
    // Use: DATA.OVL entries with their sigils.
    h.key('u');
    h.run(100);
    dump("p3-use");
    bool runes = false;
    for (int r = 1; r <= 3; ++r) runes = runes || has(panel_row(r, 1, 14), "#");
    check(h.rt->ui()->mode() == UiMode::InventorySelection && frame(h) && frame(h)->panel &&
              std::string(frame(h)->panel->banner) == "Items:" && runes && !strip_question(h).empty(),
          "L5 Use: the 'Items:' page with the (U)se sigils (RUNES.CH cells), the whole name on the strip: " + strip_question(h));
    h.mic();
    h.run(100);
    // Cast: the member on the roster, then the spell list.
    h.key('c');
    h.run(100);
    const auto caster = h.rt->party_highlight_for_test();
    dump("p3-cast-who");
    h.down();
    h.run(50);
    const auto caster2 = h.rt->party_highlight_for_test();
    check(caster.select_banner && caster2.cursor == 1 && has(strip_detail(h), "Iolo") && has(strip_detail(h), "MP 20"),
          "L6 Cast's member: the roster cursor moves to Iolo; 'MP 20' on the strip: " + strip_detail(h));
    h.key('\r');
    h.run(100);
    dump("p3-cast-spells");
    const auto *sf = frame(h);
    std::string spell_rows = trim(panel_row(1, 1, 14)) + "|" + trim(panel_row(2, 1, 14));
    check(h.rt->ui()->mode() == UiMode::SpellSelection && sf && sf->panel && has(spell_rows, "3-") &&
              has(strip_detail(h), "MP") && has(strip_detail(h), ";") && !strip_question(h).empty(),
          "L7 the spell list: framed rows 'NN-name' (DATA.OVL's short names), effect / target / MP on the strip: " +
              spell_rows + " / " + strip_detail(h));
    h.mic();
    h.mic();
    h.run(100);
    // Mix: all 48 spells -- a long list, the bar in view and the arrow at every step.
    h.key('m');
    h.run(100);
    bool in_view = h.rt->ui()->mode() == UiMode::SpellSelection, arrows = true;
    int steps = 0;
    std::set<std::string> seen;
    for (; in_view && steps < 48; ++steps) {
        int bar = -1;
        for (int r = 1; r <= 7; ++r) {
            bool inv = false;
            panel_row(r, 1, 14, &inv);
            if (inv) bar = r;
        }
        in_view = bar >= 1 && bar <= 7;
        const auto *mf = frame(h);
        const size_t total = 48;
        arrows = arrows && mf && mf->panel && (steps == 0 ? mf->panel->arrow == kDosArrowDown
                                                : steps == 47 ? mf->panel->arrow == kDosArrowUp
                                                              : mf->panel->arrow != 0);
        seen.insert(strip_question(h));
        (void)total;
        h.down();
        h.run(20);
    }
    dump("p3-mix-spells-end");
    check(in_view && arrows && seen.size() == 48,
          "L8 Mix's 48 spells scroll end to end: the bar always in the frame, the arrow down / both / up, 48 distinct names on the strip (" +
              n(long(seen.size())) + ")");
    h.mic();
    h.run(100);
    // Mix's reagents.
    h.key('m');
    h.run(100);
    h.key('\r');
    h.run(100);
    dump("p3-mix-reagents");
    const auto *rf = frame(h);
    check(rf && rf->panel && has(strip_question(h), "Mix:") && has(strip_detail(h), "x3") &&
              trim(panel_row(1, 0, 16)).size() >= 4,
          "L9 Mix's reagents: the 1988 'Reagents:' panel; the spell and the reagent's whole name and count on the strip: " +
              strip_question(h) + " / " + strip_detail(h));
    h.key('\r');
    h.run(50);
    check(has(strip_detail(h), "(marked)"), "L10 Enter marks the reagent; the strip says so: " + strip_detail(h));
    h.mic();
    h.mic();
    h.run(100);
    // Z-stats: the member, then the 1988 pages with the DOS names.
    h.key('z');
    h.run(100);
    const auto zp = h.rt->party_highlight_for_test();
    dump("p3-zstats-who");
    h.key('\r');
    h.run(100);
    dump("p3-zstats-stats");
    const bool stats_page = h.rt->zstats_active() && frame(h) && frame(h)->panel && has(strip_question(h), "Z-stats");
    h.ball(RawInputKind::TrackballRight);
    h.run(100);
    dump("p3-zstats-arms");
    std::string arms;
    for (int r = 2; r <= 4; ++r) arms += trim(panel_row(r, 0, 16)) + "|";
    check(zp.select_banner && stats_page && has(arms, "Leath Helm") && has(strip_detail(h), "Arms"),
          "L11 Z-stats: member on the roster, then the pages; Arms names DS 0x1962's short names: " + arms + " / " +
              strip_detail(h));
    h.mic();
    h.run(100);
    check(!h.rt->zstats_active() && frame_drawn() && rows_black(200, 239), "L12 closing Z-stats: the roster back, strip black");
    check(bus::stats().malformed == 0 && windows_in_bounds(), "L13 no malformed pixel data, no window outside the panel");
    set_platform_profile(PlatformProfile{});
}

// ---------------------------------------------------------------------------
void test_movement_and_prompts() {
    std::printf("\nM/P  MOVE and the strip's transitions\n");
    Run h(kParty);
    boot(h, PrototypePresentation::Native);
    check(rows_black(200, 239) && frame(h) && !frame(h)->strip.move, "M1 WASD Mode off: the strip is black");
    trackball_click(h);
    dump("p3-move");
    const bool move_on = h.rt->movement_mode() && frame(h)->strip.move && !frame(h)->strip.active;
    check(move_on && ink(292, 228, 315, 236, kGreen) > 0 && rows_black(200, 227) && rows_black(228, 239, 0, 285) &&
              ink(160, 0, 319, 199, kGreen) == 0,
          "M2 the trackball click turns WASD Mode on: 'MOVE' (green) at the strip's right, nothing else; the DOS screen untouched");
    h.key('d');
    h.run(200);
    check(h.rt->movement_mode() && ink(292, 228, 315, 236, kGreen) > 0, "M3 'd' moves (WASD) and MOVE stays");
    // A prompt over WASD Mode: (L)ook is not a WASD key.
    h.key('l');
    h.run(50);
    dump("p3-move-look");
    const auto *lf = frame(h);
    // (L)ook's direction is a movement context: MOVE stays, the prompt's keys left of it.
    check(h.rt->ui()->mode() == UiMode::TargetSelection && lf && lf->strip.active && lf->strip.move && strip_rule() &&
              strip_ink(206, 214, kCyan) > 0 && ink(4, 228, 285, 236, kGreen) > 0 && ink(292, 228, 315, 236, kGreen) > 0,
          "M4 a prompt in WASD Mode (Look's direction): the question, the keys, and MOVE at the right");
    // M4b: the keys' row stops short of MOVE whatever its length (a 52-cell legend, on the Board itself).
    {
        DeviceNativeFrame wide{};
        wide.strip.active = wide.strip.move = true;
        std::snprintf(wide.strip.question, sizeof(wide.strip.question), "Wide");
        std::memset(wide.strip.keys, 'W', tdeck::kNativeStripColumns);
        bus::reset_stats();
        h.board.set_native_frame(&wide);
        h.board.show_alpha(h.rt->composed_viewport(), *h.rt->ui(), h.rt->game(), h.rt->turn(), HudWorldState{},
                           pack->runes_font);
        dump("p3-move-wide-keys");
        // The keys' window ends before MOVE's (no pixel written twice); MOVE keeps its four
        // cells (x 292..315) and the blank cell before them (x 286..291).
        bool keys_short = false, overdraw = false;
        for (const auto &w : bus::windows()) {
            if (w.y0 != 228) continue;
            keys_short = keys_short || (w.x0 == 4 && w.x1 == 285);
            overdraw = overdraw || (w.x0 < 286 && w.x1 >= 286);
        }
        check(keys_short && !overdraw && ink(286, 228, 291, 236) == 0 && ink(292, 228, 315, 236, kGreen) > 0 &&
                  ink(4, 228, 280, 236, kGreen) > 0,
              "M4b a 52-cell legend's window stops short of MOVE's (no overdraw); MOVE keeps its cells");
        h.render(true);
    }
    h.mic();
    h.run(50);
    check(h.rt->ui()->mode() == UiMode::Exploration && ink(292, 228, 315, 236, kGreen) > 0 && !strip_rule() &&
              rows_black(200, 227),
          "M5 Mic closes the prompt: the rule and rows go, MOVE comes back");
    // Mic held: the other toggle.
    tdeck::RawInputEvent mic{};
    mic.kind = RawInputKind::Keyboard;
    mic.column = tdeck::kMicrophoneKeyColumn;
    mic.row = tdeck::kMicrophoneKeyRow;
    mic.transition = tdeck::KeyTransition::Pressed;
    raw_key(h, mic);
    h.run(1500);
    mic.transition = tdeck::KeyTransition::Released;
    raw_key(h, mic);
    h.run(100);
    check(!h.rt->movement_mode() && rows_black(200, 239), "M6 Mic held 1.1 s turns WASD Mode off: the strip black");
    // P: Y/N, then the text entry of a conversation-like prompt (Yell), then nothing.
    h.key('y');
    h.run(50);
    dump("p3-yell");
    const bool text = h.rt->ui()->mode() == UiMode::TextEntry && strip_rule() && strip_ink(206, 214, kCyan) > 0;
    h.key('h');
    h.key('i');
    h.run(50);
    const std::string typed = strip_question(h);
    h.mic();
    h.mic();
    h.run(100);
    check(text && has(typed, "hi") && rows_black(200, 239),
          "P1 a text prompt: the question with what is typed ('" + typed + "'), black again when it closes");
    // A long, two-step modal (Cast -> member -> spell) back to nothing: no stale detail row.
    h.key('c');
    h.run(50);
    const bool detail_shown = strip_ink(217, 225, kWhite) > 0;
    h.key('\r');
    h.run(50);
    h.mic();
    h.run(50);
    h.mic();
    h.run(50);
    check(detail_shown && rows_black(200, 239) && h.rt->ui()->mode() == UiMode::Exploration,
          "P2 Cast, its spell list, Mic, Mic: the detail row came and went; nothing stale under the picture");
    set_platform_profile(PlatformProfile{});
}

// ---------------------------------------------------------------------------
void test_redraws_and_switch() {
    std::printf("\nR/X  redraws, and switching the presentation of a running game\n");
    Run h(kParty);
    auto &g = h.rt->game();
    g.equipment_quantities[0] = 1;
    g.gold = 9999;
    boot(h, PrototypePresentation::Native);
    // R1: the System Menu over an open list, and back.
    h.key('r');
    h.run(50);
    h.key('\r');
    h.run(50);
    const std::string before = panel_row(1, 1, 14);
    dump("p3-r1-before");
    h.key('m', true);
    h.run(200);
    dump("p3-r1-menu");
    const bool menu = !frame_drawn();
    bus::reset_stats();
    h.mic();
    h.run(200);
    dump("p3-r1-after");
    check(menu && full_window_written() && frame_drawn(false) && panel_row(1, 1, 14) == before && strip_rule(),
          "R1 Alt+M over the Ready page, Mic: a whole repaint with the page and its strip as they were");
    h.mic();
    h.mic();
    h.run(100);
    // R2: an unchanged frame after a list closes writes nothing.
    bus::reset_stats();
    h.render();
    check(bus::windows().empty(), "R2 an unchanged frame after the list closed writes no window");
    // X: switch at the safe boundary (exploration), with a list open, and with a shop open.
    const auto state_before = fnv(&g, sizeof(g));
    bus::reset_stats();
    select(h, PrototypePresentation::Enhanced);
    h.render();
    check(full_window_written() && !frame_drawn() && fnv(&g, sizeof(g)) == state_before,
          "X1 Native -> Enhanced at the command prompt: a whole repaint of the handheld screen; game state untouched");
    bus::reset_stats();
    select(h, PrototypePresentation::Native);
    h.render();
    check(full_window_written() && frame_drawn() && rows_black(200, 239) && fnv(&g, sizeof(g)) == state_before,
          "X2 Enhanced -> Native: a whole repaint of the DOS frame, the strip black");
    h.key('c');
    h.run(50);
    select(h, PrototypePresentation::Enhanced);
    h.render();
    const bool handheld_list = h.rt->selection_view_for_test().active && !frame_drawn();
    bus::reset_stats();
    select(h, PrototypePresentation::Native);
    h.render();
    check(handheld_list && full_window_written() && frame_drawn() && h.rt->party_highlight_for_test().select_banner &&
              strip_rule(),
          "X3 with Cast's member list open: Enhanced shows its list; back to Native, the roster picker and the strip");
    h.mic();
    h.run(50);
    uint8_t town = 0;
    open_shop(h, 0, town);
    h.key('\r');
    h.run(50);
    h.key('b');
    h.run(50);
    select(h, PrototypePresentation::Enhanced);
    h.render();
    const auto enhanced_shop = h.rt->shop_view_for_test();
    const bool handheld_shop = enhanced_shop.active && white_run(2, 182, 318);
    bus::reset_stats();
    select(h, PrototypePresentation::Native);
    h.render();
    const auto p = probe_shop(h);
    check(handheld_shop && full_window_written() && p.frame && p.native_panel && p.banner == "Buy" && p.strip,
          "X4 with the Buy list open: Enhanced's shop panel, then Native's -- each a whole repaint");
    leave_shop(h);
    check(frame_drawn() && rows_black(200, 239) && h.board.handheld_fallbacks() == 0,
          "X5 after the switches the shop closes to a clean Native screen; no handheld fallback at any point");
    // E: an ending page (ENDGAME.OVL's 320 x 200 screens) on each screen, and back.
    std::vector<uint8_t> page(320 / 2 * 200, 0x11); // every pixel palette entry 1
    uint16_t palette[16]{};
    palette[1] = 0x1234;
    h.board.show_endgame_page(page.data(), palette, nullptr, nullptr, nullptr, 1, true);
    const bool native_page = ink(0, 0, 319, 199, 0x1234) == 320 * 200 && rows_black(200, 239);
    bus::reset_stats();
    h.render(true);
    const bool back = full_window_written() && frame_drawn() && rows_black(200, 239);
    select(h, PrototypePresentation::Enhanced);
    h.board.show_endgame_page(page.data(), palette, nullptr, nullptr, nullptr, 2, true);
    const bool enhanced_page = ink(0, 20, 319, 219, 0x1234) == 320 * 200 && rows_black(0, 19) && rows_black(220, 239);
    select(h, PrototypePresentation::Native);
    h.render(true);
    check(native_page && back && enhanced_page && frame_drawn() && rows_black(200, 239),
          "E1 an ending page fills rows 0..199 on the Native screen (y 20 on Enhanced), the strip black; the game screen repaints whole after it");
    set_platform_profile(PlatformProfile{});
}

// ---------------------------------------------------------------------------
// I: one raw script through shops and lists under both presentations.
struct Trace {
    std::vector<std::string> steps;
    std::string transcript;
    std::vector<uint8_t> gam;
    uint64_t state = 0;
    uint32_t fallbacks = 0;
};
std::string snapshot(Run &h, const std::string &label) {
    auto *ui = h.rt->ui();
    UiSelectionView sv{};
    const bool sel = ui->selection_view(sv);
    const auto bar = h.rt->context_bar_for_test();
    const auto view = h.rt->selection_view_for_test();
    const auto shop = h.rt->shop_view_for_test();
    std::string tail;
    const size_t count = ui->transcript_size();
    for (size_t i = count > 3 ? count - 3 : 0; i < count; ++i)
        if (const auto *b = ui->transcript_at(i)) tail += std::string(b->text) + "|";
    const auto &g = h.rt->game();
    char line[1600];
    std::snprintf(line, sizeof(line),
                  "%s: mode=%d req=%d phase=%d cursor=%d prompt=[%s] sel=%d/%zu/%zu legend=[%s] bar=%d[%s|%s] shop=%d/%zu "
                  "tail=[%s] loc=%u xy=%u,%u turns=%lld gold=%u food=%u eq=%016llx sp=%016llx rg=%016llx hp=%u/%u",
                  label.c_str(), int(ui->mode()), int(ui->request()), int(ui->shop_phase()), int(ui->shop_cursor()),
                  ui->prompt() ? ui->prompt() : "", int(sel), sel ? sv.cursor : size_t(0), sel ? sv.count : size_t(0),
                  view.context.actions, int(bar.active), bar.status, bar.actions, int(shop.active), shop.total, tail.c_str(),
                  unsigned(g.position.map.location), unsigned(g.position.xy.x), unsigned(g.position.xy.y),
                  (long long)g.turns_since_start, unsigned(g.gold), unsigned(g.food),
                  (unsigned long long)fnv(g.equipment_quantities, sizeof(g.equipment_quantities)),
                  (unsigned long long)fnv(g.spell_quantities, sizeof(g.spell_quantities)),
                  (unsigned long long)fnv(g.reagent_quantities, sizeof(g.reagent_quantities)),
                  unsigned(g.party.characters[1].current_hp), unsigned(g.party.characters[0].armor));
    return line;
}
Trace walk(PrototypePresentation p) {
    tdeck::host_memory_save_forget_for_test();
    Trace t;
    Run h(kParty);
    auto &g = h.rt->game();
    g.gold = 3000;
    g.equipment_quantities[0] = 2;
    g.reagent_quantities[0] = 4;
    g.reagent_quantities[1] = 4;
    g.spell_quantities[0] = 2;
    g.party.characters[1].current_mp = 20;
    g.party.characters[1].current_hp = 30;
    boot(h, p);
    auto snap = [&](const std::string &s) { t.steps.push_back(snapshot(h, s)); };
    snap("boot");
    uint8_t town = 0;
    // The blacksmith: buy, decline, refuse, sell, leave.
    open_shop(h, 0, town); snap("smith");
    h.key('\r'); h.run(100); snap("smith-menu");
    h.key('b'); h.run(100); snap("buy");
    h.down(); h.run(50); h.down(); h.run(50); snap("buy-down");
    h.key('\r'); h.run(100); snap("buy-deal");
    h.key('y'); h.run(300); snap("buy-yes");
    h.key('\r'); h.run(100); snap("buy-again");
    h.key('b'); h.run(100); h.key('\r'); h.run(100); h.key('n'); h.run(300); snap("buy-no");
    h.key('s'); h.run(100); snap("sell");
    h.key('\r'); h.run(100); h.key('y'); h.run(300); snap("sell-yes");
    leave_shop(h);
    snap("smith-left");
    // The healer: heal Iolo.
    open_shop(h, 6, town); snap("healer");
    h.key('\r'); h.run(100); h.key('h'); h.run(100); snap("heal-who");
    h.down(); h.run(50); h.key('\r'); h.run(100); snap("heal-iolo");
    h.key('y'); h.run(300); snap("heal-yes");
    leave_shop(h);
    snap("healer-left");
    // Reagents: buy one.
    open_shop(h, 4, town); snap("reagents");
    for (int i = 0; i < 3 && h.rt->ui()->mode() == UiMode::Shop && !h.rt->shop_view_for_test().active; ++i) { h.key('\r'); h.run(100); }
    snap("reagents-list");
    h.key('\r'); h.run(100); h.key('y'); h.run(300); snap("reagent-yes");
    leave_shop(h);
    snap("reagents-left");
    // Lists: Ready a helm, Cast with Iolo, Mix, Z-stats.
    h.key('r'); h.run(50); h.key('\r'); h.run(50); snap("ready");
    h.key('\r'); h.run(200); snap("ready-chosen");
    h.key('c'); h.run(50); h.down(); h.run(50); h.key('\r'); h.run(50); snap("cast-spells");
    h.mic(); h.mic(); h.run(50); snap("cast-mic");
    h.key('m'); h.run(50); for (int i = 0; i < 5; ++i) { h.down(); h.run(20); } snap("mix-down");
    h.key('\r'); h.run(50); h.key('\r'); h.run(50); snap("mix-mark");
    h.mic(); h.mic(); h.run(50); snap("mix-mic");
    h.key('z'); h.run(50); h.key('\r'); h.run(50); h.ball(RawInputKind::TrackballRight); h.run(50); snap("zstats");
    h.mic(); h.run(50); snap("zstats-closed");
    h.key('s', true); h.run(500); h.key('\r'); h.run(1000); snap("saved");
    t.gam = tdeck::host_memory_save_gam_for_test();
    t.transcript = h.transcript();
    t.state = fnv(&g, sizeof(g));
    t.fallbacks = h.board.handheld_fallbacks();
    tdeck::host_memory_save_forget_for_test();
    set_platform_profile(PlatformProfile{});
    return t;
}

#if !OPENU5_HANDHELD_INTERACTION_IS_FIXED
// W: the same runtime and Board built with the dynamic core (as Windows is):
// the Windows Original profile (the 1988 interaction AND presentation) and
// the Enhanced one (none) never get a Native frame, never a strip, never a
// handheld fallback -- Phase 3's forms are for the handheld interaction only.
bool g_original = true;
bool original_switch(void *) { return g_original; }
void test_windows_profiles() {
    std::printf("\nW  Windows Original and Enhanced compose no Native frame (dynamic core)\n");
    Run h(kParty);
    h.rt->game().gold = 9999;
    h.rt->game().equipment_quantities[0] = 1;
    PlatformProfile original{};
    original.dos_interface = &original_switch;
    original.dos_panels = &original_switch;
    original.dos_screen = &original_switch;
    set_platform_profile(original);
    h.board.set_dos_layout(true);
    h.render(true);
    h.run(100);
    bool none = !frame(h) && rows_black(200, 239);
    h.key('r');
    h.run(100);
    none = none && !frame(h) && rows_black(200, 239);
    h.key('\r');
    h.run(100);
    none = none && !frame(h) && rows_black(200, 239);
    h.mic();
    h.run(100);
    uint8_t town = 0;
    const bool shop = open_shop(h, 0, town);
    for (const char k : {'\r', 'b', 'a'}) {
        h.key(uint8_t(k));
        h.run(100);
        none = none && !frame(h) && rows_black(200, 239);
    }
    check(shop && none && h.board.handheld_fallbacks() == 0,
          "W1 Windows Original (the 1988 interaction): Ready, the roster picker, a console shop -- no Native frame, no strip, no fallback");
    leave_shop(h);
    set_platform_profile(PlatformProfile{});
    h.board.set_dos_layout(false);
    h.render(true);
    h.key('c');
    h.run(100);
    check(!frame(h) && h.rt->selection_view_for_test().active, "W2 Windows Enhanced / the handheld screen: no Native frame");
}
#endif

void test_identity() {
    std::printf("\nI  the same raw input through shops and lists under Enhanced and Native\n");
    const Trace enhanced = walk(PrototypePresentation::Enhanced);
    const Trace native = walk(PrototypePresentation::Native);
    size_t first = enhanced.steps.size();
    for (size_t i = 0; i < enhanced.steps.size() && i < native.steps.size(); ++i)
        if (enhanced.steps[i] != native.steps[i]) { first = i; break; }
    check(native.steps.size() == enhanced.steps.size() && first == enhanced.steps.size(),
          "I1 the interaction trace is identical at all " + n(long(enhanced.steps.size())) +
              " steps (modes, phases, cursors, prompts, legends, gold, items, spells, reagents, HP)" +
              (first < enhanced.steps.size() ? "\n  E: " + enhanced.steps[first] + "\n  N: " + native.steps[first] : std::string()));
    check(native.transcript == enhanced.transcript, "I2 the whole transcript is the same");
    check(native.state == enhanced.state, "I3 the whole GameState is byte-identical at the end");
    check(!enhanced.gam.empty() && native.gam == enhanced.gam, "I4 Alt+S saved the same GAM bytes (" + n(long(enhanced.gam.size())) + ")");
    bool bought = false, healed = false, sold = false;
    for (const auto &s : native.steps) {
        bought = bought || s.rfind("buy-yes:", 0) == 0;
        healed = healed || s.rfind("heal-yes:", 0) == 0;
        sold = sold || s.rfind("sell-yes:", 0) == 0;
    }
    const bool gold_moved = enhanced.steps.size() > 6 && has(enhanced.steps[5], "gold=") &&
                            enhanced.steps[5].substr(enhanced.steps[5].find("gold=")) != enhanced.steps[6].substr(enhanced.steps[6].find("gold="));
    check(bought && healed && sold && gold_moved && native.fallbacks == 0,
          "I5 the script bought, sold and healed (gold moved), and Native never fell back to the handheld screen");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: tdeck_native_view_phase3_runtime <openu5-alpha1-resources.bin> [--dump <dir>]\n");
        return 2;
    }
    g_dump = arg_after(argc, argv, "--dump");
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
#if OPENU5_HANDHELD_INTERACTION_IS_FIXED
    // The pack's data.json, as the prototype reads it at boot.
    {
        tdeck::AlphaResourcePack source;
        tdeck::AlphaResourceReport report{};
        if (source.open(argv[1], report) == ESP_OK) {
            g_data_json.resize(source.entry_length("data.json"));
            if (source.read_entry("data.json", g_data_json.data(), g_data_json.size()) != ESP_OK) g_data_json.clear();
        }
    }
    if (g_data_json.empty()) {
        std::printf("RED the pack has no data.json\n");
        return 1;
    }
    test_names();
    test_shops();
    test_lists();
    test_movement_and_prompts();
    test_redraws_and_switch();
    test_identity();
#else
    test_windows_profiles();
#endif
    std::printf("tdeck_native_view_phase3_runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
