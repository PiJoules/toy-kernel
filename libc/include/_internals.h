#ifndef _INTERNALS_H_
#define _INTERNALS_H_

// C++ header guards.
#ifdef __cplusplus
#define __BEGIN_CDECLS extern "C" {
#define __END_CDECLS }
#else
#define __BEGIN_CDECLS
#define __END_CDECLS
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL (char *(0))
#endif
#endif

#endif
