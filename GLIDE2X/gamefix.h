//
// gamefix.h -- per-game runtime binary patches.
//
// Deliberately free of any GlideXP internals: this pair of files depends on
// nothing but <windows.h>, so it can be dropped into glide3x.dll unchanged for
// Glide3-only games where this wrapper is not in the call path.
//

#ifndef GAMEFIX_H
#define GAMEFIX_H

#ifdef __cplusplus
extern "C" {
#endif

// Identify the host process and apply whatever patches its profile lists.
//
// Idempotent and safe to call repeatedly: a patch whose "find" bytes are no
// longer present is simply skipped, so a second call after a successful first
// one does nothing.  That matters because the intended call site is
// DLL_PROCESS_ATTACH, which is early enough for the current target but is not
// guaranteed to be early enough for every future one -- a later retry from a
// Glide entry point costs nothing.
//
// Never fails loudly.  A game running unpatched at its stock resolution is a
// far better outcome than one that refuses to start.
void GameFix_Apply(void);

#ifdef __cplusplus
}
#endif

#endif /* GAMEFIX_H */
