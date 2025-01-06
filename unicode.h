#include <stddef.h>
#include <stdint.h>

typedef uint_least32_t Rune;

size_t utf8_to_ucs4(const char *c, Rune** u, size_t clen);
