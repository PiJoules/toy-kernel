#ifndef PANIC_H_
#define PANIC_H_

[[noreturn]] void Panic(const char *msg, const char *file, int line);

#define PANIC(msg) Panic(msg, __FILE__, __LINE__)

#endif
