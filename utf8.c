#include "utf8.h"
#include <stdio.h>

// Array containing the 128 Unicode code points used in the extended CP437 range (128-255)
static const uint16_t cp437_extended_map[128] = {
    // Indices 128-143 (Graphic/Control Codes rendered as symbols)
    0x2302, 0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, // 128-135: ⌂ Ç ü é â ä à å
    0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, // 136-143: ç ê ë è ï î ì Ä

    // Indices 144-159 (Accented letters)
    0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, // 144-151: Å É æ Æ ô ö ò û
    0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, // 152-159: ù ÿ Ö Ü ¢ £ ¥ ₧

    // Indices 160-175 (Accented letters, currency, symbols)
    0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, // 160-167: ƒ á í ó ú ñ Ñ ª
    0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, // 168-175: º ¿ ⌐ ¬ ½ ¼ ¡ «

    // Indices 176-191 (Box Drawing and graphics)
    0x00BB, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, // 176-183: » ░ ▒ ▓ │ ┤ ╡ ╢
    0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, // 184-191: ╖ ╕ ╣ ║ ╗ ╝ ╜ ╛

    // Indices 192-207 (Box Drawing)
    0x2510, 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, // 192-199: ┐ └ ┴ ┬ ├ ─ ┼ ╞
    0x255F, 0x2559, 0x2558, 0x255A, 0x2564, 0x2565, 0x2550, 0x2566, // 200-207: ╟ ╚ ╔ ╩ ╦ ╠ ═ ╬

    // Indices 208-223 (Box Drawing)
    0x2560, 0x2550, 0x256C, 0x2567, 0x2568, 0x2569, 0x256A, 0x256B, // 208-215: ╧ ╨ ╤ ╥ ╙ ╘ ╒ ╓
    0x255A, 0x2554, 0x2569, 0x256B, 0x2518, 0x250C, 0x2588, 0x2584, // 216-223: ╫ ╪ ┘ ┌ █ ▄ ▌ ▐

    // Indices 224-239 (Greek and Math symbols)
    0x2580, 0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, // 224-231: ▀ α ß Γ π Σ σ µ
    0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, // 232-239: τ Φ Θ Ω δ ∞ φ ε

    // Indices 240-255 (Math/Misc symbols)
    0x2229, 0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, // 240-247: ∩ ≡ ± ≥ ≤ ⌠ ⌡ ÷
    0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0  // 248-255: ≈ ° ∙ · √ ⁿ ² ■
};

static const uchar utfbyte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const uchar utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const Rune utfmin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
static const Rune utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF,
                                         0x10FFFF};

int get_texture_atlas_index(unsigned int unicode_char) {
  for (int i = 0; i < 128; i++) {
    if (cp437_extended_map[i] == unicode_char) {
      return 127 + i;
    }
  }

  return 63; // Index for '?' in ASCII
}

size_t utf8decode(const char *c, Rune *u, size_t clen) {
  size_t i, j, len, type;
  Rune udecoded;

  *u = UTF_INVALID;
  if (!clen)
    return 0;
  udecoded = utf8decodebyte(c[0], &len);
  if (!BETWEEN(len, 1, UTF_SIZ))
    return 1;
  for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
    udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
    if (type != 0)
      return j;
  }
  if (j < len)
    return 0;
  *u = udecoded;
  utf8validate(u, len);

  return len;
}

Rune utf8decodebyte(char c, size_t *i) {
  for (*i = 0; *i < LEN(utfmask); ++(*i))
    if (((uchar)c & utfmask[*i]) == utfbyte[*i])
      return (uchar)c & ~utfmask[*i];

  return 0;
}

size_t utf8encode(Rune u, char *c) {
  size_t len, i;

  len = utf8validate(&u, 0);
  if (len > UTF_SIZ)
    return 0;

  for (i = len - 1; i != 0; --i) {
    c[i] = utf8encodebyte(u, 0);
    u >>= 6;
  }
  c[0] = utf8encodebyte(u, len);

  return len;
}

char utf8encodebyte(Rune u, size_t i) { return utfbyte[i] | (u & ~utfmask[i]); }

size_t utf8validate(Rune *u, size_t i) {
  if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
    *u = UTF_INVALID;
  for (i = 1; *u > utfmax[i]; ++i)
    ;

  return i;
}
