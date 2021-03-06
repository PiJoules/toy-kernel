#include <ctype.h>

bool isprint(char c) { return c >= 32 && c < 127; }

bool isspace(char c) {
  switch (c) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
      return true;
    default:
      return false;
  }
}
