#include "openu5/dos_text_tables.h"

#include <cstring>

namespace openu5 {

const uint16_t kDosTextLiteralAddresses[DosTextBlock::kLiterals] = {
    0x9778, // "--"              print_list_row's zero count
    0x977c, // 0x1c " + "        a scroll's decoration (RUNES.CH)
    0x9782, // 0x1d " + "        a potion's decoration (RUNES.CH)
    0x9788, // "Moonstone "
    0x9794, // "(None owned!)"   Z-stats' empty list
    0x8f64, // "Reagents:"       Mix's banner
    0x48b8, // "Items:"          (U)se's banner
    0x4fc0, // "    GUEST"       the inn's register
    0x4fca, // "  REGISTER:\n\n"
};

namespace {
constexpr int kMaxDepth = 16;
constexpr size_t kNoPosition = ~size_t(0);

// A single-pass reader over data.json's bytes: it skips what it does not want
// and writes the strings it keeps straight into the block's arena.
class Scanner {
  public:
    Scanner(const char *s, size_t n, DosTextBlock &block) : s_(s), n_(n), b_(block) {}

    bool document() {
        uint16_t scrolls[8]{}, phases[8]{}, special[5]{}, shards[3]{}, special2[6]{};
        size_t count = 0;
        bool potions = false, reagents = false, spells = false, have_special = false, have_shards = false,
             have_special2 = false, have_pools = false;
        space();
        if (!take('{')) return false;
        space();
        if (take('}')) return false;
        for (;;) {
            char key[32]{};
            space();
            if (!key_text(key, sizeof(key))) return false;
            space();
            if (!take(':')) return false;
            space();
            if (std::strcmp(key, "potions") == 0) {
                if (!string_array(b_.potion, 8, count)) return false;
                potions = count == 8;
            } else if (std::strcmp(key, "reagents") == 0) {
                if (!string_array(b_.reagent, 8, count)) return false;
                reagents = count == 8;
            } else if (std::strcmp(key, "spells") == 0) {
                if (!string_array(b_.spell, 48, count)) return false;
                spells = count == 48;
            } else if (std::strcmp(key, "specialItemNames") == 0) {
                if (!string_array(special, 5, count)) return false;
                have_special = count == 5;
            } else if (std::strcmp(key, "shards") == 0) {
                if (!string_array(shards, 3, count)) return false;
                have_shards = count == 3;
            } else if (std::strcmp(key, "specialItemNames2") == 0) {
                if (!string_array(special2, 6, count)) return false;
                have_special2 = count == 6;
            } else if (std::strcmp(key, "stringPools") == 0) {
                if (!pools(scrolls, phases)) return false;
                have_pools = true;
            } else if (!skip_value(0)) {
                return false;
            }
            space();
            if (take(',')) continue;
            if (take('}')) break;
            return false;
        }
        bool sigils = true;
        for (int i = 0; i < 8; ++i) sigils = sigils && scrolls[i] && phases[i];
        if (!potions || !reagents || !spells || !have_special || !have_shards || !have_special2 || !have_pools ||
            !sigils)
            return false;
        // The (U)se table, DS 0x1916 (re/notes/use-picker-panel.md section 4):
        // the eight scroll codes ("*VL"), eight potion sigils ("!"), the
        // carpet, keys and regalia, the eight moonstone phases ("(0"), the
        // shards and the six tools -- 38 entries, in the extended table's order.
        uint16_t potion_sigil = 0;
        if (!store("!", 1, potion_sigil)) return false;
        size_t at = 0;
        auto add = [&](const uint16_t *from, size_t n) {
            for (size_t i = 0; i < n; ++i) b_.use_item[at++] = from[i];
        };
        add(scrolls, 8);
        for (int i = 0; i < 8; ++i) b_.use_item[at++] = potion_sigil;
        add(special, 5);
        add(phases, 8);
        add(shards, 3);
        add(special2, 6);
        return at == 38;
    }

