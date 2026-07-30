//
// g2new.cpp -- minimal operator new/delete, so the DLL does not need libstdc++.
//
// GLIDE2X uses C++ for namespaces and four allocations in G2STATE.CPP, nothing
// else.  Pulling in libstdc++ for those four is expensive in a way that matters
// here:
//
//   - libstdc++.a(eh_globals.o) drags in libgcc.a(gthr-win32.o), which imports
//     CreateSemaphoreW.  Windows 98 exports that name only as a stub that fails
//     with ERROR_CALL_NOT_IMPLEMENTED, so it is a landmine on the target box.
//   - It also drags in the DWARF unwinder (unwind-dw2, eh_personality) even
//     though the build is -fno-exceptions, and the msvcrt _write/_iob imports
//     that the MSVC-built reference DLL does not have.
//
// StateBuffer and G2State are plain structs with no constructors or
// destructors, so new/delete over malloc/free is exactly what the compiler
// would have emitted anyway -- no behaviour changes.  Defining these here lets
// the DLL link with gcc rather than g++.
//
// There is deliberately no throwing on failure: -fno-exceptions is in force,
// and the one caller (g2GetStateBuffer) already null-checks.
//

#include <stdlib.h>

void *operator new(size_t size)         { return malloc(size ? size : 1); }
void *operator new[](size_t size)       { return malloc(size ? size : 1); }

void  operator delete(void *p)          { if (p) free(p); }
void  operator delete[](void *p)        { if (p) free(p); }

// Sized deallocation (C++14).  GCC 6 defaults to -std=gnu++14, so it emits
// calls to these rather than the unsized forms.
void  operator delete(void *p, size_t)   { if (p) free(p); }
void  operator delete[](void *p, size_t) { if (p) free(p); }
