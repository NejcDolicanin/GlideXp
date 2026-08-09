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

// Tell gamefix which resolution the screen will actually be, as a Glide
// resolution enum (GR_RESOLUTION_*).  Call BEFORE GameFix_Apply.
//
// The wide driver's "Glide Override Resolution" setting is the single source of
// truth for this: whatever is selected there is what the driver will force, so
// it is what the game's projection, HUD layout and mode list all have to agree
// with.  Hardcoding a resolution here would silently disagree with the driver
// the moment the setting changed.
//
// Values <= 1 mean "Disabled" and are rejected, matching the driver's own test
// (`glideResOverride > 1`, gsst.c:1558).  Enums outside the wide driver's list
// are rejected too.
//
// Returns non-zero if the resolution was recognised and adopted.  On failure
// the target stays 640x480, which leaves every fix inert -- the game then runs
// exactly as it would unpatched.
int GameFix_SetResolutionEnum(unsigned int glideEnum);

// TEMPORARY.  Called once per grBufferSwap so the MDK diagnostic has somewhere
// to flush from.  Goes when the diagnostic goes -- see gamefix.cpp.
void GameFix_Tick(void);

#ifdef __cplusplus
}
#endif

#endif /* GAMEFIX_H */
