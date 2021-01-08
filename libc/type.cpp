#include <type.h>

bool isalpha(int ch) { return islower(ch) || isupper(ch); }

bool isdigit(int ch) { return '0' <= ch && ch <= '9'; }

bool islower(int ch) { return 'a' <= ch && ch <= 'z'; }

bool isupper(int ch) { return 'A' <= ch && ch <= 'Z'; }

int toupper(int ch) {
  if (islower(ch)) return ch - ('a' - 'A');
  return ch;
}
