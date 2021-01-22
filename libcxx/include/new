#ifndef NEW_H_
#define NEW_H_

// Declarations for placement new.
inline void *operator new(size_t, void *p) { return p; }
inline void *operator new[](size_t, void *p) { return p; }
inline void operator delete(void *, void *){};
inline void operator delete[](void *, void *){};

#endif