  private:
    bool more() const { return i_ < n_; }
    void space() {
        while (more() && (s_[i_] == ' ' || s_[i_] == '\n' || s_[i_] == '\r' || s_[i_] == '\t')) ++i_;
    }
    bool take(char c) {
        if (!more() || s_[i_] != c) return false;
        ++i_;
        return true;
    }
    static int hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    /**
     * The string at the cursor, one decoded byte at a time to `put` (false
     * stops with an error). Escapes and UTF-8 below 0x100 are that byte;
     * anything above is '?'.
     */
    template <class Put> bool string(Put put) {
        if (!take('"')) return false;
        while (more()) {
            const auto c = static_cast<unsigned char>(s_[i_++]);
            if (c == '"') return true;
            unsigned code = c;
            if (c == '\\') {
                if (!more()) return false;
                const char e = s_[i_++];
                switch (e) {
                case 'n': code = '\n'; break;
                case 'r': code = '\r'; break;
                case 't': code = '\t'; break;
                case 'b': code = '\b'; break;
                case 'f': code = '\f'; break;
                case 'u': {
                    if (n_ - i_ < 4) return false;
                    code = 0;
                    for (int k = 0; k < 4; ++k) {
                        const int h = hex(s_[i_++]);
                        if (h < 0) return false;
                        code = code << 4 | unsigned(h);
                    }
                    break;
                }
                default: code = static_cast<unsigned char>(e); break;
                }
            } else if (c >= 0x80) {
                // A UTF-8 sequence: its lead byte says how many continuation bytes follow.
                const int extra = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : 0;
                code = extra == 1 ? c & 0x1fU : 0x100;
                for (int k = 0; k < extra; ++k) {
                    if (!more() || (static_cast<unsigned char>(s_[i_]) & 0xc0) != 0x80) return false;
                    code = code << 6 | (static_cast<unsigned char>(s_[i_++]) & 0x3fU);
                }
                if (!extra) code = 0x100;
            }
            if (!put(char(code < 0x100 ? code : '?'))) return false;
        }
        return false;
    }
    bool key_text(char *out, size_t capacity) {
        size_t n = 0;
        return string([&](char c) {
            if (n + 1 < capacity) out[n++] = c;
            out[n] = 0;
            return true;
        });
    }
    /** Appends `n` bytes and a NUL to the arena; `at` = offset + 1. */
    bool store(const char *text, size_t n, uint16_t &at) {
        if (b_.used + n + 1 > DosTextBlock::kArenaBytes) return false;
        std::memcpy(b_.arena + b_.used, text, n);
        b_.arena[b_.used + n] = 0;
        at = uint16_t(b_.used + 1);
        b_.used = uint16_t(b_.used + n + 1);
        return true;
    }
    /** The string at the cursor into the arena. */
    bool stored_string(uint16_t &at) {
        const size_t start = b_.used;
        size_t n = 0;
        if (!string([&](char c) {
                if (start + n + 1 >= DosTextBlock::kArenaBytes) return false;
                b_.arena[start + n++] = c;
                return true;
            }))
            return false;
        b_.arena[start + n] = 0;
        at = uint16_t(start + 1);
        b_.used = uint16_t(start + n + 1);
        return true;
    }
    /** A string's decoded length, nothing kept. */
    bool string_length(size_t &n) {
        n = 0;
        return string([&](char) {
            ++n;
            return true;
        });
    }
    bool string_array(uint16_t *out, size_t capacity, size_t &count) {
        count = 0;
        if (!take('[')) return false;
        space();
        if (take(']')) return true;
        for (;;) {
            space();
            uint16_t at = 0;
            if (count < capacity) {
                if (!stored_string(at)) return false;
                out[count] = at;
            } else if (!skip_value(0)) {
                return false;
            }
            ++count;
            space();
            if (take(',')) continue;
            return take(']');
        }
    }
    bool number(long &out) {
        const size_t start = i_;
        bool negative = take('-');
        long value = 0;
        bool digits = false;
        while (more() && s_[i_] >= '0' && s_[i_] <= '9') {
            if (value < 0x1000000) value = value * 10 + (s_[i_] - '0');
            ++i_;
            digits = true;
        }
        while (more() && (s_[i_] == '.' || s_[i_] == 'e' || s_[i_] == 'E' || s_[i_] == '+' || s_[i_] == '-' ||
                          (s_[i_] >= '0' && s_[i_] <= '9')))
            ++i_;
        out = negative ? -value : value;
        return digits && i_ > start;
    }
    bool literal(const char *word) {
        const size_t n = std::strlen(word);
        if (n_ - i_ < n || std::strncmp(s_ + i_, word, n) != 0) return false;
        i_ += n;
        return true;
    }
    bool skip_value(int depth) {
        if (depth > kMaxDepth || !more()) return false;
        space();
        if (!more()) return false;
        const char c = s_[i_];
        if (c == '"') {
            size_t ignored = 0;
            return string_length(ignored);
        }
        if (c == '{' || c == '[') {
            const char close = c == '{' ? '}' : ']';
            ++i_;
            space();
            if (take(close)) return true;
            for (;;) {
                space();
                if (c == '{') {
                    size_t ignored = 0;
                    if (!string_length(ignored)) return false;
                    space();
                    if (!take(':')) return false;
                }
                if (!skip_value(depth + 1)) return false;
                space();
                if (take(',')) continue;
                return take(close);
            }
        }
        if (literal("true") || literal("false") || literal("null")) return true;
        long ignored = 0;
        return number(ignored);
    }
    /**
     * stringPools: [{name, offset, strings}, ...]. Every pool is a run of
     * NUL-terminated DATA.OVL strings from its file offset (DS + 0x10): the
     * wanted literals are kept by their DS address, and two pools whole --
     * the scroll codes and the moonstone phases of the (U)se table.
     */
    bool pools(uint16_t (&scrolls)[8], uint16_t (&phases)[8]) {
        if (!take('[')) return false;
        space();
        if (take(']')) return true;
        for (;;) {
            space();
            if (!take('{')) return false;
            char name[32]{};
            long offset = -1;
            size_t strings = kNoPosition;
            space();
            if (!take('}')) {
                for (;;) {
                    char key[16]{};
                    space();
                    if (!key_text(key, sizeof(key))) return false;
                    space();
                    if (!take(':')) return false;
                    space();
                    if (std::strcmp(key, "name") == 0) {
                        if (!key_text(name, sizeof(name))) return false;
                    } else if (std::strcmp(key, "offset") == 0) {
                        if (!number(offset)) return false;
                    } else {
                        if (std::strcmp(key, "strings") == 0) strings = i_;
                        if (!skip_value(0)) return false;
                    }
                    space();
                    if (take(',')) continue;
                    if (take('}')) break;
                    return false;
                }
            }
            if (strings != kNoPosition && offset >= 0x10) {
                const size_t resume = i_;
                i_ = strings;
                uint16_t *whole = std::strcmp(name, "scrollSpellCodes") == 0    ? scrolls
                                  : std::strcmp(name, "parenDigitStrings") == 0 ? phases
                                                                                : nullptr;
                if (!pool_strings(offset - 0x10, whole)) return false;
                i_ = resume;
            }
            space();
            if (take(',')) continue;
            return take(']');
        }
    }
    bool pool_strings(long address, uint16_t *whole) {
        if (!take('[')) return false;
        space();
        if (take(']')) return true;
        for (size_t index = 0;; ++index) {
            space();
            const size_t start = i_;
            size_t n = 0;
            if (!string_length(n)) return false;
            int wanted = -1;
            for (size_t k = 0; k < DosTextBlock::kLiterals; ++k)
                if (long(kDosTextLiteralAddresses[k]) == address) wanted = int(k);
            if (wanted >= 0 || (whole && index < 8)) {
                const size_t end = i_;
                i_ = start;
                uint16_t at = 0;
                if (!stored_string(at)) return false;
                i_ = end;
                if (wanted >= 0) b_.literal[wanted] = at;
                if (whole && index < 8) whole[index] = at;
            }
            address += long(n) + 1;
            space();
            if (take(',')) continue;
            return take(']');
        }
    }

