#ifndef STACKTRACE_H_
#define STACKTRACE_H_

// FIXME: This is essentially an unwinder which should not be part of libc.

namespace stacktrace {

void PrintStackTrace();

}  // namespace stacktrace

#endif