    const char *s_;
    size_t n_;
    size_t i_ = 0;
    DosTextBlock &b_;
};
// In place: a `DosTextBlock{}` temporary would put the whole block on the stack.
void clear(DosTextBlock &block) {
    std::memset(block.arena, 0, sizeof(block.arena));
    block.used = 0;
    std::memset(block.potion, 0, sizeof(block.potion));
    std::memset(block.reagent, 0, sizeof(block.reagent));
    std::memset(block.spell, 0, sizeof(block.spell));
    std::memset(block.use_item, 0, sizeof(block.use_item));
    std::memset(block.literal, 0, sizeof(block.literal));
    block.loaded = false;
}
} // namespace

bool dos_text_extract(const char *json, size_t length, DosTextBlock &out) {
    clear(out);
    if (!json) return false;
    Scanner scanner(json, length, out);
    if (!scanner.document()) {
        clear(out);
        return false;
    }
    out.loaded = true;
    return true;
}

const char *dos_text_lookup(const DosTextBlock &block, DosText table, int index) {
    if (!block.loaded) return nullptr;
    auto at = [&](const uint16_t *offsets, size_t count) -> const char * {
        return index >= 0 && size_t(index) < count && offsets[index] ? block.arena + offsets[index] - 1 : nullptr;
    };
    switch (table) {
    case DosText::UseItem: return at(block.use_item, 38);
    case DosText::PotionColour: return at(block.potion, 8);
    case DosText::Reagent: return at(block.reagent, 8);
    case DosText::Spell: return at(block.spell, 48);
    case DosText::Literal:
        for (size_t k = 0; k < DosTextBlock::kLiterals; ++k)
            if (int(kDosTextLiteralAddresses[k]) == index && block.literal[k])
                return block.arena + block.literal[k] - 1;
        return nullptr;
    case DosText::EquipmentShort:
    case DosText::EquipmentGlyph: return nullptr;
    }
    return nullptr;
}

} // namespace openu5
