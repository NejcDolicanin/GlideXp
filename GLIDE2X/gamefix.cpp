//
// gamefix.cpp -- per-game runtime binary patches.
//
// See gamefix.h.  No GlideXP internals; <windows.h> only.
//
//
// GTA2 @ 2560x1080
// ----------------
// GTA2 does not ask the driver what resolution it is running at -- Glide is a
// screen-space API, so the game does its own projection against whatever size
// it believes it has.  Forcing a larger framebuffer behind its back just draws
// a 640x480 image into the corner of a big screen.  The game has to be told.
//
// Three things are needed, and they are independent:
//
//   1. the mode has to EXIST      -> DMAGlide.dll mode-list patch (below)
//   2. the game has to ASK for it -> full_width / full_height, REG_DWORD, under
//                                    HKLM\SOFTWARE\DMA Design Ltd\GTA2\Screen
//   3. the projection has to be   -> SetupViewport scale patch + camera hook
//      right for a non-4:3 screen
//
// Patching DMAGlide.dll rather than gta2.exe is not merely tidier.  The retail
// CD build is SafeDisc: gta2.exe is a loader stub and the real image is
// gta2.icd, whose .text is encrypted on disk (entropy ~7.0-7.95 bits/byte).
// DMAGlide.dll is plain, unencrypted, and 39 KB.  We never touch the
// protection.  The game image can still be patched -- by the time grGlideInit
// runs, SafeDisc has decrypted it in memory -- but its byte patterns had to be
// recovered from a runtime dump rather than from the file.
//
// Timing: GameFix_Apply is called from both DLL_PROCESS_ATTACH and grGlideInit.
// DllMain alone is not enough -- GTA2 pulls DMAGlide.dll in with LoadLibrary,
// and on Win9x GetModuleHandleA returns NULL while that load is in progress.
// grGlideInit is reliable: DMAGlide's Vid_Init_SYS calls it as its very first
// Glide call, before it builds the mode list.  Everything here is idempotent.
//

#include <windows.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "gamefix.h"


//
// A single patch.  `find` and `replace` are the same length by construction;
// nothing here resizes or relocates anything.
//
struct BytePatch {
    const char          *name;
    const unsigned char *find;
    const unsigned char *replace;
    unsigned int         length;
};

//
// A game profile.  `exeName` is matched against the host process's own module
// name; `moduleName` is the DLL actually carrying the code to patch, which for
// GTA2 is not the exe at all.  NULL means the host's own main image.
//
struct GameProfile {
    const char        *exeName;
    const char        *moduleName;
    const BytePatch   *patches;
    unsigned int       patchCount;
};

// The configuration file, read from beside the host exe.
#define GAMEFIX_INI "wideDriver.ini"

//
// Target resolution -- taken from the driver, not hardcoded.
//
// The wide driver's own "Glide Override Resolution" setting is the single
// source of truth: whatever the user picked there is what the screen will
// actually be, so it is what every calculation here has to agree with.  It is
// read through the driver's own lookup (see GameFix_SetResolutionEnum) rather
// than by reimplementing the registry path.
//
// 640x480 is the default because it makes every fix below inert:
//
//   vw    = W*vh/H  = 640    -> msg_sites rewrite 640 to 640, a no-op
//   delta = W - 640*scale    = 0, so the HUD moves nowhere
//   the popup's scale        = 1.0, so its position is unchanged
//
// So with the override disabled the game behaves exactly as it always did,
// without needing a switch anywhere.  The one exception is the DMAGlide mode
// list, which is skipped outright -- rewriting 800x600 to 640x480 would lose a
// mode rather than add one.
//
static unsigned int g_targetW   = 640;
static unsigned int g_targetH   = 480;
static unsigned int g_targetRes = 0;      /* Glide enum; 0 = no override */

//
// Glide resolution enum -> pixels.
//
// Mirrors _resTable in glide3x/h5/glide3/src/gsst.c, which the driver indexes
// by the enum directly (`_resTable[resolution].xres`), so the enum IS the
// index.  Only the modes the wide driver offers in its "Glide Override
// Resolution" list are listed -- the stock 0x00-0x17 range is deliberately
// absent, since selecting one of those is not a widescreen case and needs no
// patching.  The values match the driver's Tweak Map one for one.
//
struct GlideRes {
    unsigned char res;
    unsigned short w, h;
};

static const GlideRes g_glideRes[] = {
    { 0x0c, 1024,  768 },
    { 0x0d, 1280, 1024 },
    { 0x0e, 1600, 1200 },
    { 0x18, 1280,  720 },
    { 0x19, 1280,  800 },
    { 0x1a, 1360,  768 },
    { 0x1b, 1440,  900 },
    { 0x1c, 1600,  900 },
    { 0x1d, 1680,  720 },
    { 0x1e, 1680, 1050 },
    { 0x1f, 1792,  768 },
    { 0x20, 1920,  800 },
    { 0x21, 1920, 1080 },
    { 0x22, 1920, 1200 },
    { 0x23, 1960,  840 },
    { 0x24, 2096,  900 },
    { 0x25, 2304,  960 },
    { 0x26, 2560, 1080 },
    // Appended 2026-08, and appended in the driver's enum too -- see the note
    // in sst1vid.h.  600-line widescreen modes, for games whose UI is fixed to
    // a stock height; none of this wrapper's games uses one, but the dropdown
    // offers them, so a miss here would be a silent no-patch.
    { 0x27,  768,  480 },
    { 0x28,  960,  600 },
    { 0x29, 1064,  600 },
    { 0x2a, 1144,  480 },
    { 0x2b, 1400,  600 },
};


// ==========================================================================
// Shared machinery
// ==========================================================================

/* Little-endian store, so the byte tables above stay readable as x86. */
static void PutU32(unsigned char *at, unsigned int v)
{
    at[0] = (unsigned char)(v      );
    at[1] = (unsigned char)(v >>  8);
    at[2] = (unsigned char)(v >> 16);
    at[3] = (unsigned char)(v >> 24);
}

static unsigned int ReadU32(const unsigned char *at)
{
    return (unsigned int)at[0]
         | ((unsigned int)at[1] <<  8)
         | ((unsigned int)at[2] << 16)
         | ((unsigned int)at[3] << 24);
}

/* The raw bits of a float, so it can be written as an x86 immediate. */
static unsigned int FloatBits(float f)
{
    union { float f; unsigned int u; } c;
    c.f = f;
    return c.u;
}

/* Emit `movl $imm,ds:addr` -- the ten-byte form the viewport patch uses. */
static void PutMovAbsImm(unsigned char *at, unsigned int addr, unsigned int imm)
{
    at[0] = 0xc7;
    at[1] = 0x05;
    PutU32(at + 2, addr);
    PutU32(at + 6, imm);
}


// ---------------------------------------------------------------------------
// machinery
// ---------------------------------------------------------------------------

//
// Case-insensitive tail comparison: does `path` end in `name`?  Used so a
// profile can say "gta2.exe" and still match whatever absolute path the process
// was launched from.  Deliberately ASCII-only and locale-independent; _stricmp
// would drag in locale handling for no gain here.
//
static BOOL PathEndsWith(const char *path, const char *name)
{
    size_t lp, ln;
    const char *tail;

    if (!path || !name) return FALSE;

    lp = strlen(path);
    ln = strlen(name);
    if (ln > lp) return FALSE;

    tail = path + (lp - ln);
    while (*tail) {
        char a = *tail++;
        char b = *name++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return FALSE;
    }
    return TRUE;
}

//
// Build a path to a file sitting next to the host exe.  Not the current
// directory -- the game may have chdir'd by the time we run.
//
static BOOL PathBesideExe(const char *leaf, char *out)
{
    char *slash, *p;

    out[0] = '\0';
    if (GetModuleFileNameA(NULL, out, MAX_PATH) == 0) return FALSE;

    slash = out;
    for (p = out; *p; p++)
        if (*p == '\\' || *p == '/') slash = p + 1;

    if ((size_t)(slash - out) + lstrlenA(leaf) + 1 >= MAX_PATH) return FALSE;
    lstrcpyA(slash, leaf);
    return TRUE;
}

//
// Locate a module's executable code range from its PE headers.
//
// Scanning only the code section keeps the search small and, more importantly,
// keeps it off the import/data sections where a byte sequence could coincide
// without being an instruction.  Returns nothing rather than guessing.
//
static BOOL GetCodeRange(HMODULE mod, unsigned char **base, unsigned int *size)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)mod;
    const IMAGE_NT_HEADERS *nt;

    if (!mod) return FALSE;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;

    nt = (const IMAGE_NT_HEADERS *)((const unsigned char *)mod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    if (!nt->OptionalHeader.BaseOfCode || !nt->OptionalHeader.SizeOfCode)
        return FALSE;

    *base = (unsigned char *)mod + nt->OptionalHeader.BaseOfCode;
    *size = (unsigned int)nt->OptionalHeader.SizeOfCode;
    return TRUE;
}

//
// Locate a named section, clamped to its INITIALISED part.
//
// Needed because some patch targets are data, not code -- Driver's video mode
// list is a table in .data -- and GetCodeRange deliberately cannot see them.
//
// The clamp is the point.  A section's VirtualSize covers its BSS tail, and
// that tail can be enormous: Driver's .data is 0x33000 bytes of raw data
// followed by 14 MB of zeroes.  Scanning the virtual extent would fault every
// one of those pages in for nothing.  Only the raw part can contain a pattern
// that was in the file.
//
static BOOL GetSectionRange(HMODULE mod, const char *name,
                            unsigned char **base, unsigned int *size)
{
    const IMAGE_DOS_HEADER  *dos = (const IMAGE_DOS_HEADER *)mod;
    const IMAGE_NT_HEADERS  *nt;
    const IMAGE_SECTION_HEADER *sec;
    unsigned int i, n;

    if (!mod) return FALSE;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;

    nt = (const IMAGE_NT_HEADERS *)((const unsigned char *)mod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    sec = IMAGE_FIRST_SECTION(nt);
    n   = nt->FileHeader.NumberOfSections;

    for (i = 0; i < n; i++) {
        unsigned int len;
        char         nm[9];

        memcpy(nm, sec[i].Name, 8);
        nm[8] = '\0';
        if (lstrcmpiA(nm, name) != 0) continue;

        len = sec[i].SizeOfRawData;
        if (sec[i].Misc.VirtualSize && sec[i].Misc.VirtualSize < len)
            len = sec[i].Misc.VirtualSize;
        if (!len) return FALSE;

        *base = (unsigned char *)mod + sec[i].VirtualAddress;
        *size = len;
        return TRUE;
    }
    return FALSE;
}

//
// Find `pattern` in [base, base+size).  Requires exactly one match.
//
// Insisting on uniqueness is the point.  A pattern that has become ambiguous in
// some other build of the target is a pattern we no longer understand, and
// patching the first of several candidates would be a guess.  Not patching is
// the safe answer.
//
static unsigned char *FindUnique(unsigned char *base, unsigned int size,
                                 const unsigned char *pattern, unsigned int len)
{
    unsigned char *hit = NULL;
    unsigned int i;

    if (len == 0 || len > size) return NULL;

    for (i = 0; i <= size - len; i++) {
        if (base[i] == pattern[0] && memcmp(base + i, pattern, len) == 0) {
            if (hit) return NULL;          // ambiguous -- refuse
            hit = base + i;
        }
    }
    return hit;
}

//
// Write over read-execute image pages.
//
// Win9x honours VirtualProtect on mapped image sections, and the pages are
// copy-on-write, so this affects only our process.  The old protection is put
// back rather than left writable: leaving a game's code section RWX for the
// rest of the run would be a gratuitous change to its memory hygiene.
//
static BOOL WriteCode(unsigned char *at, const unsigned char *bytes,
                      unsigned int len)
{
    DWORD oldProtect = 0;

    if (!VirtualProtect(at, len, PAGE_EXECUTE_READWRITE, &oldProtect))
        return FALSE;

    memcpy(at, bytes, len);

    VirtualProtect(at, len, oldProtect, &oldProtect);

    // Harmless on the single-core boxes this targets, but correct on anything
    // that caches decoded instructions.
    FlushInstructionCache(GetCurrentProcess(), at, len);
    return TRUE;
}

static void ApplyProfile(const GameProfile *profile)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0;
    unsigned int   i;

    // NULL moduleName means the host process's own main image -- for GTA2 that
    // is the SafeDisc-decrypted GTA2.ICD, only readable this late.
    //
    // Otherwise: not LoadLibrary.  If the module is not already mapped we are
    // simply too early (or this game does not use it), and forcing it in would
    // change the game's own load order.
    mod = GetModuleHandleA(profile->moduleName);
    if (!mod) return;

    if (!GetCodeRange(mod, &code, &codeSize)) return;

    for (i = 0; i < profile->patchCount; i++) {
        const BytePatch *p = &profile->patches[i];
        unsigned char   *at;

        at = FindUnique(code, codeSize, p->find, p->length);
        if (!at) continue;      // already patched, absent, or ambiguous

        WriteCode(at, p->replace, p->length);
    }
}



// ==========================================================================
// GTA2 @ 2560x1080
// ==========================================================================

/* Shorthand used throughout this section only. */
#define GTA2_TARGET_W ((int)g_targetW)
#define GTA2_TARGET_H ((int)g_targetH)

// ---------------------------------------------------------------------------
// 1. The mode list, in DMAGlide.dll (GTA2's Glide video device)
// ---------------------------------------------------------------------------
//
// GTA2 learns which modes exist from DMAGlide.dll via Vid_FindFirstMode /
// Vid_FindNextMode / Vid_CheckMode.  Vid_Init_SYS builds a linked list of
// 0x40-byte mode structs and hardcodes exactly two entries, 640x480 and
// 800x600.  That is the real reason the registry keys appear to do nothing:
// Vid_CheckMode walks that two-entry list, matches nothing else, and the game
// silently falls back.
//
// We rewrite the 800x600 entry, leaving 640x480 as a fallback.  Two sites have
// to agree or the mode is advertised but unusable:
//
//   A) the width/height pushed in Vid_Init_SYS -- what the game is told exists
//   B) the (width,height) -> Glide enum arm writing mode+0x3c -- what DMAGlide
//      later passes to grSstWinOpen.  Its default arm writes 0xFF,
//      "unsupported", so leaving B alone yields a mode the game offers and then
//      cannot open.
//
// 2560 = 0xa00, 1080 = 0x438, GR_RESOLUTION_2560x1080 = 0x26 (sst1vid.h:148).
//
// Matched by pattern, not address.  Both occur exactly once in the shipped
// DMAGlide.dll.  Shorter forms are NOT safe: the six-byte `cmp $0x280,%ecx`
// matches twice, being both the 640x400 and 640x480 arms of the same chain.
//

//
// A) Vid_Init_SYS @0x10013fa.  AddMode(device, width, height) is stdcall, so
//    arguments are pushed right-to-left and the height goes first.  The
//    trailing `56 e8` anchors the match to a call site; the call's relative
//    displacement is deliberately excluded.
//
static const unsigned char gta2_mode_find[] = {
    0x68, 0x58, 0x02, 0x00, 0x00,   // push $0x258   (600)
    0x68, 0x20, 0x03, 0x00, 0x00,   // push $0x320   (800)
    0x56,                           // push %esi
    0xe8                            // call
};
//
// Built at runtime from the driver's resolution -- the immediates below are
// only placeholders.  Offsets: height at [1], width at [6].
//
static unsigned char gta2_mode_replace[] = {
    0x68, 0x38, 0x04, 0x00, 0x00,   // push $height
    0x68, 0x00, 0x0a, 0x00, 0x00,   // push $width
    0x56,
    0xe8
};
#define MODE_H_AT 1
#define MODE_W_AT 6

//
// B) the (width,height) -> Glide enum arm @0x10010ca.  The two jne
//    displacements are unchanged: the replacement is the same length, so the
//    branch targets have not moved.
//
static const unsigned char gta2_enum_find[] = {
    0x81, 0xf9, 0x20, 0x03, 0x00, 0x00,        // cmp  $0x320,%ecx    (800)
    0x75, 0x13,
    0x81, 0xfe, 0x58, 0x02, 0x00, 0x00,        // cmp  $0x258,%esi    (600)
    0x75, 0x0b,
    0xc7, 0x40, 0x3c, 0x08, 0x00, 0x00, 0x00   // movl $0x8,0x3c(%eax)
};
//
// Also built at runtime.  The two jne displacements stay as they are: the
// replacement is the same length, so no branch target has moved.
// Offsets: width at [2], height at [10], Glide enum at [19].
//
static unsigned char gta2_enum_replace[] = {
    0x81, 0xf9, 0x00, 0x0a, 0x00, 0x00,        // cmp  $width,%ecx
    0x75, 0x13,
    0x81, 0xfe, 0x38, 0x04, 0x00, 0x00,        // cmp  $height,%esi
    0x75, 0x0b,
    0xc7, 0x40, 0x3c, 0x26, 0x00, 0x00, 0x00   // movl $enum,0x3c(%eax)
};
#define ENUM_W_AT    2
#define ENUM_H_AT   10
#define ENUM_RES_AT 19


static const BytePatch gta2_dmaglide_patches[] = {
    { "advertised mode 800x600 -> 2560x1080",
      gta2_mode_find, gta2_mode_replace, sizeof(gta2_mode_find) },
    { "enum arm 800x600 -> 2560x1080 (0x26)",
      gta2_enum_find, gta2_enum_replace, sizeof(gta2_enum_find) },
};


// ---------------------------------------------------------------------------
// 2. Aspect ratio, in the game image itself
// ---------------------------------------------------------------------------
//
// SetupViewport(width, height) @0x434e54 (thiscall, ecx = viewport object):
//
//     obj->0x68 = width          obj->0x6c = height
//     obj->0x70 = width/2        obj->0x74 = height/2      <- centres, correct
//     obj->0xa8 = (width << 14) / 640    <- ONE scale, 16.14, from WIDTH ONLY
//
// With scale = width/640, x=320 lands on width/2 at any resolution, so X is
// correct by construction.  y=240 lands on 240*width/640, which equals
// height/2 only at 4:3.  GTA2 shipped 640x480 and 800x600, both 4:3, so this
// was unreachable until now; at 2560x1080 text sat exactly two-thirds down.
//
// Deriving the scale from the HEIGHT instead is uniform -- one scale, both
// axes -- so nothing is stretched and the wider screen simply shows more world.
// It is also a *no-op at 4:3*: height/480 gives 1.0 at 640x480 and 1.25 at
// 800x600, exactly what width/640 already produced.  Only non-4:3 modes change,
// which makes it safe to leave in permanently.
//
// obj->0xa8 drives HUD/UI size only -- confirmed on hardware, where changing
// the divisor resized the HUD and left the camera untouched.  The world and
// sprites are scaled by the camera hook further down.
//
// Registers at the patch point: ecx = this (must not be clobbered), esi =
// width, eax = width<<14, edx = free.  ebx is callee-saved, hence the push/pop.
// The replacement is byte-for-byte the same length (30), so nothing moves.
//
static const unsigned char gta2_scale_find[] = {
    0x8b, 0xd0,                                 // mov  %eax,%edx
    0xb8, 0x67, 0x66, 0x66, 0x66,               // mov  $0x66666667,%eax
    0xf7, 0xea,                                 // imul %edx
    0xc1, 0xfa, 0x08,                           // sar  $0x8,%edx     -> /640
    0x8b, 0xc2,                                 // mov  %edx,%eax
    0xc1, 0xe8, 0x1f,                           // shr  $0x1f,%eax
    0xd1, 0xee,                                 // shr  %esi          -> width/2
    0x03, 0xd0,                                 // add  %eax,%edx
    0x89, 0x71, 0x70,                           // mov  %esi,0x70(%ecx)
    0x89, 0x91, 0xa8, 0x00, 0x00, 0x00          // mov  %edx,0xa8(%ecx)
};
//
// Not const: the 480 immediate is the HUD-size knob, rewritten at patch time
// from the ini.  See GTA2_VHEIGHT_OFFSET.
//
static unsigned char gta2_scale_replace[] = {
    0x53,                                       // push %ebx
    0x8b, 0x41, 0x6c,                           // mov  0x6c(%ecx),%eax   height
    0xc1, 0xe0, 0x0e,                           // shl  $0xe,%eax         <<14
    0x99,                                       // cltd
    0xbb, 0xe0, 0x01, 0x00, 0x00,               // mov  $480,%ebx   <-- offset 9
    0xf7, 0xfb,                                 // idiv %ebx              /480
    0x5b,                                       // pop  %ebx
    0x8b, 0xd0,                                 // mov  %eax,%edx
    0xd1, 0xee,                                 // shr  %esi              width/2
    0x89, 0x71, 0x70,                           // mov  %esi,0x70(%ecx)
    0x89, 0x91, 0xa8, 0x00, 0x00, 0x00,         // mov  %edx,0xa8(%ecx)
    0x90                                        // nop  (pad to 30)
};

#define GTA2_VHEIGHT_OFFSET  9      /* the 32-bit immediate above */
#define GTA2_VHEIGHT_DEFAULT 480

static const BytePatch gta2_exe_patches[] = {
    { "viewport scale: width/640 -> height/480 (uniform)",
      gta2_scale_find, gta2_scale_replace, sizeof(gta2_scale_find) },
};


// ---------------------------------------------------------------------------
// 3. Camera and sprite scaling -- an inline hook, not a byte patch
// ---------------------------------------------------------------------------
//
// GTA2 applies its camera zoom in NINE places (`imull 0x60(<reg>)`), and 16.14
// fixed-point scaling appears in ~2600 more.  Patching those sites individually
// does not converge: five were patched during development and the player,
// pedestrians and cars still never scaled.
//
// The fix is upstream, and comes from ThirteenAG's WidescreenFixesPack
// (source/GTA2.WidescreenFix/dllmain.cpp).  He adjusts the camera's own zoom
// field once, so every consumer -- map geometry and sprites alike -- scales
// consistently:
//
//     pattern "B9 2F 00 00 00 F3 A5"                 // mov $0x2f,%ecx ; rep movsl
//     regs.ecx = 0x2F;
//     *(int32_t*)(regs.ebp + 0x138) = hud_scale * 16384;
//     *(int32_t*)(regs.ebp + 0x2B0) = hud_scale * 16384;
//     *(int32_t*)(regs.esi + 0x8)  += (camera_scale - 1.0f) * 16384;
//
//     hud_scale    = screen_height / 480
//     camera_scale = (screen_width / 640) / ((4/3) / (screen_width/screen_height))
//
// His 16384 is our 16.14 format.  His two "HUD fields" are the very field
// SetupViewport writes: the viewport objects sit at ebp+0x90 / +0x14c / +0x208,
// and +0xa8 within them gives exactly 0x138 and 0x2B0.  So the hook rewrites
// obj->0xa8 for two of the three viewports on every camera update -- which is
// why virtualHeight is threaded through here rather than hardcoded to 480.
// Hardcoding it would silently override the ini for those two viewports and
// leave the third at a different scale.
//
// The pattern exists in 10.3, twice, as in his build.  But the two sites use
// different base registers and only the ebp one matches his code:
//
//     0x564d99  lea 0x208(%ebp),%esi ; lea 0x14c(%ebp),%edi   <- this one
//     0x564f75  lea 0x14c(%ebx),%edi ; mov 0x6c(%ebx),%eax
//
// So the signature includes the ebp-form `lea` rather than copying his
// `.count(2).get(1)` index, which would have picked the ebx site and written
// through the wrong register.
//
// `mov $0x2f,%ecx` is 5 bytes and `call rel32` is 5 bytes, so the hook goes in
// place with no code cave.  The stub restores ecx and brackets the `add` with
// pushfd/popfd -- `rep movsl` only cares about DF, which `add` does not touch,
// but the insurance is two bytes.
//
// The `+=` is deliberate, matching his fix: camera[0x8] is recomputed each
// frame (the surrounding code calls a method on the camera immediately before
// the copy), so this does not accumulate, and it preserves the game's own
// speed-dependent zoom rather than replacing it with a constant.
//
static const unsigned char gta2_camhook_sig[] = {
    0x8d, 0xbd, 0x4c, 0x01, 0x00, 0x00,   // lea 0x14c(%ebp),%edi   (ebp form)
    0xb9, 0x2f, 0x00, 0x00, 0x00,         // mov $0x2f,%ecx         <- replaced
    0xf3, 0xa5                            // rep movsl
};
#define CAMHOOK_MOV_AT 6                  /* offset of the mov within the sig */

static unsigned char camhook_stub[] = {
    0x9c,                                             // pushfd
    0xc7, 0x85, 0x38, 0x01, 0x00, 0x00, 0,0,0,0,      // movl $hud,0x138(%ebp)
    0xc7, 0x85, 0xb0, 0x02, 0x00, 0x00, 0,0,0,0,      // movl $hud,0x2b0(%ebp)
    0x81, 0x46, 0x08,                0,0,0,0,         // addl $cam,0x8(%esi)
    0x9d,                                             // popfd
    0xb9, 0x2f, 0x00, 0x00, 0x00,                     // mov  $0x2f,%ecx
    0xc3                                              // ret
};
#define STUB_HUD1_AT  7
#define STUB_HUD2_AT 17
#define STUB_CAM_AT  24


// ---------------------------------------------------------------------------
// Profiles
// ---------------------------------------------------------------------------
//
// Both exe names are listed on purpose.  Under SafeDisc the process is launched
// as gta2.exe (the loader stub) and the real image is gta2.icd; which of the two
// GetModuleFileName(NULL) reports depends on how the stub hands over.  A runtime
// dump reported exe=E:\GAMES\GTA2\GTA2.ICD, imagebase=0x00400000 -- so the .icd
// form is the one that actually matches, but both cost nothing.  A profile
// matching twice is harmless: the second pass finds its `find` bytes gone.
//
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const GameProfile g_profiles[] = {
    // Mode list, in the Glide video device DLL.
    { "gta2.exe",         "DMAGlide.dll", gta2_dmaglide_patches, COUNT(gta2_dmaglide_patches) },
    { "gta2.icd",         "DMAGlide.dll", gta2_dmaglide_patches, COUNT(gta2_dmaglide_patches) },

    // "gta2 manager.exe" is the configuration front-end, a separate process that
    // loads the same video device DLL to enumerate modes and then writes
    // full_width / full_height.  Patching it is what makes the mode appear in
    // its resolution list.  It gets the DMAGlide patches only; the aspect fix is
    // game code it never runs.
    { "gta2 manager.exe", "DMAGlide.dll", gta2_dmaglide_patches, COUNT(gta2_dmaglide_patches) },

    // Aspect ratio, in the game image itself.  NULL = the host's own module.
    { "gta2.exe",         NULL,           gta2_exe_patches,      COUNT(gta2_exe_patches) },
    { "gta2.icd",         NULL,           gta2_exe_patches,      COUNT(gta2_exe_patches) },
};

static const unsigned int g_profileCount = COUNT(g_profiles);


//
// virtual_height: the HUD/UI size knob.
//
// The patched SetupViewport computes scale = (height << 14) / virtual_height,
// and obj->0xa8 drives HUD and UI element size.  480 reproduces the original
// proportions; larger values shrink the HUD.
//
// Read from wideDriver.ini beside the exe.  GetPrivateProfileIntA does the
// parsing and is present on Win98; a missing file or key yields the default,
// so the absence of an ini changes nothing.
//
static unsigned int ReadVirtualHeight(const char *ini)
{
    UINT v = GetPrivateProfileIntA("GTA2", "virtual_height",
                                   GTA2_VHEIGHT_DEFAULT, ini);

    // Clamp.  A zero would be a divide-by-zero fault inside the game's own
    // code, which would be a miserable thing to debug from an ini typo.
    if (v < 120)  v = 120;
    if (v > 8192) v = 8192;

    gta2_scale_replace[GTA2_VHEIGHT_OFFSET + 0] = (unsigned char)(v      );
    gta2_scale_replace[GTA2_VHEIGHT_OFFSET + 1] = (unsigned char)(v >>  8);
    gta2_scale_replace[GTA2_VHEIGHT_OFFSET + 2] = (unsigned char)(v >> 16);
    gta2_scale_replace[GTA2_VHEIGHT_OFFSET + 3] = (unsigned char)(v >> 24);

    return v;
}

//
// Install the camera hook.
//
// extraZoomHundredths is ThirteenAG's fZoom in 1/100 units, added on top of the
// aspect correction: 0 leaves the framing his fix produces, positive pulls out.
//
static void InstallCameraHook(int extraZoomHundredths, unsigned int virtualHeight)
{
    HMODULE        mod;
    unsigned char *code = NULL, *at, *movAt, *stub;
    unsigned int   codeSize = 0;
    long long      camScaleFixed;
    int            hud, camAdd, rel;
    unsigned char  call[5];

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    at = FindUnique(code, codeSize, gta2_camhook_sig, sizeof(gta2_camhook_sig));
    if (!at) return;                    // absent, ambiguous, or already hooked
    movAt = at + CAMHOOK_MOV_AT;

    if (virtualHeight < 120) virtualHeight = 120;

    // hud_scale * 16384, using the same divisor as the SetupViewport patch so
    // the two mechanisms agree on the viewports they both touch.
    hud = (int)(((long long)GTA2_TARGET_H << 14) / virtualHeight);

    // camera_scale * 16384.  Expanding his macro:
    //     (W/640) / ((4/3)/(W/H))  ==  3*W*W / (2560*H)
    // Computed in 64-bit: 3*W*W*16384 overflows 32 bits above ~W=1900.
    camScaleFixed = ((long long)3 * GTA2_TARGET_W * GTA2_TARGET_W * 16384)
                    / ((long long)2560 * GTA2_TARGET_H);
    camAdd  = (int)(camScaleFixed - 16384);             // (camera_scale - 1.0)
    camAdd += (int)(((long long)extraZoomHundredths * 16384) / 100);

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(camhook_stub),
                                         MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, camhook_stub, sizeof(camhook_stub));
    memcpy(stub + STUB_HUD1_AT, &hud,    4);
    memcpy(stub + STUB_HUD2_AT, &hud,    4);
    memcpy(stub + STUB_CAM_AT,  &camAdd, 4);

    // call rel32, relative to the end of the instruction
    rel = (int)(stub - (movAt + 5));
    call[0] = 0xe8;
    memcpy(call + 1, &rel, 4);

    if (!WriteCode(movAt, call, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

// ---------------------------------------------------------------------------
// 4. HUD horizontal placement
// ---------------------------------------------------------------------------
//
// The HUD is laid out in a 640-wide virtual space, so on 2560x1080 the
// right-hand cluster (x1 / x5 / $0 / hearts) stops at 640*2.25 = 1440 instead
// of reaching the screen edge.
//
// Finding the code that positions it took a runtime diagnostic, after six
// static approaches came up empty.  Two transforms carry HUD coordinates:
//
//     0x5d0640   x = 611, 631, 620, 507, 16    (1061 records)
//     0x4932c0   x = 633, 518                  (139 records)
//
// Two traps cost a hardware test each, and both are invisible from the screen:
//
//   1. These coordinates are 16.14 FIXED POINT.  x=633 is stored as
//      633*16384 = 10,371,072.  An earlier build compared against a plain 320
//      and added a plain 1120, which moved things by 1120/16384 = 0.07 px --
//      indistinguishable from the hook never firing.
//
//   2. Each transform scales TWO coordinates and the FIRST one is Y.  Hooking
//      it offset the vertical instead: the HUD moved *down* by 560 px and a
//      portrait went off the bottom of the screen.  The hook belongs on the
//      SECOND multiply's shift.
//
//     delta  = screen_width - 640*scale       = 2560 - 1440 = 1120 px
//     centre : x += (delta/2) << 14
//     spread : x += delta << 14   when x_virtual >= 320 << 14
//
// A blanket offset is still too blunt -- it dragged the world-anchored crash
// popup far left and stretched the car-name frame (whose left edge is below
// x=320 and right edge above).  ThirteenAG solves this with a per-element
// posType; the diagnostic gave us the same discrimination for free by
// recording each record's CALLER.  The status bar comes from a known few:
//
//     0x4932c0 site : 0x4903f8                     x 518..633
//     0x5d0640 site : 0x5c9389                     x 551..631
//                     0x5ced73 0x5cefe2 0x5cf02b   x 507..620
//
// (the left-hand bars come from 0x5c8b94/8bc8/8bfd/8d66/8da6 at x 16..93 and
// need nothing -- they are already below the threshold)
//
// So the offset applies only to whitelisted callers.  Everything else --
// world-anchored popups, message text, anything unclassified -- passes through
// untouched, which is the safe default.
//
// The patched instruction is `mov $0xe,%ecx ; call __allshr` (10 bytes).  The
// stub does that shift inline with shrd, which is exact: __allshr is an
// arithmetic 64-bit shift and only the low dword was ever consumed.  ecx and
// edx are free -- the displaced code clobbered both, and the instruction after
// the patch site rewrites them before use.
//
// Stack arithmetic: the call pushes a return address (+4) and the stub pushes
// flags (+4), so the site's X offset gains 8.  The caller's return address is
// at 0x10(%esp) at both patch points, hence 0x18(%esp) inside the stub.
//
//
// Per-caller offsets, not a threshold.
//
// The first version used an `x >= 320` rule to decide what to shift.  That was
// always a heuristic standing in for knowledge we now have: the caller's return
// address identifies the element exactly, so each caller can carry its own
// offset and no threshold is needed.
//
// It also fixes the frame.  The area/car-name routine calls the 0x5d0640
// transform twice (0x5ce460 and 0x5ce49b) for its frame, separately from the
// text path.  Under the threshold rule the frame's left edge fell below 320 and
// its right edge above, so only the right edge moved and the frame stretched --
// exactly what hardware showed.  A frame wants CENTRING: both edges shifted by
// delta/2.  With per-caller offsets that is simply a different table entry.
//
//   OFF_SPREAD  = delta      right-anchored: HUD cluster reaching the edge
//   OFF_CENTRE  = delta/2    centred: the name frame
//   (absent)    = 0          everything else, including world-anchored popups
//
#define HUD_MAX_ENTRIES 24

struct HudCaller {
    DWORD ret;                 // caller return address, 0 terminates
    int   kind;                // 1 = spread (delta), 2 = centre (delta/2)
};

struct HudSite {
    const char   *note;
    unsigned char sigLen;
    unsigned char sig[10];
    HudCaller     callers[HUD_MAX_ENTRIES];
};

static const HudSite g_hudSites[] = {
    { "0x4932fd -- X shift in the 0x4932c0 transform", 10,
      { 0xb9, 0x0e, 0x00, 0x00, 0x00, 0xe8, 0xa9, 0x31, 0x15, 0x00 },
      { { 0x004903f8, 1 },                       /* HUD, right-anchored     */

        /* The counter's extra glyph.  With only 0x4903f8 whitelisted, "$10"
           put "$1" at the right edge and left the "0" behind at 4:3 -- the two
           halves of one string in different places.  Found from the caller map
           (x=633, y=14, 226 hits) rather than by guessing: an earlier guess at
           0x5c93df in the other transform was simply wrong. */
        { 0x0049047d, 1 },

        { 0, 0 } } },
    { "0x5d067d -- X shift in the 0x5d0640 transform", 10,
      { 0xb9, 0x0e, 0x00, 0x00, 0x00, 0xe8, 0x29, 0x5e, 0x01, 0x00 },
      { { 0x005c9389, 1 },                       /* HUD, right-anchored     */

        /* HALF heart -- the odd unit when health is not an even number.
           Found statically, not from a capture: it is the tail of the very
           routine that draws the full hearts, so it cannot be anything else.

               5c9350  test %eax,%eax        ; count of FULL hearts
               5c9356  loop: push $0x71 ; call 0x5d0640   -> 0x5c9389
               5c9389  add  $0x14,%esi       ; 20 per heart
               5c938d  jne  loop
               5c9393  ; (hp & 1) == 1 ?
               5c93a4  jne  skip
               5c93aa  sub  %ebx,%esi        ; back up 2
               5c93ac  push $0x72 ; call 0x5d0640          -> 0x5c93df

           Sprite 0x71 is a whole heart, 0x72 the half.  Same transform, same
           anchor, so kind 1 like its neighbours.

           (0x5c93df is the address once guessed for the money counter and
           reported as "wrong".  It was a real caller all along -- just a
           different element.  Guessing an address is not the same as knowing
           what draws there.) */
        { 0x005c93df, 1 },

        { 0x005ced73, 1 },
        { 0x005cefe2, 1 },
        { 0x005cf02b, 1 },

        /* Equipped-item indicator, virtual (623,117) -- present exactly while
           an item is held, one call per frame.  Right-anchored like the rest of
           the cluster, just further down and in a routine of its own, which is
           why it was never in the map: every earlier capture happened to be
           taken empty-handed.  Confirmed on hardware. */
        { 0x005cf1f2, 1 },

        /* Car-name frame: routine at 0x5ce420, two frame pieces, its text
           being msg_sites bit 8 (0x5ce4c6).  Confirmed centred on hardware. */
        { 0x005ce465, 2 },
        { 0x005ce4a0, 2 },

        /* Area-name frame: routine at ~0x5ce920, same shape but a FOUR-piece
           frame, its text being msg_sites bit 11 (0x5cea62).  Found by walking
           the call sites of this transform rather than guessing -- both
           routines read the same width global 0x7008a8 and end in the same
           0x5d06d0 / 0x435690 pair. */
        { 0x005ce94a, 2 },
        { 0x005ce98d, 2 },
        { 0x005ce9d8, 2 },
        { 0x005cea1e, 2 },

        /* Mission dialogue AVATAR, virtual (32,443).  Centring it alone split
           the pair, because the text goes through the 0x5d06f0 copy of the
           transform -- a fourth copy that was not being watched.  With that
           copy hooked too (below), the two move together. */
        { 0x005cd6d1, 2 },

        /* Weapon ICON, virtual (594,67).  Its ammo TEXT is a separate caller
           on the next transform copy (0x5cf175, below) -- the same split that
           caught out the avatar and its dialogue, so both go in together or the
           icon moves and the number stays behind.  Confirmed on hardware. */
        { 0x005cf0dc, 1 },

        { 0, 0 } } },

    //
    // The transform has FOUR copies.  This is the third, and it went unwatched
    // for a long time: the dialogue text runs through it, which is why
    // centring the avatar (in the copy above) moved the face and left the words
    // behind.  Found only after widening the diagnostic to all four.
    //
    { "0x5d072d -- X shift in the 0x5d06f0 transform", 10,
      { 0xb9, 0x0e, 0x00, 0x00, 0x00, 0xe8, 0x79, 0x5d, 0x01, 0x00 },
      { { 0x005cd733, 2 },                       /* dialogue text, y=405    */

        /* Mission title ("TUTORIAL!") at virtual (198,104).  It matches the
           screenshot measurement exactly -- the glyphs span x 199..438,
           y 114..165 -- and it was present in the previous capture too, logged
           as y=26 because site 2 was being decoded with site 1's stack slots.
           Fixing that decoding is what made it readable.  Two earlier attempts
           to find this by patching 640 and 320 constants were both dead ends;
           it was on the transform all along, in the copy nobody was watching. */
        { 0x005caede, 2 },

        /* Ammo count, virtual (625,82) -- right-anchored, so kind 1 to match
           its icon at 0x5cf0dc on the previous copy. */
        { 0x005cf175, 1 },

        /* Armour count, virtual (617,127) -- same arrangement one row down:
           icon on the previous copy (0x5cf1f2), count here.  Item icon and
           item COUNT are consistently split across the two transform copies,
           so assume any future "the number stayed behind" report is another
           caller in this table, not a missing one in the previous.
           Confirmed on hardware. */
        { 0x005cf25d, 1 },

        { 0, 0 } } },
};

//
//    0  pushfd
//    1  shrd $0xe,%edx,%eax         the shift the displaced code owed
//    5  cmpl $thr,<x>(%esp)         x >= 320<<14 ?
//   13  jl   done
//   15  mov  0x18(%esp),%ecx        caller return address
//   19  mov  $table,%edx
//   24  loop: cmp %ecx,(%edx) / je apply / add $4,%edx / cmpl $0,(%edx) / jne loop
//   36  jmp  done
//   40  apply: add $delta,%eax
//   45  done: popfd
//   46  ret
//   48  caller table (0-terminated)
//
//
//    0  pushfd
//    1  shrd $0xe,%edx,%eax        the shift the displaced code owed
//    5  mov  0x18(%esp),%ecx       caller return address
//    9  mov  $table,%edx
//   14  loop: cmpl $0,(%edx)  je done  /  cmp %ecx,(%edx)  je found
//        add $8,%edx  /  jmp loop
//   28  found: add 0x4(%edx),%eax  apply this caller's offset
//   31  done: popfd
//   32  ret
//   40  table: { caller, offset } pairs, 0-terminated
//
// Verified by disassembly before shipping: je 0x1f -> popf, je 0x1c -> the
// add, jmp 0xe -> the loop head.
//
static unsigned char hud_stub_tmpl[] = {
    0x9c,
    0x0f, 0xac, 0xd0, 0x0e,
    0x8b, 0x4c, 0x24, 0x18,
    0xba, 0, 0, 0, 0,                     /* table at 10 */
    0x83, 0x3a, 0x00,
    0x74, 0x0c,
    0x39, 0x0a,
    0x74, 0x05,
    0x83, 0xc2, 0x08,
    0xeb, 0xf2,
    0x03, 0x42, 0x04,
    0x9d,
    0xc3
};
#define HUD_TABLE_AT  10
#define HUD_TABLE_OFF 40

static void InstallHudHook(unsigned int virtualHeight)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0, i;
    int            hudWidth, delta, offSpread, offCentre;

    if (virtualHeight < 120) virtualHeight = 120;

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    hudWidth = (int)(((long long)640 * GTA2_TARGET_H) / virtualHeight);
    delta    = GTA2_TARGET_W - hudWidth;
    if (delta < 0) delta = 0;

    // 16.14 -- these are added to an already-shifted fixed-point pixel value.
    offSpread = delta << 14;
    offCentre = (delta / 2) << 14;

    for (i = 0; i < sizeof(g_hudSites) / sizeof(g_hudSites[0]); i++) {
        const HudSite *s = &g_hudSites[i];
        unsigned char *at, *stub, *table, call[10];
        unsigned int   nc;
        int            rel;

        at = FindUnique(code, codeSize, s->sig, s->sigLen);
        if (!at) continue;

        stub = (unsigned char *)VirtualAlloc(
                   NULL, HUD_TABLE_OFF + sizeof(s->callers),
                   MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!stub) continue;

        memcpy(stub, hud_stub_tmpl, sizeof(hud_stub_tmpl));

        table = stub + HUD_TABLE_OFF;
        for (nc = 0; nc < HUD_MAX_ENTRIES; nc++) {
            DWORD c   = s->callers[nc].ret;
            int   off = (s->callers[nc].kind == 2) ? offCentre : offSpread;
            memcpy(table + nc * 8,     &c,   4);
            memcpy(table + nc * 8 + 4, &off, 4);
            if (!c) break;
        }
        memcpy(stub + HUD_TABLE_AT, &table, 4);

        rel = (int)(stub - (at + 5));
        call[0] = 0xe8;
        memcpy(call + 1, &rel, 4);
        memset(call + 5, 0x90, 5);

        if (!WriteCode(at, call, 10))
            VirtualFree(stub, 0, MEM_RELEASE);
    }
}

// ---------------------------------------------------------------------------
// 5. Message-text placement -- one element at a time, bisectable
// ---------------------------------------------------------------------------
//
// The messages module (0x5c8000-0x5d0000) lays its text out against a hardcoded
// 640-unit width.  There are exactly 15 immediate loads of 640 in it, and they
// are all layout maths -- centring `(640-w)/2`, right-anchoring `640-w`, and one
// clamp.  Rewriting that 640 to the virtual width that actually fills the
// screen -- W*virtual_height/H, 1137 at 2560x1080 -- re-centres centred text on
// the real screen and pushes right-anchored text to the real edge.
//
// Only the sites actually needed are listed below.  The module has 15 such
// constants; an early build rewrote all 15 at once, which fixed the ESC prompt
// and the area-name text but also dragged the world-anchored crash popup off
// position and left the area-name frame behind, so frame and text disagreed.
// The sweep was right in kind and wrong in scope.
//
// The eight that were never needed have been dropped rather than left switchable
// -- they are recorded in git history and in CLAUDE.md if one is ever wanted.
// The mission title went with them: its site (0x5cf426) never executes, and the
// title is handled by the HUD caller table at 0x5caede instead.
//
// Two kinds of constant appear here:
//
//   WIDTH  -- a 640 used as the layout width, e.g. `(640-w)/2` or `640-w`.
//             Becomes the virtual width that fills the screen: W*vh/H = 1137.
//   WIDTH_FX -- the same layout width, but already in 16.14 (640<<14 =
//             0xa00000) because the site works in fixed point throughout.
//             Becomes vw<<14.  Scanning for plain 640 could never have found
//             these, which is exactly why the wanted-level icons stayed put
//             through every earlier msg_sites sweep.
//
//   SCREENW_FX / SCREENH_FX
//           -- not a layout width at all but the ORIGINAL SCREEN SIZE in
//              16.14, used as a visibility cull.  Becomes the real screen
//              size.  See the score-popup notes on bits 17/18 below.
//
#define MSG_WIDTH      0
#define MSG_WIDTH_FX   1
#define MSG_SCREENW_FX 2
#define MSG_SCREENH_FX 3

struct MsgSite {
    const char   *note;
    unsigned char kind;
    unsigned char immAt;      /* offset of the imm32 within sig */
    unsigned char sigLen;
    unsigned char sig[18];
};

static const MsgSite g_msgSites[] = {
 { "0x5ca685", MSG_WIDTH, 1, 18, {0xb8,0x80,0x02,0x00,0x00,0x51,0x2b,0xc5,0x8b,0xcc,0x99,0x2b,0xc2,0xd1,0xf8,0x50,0xe8,0xf6} },
 { "0x5ca6e5", MSG_WIDTH, 1, 18, {0xb8,0x80,0x02,0x00,0x00,0x51,0x2b,0xc5,0x8b,0xcc,0x99,0x2b,0xc2,0xd1,0xf8,0x50,0xe8,0x96} },
 { "0x5ca747", MSG_WIDTH, 1, 18, {0xb8,0x80,0x02,0x00,0x00,0x51,0x2b,0xc5,0x8b,0xcc,0x99,0x2b,0xc2,0xd1,0xf8,0x50,0xe8,0x34} },
 { "0x5ce4c6", MSG_WIDTH, 1, 18, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc2,0x51,0x99,0x2b,0xc2,0x8b,0xcc,0xd1,0xf8,0x50,0xe8,0xb5} },
 { "0x5ce624", MSG_WIDTH, 1, 18, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc2,0x51,0x99,0x2b,0xc2,0x8b,0xcc,0xd1,0xf8,0x50,0xe8,0x57} },
 { "0x5ce714", MSG_WIDTH, 1, 18, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc2,0x51,0x99,0x2b,0xc2,0x8b,0xcc,0xd1,0xf8,0x50,0xe8,0x67} },
 { "0x5cea62", MSG_WIDTH, 1, 7, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc1} },

 /* bit 16 -- wanted-level icons.  A repeated-icon widget centred against a
    hardcoded 640, at the head of its drawing function:

        5c91b0  sub  $0x14,%esp
        5c91b6  mov  %ecx,%edi            ; this
        5c91b8  mov  $0xa00000,%eax       ; 640 << 14      <- this
        5c91c1  mov  0x48(%edi),%ebx      ; icon count
        5c91c4  mov  0x4c(%edi),%ecx      ; per-icon spacing
        5c91c7  imul %ebx,%ecx            ; total group width
        5c91ca  sub  %ecx,%eax            ; 640<<14 - groupWidth
        5c91e1  sar  %esi                 ; /2   -> centred
        ...loop  add $0xc,%edi / movzbw (%edi),%dx / push $6

    Invisible to every earlier probe for two independent reasons: the constant
    is 16.14 rather than plain 640, and the coordinate transform is *inlined*
    here rather than called, so none of the four transform hooks ever saw it.
    That is why the capture showed no unknown caller even with the icons on
    screen.

    The 5-byte form `mov $0xa00000,%eax` is NOT unique -- it also matches the
    edge-anchor helper at 0x591436 -- so the signature runs to 12 bytes. */
 { "0x5c91b8", MSG_WIDTH_FX, 1, 12,
   {0xb8,0x00,0x00,0xa0,0x00, 0x89,0x7c,0x24,0x1c, 0x8b,0x5f,0x48} },

 /* bits 17,18 -- score popup ("+50" on a kill or a crash).  A VISIBILITY CULL
    against the original screen size, not a layout constant:

        591f1b  cmp  %eax,%ebx          ; x < lowerBound(0x6fcd78) ?
        591f1f  jl   0x591f7a           ;   -> skip the draw entirely
        591f21  cmp  $0xa00000,%ebx     ; x > 640<<14 ?   <- bit 17
        591f27  jg   0x591f7a           ;   -> skip
        591f29  cmp  %eax,%ebp
        591f2b  jl   0x591f7a
        591f2d  cmp  $0x780000,%ebp     ; y > 480<<14 ?   <- bit 18
        591f33  jg   0x591f7a           ;   -> skip
        591f35  mov  0xa8(%edi),%eax    ; scale -- GLYPH SIZE only
        591f70  push %ebp               ; y pushed RAW
        591f71  push %ebx               ; x pushed RAW
        591f75  call 0x5d0e90

    ebx/ebp are REAL SCREEN PIXELS in 16.14: the function seeds the projection
    from 0x74(%edi), the viewport centreY (height/2, real pixels), at 0x591e69.
    So the position was always correct and no scale fix is wanted here -- the
    bug is purely that the bounds are the 1999 screen size, so on 2560x1080
    anything outside the top-left 640x480 PIXELS is silently dropped.

    That is why every proportional-error theory failed, and why the captured x
    values stop dead at 624.4: those were the only popups that survived the
    cull.  The element never moved; it simply vanished unless the event
    happened in the top-left corner.

    Unlike every other site here the constant is the REAL screen size, not the
    virtual layout width -- these are pixels, not 640-space units.  And the
    opcode is two bytes (81 fb / 81 fd), so the immediate is at offset 2. */
 { "0x591f21", MSG_SCREENW_FX, 2, 8,
   {0x81,0xfb,0x00,0x00,0xa0,0x00, 0x7f,0x51} },
 { "0x591f2d", MSG_SCREENH_FX, 2, 8,
   {0x81,0xfd,0x00,0x00,0x78,0x00, 0x7f,0x45} },
};

//
// Score popup ("+50" on a kill or a crash) -- scale its position.
//
// The drawer at 0x5d0e90 takes REAL SCREEN PIXELS in 16.14.  That is settled
// by the HUD, which reaches it through the transform at 0x5d06b5 and passes
// 2260.8 for an element that renders correctly at the right-hand edge of a
// 2560-wide screen.
//
// The popup's call site does not scale:
//
//     591f35  mov  0xa8(%edi),%eax     ; the scale IS read here...
//     591f40  imull 0x3c(%esp)         ; ...but spent on GLYPH SIZE
//     591f70  push %ebp                ; y pushed RAW
//     591f71  push %ebx                ; x pushed RAW
//     591f73  push $6
//     591f75  call 0x5d0e90
//
// So ebx/ebp stay in the game's 640x480 virtual space and land at 1/2.25 of
// where they belong -- top-left, with the error growing with distance from the
// origin.  That is both "very far left" and "sometimes the location is also
// displaced": the error is proportional, not a constant offset.
//
// Invisible for 26 years because the scale is exactly 1.0 at 640x480.  Only
// 800x600 would ever have shown it, and then only by 25%.
//
// Hooked by redirecting the call rather than editing the push sequence: the
// stub scales arguments 3 and 4 in place and tail-jumps to the real drawer, so
// the game's return address is already correct and nothing has to be moved.
// edi still holds the viewport at the call -- nothing between 0x591f35 and the
// call touches it -- so the scale is read live and tracks virtual_height
// instead of being baked in.
//
static const unsigned char popup_sig[] = {
    0x55, 0x53, 0x50, 0x6a, 0x06,          /* push ebp; push ebx; push eax; push $6 */
    0xe8, 0x16, 0xef, 0x03, 0x00           /* call 0x5d0e90                         */
};
#define POPUP_CALL_AT 5

static unsigned char popup_stub[] = {
    0x50,                                  /* push %eax                     */
    0x52,                                  /* push %edx                     */
    0x51,                                  /* push %ecx                     */
    0x8b, 0x8f, 0xa8, 0x00, 0x00, 0x00,    /* mov 0xa8(%edi),%ecx   scale   */
    0x8b, 0x44, 0x24, 0x18,                /* mov 0x18(%esp),%eax   x       */
    0xf7, 0xe9,                            /* imul %ecx                     */
    0x0f, 0xac, 0xd0, 0x0e,                /* shrd $0xe,%edx,%eax           */
    0x89, 0x44, 0x24, 0x18,                /* mov %eax,0x18(%esp)           */
    0x8b, 0x44, 0x24, 0x1c,                /* mov 0x1c(%esp),%eax   y       */
    0xf7, 0xe9,                            /* imul %ecx                     */
    0x0f, 0xac, 0xd0, 0x0e,                /* shrd $0xe,%edx,%eax           */
    0x89, 0x44, 0x24, 0x1c,                /* mov %eax,0x1c(%esp)           */
    0x59,                                  /* pop %ecx                      */
    0x5a,                                  /* pop %edx                      */
    0x58,                                  /* pop %eax                      */
    0xe9, 0, 0, 0, 0                       /* jmp 0x5d0e90   (rel32 at 41)  */
};
#define POPUP_JMP_AT 41

static void InstallPopupScale(void)
{
    HMODULE        mod;
    unsigned char *code = NULL, *at, *stub, *target, call[5];
    unsigned int   codeSize = 0;
    int            rel;

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    at = FindUnique(code, codeSize, popup_sig, sizeof(popup_sig));
    if (!at) return;

    at += POPUP_CALL_AT;                       /* the call itself */

    /* Resolve the real drawer from the call we are replacing, so the address
       never has to be hardcoded. */
    memcpy(&rel, at + 1, 4);
    target = at + 5 + rel;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(popup_stub),
                                         MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, popup_stub, sizeof(popup_stub));
    rel = (int)(target - (stub + POPUP_JMP_AT + 4));
    memcpy(stub + POPUP_JMP_AT, &rel, 4);

    rel = (int)(stub - (at + 5));
    call[0] = 0xe8;
    memcpy(call + 1, &rel, 4);

    if (!WriteCode(at, call, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

static void ApplyMsgSites(unsigned int virtualHeight)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0, i;
    int            vw;

    if (virtualHeight < 120) virtualHeight = 120;

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    // The virtual width that maps onto the whole screen at the current scale.
    vw = (int)(((long long)GTA2_TARGET_W * virtualHeight) / GTA2_TARGET_H);

    for (i = 0; i < sizeof(g_msgSites) / sizeof(g_msgSites[0]); i++) {
        unsigned char *at;

        int val;

        at = FindUnique(code, codeSize, g_msgSites[i].sig, g_msgSites[i].sigLen);
        if (!at) continue;

        switch (g_msgSites[i].kind) {
        case MSG_WIDTH_FX:    val = vw << 14;              break;
        case MSG_SCREENW_FX:  val = GTA2_TARGET_W << 14;   break;
        case MSG_SCREENH_FX:  val = GTA2_TARGET_H << 14;   break;
        default:              val = vw;                    break;
        }
        WriteCode(at + g_msgSites[i].immAt, (const unsigned char *)&val, 4);
    }
}

//
// Everything GTA2 needs, in one place.  Called unconditionally; the exe-name
// tests below are what decide whether anything happens.
//
static void Gta2Apply(const char *exePath, const char *ini, BOOL haveIni)
{
    unsigned int i, vh = GTA2_VHEIGHT_DEFAULT;
    BOOL         isGame;

    isGame = PathEndsWith(exePath, "gta2.exe") ||
             PathEndsWith(exePath, "gta2.icd");

    if (haveIni)
        vh = ReadVirtualHeight(ini);

    // The DMAGlide mode list, which "gta2 manager.exe" needs too -- hence the
    // profile table rather than the isGame test.
    for (i = 0; i < g_profileCount; i++) {
        if (!PathEndsWith(exePath, g_profiles[i].exeName)) continue;

        ApplyProfile(&g_profiles[i]);
    }

    if (!isGame) return;

    //
    // The camera hook is not a BytePatch: its replacement contains a rel32 to
    // memory allocated at runtime, so it cannot live in a static find/replace
    // table.  The manager process never runs this code.
    //
    if (haveIni && GetPrivateProfileIntA("GTA2", "aspect_fix", 1, ini))
        InstallCameraHook(
            (int)GetPrivateProfileIntA("GTA2", "extra_zoom", 0, ini), vh);

    //
    // UI placement.  These three are unconditional rather than ini toggles:
    // each is a plain correctness fix -- put the element where it belongs --
    // and there is no configuration in which leaving it wrong is wanted.  They
    // are not BytePatches because their stubs hold runtime-allocated tables.
    //
    // They are also a set.  The popup fix converts the score popup's position
    // to real pixels, which only works because ApplyMsgSites has raised the
    // 640x480 visibility cull to the real screen size; enabling one without the
    // other would drop every popup outside the top-left corner.
    //
    InstallHudHook(vh);
    InstallPopupScale();
    ApplyMsgSites(vh);
}

//
// Bake the chosen mode into the DMAGlide replacement bytes.  Called from
// GameFix_SetResolutionEnum, which is the only thing that knows the mode and
// deliberately knows nothing about DMAGlide.
//
static void Gta2AdoptResolution(void)
{
    PutU32(gta2_mode_replace + MODE_H_AT,   g_targetH);
    PutU32(gta2_mode_replace + MODE_W_AT,   g_targetW);
    PutU32(gta2_enum_replace + ENUM_W_AT,   g_targetW);
    PutU32(gta2_enum_replace + ENUM_H_AT,   g_targetH);
    PutU32(gta2_enum_replace + ENUM_RES_AT, g_targetRes);
}


// ==========================================================================
// Turok: Dinosaur Hunter (1997, Acclaim)
// ==========================================================================
//
// Far smaller than GTA2, for two reasons worth stating because each one removed
// a whole phase of the playbook:
//
//   - Turok.exe is a plain unencrypted PE (.text entropy 6.53).  No SafeDisc,
//     so no runtime dump and no address recovery from memory: everything below
//     was read straight off the file.  It also has no .reloc section, so it can
//     never be rebased and absolute displacements in its signatures are stable.
//
//   - Video_3DFx.dll is an N64 RSP/RDP emulator.  Turok PC is a port and still
//     feeds its renderer F3D display lists, so the engine works in the N64's
//     320x240 screen space and EVERY screen coordinate -- world vertices,
//     texture rectangles, fill rects, the clip window -- passes through one
//     pair of scale factors.
//
// The game does choose a resolution of its own (320x240 / 512x384 / 640x480 /
// 800x600, mapped to a Glide enum at 0x10002630), but nothing here has to agree
// with it: the driver's override decides the real screen size, and T1 writes
// the renderer's screen-space constants absolutely.
//
// Timing is unusually kind.  GFXDLL_InitializeDriver (0x10004bd0) calls the
// Glide init, which calls grGlideInit as its first instruction, and only then
// calls the viewport init -- so by the time we are invoked the module is
// certainly mapped (we are being called from inside it) and the function we
// rewrite has not run yet.
//

// ---------------------------------------------------------------------------
// T1. N64 screen space -> real pixels, in Video_3DFx.dll
// ---------------------------------------------------------------------------
//
// 0x10007a30 sets the entire mapping, in five floats:
//
//     screen width   \  used only to re-centre a shrunken viewport
//     screen height  /  (the size slider below)
//     viewport size, GFXDLL_changeViewportSize
//     sx: pixels per N64 x unit = W/320
//     sy: pixels per N64 y unit = H/240
//
// 0x10007a70 derives the four values every consumer actually reads:
//
//     screenX = n64X * sx*S + (1-S)*W/2
//     screenY = (H - (1-S)*H/2) - n64Y * sy*S
//
// so sx = W/320 and sy = H/240 make the whole renderer fill the real screen.
// The clip window follows too: 0x10004d90 builds it from the same globals via
// the viewport's vscale/vtrans, so grBufferClear keeps clearing the full frame.
//
// Nothing else in the DLL assumes a fixed size on the drawing path -- the F12
// screen grab (0x10004a10) already asks grSstScreenWidth/Height.
//
// On its own this is a horizontal STRETCH -- the projection matrix still
// assumes 4:3.  T2 is what turns it into widescreen.
//
// Every instruction involved names its global by absolute address and
// Video_3DFx.dll carries relocations, so the signature is built from the
// module's actual base rather than written out as a static table.  Nothing here
// depends on the DLL landing at its preferred 0x10000000.
//
#define TUROK_VID_DLL      "Video_3DFx.dll"

/* N64 framebuffer the engine lays everything out against. */
#define TUROK_N64_W 320.0f
#define TUROK_N64_H 240.0f

//
// The target is the retail build:
//
//     Turok.exe  1,154,560  (1997-11-11)
//     Video_3DFx.dll 103,936 (1997-11-10)
//
// An earlier 1997 build exists (Turok.exe 1,088,000, Video_3DFx.dll 82,432) and
// was supported for a while.  That is gone: none of the signatures below match
// it, so it now runs unpatched -- which is what any unrecognised build should
// do anyway.
//
// Despite shipping in a folder called Turok_D3D, the game carries both
// renderers.  This patches the Glide one; Video_D3D.dll is never touched.
//
// The five viewport floats are contiguous, in this order:
//
//     +0x00 screen width   +0x04 screen height   +0x08 viewport-size slider
//     +0x0c sx (pixels per N64 x unit)           +0x10 sy
//
#define TUROK_VP_W   0x00u
#define TUROK_VP_H   0x04u
#define TUROK_VP_S   0x08u
#define TUROK_VP_SX  0x0cu
#define TUROK_VP_SY  0x10u

//
// 0x10007a30 COMPUTES those five from the width and height the game chose,
// which GFXDLL_InitializeDriver has already latched into registers before it
// calls us -- so the mode globals cannot be patched from grGlideInit, but this
// function has not run yet and its whole body can be replaced.  63 bytes of
// room for the 51 we need.
//
#define TUROK_VP_GLOBALS    0x00019e38u
#define TUROK_VP_SLIDER1    0x00013108u   /* 1.0f, the slider's initial value */
#define TUROK_VP_K320       0x00013110u   /* 1/320, double */
#define TUROK_VP_K240       0x00013118u   /* 1/240, double */
#define TUROK_2D_SIGGLOBAL  0x00019e6cu   /* the X scale, named in its sig */

// ---------------------------------------------------------------------------
// T2. Projection aspect, in Turok.exe
// ---------------------------------------------------------------------------
//
// 0x47fa70 is guPerspective(mtx, perspNorm, fovy, aspect, near, far, scale) --
// its one call site is 0x44060d, and it writes m00 = cot(fovy/2) / aspect.
// The aspect argument is built immediately before it:
//
//     0x4405bd  fld   [esi+0x158]        viewport width fraction  (1.0)
//     0x4405c3  fmul  ds:0x48dfe0        * 320.0        <-- THE CONSTANT
//     0x4405c9  fld   ds:0x48dfa4        1.0
//     0x4405cf  fsub  [esi+0x2c]         - letterbox    (0.0)
//     0x4405d2  fmul  [esi+0x15c]        * viewport height fraction (1.0)
//     0x4405d8  fmul  ds:0x48dfe4        * 240.0
//     0x4405de  fdivrp                   aspect = num / den
//
// At stock settings that is 320/240 = 4:3.  Rewriting the 320.0 to 240.0*W/H
// makes it W/H, which is Hor+ widescreen: the vertical field of view is
// untouched and the wider screen simply shows more to the sides.
//
// Doing this in the exe rather than scaling the projection matrix inside
// Video_3DFx.dll is deliberate.  The exe culls and clips against the matrix it
// built; widening the frustum only on the renderer's side would leave geometry
// culled to the old 4:3 frustum, which shows up as world popping in and out at
// the screen edges.  Fixing the aspect at its source keeps everything downstream
// -- matrix, perspNorm, culling -- consistent by construction.
//
// The constant lives in .rdata, so it cannot be located by a code scan.  It is
// reached instead through the disp32 of the `fmul` that reads it: the 35-byte
// signature below occurs exactly once in Turok.exe, and 0x48dfe0 is referenced
// from nowhere else in the image.
//
static const unsigned char turok_aspect_sig[] = {
    0xd9, 0x86, 0x58, 0x01, 0x00, 0x00,   // fld    0x158(%esi)
    0xd8, 0x0d, 0xe0, 0xdf, 0x48, 0x00,   // fmul   0x48dfe0      <- disp32 @8
    0xd9, 0x05, 0xa4, 0xdf, 0x48, 0x00,   // fld    0x48dfa4
    0xd8, 0x66, 0x2c,                     // fsub   0x2c(%esi)
    0xd8, 0x8e, 0x5c, 0x01, 0x00, 0x00,   // fmul   0x15c(%esi)
    0xd8, 0x0d, 0xe4, 0xdf, 0x48, 0x00,   // fmul   0x48dfe4
    0xde, 0xf9                            // fdivrp %st,%st(1)
};
#define TUROK_ASPECT_DISP_AT 8

// ---------------------------------------------------------------------------
// T3. The FOV knob, in Turok.exe -- one double inside guPerspective
// ---------------------------------------------------------------------------
//
// guPerspective's first act is to turn fovy into cot(fovy/2):
//
//     47fa70  fld   [esp+0xc]        fovy, in degrees
//     47fa74  fmul  ds:0x48f398      * pi/180
//     47fa7a  mov   eax,[esp+0x18]
//     47fa7e  push  eax
//     47fa7f  push  $1.0
//     47fa84  fmul  ds:0x48f3a0      * 0.5           <-- THE CONSTANT
//     47fa8a  fptan
//     47fa8e  fdivr ds:0x48f3a8      1 / tan  ->  cot(fovy/2)
//
// Scaling that 0.5 scales the half-angle, so `fov` is exactly "percent of the
// game's own field of view, in degrees" -- linear and easy to reason about.
// Because cot(fovy/2) lands in BOTH m00 and m11, it widens the view vertically
// and horizontally in the same proportion: no distortion at any setting, and
// completely independent of the aspect fix above.
//
// 0x48f3a0 is a QWORD double referenced from exactly one instruction, and
// guPerspective itself has exactly one call site, so nothing else in the game
// can see the change.
//
// The one caveat worth stating: the game's own visibility culling uses its
// unscaled fovy, not the matrix.  At large `fov` values geometry may be culled
// slightly before it leaves the screen.  That is why the default is 100.
//
static const unsigned char turok_fov_sig[] = {
    0xd9, 0x44, 0x24, 0x0c,               // fld    0xc(%esp)
    0xdc, 0x0d, 0x98, 0xf3, 0x48, 0x00,   // fmull  0x48f398       (pi/180)
    0x8b, 0x44, 0x24, 0x18,               // mov    0x18(%esp),%eax
    0x50,                                 // push   %eax
    0x68, 0x00, 0x00, 0x80, 0x3f,         // push   $0x3f800000
    0xdc, 0x0d, 0xa0, 0xf3, 0x48, 0x00,   // fmull  0x48f3a0  (0.5) <- disp @22
    0xd9, 0xf2                            // fptan
};
#define TUROK_FOV_DISP_AT 22

// ---------------------------------------------------------------------------
// T4. The HUD, in Video_3DFx.dll -- an inline hook on the 2D rect drawer
// ---------------------------------------------------------------------------
//
// Turok's 2D -- health bar, face icon, ammo, text -- is drawn in the SAME
// 320x240 N64 screen space as the world, through the same globals, so T1's
// sx = W/320 stretches it by sx/sy (1.78x at 2560x1080).  A constant cannot
// separate the two; this needs a hook.
//
// 0x10003a90 is the 2D rectangle drawer, reached from four display-list opcode
// handlers (0x100058ed, 0x1000594e, 0x100059c4, 0x10005a2d).  Hooking its entry
// covers all of them at once, which is also why the hook adjusts ARGUMENTS
// rather than the transform: the same function has to keep working for every
// caller.
//
// Argument order is (x, y, x, y), read off the function itself rather than
// assumed.  It loads its four arguments immediately and then interleaves them:
//
//     flds a1 ; flds a2 ; flds a3 ; flds a4     -> st0=a4 a3 a2 a1
//     fxch %st(3)  fmul <X scale>               -> a1 is X
//     fxch %st(2)  fmul <X scale>               -> a3 is X
//     ... fmul <Y scale> on a2 and a4
//
// The scissor opcode at 0x10005ef7 corroborates it: it builds a
// grClipWindow(minx,miny,maxx,maxy) call -- a known signature -- and the
// arguments it scales with the X globals are exactly the ones taken from bits
// 8-19 of the command words, the Y ones from bits 20-31.
//
// The correction is applied in pre-transform N64 units, which makes it
// independent of both the screen size and the viewport-size slider: whatever S
// and W are, x=0 lands on the left edge of the viewport and x=320 on the right.
//
//     ratio = sy / sx                 (0.5625 at 2560x1080)
//     span  = 320 * (1 - ratio)       the slack to distribute
//     x' = x * ratio + off
//
// `off` is span/2 -- one offset, every rectangle, so the 320-wide layout is
// reproduced exactly inside a centred 4:3 island.  Nothing can be split,
// nothing can move relative to anything else.
//
// Rects at least TUROK_HUD_FULLWIDTH wide are passed through untouched:
// full-screen fades and backgrounds have to keep covering the screen, and
// stretching a solid colour is invisible.  256 is hardware-confirmed -- it is
// what keeps the black fade in/out covering the whole screen.
//
// WHY THERE IS NO EDGE-ANCHORED MODE
//
// Two were tried, to put the HUD back in the screen corners, and both failed on
// hardware for the same underlying reason.  Recorded here because the idea is
// an obvious one to have again.
//
// Turok's 2D is drawn ONE RECTANGLE PER GLYPH, and this renderer offers no way
// to tell one element from another: every 2D primitive arrives through this one
// drawer from a display-list interpreter, so there is no distinguishing call
// site (the GTA2 technique) and no semantic tag.  All that is available is the
// rectangle's own position -- and position does not separate the cases:
//
//     centred text  "LOCATE THE HUB RUINS"   37.8 .. 282.2   (centre 160.0)
//     corner HUD    ammo counter + icon     271.1 .. 307.6
//
// Anchoring per rectangle by which third it fell in cut strings into three
// columns; the fragments met at exactly 106.7 and 213.3, the zone boundaries.
// Anchoring per RUN instead -- inherit the offset from an adjacent rectangle on
// the same row -- fixed the cutting, but then the threshold that decides a run
// has to reach >= 33.3 units from the right edge to catch the ammo group by its
// leading glyph, while staying < 37.8 so it does not grab the leading glyph of
// centred text.  A 4.5-unit window out of 320, narrower than the measurement
// error.  There is no such threshold.
//
// The run rule also glued unrelated things together: the pause menu's backing
// box is one tall rectangle overlapping nearly every row, so whatever was drawn
// next inherited its offset.
//
// So the 2D layer is corrected in size only, and the centred result is
// unconditional.  Any real fix needs per-element knowledge this renderer does
// not carry.
//
#define TUROK_HUD_FULLWIDTH 256.0f
#define TUROK_2D_ENTRY_LEN  6             /* the displaced `sub $N,%esp` */

//
// Stub, reached by a `call rel32` overwriting the drawer's first 6 bytes.
//
// It begins `add $4,%esp` to discard the return address our own call pushed:
// after that esp is exactly what the drawer's entry saw, so the argument
// offsets are the original ones and the displaced `sub` can run unchanged.
// It ends in a `jmp` back rather than a `ret` for the same reason (see
// GAME-PATCHING.md section 5a -- getting this wrong silently shifts every
// argument).
//
// The displaced instruction is COPIED FROM THE SITE rather than written out
// here: copying is shorter than spelling it out, and stays correct if the
// instruction ever differs.
//
static unsigned char turok_hud_stub[] = {
    0x83, 0xc4, 0x04,                     // add   $0x4,%esp
    0x60,                                 // pushad
    0x8d, 0x44, 0x24, 0x24,               // lea   0x24(%esp),%eax   -> &arg1
    0x50,                                 // push  %eax
    0xe8, 0,0,0,0,                        // call  TurokFixRect2D    <- @9
    0x83, 0xc4, 0x04,                     // add   $0x4,%esp
    0x61,                                 // popad
    0,0,0,0,0,0,                          // <the drawer's own first 6 bytes> @18
    0xe9, 0,0,0,0                         // jmp   drawer+6          <- @24
};
#define TUROK_STUB_CALL_AT   9
#define TUROK_STUB_DISP_AT  18
#define TUROK_STUB_JMP_AT   24

/* Live pointers into Video_3DFx.dll, so the hook follows any later change. */
static const float *g_turokSx = NULL;
static const float *g_turokSy = NULL;

//
// The body of the hook, in C rather than hand-assembled bytes.
//
// `a` points at the drawer's four float arguments: a[0] and a[2] are X, a[1]
// and a[3] are Y.  Y is left alone -- the vertical mapping was never wrong.
//
// Not static, and marked used/noinline, because its only reference is the
// rel32 written into the stub above: nothing in this translation unit calls it,
// and it must keep a plain cdecl frame.
//
extern "C" void __attribute__((cdecl, used, noinline))
TurokFixRect2D(float *a)
{
    float sx, sy, ratio, x0, x1, w, off;

    if (!g_turokSx || !g_turokSy) return;

    sx = *g_turokSx;
    sy = *g_turokSy;
    if (!(sx > 0.0f) || !(sy > 0.0f)) return;

    ratio = sy / sx;
    if (ratio > 0.999f) return;         // 4:3 or narrower: nothing to correct

    x0 = a[0];
    x1 = a[2];

    w = x1 - x0;
    if (w < 0.0f) w = -w;

    // Full-width elements keep the stretched mapping.  A fade or a background
    // has to span the screen; shrinking it would leave the sides uncovered,
    // and a solid colour cannot look stretched.
    if (w >= TUROK_HUD_FULLWIDTH) return;

    // One offset for every rectangle: the 320-wide layout is reproduced
    // exactly, centred.  Nothing can be split and nothing can move relative to
    // anything else -- see the note above on why there is no alternative.
    off = TUROK_N64_W * (1.0f - ratio) * 0.5f;

    a[0] = x0 * ratio + off;
    a[2] = x1 * ratio + off;
}


//
// T1: replace the viewport init's body.
//
// 0x10007a30 computes the five viewport floats from the width and height the
// game selected --
//
//     fild [esp+4] ; fild [esp+8] ; fld 1.0
//     fst W ; fmul 1/320    fst H ; fmul 1/240
//     fstp slider ; fstp sx ; fstp sy
//
// -- so sx and sy are already W/320 and H/240, just of the wrong W and H.  Its
// arguments cannot be corrected from here: GFXDLL_InitializeDriver reads the
// mode globals into registers BEFORE it calls grGlideInit, so by the time we
// run they are latched.  The function itself has not run yet, and it has
// exactly one caller, so its whole body is replaced with five absolute stores
// holding the real numbers.  63 bytes available, 51 used, the rest nops.
//
// Writing absolute values also makes the result independent of whatever the
// game's own settings dialog is set to.
//
#define TUROK_VP_BLOCK_LEN 50

static void InstallTurokViewport(void)
{
    HMODULE        mod;
    unsigned char *code = NULL, *at, *p;
    unsigned int   codeSize = 0, base, len;
    unsigned char  find[64], repl[64];

    mod = GetModuleHandleA(TUROK_VID_DLL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    base = (unsigned int)mod;

    /* The original body.  Every instruction carries an absolute address, so it
       has to be built from the module's real base rather than written out. */
    p = find;
    *p++ = 0xdb; *p++ = 0x44; *p++ = 0x24; *p++ = 0x04;   // fild 0x4(%esp)
    *p++ = 0xdb; *p++ = 0x44; *p++ = 0x24; *p++ = 0x08;   // fild 0x8(%esp)
    *p++ = 0xd9; *p++ = 0x05; PutU32(p, base + TUROK_VP_SLIDER1); p += 4;
    *p++ = 0xd9; *p++ = 0xca;                             // fxch %st(2)
    *p++ = 0xd9; *p++ = 0x15; PutU32(p, base + TUROK_VP_GLOBALS + TUROK_VP_W); p += 4;
    *p++ = 0xdc; *p++ = 0x0d; PutU32(p, base + TUROK_VP_K320);    p += 4;
    *p++ = 0xd9; *p++ = 0xc9;                             // fxch %st(1)
    *p++ = 0xd9; *p++ = 0x15; PutU32(p, base + TUROK_VP_GLOBALS + TUROK_VP_H); p += 4;
    *p++ = 0xdc; *p++ = 0x0d; PutU32(p, base + TUROK_VP_K240);    p += 4;
    *p++ = 0xd9; *p++ = 0xca;                             // fxch %st(2)
    *p++ = 0xd9; *p++ = 0x1d; PutU32(p, base + TUROK_VP_GLOBALS + TUROK_VP_S);  p += 4;
    *p++ = 0xd9; *p++ = 0x1d; PutU32(p, base + TUROK_VP_GLOBALS + TUROK_VP_SX); p += 4;
    *p++ = 0xd9; *p++ = 0x1d; PutU32(p, base + TUROK_VP_GLOBALS + TUROK_VP_SY); p += 4;
    *p++ = 0xc3;                                          // ret
    len = (unsigned int)(p - find);                       // 63

    at = FindUnique(code, codeSize, find, len);
    if (!at) return;            // already patched, or not this build

    PutMovAbsImm(repl +  0, base + TUROK_VP_GLOBALS + TUROK_VP_W,
                 FloatBits((float)g_targetW));
    PutMovAbsImm(repl + 10, base + TUROK_VP_GLOBALS + TUROK_VP_H,
                 FloatBits((float)g_targetH));
    PutMovAbsImm(repl + 20, base + TUROK_VP_GLOBALS + TUROK_VP_S,
                 FloatBits(1.0f));
    PutMovAbsImm(repl + 30, base + TUROK_VP_GLOBALS + TUROK_VP_SX,
                 FloatBits((float)g_targetW / TUROK_N64_W));
    PutMovAbsImm(repl + 40, base + TUROK_VP_GLOBALS + TUROK_VP_SY,
                 FloatBits((float)g_targetH / TUROK_N64_H));
    repl[TUROK_VP_BLOCK_LEN] = 0xc3;                      // ret
    memset(repl + TUROK_VP_BLOCK_LEN + 1, 0x90,           // pad, same length
           len - TUROK_VP_BLOCK_LEN - 1);

    WriteCode(at, repl, len);
}

//
// T2: rewrite the projection aspect constant in Turok.exe.
//
// A data patch, not a code patch, so it is located indirectly: find the unique
// instruction sequence that reads the constant, take the address out of the
// fmul's disp32, and rewrite the float there.
//
// Idempotent by value rather than by pattern -- the signature is code and
// survives the patch, so a second call simply finds the constant already
// holding the value it wants and does nothing.  Anything that is neither the
// original 320.0 nor our target is left alone: that would mean the constant is
// not what this code thinks it is.
//
//
// Shared by T2 and T3: find the signature in the host image and hand back the
// address named by the disp32 at `dispAt`, having checked it points inside the
// image.  A stray match must not turn into a wild write.
//
static unsigned char *TurokConstAt(const unsigned char *sig, unsigned int sigLen,
                                   unsigned int dispAt, unsigned int size)
{
    HMODULE                 mod;
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS *nt;
    unsigned char          *code = NULL, *at;
    unsigned int            codeSize = 0, addr, imgBase, imgSize;

    mod = GetModuleHandleA(NULL);
    if (!mod) return NULL;
    if (!GetCodeRange(mod, &code, &codeSize)) return NULL;

    at = FindUnique(code, codeSize, sig, sigLen);
    if (!at) return NULL;

    addr    = ReadU32(at + dispAt);
    dos     = (const IMAGE_DOS_HEADER *)mod;
    nt      = (const IMAGE_NT_HEADERS *)((const unsigned char *)mod + dos->e_lfanew);
    imgBase = (unsigned int)mod;
    imgSize = (unsigned int)nt->OptionalHeader.SizeOfImage;

    if (addr < imgBase || addr + size > imgBase + imgSize) return NULL;
    return (unsigned char *)addr;
}

static void InstallTurokAspect(void)
{
    unsigned char *konst;
    unsigned int   want, cur;

    konst = TurokConstAt(turok_aspect_sig, sizeof(turok_aspect_sig),
                         TUROK_ASPECT_DISP_AT, 4);
    if (!konst) return;

    cur  = ReadU32(konst);
    want = FloatBits(TUROK_N64_H * (float)g_targetW / (float)g_targetH);

    if (cur == want) return;                       // already applied
    if (cur != FloatBits(TUROK_N64_W)) return;     // not the constant we expect

    WriteCode(konst, (const unsigned char *)&want, 4);
}

//
// T3: scale guPerspective's half-angle constant.
//
// Idempotent the same way T2 is -- by value, since the code signature survives
// a change to the data it points at.  The original 0.5 is the only accepted
// starting value.
//
static void InstallTurokFov(unsigned int fovPercent)
{
    unsigned char *konst;
    double         want;
    union { double d; unsigned char b[8]; } cur, next;

    if (fovPercent == 100) return;              // exactly the shipped value
    if (fovPercent < 50)   fovPercent = 50;
    if (fovPercent > 200)  fovPercent = 200;

    konst = TurokConstAt(turok_fov_sig, sizeof(turok_fov_sig),
                         TUROK_FOV_DISP_AT, 8);
    if (!konst) return;

    memcpy(cur.b, konst, 8);
    want = 0.5 * (double)fovPercent / 100.0;
    next.d = want;

    if (memcmp(cur.b, next.b, 8) == 0) return;  // already applied
    if (cur.d != 0.5) return;                   // not the constant we expect

    WriteCode(konst, next.b, 8);
}

//
// T4: hook the 2D rect drawer in Video_3DFx.dll so the HUD keeps its aspect.
//
static void InstallTurokHud(void)
{
    HMODULE        mod;
    unsigned char *code = NULL, *at, *stub;
    unsigned int   codeSize = 0, base;
    unsigned char  sig[42], call[TUROK_2D_ENTRY_LEN];
    int            rel;

    mod = GetModuleHandleA(TUROK_VID_DLL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    base = (unsigned int)mod;

    //
    // Entry signature for the drawer at 0x10003a90.  `sub $0x108,%esp` alone is
    // nowhere near unique; what pins it is the absolute operand of the first
    // scale multiply, which is also why the pattern has to be built from the
    // module's real base.
    //
    memcpy(sig,
           "\x81\xec\x08\x01\x00\x00"            // sub  $0x108,%esp
           "\xd9\x84\x24\x0c\x01\x00\x00"        // flds 0x10c(%esp)
           "\xd9\x84\x24\x10\x01\x00\x00"        // flds 0x110(%esp)
           "\xd9\x84\x24\x14\x01\x00\x00"        // flds 0x114(%esp)
           "\xd9\x84\x24\x18\x01\x00\x00"        // flds 0x118(%esp)
           "\xd9\xcb"                            // fxch %st(3)
           "\xd8\x0d\x00\x00\x00\x00",           // fmuls <X scale>
           sizeof(sig));
    PutU32(sig + 38, base + TUROK_2D_SIGGLOBAL);

    at = FindUnique(code, codeSize, sig, sizeof(sig));
    if (!at) return;                    // absent, ambiguous, or already hooked

    g_turokSx = (const float *)(base + TUROK_VP_GLOBALS + TUROK_VP_SX);
    g_turokSy = (const float *)(base + TUROK_VP_GLOBALS + TUROK_VP_SY);

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(turok_hud_stub),
                                         MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, turok_hud_stub, sizeof(turok_hud_stub));

    // The displaced instruction, taken from the site itself.
    memcpy(stub + TUROK_STUB_DISP_AT, at, TUROK_2D_ENTRY_LEN);

    rel = (int)((unsigned char *)&TurokFixRect2D
                - (stub + TUROK_STUB_CALL_AT + 5));
    memcpy(stub + TUROK_STUB_CALL_AT + 1, &rel, 4);

    rel = (int)((at + TUROK_2D_ENTRY_LEN) - (stub + TUROK_STUB_JMP_AT + 5));
    memcpy(stub + TUROK_STUB_JMP_AT + 1, &rel, 4);

    // call rel32 over the displaced `sub`, padded to its full 6 bytes.
    rel     = (int)(stub - (at + 5));
    call[0] = 0xe8;
    memcpy(call + 1, &rel, 4);
    call[5] = 0x90;                     // nop

    if (!WriteCode(at, call, sizeof(call)))
        VirtualFree(stub, 0, MEM_RELEASE);
}

//
// Everything Turok needs, in one place.
//
// None of these is a BytePatch: T1 builds its signature from the runtime module
// base, T2 and T3 patch .rdata rather than code, and T4 is an inline hook.
//
static void TurokApply(const char *exePath, const char *ini, BOOL haveIni)
{
    if (!PathEndsWith(exePath, "turok.exe")) return;

    // Unconditional: without it the game draws a 640x480 image into the corner
    // of the screen, which is not a preference.
    InstallTurokViewport();

    // Behind an ini key only so a hardware run can separate "fills the screen"
    // from "fills the screen with the right geometry".  Off gives a full-screen
    // but horizontally stretched picture -- a useful diagnostic, and a
    // recoverable state without a rebuild.
    if (!haveIni || GetPrivateProfileIntA("TUROK", "aspect_fix", 1, ini))
        InstallTurokAspect();

    if (haveIni)
        InstallTurokFov(GetPrivateProfileIntA("TUROK", "fov", 100, ini));

    // Unconditional, with nothing to tune.  A stretched HUD is simply wrong,
    // and the one alternative to correcting it -- putting the elements back in
    // the screen corners -- was tried twice on hardware and cannot be done from
    // this renderer (see the note above T4).
    InstallTurokHud();
}


// ==========================================================================
// Ignition (UDS / Virgin Interactive, 1997)
// ==========================================================================
//
// Ign_3dfx.exe is a plain unencrypted PE (.text entropy 6.66) with relocations
// stripped, so it can never be rebased and the absolute addresses in its own
// code are stable.  It links glide2x.dll directly -- 34 imports, including
// guDrawTriangleWithClip and grLfbLock -- and there is no separate video device
// DLL: the game is its own renderer.
//
// Requirements 1 and 2 of the playbook are free here.  The single grSstWinOpen
// call passes a hardcoded `push $0x6` (GR_RESOLUTION_640x400) and the mode
// table at 0x46E7D8 has exactly one entry, so the game never asks for anything
// else -- and does not need to, because the driver override decides the real
// screen size regardless.  All that is left is convincing the game of it.
//
// The engine turns out to be almost entirely resolution-parametric, which is
// what keeps this small.  Two globals hold the screen size --
//
//     0x536EEC   screen width      ~115 read sites
//     0x547B18   screen height     ~80  read sites
//
// -- and the Glide clip window, the viewport rectangles, the projection centre
// and the sprite scales are all derived from them.  Setting those two is most
// of the fix.  What is NOT derived is the aspect: the engine was written for a
// 320x200 virtual space on a 4:3 display, so it carries a fixed 1.2 pixel
// aspect that has to be taken back out on a square-pixel panel.
//
// Four patches, in this order:
//
//   F1  relocate the 8bpp overlay buffer   (must succeed before any of the rest)
//   F2  screen size globals   -> the real W,H
//   F3  in-game camera focal lengths 425/348 -> derived from H
//   F4  the two remaining aspect divisors    -> derived from H
//

#define IGN_EXE          "ign_3dfx.exe"

/* The mode the engine was written for, and the display it assumed. */
#define IGN_NATIVE_W     640.0
#define IGN_NATIVE_H     400.0
#define IGN_NATIVE_ASPECT (4.0 / 3.0)

/*
 * How much narrower than tall one of the game's pixels was.  Everything that
 * mixes an X quantity with a Y one has this baked into it, and on a
 * square-pixel panel it has to come back out.
 */
#define IGN_PAR          (IGN_NATIVE_ASPECT / (IGN_NATIVE_W / IGN_NATIVE_H))

/* The per-view focal lengths the screen init hardcodes, in 640x400 pixels. */
#define IGN_FOCAL_X      425.0
#define IGN_FOCAL_Y      348.0

//
// ==========================================================================
// TWO BUILDS
// ==========================================================================
//
// Both the "3dfx patch ver3" and "ver2" executables are supported.  They are
// the same program a year apart -- every routine below was located in ver2
// structurally, never by searching for ver3's bytes, because the compiler
// scheduled them differently and almost nothing matches literally.
//
//   ver3   626,688 bytes  1998-12-29  .text 0x68EEC   "Ignition version 6.53"
//   ver2   643,072 bytes  1997-11-25  .text 0x6BB7C
//
// The build is identified by asking the image itself: each build's signatures
// occur exactly once in its own .text and NOT AT ALL in the other's, verified
// for all 21 of them, so finding one names the build outright.
//
// It was originally selected on the code size instead, and that was a mistake
// worth recording: GetCodeRange returns OptionalHeader.SizeOfCode, which is the
// alignment-rounded RAW size (0x69000 / 0x6BC00), not the section's virtual
// size (0x68EEC / 0x6BB7C) that a disassembler prints.  The table held the
// latter, nothing matched, and the game ran completely unpatched.  A byte
// pattern cannot be the wrong quantity in the way a header field can.
//
// Two differences worth knowing, because they are not cosmetic:
//
//  * ver2 needs one fewer screen-size site.  Where ver3 stores 400 as an
//    immediate in two separate instructions, ver2 holds 640 and 400 in edi and
//    esi across the whole routine and stores from the registers -- to the clip
//    window first, then to the screen globals after the set-mode call.  Both
//    registers are callee-saved and nothing between touches them, so two
//    immediates cover all four stores and `sizeGlobal` is NULL for ver2.
//
//  * ver2's first loading-screen loop counts in ecx where ver3 counts in eax,
//    so the replacement `mov` opcode differs per build, and `imul ecx` is
//    common enough that ver2's signature needs one extra byte to be unique.
//
typedef struct {
    const char   *name;

    unsigned int  fbAddr;           /* the static 8bpp overlay buffer   */
    unsigned int  fbSites;          /* how many times .text names it    */
    unsigned int  k240;             /* the double 240.0 in .rdata       */

    /* Read at runtime by the present hook. */
    unsigned int  rvaVtBlit, rvaVtLock, rvaVtUnlock;
    unsigned int  rvaScreenA, rvaScreenB, rvaPalette;
    unsigned int  rvaScreenW, rvaScreenH;

    const unsigned char *sizeMain;    unsigned int sizeMainLen;
    int           sizeMainW,  sizeMainH;
    const unsigned char *sizeGlobal;  unsigned int sizeGlobalLen;   /* may be NULL */
    int           sizeGlobalH;
    const unsigned char *sizeClip;    unsigned int sizeClipLen;
    int           sizeClipW,  sizeClipH;

    const unsigned char *focal;       unsigned int focalLen;
    int           focalX,     focalY;

    const unsigned char *sprite;      unsigned int spriteLen;
    int           spriteDisp, spriteDiv;
    const unsigned char *menu;        unsigned int menuLen;
    int           menuDisp,   menuDiv;

    const unsigned char *wipe;        unsigned int wipeLen;

    const unsigned char *present;     unsigned int presentLen;
    int           presentCallAt;

    const unsigned char *loopRead;    unsigned int loopReadLen;
    unsigned char loopReadMov;
    const unsigned char *loopWrite;   unsigned int loopWriteLen;
    unsigned char loopWriteMov;

    const unsigned char *loadCall;    unsigned int loadCallLen;
    int           loadCallAt;
} IgnBuild;

/* Selected in IgnitionApply, before anything is patched. */
static const IgnBuild *g_ign = NULL;

//
// F1.  The 8bpp overlay buffer is a fixed-size static array.
//
// Ignition's 2D layer -- HUD, text, menus, the loading screen -- is
// software-rendered into an 8bpp buffer and colour-key blitted into the Glide
// LFB through a 16-bit palette (the blit is at 0x458780; note its
// `or %al,%al ; je skip`, which is what makes it an overlay rather than a
// background).  That buffer is not allocated: it is the static array at
// 0x547CA0 (ver2: 0x54AFE0), and the engine memsets W*H bytes of it every
// time the screen size changes.  640x400 is 256,000 bytes; the next
// referenced global leaves room for 487,488 (ver2: 476,192) and no more.
// 2560x1080 wants 2,764,800.
//
// So the buffer has to move before the screen size may be raised, and if it
// cannot move then nothing else may be applied either -- a larger W*H over the
// old array would memset straight through the rest of .data.
//
// Moving it is mechanical and, unusually for this kind of edit, provably safe.
// The address occurs exactly 64 times in ver3's .text and 63 times in ver2's,
// and disassembling the whole section shows every one is a genuine
// instruction operand: immediates of push/mov/add/sub, and disp32s of forms
// like `mov 0x547ca0(%esi,%ecx,1),%dl`.  Not one is a mid-instruction
// coincidence, and the value appears in no other section.  A flat
// scan-and-replace is therefore exactly equivalent to rewriting the operands
// one at a time, and the count doubles as a second build check on top of the
// .text size.
//
static unsigned char *g_ignFrameBuffer = NULL;

static BOOL InstallIgnFrameBuffer(unsigned char *code, unsigned int codeSize)
{
    unsigned char find[4], repl[4];
    unsigned int  i, count, need;

    if (g_ignFrameBuffer) return TRUE;          // already relocated

    PutU32(find, g_ign->fbAddr);

    count = 0;
    for (i = 0; i + 4 <= codeSize; ) {
        if (memcmp(code + i, find, 4) == 0) { count++; i += 4; }
        else                                          i += 1;
    }
    if (count != g_ign->fbSites) return FALSE;  // not the build we think

    //
    // One spare page.  The engine's clears round W*H up to a multiple of four
    // and its row loops are written in terms of the surface pitch, so a little
    // slack costs nothing and removes a class of off-by-a-few.
    //
    need = g_targetW * g_targetH + 0x1000;

    g_ignFrameBuffer = (unsigned char *)VirtualAlloc(NULL, need,
                                                     MEM_COMMIT | MEM_RESERVE,
                                                     PAGE_READWRITE);
    if (!g_ignFrameBuffer) return FALSE;

    memset(g_ignFrameBuffer, 0, need);
    PutU32(repl, (unsigned int)g_ignFrameBuffer);

    for (i = 0; i + 4 <= codeSize; ) {
        if (memcmp(code + i, find, 4) == 0) {
            WriteCode(code + i, repl, 4);
            i += 4;
        } else {
            i += 1;
        }
    }
    return TRUE;
}

//
// F2.  The screen size.
//
// 0x43C8A0 is the 3dfx screen init.  It writes the Glide clip window
// (0x48EB38 / 0x48EB3C), calls the device set-mode -- which is literally
// grBufferClear twice plus grClipWindow(0,0,W,H) -- then sets the two screen
// globals and re-creates the 8bpp surface at W x H.
//
// The loader/intro path (0x411F00, and the 320x200 pixel-doubler at 0x412580)
// is deliberately left alone.  It writes into the overlay buffer with a
// hardcoded 640 pitch, so widening it there would smear the loading screen
// rather than scale it.  It runs strictly before 0x43C8A0 -- the two are
// consecutive states of the one machine at 0x445680 -- so the 640x400
// transient is confined to start-up.
//
// 0x412510 is patched even so.  All it does is set the clip window and clear,
// and the full screen is never a worse answer than the top-left 640x400 of it.
//
static const unsigned char ign_v3_size_main[] =           /* 0x43C8AB */
    "\xbf\x80\x02\x00\x00\xbd\x01\x00\x00\x00"
    "\x89\x3d\x38\xeb\x48\x00\xc7\x05\x3c\xeb"
    "\x48\x00\x90\x01\x00\x00";

static const unsigned char ign_v2_size_main[] =           /* 0x43D356 */
    "\xbf\x80\x02\x00\x00\xbe\x90\x01\x00\x00"
    "\xbb\x01\x00\x00\x00\x89\x3d\xc8\xfb\x48"
    "\x00\x89\x35\xcc\xfb\x48\x00\x89\x1d\xd4"
    "\xfb\x48\x00";

static const unsigned char ign_v3_size_global[] =         /* 0x43C907 */
    "\x89\x3d\xec\x6e\x53\x00\xc7\x05\x18\x7b"
    "\x54\x00\x90\x01\x00\x00";

static const unsigned char ign_v3_size_clip[] =           /* 0x412510 */
    "\xc7\x05\x38\xeb\x48\x00\x80\x02\x00\x00"
    "\xc7\x05\x3c\xeb\x48\x00\x90\x01\x00\x00"
    "\xc7\x05\x40\xeb\x48\x00\x08\x00\x00\x00"
    "\xc7\x05\x44\xeb\x48\x00\x01\x00\x00\x00"
    "\xe8\x43\x1e\x04\x00";

static const unsigned char ign_v2_size_clip[] =           /* 0x4128A0 */
    "\xc7\x05\xc8\xfb\x48\x00\x80\x02\x00\x00"
    "\xc7\x05\xcc\xfb\x48\x00\x90\x01\x00\x00"
    "\xc7\x05\xd0\xfb\x48\x00\x08\x00\x00\x00"
    "\xc7\x05\xd4\xfb\x48\x00\x01\x00\x00\x00"
    "\xe8\x03\x2c\x04\x00";

static void IgnPatchSize(unsigned char *code, unsigned int codeSize,
                         const unsigned char *sig, unsigned int len,
                         int wAt, int hAt)
{
    unsigned char  repl[64];
    unsigned char *at;

    at = FindUnique(code, codeSize, sig, len);
    if (!at) return;                    // already applied, or not this build

    memcpy(repl, sig, len);
    if (wAt >= 0) PutU32(repl + wAt, g_targetW);
    if (hAt >= 0) PutU32(repl + hAt, g_targetH);

    WriteCode(at, repl, len);
}

static void InstallIgnScreenSize(unsigned char *code, unsigned int codeSize)
{
    /* sizeof-1 throughout: these are string literals, so drop the NUL. */
    IgnPatchSize(code, codeSize, g_ign->sizeMain, g_ign->sizeMainLen,
                 g_ign->sizeMainW, g_ign->sizeMainH);

    /* ver2 has no second site: it stores both globals from esi/edi. */
    if (g_ign->sizeGlobal)
        IgnPatchSize(code, codeSize, g_ign->sizeGlobal, g_ign->sizeGlobalLen,
                     -1, g_ign->sizeGlobalH);

    IgnPatchSize(code, codeSize, g_ign->sizeClip, g_ign->sizeClipLen,
                 g_ign->sizeClipW, g_ign->sizeClipH);
}

//
// F3.  The in-game camera's focal lengths.
//
// The projection is integer 8.8 fixed point around a camera struct
// ([0x620368]) whose fields are focal-X (+0x80), focal-Y (+0x84), centre-X
// (+0x9C) and centre-Y (+0xA0).  That reading is not inferred from the names
// of anything: the frustum-plane builder at 0x44BE6F forms
// (clipEdge - centre) against each focal, once per edge, and 0x44C53E caches
// the two centres as `<< 8` into 0x4A24C4 / 0x4A24C8.
//
// The centres come from 0x44AB30 as W/2 and H/2, so they follow F2 for free.
// The focal lengths do not: 0x43C8A0 hardcodes 425 and 348 into every view
// (0x5BEEE4[i] + 0x58 / 0x5C), from where 0x41EE54 copies them into the camera
// on each update.  Both are pixel counts for 640x400.
//
//     fy = 348 * H/400        -- so the vertical FOV comes out independent of H
//     fx = fy * (425/348) * IGN_PAR
//
// Taking fy straight from the height is what makes this Hor+: the vertical
// view is exactly what it always was, and a wider screen becomes more world at
// the sides rather than a stretch.  On a square-pixel panel fx lands 1.8%
// above fy, which is the engine's own small deviation from a true 1.2 and is
// preserved rather than rounded away.
//
static const unsigned char ign_v3_focal[] =               /* 0x43C91B */
    "\xbf\xa9\x01\x00\x00\xba\x00\x00\x24\x40"
    "\x8b\x2d\xe4\xee\x5b\x00\x41\x89\x7c\x28"
    "\x58\x8b\x2d\xe4\xee\x5b\x00\xc7\x44\x28"
    "\x5c\x5c\x01\x00\x00";

static const unsigned char ign_v2_focal[] =               /* 0x43D3BA */
    "\xbf\xa9\x01\x00\x00\xbe\x5c\x01\x00\x00"
    "\xba\x00\x00\x24\x40\x8b\x1d\x24\x22\x5c"
    "\x00\x41\x89\x7c\x03\x58";

static void InstallIgnFocal(unsigned char *code, unsigned int codeSize)
{
    unsigned char  repl[64];
    unsigned char *at;
    double         scale;
    unsigned int   fx, fy;

    at = FindUnique(code, codeSize, g_ign->focal, g_ign->focalLen);
    if (!at) return;

    scale = (double)g_targetH / IGN_NATIVE_H;
    fy    = (unsigned int)(IGN_FOCAL_Y * scale + 0.5);
    fx    = (unsigned int)(IGN_FOCAL_X * scale * IGN_PAR + 0.5);

    memcpy(repl, g_ign->focal, g_ign->focalLen);
    PutU32(repl + g_ign->focalX, fx);
    PutU32(repl + g_ign->focalY, fy);

    WriteCode(at, repl, g_ign->focalLen);
}

//
// F4.  The two remaining aspect divisors.
//
// Both compute an X quantity from the screen WIDTH against a 320-unit virtual
// space, while the Y counterpart three instructions later uses the HEIGHT
// against 200:
//
//   0x43CAC3   sprite / particle scale, 16.16 pixels per virtual unit,
//              into 0x536C50 (X) and 0x536E3C (Y)
//   0x44AB43   the menu and car-preview camera's focal-X, into camera +0x80
//
// On a square-pixel screen the X one should come from the height as well, over
// 320 * IGN_PAR = 240 -- and 240.0 already exists in .rdata at 0x46A568, one
// instruction away from the 320.0 being replaced.  So each site is a two-field
// edit: change the esp displacement so the height is loaded instead of the
// width, and repoint the divide at 240.0.  Same length, nothing relocates, and
// no new constant has to be found room for.
//
// Neither depends on the target resolution -- they express "pixels are square"
// and nothing else.  320.0 at 0x46A850 has exactly two references image-wide
// and these are both of them, so afterwards it is unreferenced.
//
static const unsigned char ign_v3_sprite[] =              /* 0x43CAC3 */
    "\xdd\x44\x24\x10\xdc\x0d\x58\xa8\x46\x00"
    "\xa3\x30\x6e\x53\x00\xdc\x35\x50\xa8\x46"
    "\x00\xe8\xb7\x1a\x02\x00";

static const unsigned char ign_v2_sprite[] =              /* 0x43D563 */
    "\xdd\x44\x24\x10\xdc\x0d\x20\xd9\x46\x00"
    "\xa3\x70\xa1\x53\x00\xdc\x35\x28\xd9\x46"
    "\x00\xe8\x83\x23\x02\x00";

static const unsigned char ign_v3_menu[] =                /* 0x44AB43 */
    "\xd9\x44\x24\x00\xdc\x35\x50\xa8\x46\x00"
    "\xdd\x5c\x24\x04\xdd\x44\x24\x04\xdc\x0d"
    "\xf8\xa8\x46\x00";

static const unsigned char ign_v2_menu[] =                /* 0x44B610 */
    "\x83\xec\x08\xdb\x44\x24\x10\xdb\x44\x24"
    "\x0c\xd9\x5c\x24\x10\xd9\x44\x24\x10\xdc"
    "\x35\xc0\xdb\x46\x00";

static void InstallIgnAspect(unsigned char *code, unsigned int codeSize)
{
    unsigned char  repl[64];
    unsigned char *at;

    at = FindUnique(code, codeSize, g_ign->sprite, g_ign->spriteLen);
    if (at) {
        memcpy(repl, g_ign->sprite, g_ign->spriteLen);
        repl[g_ign->spriteDisp] = 0x18;             /* the height slot */
        PutU32(repl + g_ign->spriteDiv, g_ign->k240);
        WriteCode(at, repl, g_ign->spriteLen);
    }

    at = FindUnique(code, codeSize, g_ign->menu, g_ign->menuLen);
    if (at) {
        memcpy(repl, g_ign->menu, g_ign->menuLen);
        repl[g_ign->menuDisp] = 0x10;               /* the height slot */
        PutU32(repl + g_ign->menuDiv, g_ign->k240);
        WriteCode(at, repl, g_ign->menuLen);
    }
}

//
// F5.  Skip the race-start wipe.  It is 640x400-only and cannot be widened.
//
// This is what crashed a few frames into the first successful 2560x1080 race,
// at 0x436DAA, reading 0x954 bytes past the end of a heap block.
//
// 0x436C10 draws a transition wipe: it locks the LFB and, for each screen row,
// paints black wherever a stencil byte still holds its initial 0x14.  The
// stencil is `malloc(0x4F1A0)` at 0x41C1E6 -- exactly 800 x 405 bytes -- and
// the track is drawn into it as value 5.  Its callers gate on a 0..100 float
// progress at 0x536D18 and on a countdown at 0x536E2C, so this runs only for
// the second or so of the race intro.  Nothing else depends on it: the
// countdown and the state change that follows it live in the caller.
//
// 800 is 640 plus an 80-pixel margin each side, and the routine centres the
// screen on it: stencil column = screenColumn + 400 - W/2.  That is exact at
// 640 and holds for any width up to 800.  At 2560 it starts at -880 and runs
// to +1680.  The row index is no better -- it is scaled by W/320 AND again by
// W/H, both of which were 640x400 constants in disguise, and it is bounded
// against W/2+4 rather than against the stencil's 405 rows.
//
// The give-away is the twin routine at 0x406450, which does the identical job
// with every one of those quantities written out as a literal: `cmp $0x280`
// for the column loop, `0x50` for the margin, `cmp $0x144` for the row bound,
// and `640.0 / 400.0` where 0x436C10 computes W/H.  So W/320 was meant to read
// "the resolution multiplier, 2" and W/H "the aspect, 1.6".
//
// Correcting those scales is not enough, because the stencil has no data
// outside 800 columns -- widening the effect means regenerating the stencil at
// the new size, and the code that fills it is hardcoded to 800 as well.  The
// alternatives were to clamp it, which paints a 640x400 black rectangle into
// the corner of the screen for a second, or to drop it.  Dropping a transient
// effect that cannot be made correct is the smaller lie.
//
// One byte: `ret` over the prologue.  The callers are cdecl (`add $0x4,%esp`)
// and neither looks at the return value, and the grLfbLock/grLfbUnlock pair
// this function owns is skipped as a pair.
//
static const unsigned char ign_v3_wipe[] =                /* 0x436C10 */
    "\x55\x8b\xec\x83\xe4\xf8\x83\xec\x24\x8b"
    "\x0d\xec\x6e\x53\x00\xb8";

static const unsigned char ign_v2_wipe[] =                /* 0x437600 */
    "\x55\x8b\xec\x83\xe4\xf8\x83\xec\x24\x8b"
    "\x0d\x2c\xa2\x53\x00\xb8";

static void InstallIgnWipe(unsigned char *code, unsigned int codeSize)
{
    unsigned char  repl[64];
    unsigned char *at;

    at = FindUnique(code, codeSize, g_ign->wipe, g_ign->wipeLen);
    if (!at) return;                    // already applied, or not this build

    memcpy(repl, g_ign->wipe, g_ign->wipeLen);
    repl[0] = 0xC3;                     // ret

    WriteCode(at, repl, g_ign->wipeLen);
}

//
// F6.  Scale the 2D layer up when it is smaller than the screen.
//
// The menu, the loading screen and the attract-mode race behind the menu all
// run before 0x43C8A0 has switched the screen size, so they render into a
// 640x400 corner of a much larger framebuffer.  The artwork is fixed-size and
// the loader writes it with a hardcoded 640 pitch, so the layout itself cannot
// be widened -- but the finished picture can be scaled on its way out.
//
// Everything 2D reaches the screen through one place.  0x454310 is a nine
// argument pass-through to the device blit,
//
//     Blit(src, srcPitch, srcX, srcY, w, h, dstSurface, dstX, dstY)
//
// and its `call *0x4F3348` is the single site every surface-to-surface copy
// goes through -- menu art into the 8bpp overlay, and the overlay out to the
// Glide LFB.  Replacing that one call with our own lets us take the second
// case and hand back the first untouched.
//
// The guard is deliberately narrow: we act only when the destination is one of
// the two screen surfaces AND the source rectangle is smaller than the real
// screen.  In a race the game passes the full W x H, so the test fails and the
// game's own blit runs exactly as before -- this cannot touch the HUD.
//
// Aspect is preserved rather than stretched to fill.  640x400 is 1.6 and the
// screen is 2.37 at 2560x1080, so stretching would make everything 48% too
// wide; scaling to fit gives 1728x1080 with pillarbox bars, which are cleared
// to black so nothing stale shows at the edges.  Transparent source pixels
// (index 0) are still skipped inside the image, exactly as the game's blit
// does, so anything drawn underneath still shows through.
//
/*
 * The vtable slots, the two screen surfaces, the palette and the screen-size
 * globals all move between builds, so they live in the build table.  ver2's
 * whole device vtable sits exactly +0x30F8 from ver3's, which is how the
 * lock/unlock slots were found -- and then confirmed independently, by reading
 * back what each slot is initialised with.
 */

/* Fields of the surface descriptor, read off 0x458A60 and 0x458780. */
#define IGN_SURF_LOCKTYPE   0x0C        /* 0 = none, 1 = read, 3 = write */
#define IGN_SURF_WRITEPTR   0x14        /* LFB pointer while write-locked */
#define IGN_SURF_STRIDE     0x18        /* strideInBytes, from grLfbLock  */

#define IGN_LOCK_WRITE      3

typedef int (*IgnBlitFn)(const unsigned char *, int, int, int, int, int,
                         void *, int, int);
typedef int (*IgnLockFn)(void *, int);
typedef int (*IgnUnlockFn)(void *);

static unsigned int g_ignImage = 0;     /* the game's module base */

static unsigned int IgnField(void *surf, unsigned int off)
{
    return ReadU32((const unsigned char *)surf + off);
}


//
// Not static, and marked used/noinline, for the same reason as TurokFixRect2D:
// its only reference is the rel32 written into the game's code, so it has to
// keep a plain cdecl frame that nothing is allowed to specialise away.
//
extern "C" int __attribute__((cdecl, used, noinline))
IgnPresentScaled(const unsigned char *src, int pitch, int sx, int sy,
                 int w, int h, void *dst, int dx, int dy)
{
    IgnBlitFn             orig;
    IgnLockFn             lock;
    IgnUnlockFn           unlock;
    const unsigned short *pal;
    const unsigned char  *srcTop;
    unsigned char        *base;
    unsigned int          stride, prevLock;
    int                   scale, outW, outH, ox, oy, stepX, stepY, x, y;
    int                   composite;
    int                   screenW = (int)g_targetW;
    int                   screenH = (int)g_targetH;

    // Unreachable -- the hook is written only after g_ignImage is set -- but
    // the alternative to checking is a wild read at 0x000F3348.
    if (!g_ignImage || !g_ign) return 0;

    orig   = *(IgnBlitFn *)  (g_ignImage + g_ign->rvaVtBlit);
    lock   = *(IgnLockFn *)  (g_ignImage + g_ign->rvaVtLock);
    unlock = *(IgnUnlockFn *)(g_ignImage + g_ign->rvaVtUnlock);

    // (The loading screen arrives here as a plain 640x400 source, because F8
    // puts the screen globals back to 640x400 for the duration of the load.)

    //
    // Not ours: hand it straight back.  Note this also covers the case where
    // the vtable is not populated yet, since `orig` is then NULL and the game
    // would have crashed on the original instruction too.
    //
    //
    // F10.  Widths that are not a multiple of 160.
    //
    // The game's own blit has two paths.  The good one converts 8bpp indices
    // through the palette and skips index 0, so the overlay composites over
    // the 3D; it is chosen only when the width divides by 160 AND the pointers
    // are 4-aligned.  Otherwise it falls through to a plain rep movsb/movsd
    // memory copy -- no palette, no colour key, no 8-to-16bpp conversion.
    //
    // That raw path copies `width` BYTES into a row that is `2*width` bytes
    // wide, and advances the destination by twice the stride per row, so it
    // covers the left half of every other line; and since the overlay is
    // mostly index 0, what lands there is zeros.  Measured on a 2096x900
    // screenshot: left of x=1048 the even rows are pure black (mean 0) and the
    // odd rows normal, with the HUD sitting on top undamaged.
    //
    // It is a latent bug in the game, not a regression: 640 and 800 are both
    // multiples of 160, so the raw path could never run for a screen blit at
    // any resolution Ignition shipped with.  Only an override picks a width
    // that is not -- 1920 and 2560 divide by 160 and are fine, 2096 and 2304
    // do not and are broken.
    //
    // So when the game would take the raw path, do the blit here instead: the
    // scaler below degenerates to 1:1 when the source already fills the
    // screen, and `composite` makes it skip transparent pixels rather than
    // writing black, which is what the overlay needs over live 3D.  At widths
    // the game handles correctly it still runs its own hand-tuned loop.
    //
    composite = (w >= screenW && h >= screenH);

    if (!dst || !orig || w <= 0 || h <= 0 ||
        ((unsigned int)dst != g_ignImage + g_ign->rvaScreenA &&
         (unsigned int)dst != g_ignImage + g_ign->rvaScreenB) ||
        (composite && (w % 160) == 0))
        return orig ? orig(src, pitch, sx, sy, w, h, dst, dx, dy) : 0;

    /* Largest scale that fits, 16.16. */
    scale = (screenW << 16) / w;
    y     = (screenH << 16) / h;
    if (y < scale) scale = y;

    outW = (int)(((unsigned int)w * (unsigned int)scale) >> 16);
    outH = (int)(((unsigned int)h * (unsigned int)scale) >> 16);
    if (outW <= 0 || outH <= 0 || outW > screenW || outH > screenH)
        return orig(src, pitch, sx, sy, w, h, dst, dx, dy);

    ox = (screenW - outW) / 2;
    oy = (screenH - outH) / 2;

    stepX = (w << 16) / outW;
    stepY = (h << 16) / outH;

    //
    // Take a write lock the same way the game's own blit does, and put the
    // previous state back afterwards.  Leaving the surface locked differently
    // from how we found it would strand the caller.
    //
    prevLock = IgnField(dst, IGN_SURF_LOCKTYPE);
    if (prevLock != IGN_LOCK_WRITE) {
        if (prevLock && unlock) unlock(dst);
        if (!lock || !lock(dst, IGN_LOCK_WRITE)) return 0;
    }

    base   = (unsigned char *)IgnField(dst, IGN_SURF_WRITEPTR);
    stride = IgnField(dst, IGN_SURF_STRIDE);

    if (base && stride) {
        pal    = (const unsigned short *)(g_ignImage + g_ign->rvaPalette);
        srcTop = src + (unsigned int)pitch * (unsigned int)sy + sx;

        for (y = 0; y < screenH; y++) {
            unsigned short      *row = (unsigned short *)(base + stride * (unsigned int)y);
            const unsigned char *srow;
            int                  acc;

            if (y < oy || y >= oy + outH) {          /* top / bottom bar */
                if (!composite)
                    for (x = 0; x < screenW; x++) row[x] = 0;
                continue;
            }

            srow = srcTop + (unsigned int)pitch
                          * (unsigned int)((((y - oy) * stepY) >> 16));

            if (!composite)
                for (x = 0; x < ox; x++) row[x] = 0;            /* left bar */

            acc = 0;
            if (composite) {
                //
                // Over live 3D: transparent means leave the pixel alone.  This
                // is the game's own rule -- its good path is `or al,al ; je` --
                // and writing 0 here instead would black out the world.
                //
                // This one runs every frame of a race, so the 1:1 case (which
                // is every case here, the source already filling the screen)
                // drops the 16.16 stepping and indexes directly.
                //
                if (stepX == 0x10000) {
                    for (x = 0; x < outW; x++) {
                        unsigned char b = srow[x];
                        if (b) row[ox + x] = pal[b];
                    }
                } else {
                    for (x = 0; x < outW; x++) {
                        unsigned char b = srow[acc >> 16];
                        if (b) row[ox + x] = pal[b];
                        acc += stepX;
                    }
                }
            } else {
                for (x = 0; x < outW; x++) {
                    unsigned char b = srow[acc >> 16];
                    row[ox + x] = b ? pal[b] : 0;
                    acc += stepX;
                }
            }

            if (!composite)
                for (x = ox + outW; x < screenW; x++) row[x] = 0;  /* right bar */
        }
    }

    if (prevLock != IGN_LOCK_WRITE) {
        if (unlock) unlock(dst);
        if (prevLock && lock) lock(dst, (int)prevLock);
    }
    return 1;
}

//
// The hook itself.  `call *0x4F3348` is six bytes, `call rel32` is five, so it
// is replaced in place with one nop of padding -- no code cave, and the
// caller's `add $0x24,%esp` still balances because our function is cdecl too.
//
static const unsigned char ign_v3_present[] =             /* 0x454336 */
    "\x8b\x54\x24\x1c\x50\x51\x52\xff\x15\x48"
    "\x33\x4f\x00\x83\xc4\x24\xc3";

static const unsigned char ign_v2_present[] =             /* 0x455486 */
    "\x8b\x54\x24\x1c\x50\x51\x52\xff\x15\x40"
    "\x64\x4f\x00\x83\xc4\x24\xc3";

static void InstallIgnPresent(unsigned char *code, unsigned int codeSize,
                              unsigned int imageBase)
{
    unsigned char  repl[64];
    unsigned char *at;
    int            rel, callAt;

    at = FindUnique(code, codeSize, g_ign->present, g_ign->presentLen);
    if (!at) return;                    // already applied, or not this build

    g_ignImage = imageBase;
    callAt     = g_ign->presentCallAt;

    memcpy(repl, g_ign->present, g_ign->presentLen);

    rel = (int)((unsigned char *)&IgnPresentScaled - (at + callAt + 5));

    repl[callAt] = 0xE8;
    PutU32(repl + callAt + 1, (unsigned int)rel);
    repl[callAt + 5] = 0x90;            // pad the sixth byte

    WriteCode(at, repl, g_ign->presentLen);
}

//
// F7.  The other screen-sized buffer.
//
// This is the second-race crash, and it is ours: the first race is fine, the
// second always dies, in championship mode too -- so it is the second run of
// the load path, not the menu.
//
// 0x412580 is the loading-screen builder (the 320x200 pixel-doubler).  It ends
// with two loops over the whole screen, both counting W*H out of the live
// globals:
//
//   0x4127A5   overlay[i] = table[src[i]][overlay[i]]      (reads src)
//   0x4127DC   src[i]     = constant                       (writes src)
//
// where `src` is the buffer whose pointer lives at 0x547C40.  That buffer is a
// single allocation of 0x77240 = 487,488 bytes, made once at 0x412AE1 -- which
// is 800x600 rounded up, i.e. the engine's largest stock mode.  So the game's
// own invariant is `W*H <= 487,488`, and it holds for every resolution
// Ignition ever shipped with.
//
// F2 breaks it.  At start-up the loader has just set the globals to 640x400,
// so the first pass counts 256,000 and fits.  On the second race the globals
// are still W x H from the first one -- the standing note that the 640x400
// transient is "confined to start-up" is simply wrong, because the loader runs
// again for every race -- so the count becomes 1,536,000 at 1920x800 and both
// loops run off the end.  The read faulted first, 4KB past the block at the
// first unmapped page, which is exactly what the crash log showed:
//
//   at=004127b1 exe+000127b1   read at 01bc8000   fb+00b08000
//   eax=00177000 (= 1920*800)  live W=1920 H=800  clip=640x400
//
// GROWING THAT ALLOCATION WAS THE WRONG FIX, AND IT IS RECORDED HERE BECAUSE
// THE REASON IS NOT OBVIOUS.  Enlarging the 0x77240 request to W*H did stop
// this fault -- and moved it four instructions along, to 0x4127CA, with the
// table pointer 0x5C3D60 reading NULL.  That pointer is allocated from the
// SAME arena (handle 0x536F78, at 0x412E05), so taking an extra megabyte for
// `src` exhausted the arena and the next allocation out of it failed.  The
// size constant being unique in the image said nothing about the pool it comes
// out of.  Do not try to grow it again.
//
// The right fix needs no allocation at all.  The first race already works, and
// what makes it work is that the loader has just set the globals to 640x400,
// so these loops count 640*400 = 256,000.  That is not a coincidence of the
// stock resolution -- it is the size of the thing being processed.  The
// loading screen is 640x400 artwork written at a hardcoded 640 pitch (the
// pixel-doubler this routine is built around), so 256,000 contiguous bytes is
// exactly the image, at any screen size.
//
// So both counts are pinned to 640x400, which reproduces the known-good first
// pass on every subsequent one, fits the stock buffer with 231,488 bytes to
// spare, and leaves every allocation exactly as the game made it.
//
// Only these two sites need it.  Of the nine W*H products in the image the
// other six that touch a buffer all feed `mov edi,0x547CA0` -- overlay memsets
// -- and the overlay is W*H+4096 by F1, so they are correct as they stand.
//
static const unsigned char ign_v3_loop_read[] =           /* 0x412785 */
    "\x0f\xaf\x05\xec\x6e\x53\x00";

static const unsigned char ign_v2_loop_read[] =           /* 0x412B31 */
    "\x0f\xaf\x0d\x2c\xa2\x53\x00\x33";
static const unsigned char ign_v3_loop_write[] =          /* 0x4127EC */
    "\x0f\xaf\x15\xec\x6e\x53\x00";

static const unsigned char ign_v2_loop_write[] =          /* 0x412B8E */
    "\x0f\xaf\x15\x2c\xa2\x53\x00";

/* imul <reg>,ds:W  ->  mov <reg>,imm32 + 2 nops.  Same 7 bytes. */

//
// The signature may be longer than the seven bytes being replaced -- ver2's
// first loop counts in ecx, and `imul ecx,ds:screenW` occurs nine times in
// that image, so it needs one trailing byte of context to be unique.  Only the
// leading seven are ever written.
//
static void IgnPinLoop(unsigned char *code, unsigned int codeSize,
                       const unsigned char *sig, unsigned int sigLen,
                       unsigned char movOp, unsigned int count)
{
    unsigned char  repl[7];
    unsigned char *at;

    at = FindUnique(code, codeSize, sig, sigLen);
    if (!at) return;                    // already applied, or not this build

    repl[0] = movOp;
    PutU32(repl + 1, count);
    repl[5] = 0x90;
    repl[6] = 0x90;

    WriteCode(at, repl, 7);
}

static void InstallIgnLoadLoops(unsigned char *code, unsigned int codeSize)
{
    unsigned int count = 640u * 400u;

    /* Cannot exceed the overlay, for a target smaller than the stock mode. */
    if (count > g_targetW * g_targetH) count = g_targetW * g_targetH;

    IgnPinLoop(code, codeSize, g_ign->loopRead,  g_ign->loopReadLen,
               g_ign->loopReadMov,  count);
    IgnPinLoop(code, codeSize, g_ign->loopWrite, g_ign->loopWriteLen,
               g_ign->loopWriteMov, count);
}

//
// F8.  The loading screen is a 640x400 screen, so say so.
//
// The screen globals 0x536EEC / 0x547B18 have exactly TWO writers in the whole
// image: the start-up loader 0x411F00, which sets 640x400 and runs once, and
// the race init 0x43C907, which F2 makes W x H.  Nothing ever puts them back.
//
// The per-load state handler is 0x412E70.  It is reached through the state
// table -- which is why nothing calls it directly, and why this file long
// recorded it as "dead"; in this exe "no direct callers" does not mean dead.
// It resets the Glide clip window to 640x400 and calls the loading-screen
// builder at 0x412EF2, but it leaves the screen globals alone.
//
// So the first load runs with globals 640x400 (start-up had just set them) and
// every later one runs with W x H, while the artwork is still written at a
// hardcoded 640 pitch.  Two things came out of that, and both were reported:
//
//   * the present takes its source rect AND pitch from those globals, so the
//     background was read at a 1920 stride and appeared as three squashed
//     copies across each row -- 1920/640 = 3;
//   * the loading indicator is drawn at the same stale pitch, so its rows land
//     three apart instead of one, which is the interlacing.
//
// Both are the one fault, so both get one fix: reproduce the first load.  The
// call at 0x412EF2 is redirected to a stub that sets the globals to 640x400
// and jumps to the builder, and the race init sets them back to W x H when the
// race actually starts -- which is the existing, already-correct behaviour.
//
// The stub writes two immediates and jumps.  `mov [mem],imm32` touches no
// register and no flag, and the jump keeps the caller's return address, so
// nothing else has to be preserved.
//
static const unsigned char ign_v3_load_call[] =           /* 0x412EEA */
    "\x33\xc0\x5f\x5e\x83\xc4\x70\xc3\xe8\x89"
    "\xf6\xff\xff\x68\x20\x6e\x53\x00";

static const unsigned char ign_v2_load_call[] =           /* 0x4132EE */
    "\x33\xc0\x5f\x5e\x83\xc4\x70\xc3\xe8\x15"
    "\xf6\xff\xff\x68\x60\xa1\x53\x00";

static unsigned char *g_ignLoadStub = NULL;

static void InstallIgnLoadState(unsigned char *code, unsigned int codeSize,
                                unsigned int imageBase)
{
    unsigned char  repl[64];
    unsigned char *at, *stub;
    unsigned int   builder;
    int            rel, callAt;

    at = FindUnique(code, codeSize, g_ign->loadCall, g_ign->loadCallLen);
    if (!at) return;                    // already applied, or not this build

    callAt = g_ign->loadCallAt;

    /* Where the original call was going. */
    builder = (unsigned int)(at + callAt + 5) + ReadU32(at + callAt + 1);

    stub = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;
    g_ignLoadStub = stub;

    /* mov dword [screenW], 640 */
    stub[0] = 0xC7; stub[1] = 0x05;
    PutU32(stub + 2, imageBase + g_ign->rvaScreenW);
    PutU32(stub + 6, 640);
    /* mov dword [screenH], 400 */
    stub[10] = 0xC7; stub[11] = 0x05;
    PutU32(stub + 12, imageBase + g_ign->rvaScreenH);
    PutU32(stub + 16, 400);
    /* jmp builder */
    stub[20] = 0xE9;
    PutU32(stub + 21, (unsigned int)(builder - ((unsigned int)stub + 25)));

    memcpy(repl, g_ign->loadCall, g_ign->loadCallLen);
    rel = (int)((unsigned int)stub - ((unsigned int)at + callAt + 5));
    PutU32(repl + callAt + 1, (unsigned int)rel);

    WriteCode(at, repl, g_ign->loadCallLen);
}

//
// The two supported builds.  `S(x)` pairs a signature with its length so the
// two can never drift apart.
//
#define S(x)   (x), (unsigned int)(sizeof(x) - 1)

static const IgnBuild ign_builds[] = {
  { "ver3 (1998-12-29)",
    0x547CA0u, 64, 0x46A568u,
    0x0F3348u, 0x0F3358u, 0x0F335Cu,        /* blit / lock / unlock  */
    0x0F2F40u, 0x0F2E50u, 0x21E520u,        /* screen A / B / palette */
    0x136EECu, 0x147B18u,                   /* screen W / H globals   */
    S(ign_v3_size_main),   1, 22,
    S(ign_v3_size_global),    12,
    S(ign_v3_size_clip),   6, 16,
    S(ign_v3_focal),       1, 31,
    S(ign_v3_sprite),      3, 17,
    S(ign_v3_menu),        3,  6,
    S(ign_v3_wipe),
    S(ign_v3_present),     7,
    S(ign_v3_loop_read),  0xB8,             /* mov eax,imm32 */
    S(ign_v3_loop_write), 0xBA,             /* mov edx,imm32 */
    S(ign_v3_load_call),   8 },

  { "ver2 (1997-11-25)",
    0x54AFE0u, 63, 0x46DBE0u,
    0x0F6440u, 0x0F6450u, 0x0F6454u,
    0x0F6038u, 0x0F5F48u, 0x221860u,
    0x13A22Cu, 0x14AE58u,
    S(ign_v2_size_main),   1,  6,
    NULL, 0,                  0,            /* no second size site   */
    S(ign_v2_size_clip),   6, 16,
    S(ign_v2_focal),       1,  6,
    S(ign_v2_sprite),      3, 17,
    S(ign_v2_menu),       10, 21,
    S(ign_v2_wipe),
    S(ign_v2_present),     7,
    S(ign_v2_loop_read),  0xB9,             /* mov ecx,imm32 */
    S(ign_v2_loop_write), 0xBA,
    S(ign_v2_load_call),   8 },
};

#undef S

//

// Everything Ignition needs.
//
// The order is load-bearing exactly once: F1 has to come first and, if it
// fails, everything else has to be skipped.  Raising the screen size over the
// original 256,000-byte overlay array would have the engine memset through the
// rest of .data on the first mode change.  Failing closed leaves the game
// running stock, which is the correct outcome for a binary this code does not
// recognise.
//
// No ini keys.  A game drawing into the corner of the screen, or drawing it
// with the wrong aspect, is not a preference.
//
static void IgnitionApply(const char *exePath)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0, i;

    if (!PathEndsWith(exePath, IGN_EXE)) return;

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    //
    // Identify the build from its own bytes.  An unrecognised image -- or one
    // already patched, since the probe pattern is among the things we rewrite
    // -- is left completely alone, which is the right answer in both cases.
    //
    g_ign = NULL;
    for (i = 0; i < COUNT(ign_builds); i++)
        if (FindUnique(code, codeSize,
                       ign_builds[i].wipe, ign_builds[i].wipeLen)) {
            g_ign = &ign_builds[i];
            break;
        }
    if (!g_ign) return;

    if (!InstallIgnFrameBuffer(code, codeSize)) return;

    InstallIgnLoadLoops(code, codeSize);
    InstallIgnScreenSize(code, codeSize);
    InstallIgnFocal(code, codeSize);
    InstallIgnAspect(code, codeSize);
    InstallIgnWipe(code, codeSize);
    InstallIgnLoadState(code, codeSize, (unsigned int)mod);
    InstallIgnPresent(code, codeSize, (unsigned int)mod);
}


// ==========================================================================
// MDK (1997, Shiny Entertainment) -- MDK3DFX.EXE
// ==========================================================================
//
// MDK shipped as a DOS/4GW game (MDK.EXE is an LE binary) with four separate
// Win32 renderers beside it.  Only MDK3DFX.EXE is touched here; it is the Glide
// one and it imports glide2x.dll directly, so this wrapper is in its call path
// and grGlideInit is a valid hook point.  MDK95 / MDKD3D / Mdka3d3d are a
// software rasteriser and two Direct3D builds and are never in ours.
//
// MDK3DFX.EXE is a plain unencrypted PE built with Watcom (sections AUTO /
// DGROUP / .bss rather than .text / .data), so everything below was read
// straight off the file -- no runtime dump, no protection to work around.
//
// Requirements 1 and 2 of the playbook are free, as they were for Turok and
// Ignition: the single grSstWinOpen (0x470EA8) passes a hardcoded push $0x7
// (GR_RESOLUTION_640x480), so there is no mode list to patch and the driver's
// override alone decides the real screen size.
//
// What is left is the projection, and MDK's screen model is unusual enough to
// state in full.  It does NOT render to the whole 640x480 frame: the 3D view is
// a 600x360 window at (20,60), leaving a black border.  The window's ORIGIN is
// a pair of globals (0x492BE0 / 0x492BE4) that the game moves; its SIZE is
// hardcoded as `origin + 600` / `origin + 360` immediates at every site that
// builds a grClipWindow call.
//
// That window holds the HUD as well as the world.  0x470F78 locks the LFB and
// hands back `lfbPtr + strideInBytes*originY + 2*originX`, so the entire 2D
// layer is drawn relative to the same origin -- and it takes the stride from
// grLfbLock at runtime, so unlike a fixed-pitch 2D layer there is no stride
// constant here that a resolution change would falsify.
//
// So three things, and they are independent:
//
//   K1  the 3D window becomes the whole screen:  size 600x360 -> W x H and
//       origin (20,60) -> (0,0), which makes every clip window (0,0,W,H)
//   K2  the projection's pixel extents, so the world spans that window
//   K3  the frustum aspect, so it is not stretched
//
// K1 alone gives a full-screen but horizontally STRETCHED picture; K3 is what
// turns it into widescreen.
//
// K2 and K3 are exact no-ops at the stock 600x360 -- they are written as ratios
// against it rather than as fresh values, so they reproduce the shipped
// constants bit for bit.  K1 is deliberately not: it removes MDK's border, and
// there is no size at which "the 3D window is the whole screen" and "the 3D
// window is 600x360 at (20,60)" are the same statement.  That is the intended
// change, not an oversight, and it is the reason none of this may run with the
// override disabled.
//
// The HUD is deliberately NOT addressed here.  The 2D layer clips itself to
// 600x360 in software (0x40A940) and its background blitter walks 600-pixel
// rows (0x46F52A), so with the origin at (0,0) it lands as a 600x360 island in
// the top-left corner.  That is the predicted outcome of this change, not a
// failure of it -- moving the island is a separate patch on 0x470F78, and
// mixing the two would make the first hardware result unattributable.
//
#define MDK_EXE          "mdk3dfx.exe"

//
// Every signature below carries absolute addresses in its instruction
// operands, and MDK3DFX.EXE -- unlike Turok.exe -- does have a .reloc section,
// so it is not structurally impossible for it to be rebased.  In practice the
// main image of a process is always placed at its preferred base, because
// nothing else is mapped yet.  Rather than rebuild 37 displacements at runtime
// for a case that cannot occur, the base is simply checked and the whole game
// skipped if it is ever wrong.
//
#define MDK_IMAGE_BASE   0x00400000u

/* The stock 3D window: what every constant below is being scaled away from. */
#define MDK_VIEW_W       600
#define MDK_HUD_LEFT_X   100   /* K15: the counter/staircase band */
#define MDK_VIEW_H       360

/* K9's entry point -- MDK's own clip+clear routine.  See the note above it. */
static void (*g_mdkClear)(void) = NULL;

// ---------------------------------------------------------------------------
// K1.  The 3D window becomes the whole screen
// ---------------------------------------------------------------------------
//
// Thirty-seven immediates in fifteen spans.  They are all one of five things:
//
//     600 / 601   the window width  (the +1 form is a max-corner)
//     360 / 361   the window height
//     20 / 60     the window origin, written to 0x492BE0 / 0x492BE4 and pushed
//                 as the min corner of the same grClipWindow call
//
// Setting the origin to (0,0) and the size to W x H makes every one of the
// twelve grClipWindow calls (0,0,W,H).  It also settles the 3D vertex path for
// free: 0x470074 and its siblings add `fild 0x492BE0` to each vertex's x, so
// with the origin at zero the projection's own output is used unmodified.
//
// Two consequences worth naming because they are load-bearing rather than
// incidental:
//
//   - grBufferClear is bounded by the clip window, so a full-screen clip is
//     also what stops an uncleared border appearing round the picture.
//   - 0x471D28 clears the strip exposed when the window moves, by comparing
//     0x492BE0 against its previous value 0x492BE8.  With the origin constant
//     that path simply never fires; its two immediates are patched anyway so
//     the module cannot end up half-converted.
//
// Each span was grown until it occurs exactly once in the code section, which
// is why several of them start mid-instruction-stream on a `call rel32`: the
// six-byte tails that actually carry the immediates repeat verbatim four times
// over, and only the neighbouring call displacement separates them.  Note the
// rel32s are relative and so survive rebasing even though the absolute
// displacements do not.
//
enum {
    MDK_W = 0,      /* screen width                                        */
    MDK_H,          /* screen height                                       */
    MDK_W1,         /* screen width  + 1                                   */
    MDK_H1,         /* screen height + 1                                   */
    MDK_ZERO,       /* the window origin                                   */
    MDK_SCALE_H     /* the signature's own value, scaled by H/360 (see K6) */
};

struct MdkField {
    unsigned short at;          /* byte offset of the imm32 within the span */
    unsigned short kind;
};

struct MdkSpan {
    const unsigned char *sig;
    unsigned int         len;
    const MdkField      *fields;
    unsigned int         fieldCount;
};

static const unsigned char mdk_win00[] =    /* 0x0046e385 */
    "\x8d\x8a\x68\x01\x00\x00\x89\x7d\xf4\xe8"
    "\x95\x39\x00\x00\x8b\x7d\xf4\x8b\x0d\xec"
    "\x2b\x49\x00\x81\xc7\x58\x02\x00\x00";
static const MdkField mdk_win00_f[] = { {2,MDK_H}, {25,MDK_W} };

static const unsigned char mdk_win01[] =    /* 0x0046e3e2 */
    "\x05\x68\x01\x00\x00\x50\x8b\x45\xf0\x05"
    "\x58\x02\x00\x00";
static const MdkField mdk_win01_f[] = { {1,MDK_H}, {10,MDK_W} };

static const unsigned char mdk_win02[] =    /* 0x0046e407 */
    "\x8d\x9a\x59\x02\x00\x00\x8d\x86\x58\x02"
    "\x00\x00\x8b\x15\xe4\x2b\x49\x00\x89\xdf"
    "\x8d\x8a\x68\x01\x00\x00";
static const MdkField mdk_win02_f[] = { {2,MDK_W1}, {8,MDK_W}, {22,MDK_H} };

static const unsigned char mdk_win03[] =    /* 0x0046e43c */
    "\x8d\x90\x68\x01\x00\x00\x81\xc1\x69\x01"
    "\x00\x00";
static const MdkField mdk_win03_f[] = { {2,MDK_H}, {8,MDK_H1} };

static const unsigned char mdk_win04[] =    /* 0x0046e46f */
    "\xba\x14\x00\x00\x00\xb9\x3c\x00\x00\x00";
static const MdkField mdk_win04_f[] = { {1,MDK_ZERO}, {6,MDK_ZERO} };

static const unsigned char mdk_win05[] =    /* 0x0046e49b */
    "\xe8\x6a\xb3\x01\x00\xa1\xe4\x2b\x49\x00"
    "\x05\x68\x01\x00\x00\x50\xa1\xe0\x2b\x49"
    "\x00\x05\x58\x02\x00\x00\x50\x8b\x1d\xe4"
    "\x2b\x49\x00";
static const MdkField mdk_win05_f[] = { {11,MDK_H}, {22,MDK_W} };

static const unsigned char mdk_win06[] =    /* 0x0046e4e2 */
    "\xba\x14\x00\x00\x00\x68\xe0\x01\x00\x00"
    "\xb9\x3c\x00\x00\x00\xa1\xe0\x2b\x49\x00"
    "\x68\x80\x02\x00\x00";
static const MdkField mdk_win06_f[] = { {1,MDK_ZERO}, {6,MDK_H}, {11,MDK_ZERO}, {21,MDK_W} };

static const unsigned char mdk_win07[] =    /* 0x0046e629 */
    "\x68\xa4\x01\x00\x00\xa3\xe8\x2b\x49\x00"
    "\xa1\xe4\x2b\x49\x00\x68\x6c\x02\x00\x00"
    "\xa3\xec\x2b\x49\x00\xb8\x3c\x00\x00\x00"
    "\x50\xbf\x14\x00\x00\x00";
static const MdkField mdk_win07_f[] = { {1,MDK_H}, {16,MDK_W}, {26,MDK_ZERO}, {32,MDK_ZERO} };

static const unsigned char mdk_win08[] =    /* 0x0046e66c */
    "\x52\x68\xe0\x01\x00\x00\x68\x80\x02\x00"
    "\x00\x6a\x00";
static const MdkField mdk_win08_f[] = { {2,MDK_H}, {7,MDK_W} };

static const unsigned char mdk_win09[] =    /* 0x0046e6a4 */
    "\x68\xa4\x01\x00\x00\xba\x14\x00\x00\x00"
    "\xa1\xe0\x2b\x49\x00\x68\x6c\x02\x00\x00"
    "\xb9\x3c\x00\x00\x00";
static const MdkField mdk_win09_f[] = { {1,MDK_H}, {6,MDK_ZERO}, {16,MDK_W}, {21,MDK_ZERO} };

static const unsigned char mdk_win10[] =    /* 0x0046e6eb */
    "\x53\x51\x52\x56\xa1\xe4\x2b\x49\x00\x05"
    "\x68\x01\x00\x00\x50\xa1\xe0\x2b\x49\x00"
    "\x05\x58\x02\x00\x00\x50\x8b\x15\xe4\x2b"
    "\x49\x00\x52\x8b\x0d\xe0\x2b\x49\x00\x51"
    "\xe8\xec\xb0\x01\x00";
static const MdkField mdk_win10_f[] = { {10,MDK_H}, {21,MDK_W} };

static const unsigned char mdk_win11[] =    /* 0x0046e71e */
    "\xe8\xdb\xb0\x01\x00\xa1\xe4\x2b\x49\x00"
    "\x05\x68\x01\x00\x00\x50\xa1\xe0\x2b\x49"
    "\x00\x05\x58\x02\x00\x00\x50\x8b\x1d\xe4"
    "\x2b\x49\x00";
static const MdkField mdk_win11_f[] = { {11,MDK_H}, {22,MDK_W} };

static const unsigned char mdk_win12[] =    /* 0x0046e75b */
    "\x53\x51\x52\x56\xa1\xe4\x2b\x49\x00\x05"
    "\x68\x01\x00\x00\x50\xa1\xe0\x2b\x49\x00"
    "\x05\x58\x02\x00\x00\x50\x8b\x15\xe4\x2b"
    "\x49\x00\x52\x8b\x0d\xe0\x2b\x49\x00\x51"
    "\x31\xdb";
static const MdkField mdk_win12_f[] = { {10,MDK_H}, {21,MDK_W} };

static const unsigned char mdk_win13[] =    /* 0x0046e7a9 */
    "\xe8\x50\xb0\x01\x00\xa1\xe4\x2b\x49\x00"
    "\x05\x68\x01\x00\x00\x50\xa1\xe0\x2b\x49"
    "\x00\x05\x58\x02\x00\x00\x50\x8b\x1d\xe4"
    "\x2b\x49\x00";
static const MdkField mdk_win13_f[] = { {11,MDK_H}, {22,MDK_W} };

static const unsigned char mdk_win14[] =    /* 0x00471d41 */
    "\xa1\xe4\x2b\x49\x00\x05\x68\x01\x00\x00"
    "\x50\xa1\xe0\x2b\x49\x00\x05\x58\x02\x00"
    "\x00\x50\x8b\x0d\xe4\x2b\x49\x00";
static const MdkField mdk_win14_f[] = { {6,MDK_H}, {17,MDK_W} };

static const MdkSpan mdk_window[] = {
    { mdk_win00, 29, mdk_win00_f, 2 },   /* 0x0046e385 */
    { mdk_win01, 14, mdk_win01_f, 2 },   /* 0x0046e3e2 */
    { mdk_win02, 26, mdk_win02_f, 3 },   /* 0x0046e407 */
    { mdk_win03, 12, mdk_win03_f, 2 },   /* 0x0046e43c */
    { mdk_win04, 10, mdk_win04_f, 2 },   /* 0x0046e46f */
    { mdk_win05, 33, mdk_win05_f, 2 },   /* 0x0046e49b */
    { mdk_win06, 25, mdk_win06_f, 4 },   /* 0x0046e4e2 */
    { mdk_win07, 36, mdk_win07_f, 4 },   /* 0x0046e629 */
    { mdk_win08, 13, mdk_win08_f, 2 },   /* 0x0046e66c */
    { mdk_win09, 25, mdk_win09_f, 4 },   /* 0x0046e6a4 */
    { mdk_win10, 45, mdk_win10_f, 2 },   /* 0x0046e6eb */
    { mdk_win11, 33, mdk_win11_f, 2 },   /* 0x0046e71e */
    { mdk_win12, 42, mdk_win12_f, 2 },   /* 0x0046e75b */
    { mdk_win13, 33, mdk_win13_f, 2 },   /* 0x0046e7a9 */
    { mdk_win14, 28, mdk_win14_f, 2 },   /* 0x00471d41 */
};
#define MDK_WINDOW_SPANS  (sizeof(mdk_window) / sizeof(mdk_window[0]))
#define MDK_SPAN_MAX      64        /* longest span in this section, rounded up */

// ---------------------------------------------------------------------------
// K2.  The projection's pixel extents
// ---------------------------------------------------------------------------
//
// MDK keeps five world->screen routines, one per viewport preset, and picks
// between them through a function pointer at 0x492B38 that 0x46CB30 sets:
//
//     mode 0 (default)  0x46C9F0   600x360 @ (0,0)   <- the game view
//     mode 1            0x46CA28   384x280 @ (108,80)
//     modes 2/3/4       0x46CA6C..  140x70 inset panels
//
// Each is a dozen instructions long and carries its own viewport rectangle as
// doubles in DGROUP.  Mode 0 is the whole of the main view:
//
//     screenX = (x/z + 1) * 299.95 + 0.05
//     screenY = (y/z + 1) * 180.4  + 0.05
//
// -- the clip-space frustum is |x| <= z and |y| <= z, so (coord/z + 1) runs
// 0..2 and the multiplier is the viewport's half extent.  (299.95 and 180.4
// rather than 300 and 180: the shipped game insets horizontally by a twentieth
// of a pixel and overscans vertically by just under one.  Scaling both by the
// same ratio preserves that exactly and makes the patch a true no-op at the
// stock size, rather than merely an inert one.)
//
// The other four routines are left alone.  They are inset panels drawn inside
// the main view during cutscenes; their rectangles stay valid, and their own
// aspect group (280 / (1/384), against the main view's 360 / (1/600)) is
// untouched by K3 for the same reason.
//
// Both constants are QWORD doubles referenced from exactly one instruction
// each, so they cannot be found by scanning code.  They are reached instead
// through the disp32s of the two `fmull`s inside the 54-byte routine, which
// occurs exactly once in the image.  Idempotency is by value: the code
// signature survives a change to the data it points at, so re-running would
// find the routine again and then decline on the constant.
//
static const unsigned char mdk_proj_sig[] =    /* 0x0046c9f0, mode 0 */
    "\x55\x89\xe5\xd9\x00\xd8\x40\x08\xd8\x70"
    "\x08\xdc\x0d\x84\xfc\x48\x00\xdd\x05\x8c"
    "\xfc\x48\x00\xd9\xc9\xd8\xc1\xd9\x58\x0c"
    "\xd9\x40\x04\xd8\x40\x08\xd8\x70\x08\xdc"
    "\x0d\x94\xfc\x48\x00\xde\xc1\xd9\x58\x10"
    "\x89\xec\x5d\xc3";
#define MDK_PROJ_KX_AT   13
#define MDK_PROJ_KY_AT   41
#define MDK_PROJ_KX      299.95     /* half width  of the stock 600x360 view */
#define MDK_PROJ_KY      180.4      /* half height                          */

// ---------------------------------------------------------------------------
// K3.  The frustum aspect
// ---------------------------------------------------------------------------
//
// The projection matrix is built from the camera's zoom (0x538D1C, 2.4 in
// normal play) and four constants:
//
//     xscale = 1 / (zoom * A)                 A = 0.5
//     yscale = 1 / (zoom * B * C * D)         B = 360.0, C = 1/600, D = 0.5
//
// so yscale/xscale is A/(B*C*D) = 600/360, the viewport aspect, exactly as a
// textbook m11/m00.  Rewriting A to `0.5 * (W*360) / (H*600)` -- that is,
// 0.3 * W/H -- makes it W/H instead, and does so without touching yscale: the
// vertical field of view is unchanged and the wider screen simply shows more to
// the sides.  Hor+, by construction rather than by tuning.  Written in that
// form rather than as 0.3*W/H so that it returns exactly 0.5 at the stock
// 600x360 and the patch is a genuine no-op there.
//
// xscale also feeds the frustum planes the game culls against (0x42DDCA and
// its siblings read 0x538DB4 four times over), so fixing the aspect at its
// source keeps matrix and culling consistent -- the alternative, widening only
// the renderer's side, is what makes geometry pop in and out at the screen
// edges (GAME-PATCHING.md section 23).
//
// There are four of these setups, one per camera mode, each with its own copy
// of the four constants in the pool.  All four were checked to carry the
// 600x360 group (B=360.0, C=1/600) rather than the 384x280 one, which is what
// identifies them as main-view cameras.  The signature is the same twenty-two
// bytes each time apart from the disp32 naming A, so it is built from a
// template and that one address.
//
static const unsigned char mdk_aspect_sig[] =  /* disp32 of A patched in @8 */
    "\xd9\x05\x1c\x8d\x53\x00\xd8\x0d\x00\x00"
    "\x00\x00\xd9\xe8\xde\xf1\xd9\x1d\xb4\x8d"
    "\x53\x00";
#define MDK_ASPECT_DISP_AT  8

static const unsigned int mdk_aspect_konst[] = {
    0x0048bf3cu, 0x0048dc54u, 0x0048dfe0u, 0x0048e098u
};
#define MDK_ASPECT_A  0.5f          /* the shipped value, at 600x360 */


//
// K1: rewrite the window.
//
// All fifteen spans are located before any of them is written.  A partial
// application would leave some clip windows full-screen and others still
// 600x360, which is a far more confusing thing to look at than no patch at
// all -- and, unlike a missing patch, not obviously wrong from a screenshot.
//
static BOOL InstallMdkWindow(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at[MDK_WINDOW_SPANS];
    unsigned char  repl[MDK_SPAN_MAX];
    unsigned int   i, j, found = 0;

    for (i = 0; i < MDK_WINDOW_SPANS; i++) {
        at[i] = FindUnique(code, codeSize, mdk_window[i].sig, mdk_window[i].len);
        if (at[i]) found++;
    }

    // Nothing found means already applied (or not this build); either way
    // there is nothing to do and nothing is wrong.
    if (found == 0) return TRUE;
    if (found != MDK_WINDOW_SPANS) return FALSE;

    // K9's entry point.  mdk_window[10] starts three bytes into 0x46E6E8, past
    // its `push %ebp; mov %esp,%ebp` -- so the span matching is also what
    // proves this is the routine.
    g_mdkClear = (void (*)(void))(at[10] - 3);

    for (i = 0; i < MDK_WINDOW_SPANS; i++) {
        const MdkSpan *s = &mdk_window[i];

        memcpy(repl, s->sig, s->len);
        for (j = 0; j < s->fieldCount; j++) {
            unsigned int v = 0;
            switch (s->fields[j].kind) {
            case MDK_W:  v = g_targetW;      break;
            case MDK_H:  v = g_targetH;      break;
            case MDK_W1: v = g_targetW + 1;  break;
            case MDK_H1: v = g_targetH + 1;  break;
            case MDK_ZERO: v = 0;            break;
            }
            PutU32(repl + s->fields[j].at, v);
        }
        WriteCode(at[i], repl, s->len);
    }
    return TRUE;
}

//
// Resolve a DGROUP constant through the disp32 of the instruction that reads
// it, and bounds-check the result against the image.  Same shape as Turok's
// equivalent; kept local so each game's block stays self-contained.
//
static unsigned char *MdkConstAt(unsigned char *code, unsigned int codeSize,
                                 const unsigned char *sig, unsigned int sigLen,
                                 unsigned int dispAt, unsigned int size)
{
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS *nt;
    unsigned char          *at;
    unsigned int            addr, imgBase, imgSize;
    HMODULE                 mod;

    at = FindUnique(code, codeSize, sig, sigLen);
    if (!at) return NULL;

    mod     = GetModuleHandleA(NULL);
    dos     = (const IMAGE_DOS_HEADER *)mod;
    nt      = (const IMAGE_NT_HEADERS *)((const unsigned char *)mod + dos->e_lfanew);
    imgBase = (unsigned int)mod;
    imgSize = (unsigned int)nt->OptionalHeader.SizeOfImage;

    addr = ReadU32(at + dispAt);
    if (addr < imgBase || addr + size > imgBase + imgSize) return NULL;
    return (unsigned char *)addr;
}

//
// Compared bit for bit rather than by value: these are x87 doubles read out of
// a game's data section, and a `==` against a literal invites the compiler to
// do the comparison at 80-bit precision.  memcmp says exactly what is meant.
//
static void MdkPatchDouble(unsigned char *konst, double from, double to)
{
    union { double d; unsigned char b[8]; } cur, want, expect;

    memcpy(cur.b, konst, 8);
    want.d   = to;
    expect.d = from;

    if (memcmp(cur.b, want.b,   8) == 0) return;    // already applied
    if (memcmp(cur.b, expect.b, 8) != 0) return;    // not the constant we expect

    WriteCode(konst, want.b, 8);
}

//
// K2: scale the mode-0 projection's half extents to the real screen.
//
static void InstallMdkProjection(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *kx, *ky;

    at = FindUnique(code, codeSize, mdk_proj_sig, sizeof(mdk_proj_sig) - 1);
    if (!at) return;

    kx = MdkConstAt(code, codeSize, mdk_proj_sig, sizeof(mdk_proj_sig) - 1,
                    MDK_PROJ_KX_AT, 8);
    ky = MdkConstAt(code, codeSize, mdk_proj_sig, sizeof(mdk_proj_sig) - 1,
                    MDK_PROJ_KY_AT, 8);
    if (!kx || !ky) return;

    MdkPatchDouble(kx, MDK_PROJ_KX,
                   MDK_PROJ_KX * (double)g_targetW / (double)MDK_VIEW_W);
    MdkPatchDouble(ky, MDK_PROJ_KY,
                   MDK_PROJ_KY * (double)g_targetH / (double)MDK_VIEW_H);
}

//
// K3: widen the frustum, for all four main-view camera setups.
//
static void InstallMdkAspect(unsigned char *code, unsigned int codeSize)
{
    unsigned char sig[sizeof(mdk_aspect_sig) - 1];
    unsigned int  i, want, cur;
    float         a;

    a = MDK_ASPECT_A * ((float)g_targetW * (float)MDK_VIEW_H)
                     / ((float)g_targetH * (float)MDK_VIEW_W);
    want = FloatBits(a);

    for (i = 0; i < sizeof(mdk_aspect_konst) / sizeof(mdk_aspect_konst[0]); i++) {
        unsigned char *konst;

        memcpy(sig, mdk_aspect_sig, sizeof(sig));
        PutU32(sig + MDK_ASPECT_DISP_AT, mdk_aspect_konst[i]);

        konst = MdkConstAt(code, codeSize, sig, sizeof(sig),
                           MDK_ASPECT_DISP_AT, 4);
        if (!konst) continue;

        cur = ReadU32(konst);
        if (cur == want) continue;                      // already applied
        if (cur != FloatBits(MDK_ASPECT_A)) continue;   // not what we expect

        WriteCode(konst, (const unsigned char *)&want, 4);
    }
}

// ---------------------------------------------------------------------------
// K4.  The screen shake, which moved the 3D window
// ---------------------------------------------------------------------------
//
// K1 forces the window origin to (0,0) everywhere it is *reset*, but MDK also
// SLIDES it: `0x46E340(newX, newY, otherX, otherY)` sets the origin globals
// outright, and its four call sites -- two twin routines, two branches each --
// feed it an animated origin from `0x538EAC`/`0x538EB4`, initialised to (20,60)
// at `0x435524`/`0x435546`.  That is the screen shake: the 600x360 window
// jiggles inside the 640x480 frame, and the black border absorbs the movement.
// `0x43820C` reads the displacement back out as `origin - (20,60)`.
//
// Without this the window would jump back to (20,60) the first time anything
// exploded, taking the clip window with it -- to (20, 60, 20+W, 60+H), which is
// off the end of the framebuffer.
//
// **The effect is deleted rather than corrected**, per GAME-PATCHING.md
// section 13.  It cannot be corrected: sliding a window that is already the
// whole screen has nowhere to slide to, so every version of "keep the shake"
// either pushes the picture off the screen or hands Glide a clip rectangle
// outside the framebuffer.  The border it used to move within no longer exists.
//
// Deleted at the four CALL SITES rather than inside 0x46E340 (section 5b), by
// zeroing the four argument registers.  That is 8 bytes of `xor` in an 18-byte
// span, needs no stub, and -- the point -- leaves the game's own shake
// bookkeeping completely untouched.  That matters more than it looks: the
// routine at `0x438260` stops the shake only when it observes the viewport back
// at exactly (20,60), so rewriting `0x538EAC`/`0x538EB4` instead would have left
// a shake that decays and then never terminates.
//
// The shake amplitude `0x538EA8` still drives its other consumers (`0x438050`,
// `0x43808D`, `0x4380B7`), so this removes the window movement, not the effect.
//
static const unsigned char mdk_shake0[] =   /* 0x0043859b */
    "\x8b\x55\xe4\x8b\x45\xe0\x8b\x0d\xb8\x8e"
    "\x53\x00\x8b\x1d\xb0\x8e\x53\x00\xe8\x8e"
    "\x5d\x03\x00";
static const unsigned char mdk_shake1[] =   /* 0x004385b7 */
    "\x8b\x55\xe4\x8b\x45\xe0\x8b\x0d\xb4\x8e"
    "\x53\x00\x8b\x1d\xac\x8e\x53\x00\xe8\x72"
    "\x5d\x03\x00";
static const unsigned char mdk_shake2[] =   /* 0x00473f52 */
    "\x8b\x55\xe4\x8b\x45\xe0\x8b\x0d\xb8\x8e"
    "\x53\x00\x8b\x1d\xb0\x8e\x53\x00\xe8\xd7"
    "\xa3\xff\xff";
static const unsigned char mdk_shake3[] =   /* 0x00473f6e */
    "\x8b\x55\xe4\x8b\x45\xe0\x8b\x0d\xb4\x8e"
    "\x53\x00\x8b\x1d\xac\x8e\x53\x00\xe8\xbb"
    "\xa3\xff\xff";

static const unsigned char *const mdk_shake[] = {
    mdk_shake0, mdk_shake1, mdk_shake2, mdk_shake3
};
#define MDK_SHAKE_SITES  (sizeof(mdk_shake) / sizeof(mdk_shake[0]))
#define MDK_SHAKE_SIG    23     /* through the call, which is what makes each
                                   site unique -- the 18 bytes before it are
                                   identical between the twins */
#define MDK_SHAKE_ZERO   18     /* the four argument loads */

// ---------------------------------------------------------------------------
// K6.  The sprite pipeline's reference size
// ---------------------------------------------------------------------------
//
// MDK draws entities as screen-space sprites: `0x46D1C8` transforms a world
// point by the projection matrix AND calls the mode-0 projection routine to
// get screen x/y, and then a caller such as `0x4110B7` computes the sprite's
// on-screen SIZE as
//
//     pixels = camW * s / (z * zoom)
//
// where `camW` is `0x538D2C`, the camera viewport's width.  That formula is
// only correct while `camW == Kx / A` -- 299.95/0.5 = 599.9 in the shipped
// game, which is why the constant is 600.  K2 and K3 changed both terms, so
// camW has to follow or every sprite in the game comes out at a third of its
// proper size.
//
//     camW' = Kx'/A' = (299.95 * W/600) / (0.5 * W*360 / (H*600)) = 600 * H/360
//
// -- the W cancels, which is the whole point: the world scale is UNIFORM H/360
// (that is what Hor+ means; the extra width buys more world, not bigger world),
// so the sprite reference scales by H/360 too and never by W/600.
//
// The same block carries `camH` (`0x538D30`) and a centre pair
// (`0x538D34`/`0x538D38`), and scaling all four by H/360 is exactly right for
// each: camH is read only as a clip bound and 360*H/360 == H, the real screen
// height.  The centre pair has ELEVEN writers and, verified by byte search,
// ZERO readers -- it is scaled only so the block cannot be read as
// half-converted later.
//
// Seven camera-setup routines write the main 600x360 view; four more write the
// inset viewports (384x280, and three 140x70 panels) used by projection modes
// 1-4 during cutscenes.  Those are deliberately NOT scaled, exactly as their
// projection routines were not: each inset is internally consistent as it
// stands, and scaling one half of it would be worse than leaving both.
//
// Because every value here is the original times H/360, the table does not name
// the values at all -- MDK_SCALE_H reads the immediate out of the signature and
// scales it.  That also means the signature failing to match cannot silently
// write the wrong constant into the right place.
//
static const unsigned char mdk_cam00[] =    /* 0x0040f9e0 */
    "\xbf\x58\x02\x00\x00\x88\x35\x56\x96\x53"
    "\x00\xe8\xa0\x18\x02\x00\xb8\x00\x35\x00"
    "\x00\xbe\xb4\x00\x00\x00\xe8\x35\xd0\x00"
    "\x00\xa3\xe4\x8c\x53\x00\xa1\xf0\x5c\x4e"
    "\x00\x31\xd2\xe8\x34\x02\x00\x00\xb8\x68"
    "\x01\x00\x00";
static const MdkField mdk_cam00_f[] = { {1,MDK_SCALE_H}, {22,MDK_SCALE_H}, {49,MDK_SCALE_H} };

static const unsigned char mdk_cam01[] =    /* 0x0040fa3f */
    "\xbb\x2c\x01\x00\x00\xa3\x30\x8d\x53\x00";
static const MdkField mdk_cam01_f[] = { {1,MDK_SCALE_H} };

static const unsigned char mdk_cam02[] =    /* 0x00429f77 */
    "\xbe\x58\x02\x00\x00\xbf\x68\x01\x00\x00"
    "\xbb\xb4\x00\x00\x00";
static const MdkField mdk_cam02_f[] = { {1,MDK_SCALE_H}, {6,MDK_SCALE_H}, {11,MDK_SCALE_H} };

static const unsigned char mdk_cam03[] =    /* 0x00429ff3 */
    "\xb9\x2c\x01\x00\x00\xa3\xf4\x8c\x53\x00";
static const MdkField mdk_cam03_f[] = { {1,MDK_SCALE_H} };

static const unsigned char mdk_cam04[] =    /* 0x0042dbc5 */
    "\xbe\x58\x02\x00\x00\xbf\x68\x01\x00\x00"
    "\xbb\x2c\x01\x00\x00";
static const MdkField mdk_cam04_f[] = { {1,MDK_SCALE_H}, {6,MDK_SCALE_H}, {11,MDK_SCALE_H} };

static const unsigned char mdk_cam05[] =    /* 0x0042dbfe */
    "\xbe\xb4\x00\x00\x00\x8d\x45\xb8";
static const MdkField mdk_cam05_f[] = { {1,MDK_SCALE_H} };

static const unsigned char mdk_cam06[] =    /* 0x00430efc */
    "\xbf\x58\x02\x00\x00\xb8\x68\x01\x00\x00"
    "\x89\x1d\x28\x8d\x53\x00\x89\x35\x1c\x8d"
    "\x53\x00\x89\x3d\x2c\x8d\x53\x00\xd8\x21"
    "\xa3\x30\x8d\x53\x00\xd9\x5d\xb4\xbf\x2c"
    "\x01\x00\x00\xb8\xb4\x00\x00\x00";
static const MdkField mdk_cam06_f[] = { {1,MDK_SCALE_H}, {6,MDK_SCALE_H}, {39,MDK_SCALE_H}, {44,MDK_SCALE_H} };

static const unsigned char mdk_cam07[] =    /* 0x00432390 */
    "\xbe\x58\x02\x00\x00\xbf\x68\x01\x00\x00"
    "\xb9\x2c\x01\x00\x00\x31\xc0\x89\x1d\x28"
    "\x8d\x53\x00\x89\x35\x2c\x8d\x53\x00\x89"
    "\x3d\x30\x8d\x53\x00\xa3\x40\x8d\x53\x00"
    "\xa3\x3c\x8d\x53\x00\xbb\xb4\x00\x00\x00";
static const MdkField mdk_cam07_f[] = { {1,MDK_SCALE_H}, {6,MDK_SCALE_H}, {11,MDK_SCALE_H}, {46,MDK_SCALE_H} };

static const unsigned char mdk_cam08[] =    /* 0x004390b8 */
    "\xbb\xb4\x00\x00\x00\xba\x00\x00\x80\xbf"
    "\x57\x89\xc8\xc1\xe9\x02\xf2\xa5\x8a\xc8"
    "\x80\xe1\x03\xf2\xa4\x5f\xb9\x9a\x99\x19"
    "\x40\xbe\x58\x02\x00\x00\x31\xc0\xbf\x68"
    "\x01\x00\x00";
static const MdkField mdk_cam08_f[] = { {1,MDK_SCALE_H}, {32,MDK_SCALE_H}, {39,MDK_SCALE_H} };

static const unsigned char mdk_cam09[] =    /* 0x0043910e */
    "\xb9\x2c\x01\x00\x00\xbe\x55\x55\x55\x3f";
static const MdkField mdk_cam09_f[] = { {1,MDK_SCALE_H} };

static const unsigned char mdk_cam10[] =    /* 0x004737a8 */
    "\xbf\x58\x02\x00\x00\xb8\x68\x01\x00\x00"
    "\xbb\x2c\x01\x00\x00\x31\xd2\x89\x35\x28"
    "\x8d\x53\x00\x89\x3d\x2c\x8d\x53\x00\xa3"
    "\x30\x8d\x53\x00\x89\x15\x40\x8d\x53\x00"
    "\x89\x15\x3c\x8d\x53\x00\xbe\xb4\x00\x00"
    "\x00";
static const MdkField mdk_cam10_f[] = { {1,MDK_SCALE_H}, {6,MDK_SCALE_H}, {11,MDK_SCALE_H}, {47,MDK_SCALE_H} };

static const MdkSpan mdk_camera[] = {
    { mdk_cam00, 53, mdk_cam00_f, 3 },   /* 0x0040f9e0 */
    { mdk_cam01, 10, mdk_cam01_f, 1 },   /* 0x0040fa3f */
    { mdk_cam02, 15, mdk_cam02_f, 3 },   /* 0x00429f77 */
    { mdk_cam03, 10, mdk_cam03_f, 1 },   /* 0x00429ff3 */
    { mdk_cam04, 15, mdk_cam04_f, 3 },   /* 0x0042dbc5 */
    { mdk_cam05,  8, mdk_cam05_f, 1 },   /* 0x0042dbfe */
    { mdk_cam06, 48, mdk_cam06_f, 4 },   /* 0x00430efc */
    { mdk_cam07, 50, mdk_cam07_f, 4 },   /* 0x00432390 */
    { mdk_cam08, 43, mdk_cam08_f, 3 },   /* 0x004390b8 */
    { mdk_cam09, 10, mdk_cam09_f, 1 },   /* 0x0043910e */
    { mdk_cam10, 51, mdk_cam10_f, 4 },   /* 0x004737a8 */
};
#define MDK_CAMERA_SPANS (sizeof(mdk_camera) / sizeof(mdk_camera[0]))

// ---------------------------------------------------------------------------
// K7.  The sprite clip bound
// ---------------------------------------------------------------------------
//
// `0x4045F0` is the sprite blitter.  It reduces the sprite's left edge to
// viewport-relative (`x - camOX`), rejects it if negative, and then takes the
// available width as `camW - left`, rejecting again if that is <= 0.  So camW
// is simultaneously the sprite pipeline's reference size (K6) and its right
// clip boundary -- and after K6 those want DIFFERENT values: 1800 and 2560 at
// 2560x1080.  They were equal in the shipped game only because the viewport
// was 4:3-ish; widening the aspect separated them for good.
//
// K6 gives the global the reference-size value, because that is the one used
// five times and the one whose failure is catastrophic (a sprite at a third or
// thirteen times its size).  This patch gives the single clip read the real
// screen width instead.
//
// Consequence for the inset viewports, stated plainly: their clip bound becomes
// the whole screen rather than their own 384 or 140.  That is benign, because
// this bound has never been what positions anything -- the projection already
// confines each mode's output to its own rectangle, and the clip only decides
// whether a sprite already at the edge gets cut.  The reverse trade (correct
// clip, wrong reference size) would have made inset sprites thirteen times too
// big.
//
static const unsigned char mdk_clipw_sig[] =    /* 0x004046a0 */
    "\x8b\x15\x2c\x8d\x53\x00"      /* mov  0x538d2c,%edx  <- camW  */
    "\x29\xf2";                     /* sub  %esi,%edx               */

// ---------------------------------------------------------------------------
// K5/K8.  The 2D layer -- HUD and menus centred, world sprites left alone
// ---------------------------------------------------------------------------
//
// Everything MDK draws in 2D goes through `0x470F78`, which locks the LFB and
// returns `lfbPtr + strideInBytes*originY + 2*originX`.  With K1 the origin is
// (0,0), so the 2D layer -- which clips itself to 600x360 in software
// (`0x40A940`) and whose background blitter walks 600-pixel rows (`0x46F52A`)
// -- lands as a 600x360 island in the top-left corner.  Centring that island is
// K5.
//
// But not everything on that path wants the same treatment.  Enumerating all
// eighteen callers and asking which touch the camera viewport globals splits
// them cleanly: exactly ONE does, `0x404755`, the sprite blitter.  The other
// seventeen use hardcoded 600/360 layout constants and are centred.
//
// So the offset is applied by CALLER (GAME-PATCHING.md section 6, "the caller's
// return address identifies the element exactly"), and the table has one entry:
// the sprite blitter gets no offset, i.e. its base is the raw framebuffer
// origin.
//
// **The original reason given for that exclusion was wrong** and is corrected
// here rather than quietly rewritten: the claim was that the blitter's
// coordinates "come from the 3D projection and are already real screen pixels".
// They are not -- the hardware logs show every sprite drawn in MDK's old
// 600x360 space, unchanged at every patch level.  The exclusion is still
// correct, but for a different reason: K10 converts the player's descriptor to
// real screen pixels at its call site, so the blitter must be working from the
// real origin, and K7's clip bound of W then agrees with that base instead of
// being cx too generous.
//
// The cost is that the other blitter callers -- 600-wide loading and cutscene
// art -- stay in the top-left rather than centred.  They do not run in
// gameplay (zero blitter calls until K10's site was identified), so this buys
// the thing that matters and defers the thing that does not.
//
// Everything else is centred, not spread.  MDK's 2D arrives at one blitter from
// an ordinary draw list; there is no further call site to classify against,
// which is exactly the situation section 24 describes.  Centring reproduces the
// 600x360 layout exactly, so nothing can split and nothing can move relative to
// anything else -- correct by construction, at the cost of the corners.  Two
// attempts at recovering the corners in Turok failed on hardware for structural
// reasons that apply here word for word; do not try a third.
//
// Implemented as an inline hook rather than an in-place rewrite because the
// arithmetic does not fit: the site is 32 bytes and the offset form needs 41.
// The stub's body is C (section 25) and the four globals it needs are read out
// of the SIGNATURE's own displacements rather than written down here, so a
// matched signature is also proof they are the right addresses.
//
// pushad/popad around the call is what makes this safe to drop in: the original
// left the computed base in eax, and the code after the site reloads eax from
// the stride global before using it again, so nothing depends on our not
// clobbering it.  Nothing downstream reads flags either.  `ebp` still addresses
// 0x470F78's own frame at the hook point, so `[ebp+4]` is the caller's return
// address.
//
// The sprite blitter's own lock call is located by signature rather than
// written down, and its return address is the byte after it.  If that signature
// is ever not found the whole hook is skipped: centring WITHOUT the exception
// is a known-broken state, not a degraded one.
//
static const unsigned char mdk_spritelock_sig[] = {   /* ends at 0x0040475a */
    0x89, 0x85, 0xac, 0xfd, 0xff, 0xff,   // mov  %eax,-0x254(%ebp)
    0x8d, 0x45, 0xd8,                     // lea  -0x28(%ebp),%eax
    0xe8, 0x1e, 0xc8, 0x06, 0x00          // call 0x470f78
};

static const unsigned char mdk_lfb_view_sig[] = {  /* 0x00470f9d */
    0xa1, 0x58, 0x6a, 0x54, 0x00,               // mov  0x546a58,%eax  stride @1
    0x0f, 0xaf, 0x05, 0xe4, 0x2b, 0x49, 0x00,   // imul 0x492be4       orgY   @8
    0x8b, 0x15, 0x54, 0x6a, 0x54, 0x00,         // mov  0x546a54,%edx  lfb    @14
    0x01, 0xd0,                                 // add  %edx,%eax
    0x8b, 0x15, 0xe0, 0x2b, 0x49, 0x00,         // mov  0x492be0,%edx  orgX   @22
    0x01, 0xd2,                                 // add  %edx,%edx
    0x01, 0xd0,                                 // add  %edx,%eax
    0x89, 0x03                                  // mov  %eax,(%ebx)
};
#define MDK_LFB_STRIDE_AT   1
#define MDK_LFB_ORGY_AT     8
#define MDK_LFB_PTR_AT     14
#define MDK_LFB_ORGX_AT    22

/* Live pointers into the game, so the hook follows whatever it does later. */
static const unsigned int *g_mdkLfbPtr    = NULL;
static const unsigned int *g_mdkStride    = NULL;
static const int          *g_mdkOrgX      = NULL;
static const int          *g_mdkOrgY      = NULL;
static int                 g_mdkHudCx     = 0;
static int                 g_mdkHudCy     = 0;

//
// The callers whose coordinates are already real screen pixels and must NOT be
// centred: the sprite blitter (K10) and the entity drawer (K11).
//
static unsigned int        g_mdkSpriteRet = 0;
static unsigned int        g_mdkEntityRet = 0;

/* K15.  The HUD drawer's own lock, so its elements can be anchored one at a
   time rather than the whole 2D layer being shifted as one block. */
static const unsigned char mdk_hudlock_sig[] =    /* 0x0041940b */
    "\x8d\x55\xe8\x8d\x45\xec\xe8\x62";
#define MDK_HUDLOCK_RET  11                       /* the call ends here */

/* --- restored -------------------------------------------------------------
   These declarations were deleted by a bad splice while repairing the two
   corrupted signatures below.  Recovered from the binary and from their use
   sites; every byte pattern here was re-read from MDK3DFX.EXE rather than
   retyped from memory. */

/* The letterboxed picture the backdrop is drawn into.  K15 anchors the HUD to
   the edges of this rather than of the screen. */
static int g_mdkBoxX0 = 0;
static int g_mdkBoxW  = 0;

/* The scope WINDOW -- the 600x360 hole inside the 640x480 composition K38
   letterboxes.  Everything drawn inside the scope maps through it. */
static int g_mdkSnipeX0 = 0, g_mdkSnipeY0 = 0;
static int g_mdkSnipeW  = 0, g_mdkSnipeH  = 0;

/* The three bullet-cam viewports, in the game's 600x360 layout: 140x70 at the
   projection constants K29 scales -- (72,10) (228,0) (384,10). */
static const short mdk_cam_box[3][4] = {
    {  72, 10, 212, 80 },
    { 228,  0, 368, 70 },
    { 384, 10, 524, 80 }
};

/* Per cam: frames of grace left since it was last given a backdrop of ANY
   kind -- the panorama while a projectile is in flight, or the game's own
   flat fill for the post-hit fade.  A cam at zero was drawn into by nothing
   at all, and it is K14's full-screen sky that then shows through its oval.

   TWO frames rather than a bare flag, so a live cam that skips one request
   does not flicker black; the cost is that the black comes back two frames
   late, on a thing that takes a second to fade anyway.  Set in two places,
   aged in exactly one (the swap) -- a setter without a matching clearer is
   a leak, which this project has paid for once already. */
static unsigned char g_mdkCamSky[3] = { 0, 0, 0 };

static int MdkCamIndex(int layoutX)
{
    int i;

    for (i = 0; i < 3; i++)
        if (layoutX >= mdk_cam_box[i][0] - 16
            && layoutX <= mdk_cam_box[i][0] + 16) return i;
    return -1;
}

/* The plain 600x360 layout box -- the whole screen fitted to the game's own
   layout space, with no composition around it.  This is what the scope
   window WAS before K38, and the bombing run still wants it: that screen is
   laid out in 600x360 and has no 640x480 body around it, so it must not
   inherit the scope's inset.  Keeping the two apart is the whole point --
   K35 shared g_mdkSnipe* only because the two boxes happened to be equal. */
static int g_mdkLayoutX0 = 0, g_mdkLayoutY0 = 0;
static int g_mdkLayoutW  = 0, g_mdkLayoutH  = 0;
#define MDK_SCOPE_W    600
#define MDK_SCOPE_H    360

/* Set when the scope art draws, cleared at the swap: true for exactly the rest
   of that frame, which is what the HUD needs.  The SKY is drawn from the
   after-clear hook at the START of a frame, when this has just been cleared,
   so it gets its own countdown instead. */
static int g_mdkScopeHold = 0;

/* Read by the wrapper's grDrawTriangle to decide whether to call in at all.
   Every other game then pays one load and a not-taken branch per triangle
   rather than a call, and MDK pays it only while the scope is up. */
int GameFix_TriHot = 0;

/* K33 depends on this, so it is not scaffolding.  0x470074 is MDK's shared
   triangle emitter -- ELEVEN callers -- so the return address the wrapper
   sees (0x4701cb, its grDrawTriangle call) names the emitter and never the
   producer.  The damage overlay and a pile of world geometry occupy the same
   screen region and cannot be told apart by shape alone, so keying on the
   producer is the only thing that separates them.

   A five-byte hook on the emitter's ENTRY records its caller, which IS the
   producer.  Written so nothing is disturbed: mov touches no flags, and eax
   is saved because it is one of the arguments. */
static unsigned int g_mdkEmitFrom = 0;

/* The bare prologue `55 89 e5 56 57` is one of the commonest byte sequences
   in the image, so the first version of this matched everywhere and declined.
   Extended through the frame size, which is distinctive; only the first FIVE
   bytes are replaced. */
static const unsigned char mdk_emit_sig[] =      /* 0x00470074 */
    { 0x55, 0x89, 0xe5, 0x56, 0x57, 0x81, 0xec, 0xc8, 0x00, 0x00, 0x00,
      0x89, 0xde };

static const unsigned char mdk_emit_stub[] = {
    0x50,                             // push %eax
    0x8b, 0x44, 0x24, 0x04,           // mov  0x4(%esp),%eax  the return addr
    0xa3, 0, 0, 0, 0,                 // mov  %eax,g_mdkEmitFrom     <- @6
    0x58,                             // pop  %eax
    0x55, 0x89, 0xe5, 0x56, 0x57,     // the displaced five
    0xe9, 0, 0, 0, 0                  // jmp  back                   <- @17
};
#define MDK_EMIT_VAR_AT   6
#define MDK_EMIT_JMP_AT  17

static void InstallMdkEmitProbe(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub;
    unsigned char  patch[5];

    at = FindUnique(code, codeSize, mdk_emit_sig, sizeof(mdk_emit_sig));
    if (!at) return;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_emit_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_emit_stub, sizeof(mdk_emit_stub));
    PutU32(stub + MDK_EMIT_VAR_AT, (unsigned int)&g_mdkEmitFrom);
    PutU32(stub + MDK_EMIT_JMP_AT,
           (unsigned int)(int)((at + 5) - (stub + MDK_EMIT_JMP_AT + 4)));


    patch[0] = 0xe9;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

/* K26.  The item frame: the general rect drawer 0x416d30, keyed to the one
   caller that draws the ammo icon frame, so its other 21 callers are left
   alone. */
static const unsigned char mdk_rectlock_sig[] =  /* 0x00416d3b */
    { 0x89, 0xd7, 0x89, 0x4d, 0xec, 0x8d, 0x55, 0xe4,
      0x8d, 0x45, 0xe8, 0xe8, 0x2d, 0xa2, 0x05, 0x00 };
/* The signature ENDS on the return address -- it runs through the call
   itself -- so no +1.  Getting that wrong by one byte is what misplaced
   the item frame after the restore: the key never matched. */
#define MDK_RECT_RET   (sizeof(mdk_rectlock_sig))       /* 0x00416d4b */

static const unsigned char mdk_itemrect_sig[] =  /* 0x0046be30 */
    { 0x8d, 0x4e, 0x37, 0x89, 0xfa, 0xe8, 0xf6, 0xae, 0xfa, 0xff };
#define MDK_ITEM_RET   (sizeof(mdk_itemrect_sig))       /* 0x0046be3a */

static unsigned int g_mdkRectRet  = 0;
static unsigned int g_mdkItemFrom = 0;

/* The enemy health bar's FRAME.  Same rect drawer as the item frame, a
   different caller, and a different anchor: the top-left of the picture.
   Its fill (K32) is offset to match, so the two stay together. */
static const unsigned char mdk_hpframe_sig[] =   /* 0x004388b8 */
    { 0x6a, 0x0a, 0x8b, 0x4d, 0xdc, 0xbb, 0x04, 0x00, 0x00, 0x00,
      0x31, 0xd2, 0x89, 0xd8 };
#define MDK_HPFRAME_RET  (sizeof(mdk_hpframe_sig) + 5)   /* 0x004388cb */

static unsigned int g_mdkHpFrom = 0;

/* The call site inside the HUD code that draws numbers and the weapon icon.
   The health widget comes through other call sites, and that is the only thing
   that separates them -- they share the same narrow band of x. */
static const unsigned char mdk_numdraw_sig[] =    /* 0x0041904a */
    { 0xba, 0xb8, 0x36, 0x54, 0x00, 0x01, 0xc8, 0x8d, 0x4d, 0xe4,
      0xe8, 0xa3, 0x03, 0x00, 0x00 };     /* ...call 0x4193fc */
#define MDK_NUMDRAW_RET  (sizeof(mdk_numdraw_sig))   /* 0x00419059 */

/* The HUD drawer entry, hooked purely to record which call site is
   drawing.

   BOTH of these were destroyed by a UTF-8 round trip -- every byte >= 0x80
   became EF BF BD -- so they matched nothing and these two hooks have
   never installed.  GCC warned about it on every single build.  Recovered
   from the binary and written as ARRAYS, which cannot suffer it again. */
static const unsigned char mdk_huddraw_sig[] =    /* 0x004193fc */
    { 0x55, 0x89, 0xe5, 0x56, 0x57, 0x83, 0xec, 0x10, 0x89, 0xc6 };

static const unsigned char mdk_huddraw_stub[] = {
    0x60,                       // pushad
    0xff, 0x74, 0x24, 0x20,     // push  0x20(%esp)  -> the drawer's caller
    0xe8, 0, 0, 0, 0,           // call  MdkHudCaller              <- @6
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0, 0, 0, 0, 0,              // the 5 displaced bytes           <- @14
    0xe9, 0, 0, 0, 0            // jmp   back                      <- @20
};
#define MDK_HUDDRAW_CALL_AT   6
#define MDK_HUDDRAW_DISP_AT  14
#define MDK_HUDDRAW_JMP_AT   20

/* K5/K15/K26/K29.  The 2D layer is centred by adjusting the base the view
 * lock hands back, and individual elements are anchored by keying on the
 * CALLER.
 *
 * RESTORED after a bad splice deleted this block.  Recovered from the last
 * good object file rather than from memory: every branch, constant and
 * comparison below was read out of the compiled MdkLfbBaseView, and the
 * signature bytes were re-read from MDK3DFX.EXE.  Checked against known-good
 * output from the hardware log:
 *
 *     hudpos  20,316 -> screen  313,756     normal play, left band
 *     hudpos 500,287 -> screen 1403,637     snipe, the scope box
 *
 * The site is the tail of the lock helper 0x470f78, 32 bytes ending in
 * `mov %eax,(%ebx)`, which is where it publishes the adjusted base:
 *
 *     470f9d  mov   0x546a58,%eax          stride   <- disp at +1
 *     470fa2  imul  0x492be4,%eax          originY  <- disp at +8
 *     470fa9  mov   0x546a54,%edx          lfbPtr   <- disp at +14
 *     470fb1  mov   0x492be0,%edx          originX  <- disp at +22
 *     470fbb  mov   %eax,(%ebx)
 *
 * Reading the four globals out of the signature's own displacements is what
 * makes a matched signature simultaneously proof that they are the right
 * addresses.
 */
static unsigned int g_mdkHudRet = 0;    /* the HUD drawer's own lock      */
static unsigned int g_mdkNumRet = 0;    /* its number/icon call site      */

/* True for exactly the frame the scope art drew in -- the HUD needs it to
   mean "this frame", or it keeps the scope layout for a few frames after you
   leave snipe mode.  Anything that runs at the START of a frame, when this
   has just been cleared, uses g_mdkScopeHold instead. */
static int g_mdkInScope = 0;

/* Hooked at the HUD drawer's entry purely to record which call site is
   drawing.  Empty in the shipped build -- the caller is recorded from the
   lock instead -- and the compiler agreed: it emitted a bare `ret`. */
extern "C" void __attribute__((cdecl, used, noinline))
MdkHudCaller(unsigned int caller)
{
    (void)caller;
}

/* Defined after g_diagGate, which it reads; declared here because the
   installer below takes its address. */
extern "C" void __attribute__((cdecl)) MdkLfbBaseView(unsigned int *,
        unsigned int, unsigned int, unsigned int);

static const unsigned char mdk_lfb_stub[] = {
    0x60,                       // pushad
    0xff, 0x75, 0x00,           // push  0x0(%ebp)     -> the caller's saved ebp
    0xff, 0x75, 0xfc,           // push  -0x4(%ebp)    -> the caller's own ebx
    0xff, 0x75, 0x04,           // push  0x4(%ebp)     -> caller's return addr
    0x53,                       // push  %ebx          -> the out pointer
    0xe8, 0, 0, 0, 0,           // call  MdkLfbBaseView           <- @12
    0x83, 0xc4, 0x10,           // add   $0x10,%esp
    0x61,                       // popad
    0xc3                        // ret
};
#define MDK_LFB_STUB_CALL_AT  12


//
// K4: stop the shake moving the window.
//
static void InstallMdkShake(unsigned char *code, unsigned int codeSize)
{
    static const unsigned char zero[MDK_SHAKE_ZERO] = {
        0x31, 0xc0,             // xor %eax,%eax   new X
        0x31, 0xd2,             // xor %edx,%edx   new Y
        0x31, 0xdb,             // xor %ebx,%ebx   other X
        0x31, 0xc9,             // xor %ecx,%ecx   other Y
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    unsigned char *at[MDK_SHAKE_SITES];
    unsigned int   i, found = 0;

    for (i = 0; i < MDK_SHAKE_SITES; i++) {
        at[i] = FindUnique(code, codeSize, mdk_shake[i], MDK_SHAKE_SIG);
        if (at[i]) found++;
    }

    // All or nothing: half the branches sliding the window and half not is a
    // worse state than either, and one that only shows up when something
    // explodes.
    if (found != MDK_SHAKE_SITES) return;

    for (i = 0; i < MDK_SHAKE_SITES; i++)
        WriteCode(at[i], zero, MDK_SHAKE_ZERO);
}

// ---------------------------------------------------------------------------
// K11.  The player character -- the real one this time
// ---------------------------------------------------------------------------
//
// Found by instrumenting every 2D drawer that runs in gameplay and reading the
// accumulated log as a sequence.  In the in-level epochs there are no sprite
// records at all; what fires instead, every time, is `0x415a38` from caller
// `0x40ab11`, with arguments like
//
//     eax=0x4f2 edx=0x333   (1266, 819)
//     eax=0x457 edx=0x28a   (1111, 650)
//     eax=0x502 edx=0x33e   (1282, 830)
//
// -- and the routine's first two instructions are
//
//     415a45  cmp %eax,$600 ; jge return
//     415a4c  cmp %edx,$360 ; jge return
//
// So the character is passed correct, fully-scaled screen coordinates and then
// thrown away by a destination surface still described as 600x360.  That is
// also why the bisect blamed K2: before the projection patch these coordinates
// were ~(370,206) and passed the test; after it they are three times larger and
// never do.  Nothing was ever culling it in the 3D sense -- this is a 2D clip.
//
// The arithmetic confirms which space they are in.  The scaled projection puts
// an old-space point at `old*3 + 380` horizontally and `old*3` vertically (the
// vertical offset vanishes because 180*3 == H/2 exactly).  Inverting the logged
// values gives old x 244..301 and old y 206..282 -- clustered around the old
// centre of (300,180), which is where a third-person character belongs.  Read
// as an unscaled `old*3` they would sit 70..127 units right of centre for no
// reason.  So these are real screen pixels.
//
// Seven bounds inside the routine: two entry rejects, a right-edge clamp pair,
// and three bottom-edge clamps.  All become the real screen.
//
// The drawer takes its framebuffer base from `0x470F78`, which K5 centres.
// Real screen coordinates must not then be offset again by (cx,cy), so its lock
// is excluded exactly as the sprite blitter's is -- the caller table now has
// two entries.  Its return address is located by signature rather than written
// down.
//
static const unsigned char mdk_ent00[] =   /* 0x00415a45 */
    "\x3d\x58\x02\x00\x00\x7d\x1c\x81\xfa\x68"
    "\x01\x00\x00";
static const MdkField mdk_ent00_f[] = { {1,MDK_W}, {9,MDK_H} };

static const unsigned char mdk_ent01[] =   /* 0x00415aa8 */
    "\x81\xfa\x58\x02\x00\x00\x0f\x8c\x9f\x02"
    "\x00\x00\xba\x58\x02\x00\x00";
static const MdkField mdk_ent01_f[] = { {2,MDK_W}, {13,MDK_W} };

static const unsigned char mdk_ent02[] =   /* 0x00415af1 */
    "\x81\xfe\x68\x01\x00\x00\x7c\xc8";
static const MdkField mdk_ent02_f[] = { {2,MDK_H} };

static const unsigned char mdk_ent03[] =   /* 0x00415b6d */
    "\x81\xfe\x68\x01\x00\x00\x7c\xcd";
static const MdkField mdk_ent03_f[] = { {2,MDK_H} };

static const unsigned char mdk_ent04[] =   /* 0x00415d78 */
    "\x81\xfe\x68\x01\x00\x00\x7c\xd3";
static const MdkField mdk_ent04_f[] = { {2,MDK_H} };

static const MdkSpan mdk_entity[] = {
    { mdk_ent00, 13, mdk_ent00_f, 2 },   /* 0x00415a45 */
    { mdk_ent01, 17, mdk_ent01_f, 2 },   /* 0x00415aa8 */
    { mdk_ent02,  8, mdk_ent02_f, 1 },   /* 0x00415af1 */
    { mdk_ent03,  8, mdk_ent03_f, 1 },   /* 0x00415b6d */
    { mdk_ent04,  8, mdk_ent04_f, 1 }    /* 0x00415d78 */
};
#define MDK_ENTITY_SPANS (sizeof(mdk_entity) / sizeof(mdk_entity[0]))

static const unsigned char mdk_entlock_sig[] =   /* ends at 0x00415a7a */
    "\x8d\x55\xcc\x8d\x45\xd0\xe8\xfe\xb4\x05"
    "\x00";

// ---------------------------------------------------------------------------
// K10.  A menu/intro sprite (NOT the character -- see K11)
// ---------------------------------------------------------------------------
//
// The player IS a 2D sprite, as first reported -- three builds went past that
// because the sprite blitter looked idle in gameplay, which it was not: the
// first capture simply missed the moment.  The hardware bisect and the logs
// named it exactly:
//
//   patches=1  (no projection patch)  pos x 277..323  y 171..195  dest ~65x110
//   patches=3  (projection patched)   pos x 277..323  y 165..194  dest ~63x107
//   patches=23 (+ camera viewport)    pos x 275..323  y 165..193  dest ~62x105
//
// Drawn once per frame (450 calls in 450 frames), roughly humanoid, wandering
// around (300,180) -- and **identical at every patch level**.  So it is
// projected entirely inside MDK's old 600x360 space by the inlined `+300`/`+180`
// transform at `0x412afb`/`0x412b0a`, which build 3 deliberately left alone as
// "dual-purpose constants in an unidentified routine".  The routine was
// `0x412978`, the caller is `0x410e0c`, and the element was the player.
//
// Once the world was rescaled and the character was not, it stayed a 65x110
// sprite in the top-left corner of a 2560x1080 screen: not invisible, just
// nowhere near where anyone would look.
//
// The transform is exact rather than fitted.  In the old space the world put a
// view-space point at `X/z · Kx/(zoom·A) + Kx`; after K2/K3 it puts it at
// `X/z · Kx'/(zoom·A') + Kx'`, and the ratio of those two scales is
//
//     (Kx'/Kx)·(A/A') = (W/600)·(H·600)/(W·360) = H/360
//
// -- the W cancels, exactly as it did for the sprite size in K6, and for the
// same reason.  So the whole mapping is a uniform scale about the old centre:
//
//     screen = (old - 300) · H/360 + W/2        (and 180, H/2 vertically)
//
// which is checked by construction: old = 300 lands on W/2, and old = 0 lands
// on (W - 600·H/360)/2, the left edge of the old view centred in the new one.
// The sprite's own scale factors get the same H/360, because the world scale is
// uniform.
//
// Hooked at the CALL rather than in the routine (section 5b): the game's return
// address is already correct, the blitter takes its descriptor in eax under
// Watcom's register convention, and pushad/popad keeps it there across the
// body.  Scoped to this one call site, so nothing else that reaches the blitter
// is touched.
//
// This deliberately produces REAL SCREEN pixels, which is why K8 must stay: the
// blitter's base is the raw framebuffer origin, so K7's clip bound of W is
// exactly right and cannot walk off the end of a row.  Centring the blitter
// instead and emitting `(old-300)·k + 300` is arithmetically equivalent but
// leaves the clip bound disagreeing with the base by cx, which is a buffer
// overrun waiting to happen.
//
static const unsigned char mdk_player_sig[] =    /* 0x00412dd1 */
    "\x8b\x45\xe8\x8b\x04\x85\x00\x5d\x4e\x00"
    "\xb9\x60\xf4\x46\x00\x89\x45\xa4\x8d\x45"
    "\x8c\x89\x4d\xa8\xe8\x02\x18\xff\xff";
#define MDK_PLAYER_CALL_AT  24      /* the `call 0x4045f0`, five bytes */

/* The old virtual view's centre: what the inlined projection adds. */
#define MDK_VCENTRE_X  300
#define MDK_VCENTRE_Y  180

static unsigned char *g_mdkBlitter = NULL;

extern "C" void __attribute__((cdecl, used, noinline))
MdkFixPlayer(int *d)
{
    int k, n;

    if (!d || !g_targetH) return;

    k = (int)g_targetH;
    n = MDK_VIEW_H;

    d[0] = (d[0] - MDK_VCENTRE_X) * k / n + (int)g_targetW / 2;
    d[1] = (d[1] - MDK_VCENTRE_Y) * k / n + (int)g_targetH / 2;

    /* Enlarge via the DRAWN SIZE fields, never via d[4]/d[5].
     *
     * This used to scale d[4] and d[5], which looked equivalent because
     * `destW = (d[4]*d[2])>>8` -- moving the factor between the two gives the
     * same destination size either way.  It is not equivalent: d[4] also feeds
     * the SOURCE ROW ADVANCE at 0x40473a, so scaling it walks the source
     * bitmap with the wrong stride and the sprite comes out as horizontal
     * streaks.  That is the scrambled crawler in the intro gameplay, and it is
     * the same fault that scrambled the Earth in 0121/0122.
     *
     * d[2]/d[3] carry the factor instead: identical drawn size, correct
     * source addressing.  Verified against the log -- d[4] read 142 after this
     * hook, i.e. its stock 64 times H/360. */
    d[2] = d[2] * k / n;
    d[3] = d[3] * k / n;
}

static const unsigned char mdk_player_stub[] = {
    0x60,                       // pushad
    0x50,                       // push  %eax        -> the descriptor
    0xe8, 0, 0, 0, 0,           // call  MdkFixPlayer         rel32 <- @3
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xe9, 0, 0, 0, 0            // jmp   the blitter          rel32 <- @12
};
/* Displacement offsets, not opcode offsets.  Verified by disassembling the
   template -- the jmp was written as 13 first (GAME-PATCHING.md section 5). */
#define MDK_PLAYER_STUB_CALL_AT   3
#define MDK_PLAYER_STUB_JMP_AT   12

// ---------------------------------------------------------------------------
// K12.  Magnify the character -- a replacement for MDK's 1:1 sprite blit
// ---------------------------------------------------------------------------
//
// K11 made the character visible; it is still a third of its proper size, and
// that turns out to be structural rather than a constant somewhere.
//
// `0x415A38` is a **1:1 RLE blitter**.  325 instructions, and the only multiply
// in it is the destination row offset -- no source stepping, no fractional
// accumulator, no scale argument.  Its caller reads the sprite's own header
//
//     u16 w ; u16 h ; s16 hotX ; s16 hotY ; RLE stream at +8
//
// and passes {w,h} straight through, so the character is drawn at whatever size
// the artist authored for a 600x360 view.  Nor can the work be handed to one of
// the game's own scaling blitters: `0x415314` and `0x4045F0` do step in 16.16,
// but both read RAW bitmaps, while `0x415A38` and `0x416C0C` read RLE and
// cannot scale.  The formats do not meet.
//
// (`0x538E70`, the value the sprite-bank cascade at `0x4636C8` tests against
// 800/500/200/100, is not a distance either -- it is written as 0x385/0x3EA
// elsewhere and reads as an animation id.  Those banks are animation ranges,
// not a size ladder, so there is nothing to select.)
//
// So the blit is reimplemented.  The format, read off `0x415CF9`-`0x415D53`:
//
//     0xFF        end of sprite
//     0xFE        end of row
//     c <  0x80   literal run of c+1 pixels, one index byte each
//     c >= 0x80   repeated run of c-0x7c pixels of one index
//
// with index 0 transparent in both, through the usual 8->16bpp palette at
// `0x546848`.
//
// Two hooks, and between them they leave the game's own lock, bounds checks and
// unlock untouched:
//
//   * `0x40AACC` (the shared sprite helper, entry): correct the hotspot, so the
//     sprite grows about its anchor rather than its top-left corner.  It is the
//     right place because all eight users of the helper pass through it.
//   * `0x415A7A` (immediately after the LFB lock): jump into the replacement,
//     which blits and then rejoins the original at its unlock.  Everything the
//     replacement needs is live at that point -- x at `[ebp-0x28]`, the
//     framebuffer at `[ebp-0x30]`, its stride at `[ebp-0x34]`, y in esi, {w,h}
//     in ebx and the stream in ecx.
//
// The scale is 16.16 rather than an integer block size, so a source pixel maps
// to the destination rectangle `[sx*S, (sx+1)*S)` and non-integer ratios (768/360
// = 2.13) come out right instead of being truncated to 2.
//
#define MDK_PAL_ADDR   0x00546848u

static const unsigned char mdk_hotspot_sig[] =   /* 0x0040aacc */
    "\x55\x89\xe5\x51\x56\x57\x83\xec\x08\x89"
    "\xc6\x85";
static const unsigned char mdk_blit_sig[] =      /* 0x00415a7a */
    "\x8b\x45\xcc\xd1\xf8\x89\x45\xcc";
static const unsigned char mdk_blitend_sig[] =   /* 0x00415d80 */
    "\xe8\x9b\xb2\x05\x00\x8d\x65\xf8";

/* Source-to-destination scale, 16.16.  1.0 means "do nothing". */
static unsigned int g_mdkSpriteScale = 0x10000u;

/* Set for the next sprite only, by the hotspot hook, when that sprite is not
   drawn at the world's own magnification.  16.16; 0 means "the default".
   The helper at 0x40aacc calls the blitter directly, so a value set at its
   entry is still current when the blit runs and nothing can come between
   them.

   It was a plain on/off "draw this one 1:1" until K38, which needs a third
   answer: the bullet-cam covers and the snipe crosshair belong to the scope
   WINDOW, which is no longer magnified by H/360 like the world.  One value
   with one meaning beats two flags whose precedence has to be remembered. */
static unsigned int g_mdkSprScale = 0;

static void MdkFillRect(unsigned char *lfb, int stride,
                        int x0, int y0, int x1, int y1, unsigned short col)
{
    int x, y;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)g_targetW) x1 = (int)g_targetW;
    if (y1 > (int)g_targetH) y1 = (int)g_targetH;

    for (y = y0; y < y1; y++) {
        unsigned short *row = (unsigned short *)(lfb + y * stride);
        for (x = x0; x < x1; x++)
            row[x] = col;
    }
}

extern "C" void __attribute__((cdecl, used, noinline))
MdkBlitScaled(unsigned int *f)
{
    const unsigned short *pal = (const unsigned short *)MDK_PAL_ADDR;
    const unsigned char  *p;
    const char           *fp;
    unsigned char        *lfb;
    const int            *size;
    unsigned int          s = g_mdkSprScale ? g_mdkSprScale
                                             : g_mdkSpriteScale;
    int   x, y, w, h, stride, sx = 0, sy = 0, guard;

    if (!f) return;

    fp     = (const char *)f[2];                 /* ebp */
    size   = (const int *)f[4];                  /* ebx -> {w,h}      */
    p      = (const unsigned char *)f[6];        /* ecx -> RLE stream */
    y      = (int)f[1];                          /* esi               */
    if (!fp || !size || !p) return;

    x      = *(const int *)(fp - 0x28);
    lfb    = *(unsigned char *const *)(fp - 0x30);
    stride = *(const int *)(fp - 0x34);
    w      = size[0];
    h      = size[1];

    if (!lfb || stride <= 0 || w <= 0 || h <= 0) return;

    // A malformed or truncated stream must not run away through memory.  Every
    // control byte consumes at least one source pixel, so the sprite's own area
    // plus its row markers is a hard upper bound.
    guard = w * h + h + 64;

    for (;;) {
        unsigned int c;
        int          n, literal, idx = 0;

        if (--guard < 0) break;
        c = *p++;

        if (c == 0xffu) break;
        if (c == 0xfeu) {
            sx = 0;
            if (++sy >= h) break;
            continue;
        }
        if (c < 0x80u) { n = (int)c + 1;      literal = 1; }
        else           { n = (int)c - 0x7c;   literal = 0; idx = *p++; }

        while (n-- > 0) {
            int v = literal ? *p++ : idx;

            if (v) {
                int x0 = x + (int)(((unsigned int)sx       * s) >> 16);
                int x1 = x + (int)(((unsigned int)(sx + 1) * s) >> 16);
                int y0 = y + (int)(((unsigned int)sy       * s) >> 16);
                int y1 = y + (int)(((unsigned int)(sy + 1) * s) >> 16);

                if (x1 <= x0) x1 = x0 + 1;
                if (y1 <= y0) y1 = y0 + 1;
                if (x0 < (int)g_targetW && y0 < (int)g_targetH && x1 > 0 && y1 > 0)
                    MdkFillRect(lfb, stride, x0, y0, x1, y1, pal[v]);
            }
            sx++;
        }
    }
}

/* K22.  The return address of the one call that draws the mouse cursor.
 *
 *   0x423430  mov  0x491c10,%eax   ; the cursor image, NULL-checked just above
 *   0x423435  mov  0x4(%eax),%ebx
 *   0x423438  add  %eax,%ebx       ; -> its pixel data
 *   0x42343a  mov  %ecx,%eax       ; -> the position
 *   0x42343c  call 0x40aacc        ; the shared sprite helper
 *   0x423441                       ; <- this
 *
 * Identified from a capture rather than by searching: the log recorded
 * `sprhdr caller=00423441 w=8 h=17 hot=0,0 at 308,182`, and an 8x17 sprite
 * whose hotspot is (0,0) is a mouse pointer. */
static const unsigned char mdk_cursor_sig[] =    /* 0x00423430 */
    "\xa1\x10\x1c\x49\x00\x8b\x58\x04\x01\xc3\x89\xc8\xe8";
#define MDK_CURSOR_RET   (sizeof(mdk_cursor_sig) - 1 + 4)   /* past the call */

static unsigned int g_mdkCursorRet = 0;

/* K25.  The ammo ICON.  It reaches the shared sprite helper from 0x46bd57,
   and the log named it: `sprhdr caller=0046bd5c w=48 h=48 hot=24,24 at
   32,328` -- a 48x48 sprite whose hotspot is dead centre, at the bottom
   left of the 600x360 layout, right beside the ammo COUNTER at 20,316.

   The counter is drawn by the HUD drawer and so gets K15's anchoring; the
   icon goes through this helper, whose lock K5 leaves uncentred, so it was
   left at raw layout coordinates in the top-left corner.  Same split as
   the mouse cursor. */
static const unsigned char mdk_ammo_sig[] =      /* 0x0046bd4c */
    "�E��]�ӋU��";
#define MDK_AMMO_RET   (sizeof(mdk_ammo_sig) - 1 + 4)   /* past the call */

static unsigned int g_mdkAmmoRet = 0;

/* The sniper crosshair.  A 52x68 sprite drawn at a HARDCODED layout position:

       437962  mov  $0xdb,%edx        ; y = 219
       437969  mov  $0x12b,%eax       ; x = 299
       43796e  call 0x40aacc

   (299,219) is the centre of the mode-1 viewport, so it is a fixed reticle
   rather than anything that tracks aim.  It goes through the shared sprite
   helper, whose lock K5 leaves uncentred, so it lands at raw layout
   coordinates -- fine while the scope was 1:1, invisible once the overlay
   filled the screen and painted over it.

   It takes the same box transform as everything else inside the scope, and
   its hotspot is pinned on BOTH axes: (26,42) of 52x68 is the aim point, not
   a ground contact, so the bottom-edge pivot below would be wrong here for
   the same reason it is wrong for the cursor. */
static const unsigned char mdk_cross_sig[] =     /* 0x00437962 */
    { 0xba, 0xdb, 0x00, 0x00, 0x00, 0x01, 0xc3,
      0xb8, 0x2b, 0x01, 0x00, 0x00 };
#define MDK_CROSS_RET   (sizeof(mdk_cross_sig) + 5)   /* past the call */

static unsigned int g_mdkCrossRet = 0;

/* The three bullet cams draw a 140x72 overlay sprite into each oval, hotspot
   dead centre, at the viewport centres (141,44) (297,34) (453,44).  THAT is
   the black cover that shows while no shot is in flight, and the brown one
   that fades after a hit -- same sprite, different image.

   It reaches the shared sprite helper, which K5 leaves uncentred, so it was
   being drawn at raw layout coordinates in the top-left and the ovals were
   left showing whatever was behind them.  Box-transformed it lands on each
   viewport centre to within two pixels, and the helper already magnifies it
   by H/360, which takes 140x72 to 311x160 against a 311x155 viewport.

   Blanking those rectangles from the overlay was the WRONG fix and is
   reverted: the cams draw BEFORE the art, so it blacked them permanently.
   The ordering I read it from was first-OCCURRENCE order in a deduplicated
   log, which is not frame order at all -- the cams simply appeared late
   because they only start once you fire. */
static const unsigned char mdk_cam_sig[] =       /* 0x00460c3b */
    { 0x8b, 0x1d, 0xfc, 0x4e, 0x54, 0x00, 0x8b, 0x55, 0x18,
      0x8b, 0x7c, 0x83, 0x04, 0x89, 0xc8, 0x01, 0xfb };
#define MDK_CAM_RET   (sizeof(mdk_cam_sig) + 5)   /* past the call */

static unsigned int g_mdkCamRet = 0;

/* K35.  The bombing run -- a third composed screen, and the whole of it is
 * one routine.
 *
 * 0x46ae78 is called once a frame while the bomb sight is up (from 0x438812,
 * its only caller) and draws exactly three things, all in the game's 600x360
 * layout space:
 *
 *     46aeab  BOMBTARG at layout (300,180)         the fixed target ring
 *     46aedc  CROSS    at ([0x538f0c],[0x538f10])  the movable cursor
 *     46af12  the bomb count, right-aligned at layout (472,56)
 *
 * The two sprites go through the shared RLE helper, whose lock K5 deliberately
 * leaves uncentred, so they were drawn at raw layout coordinates in the
 * top-left; the counter goes through the font renderer and so took the default
 * 2D centring, which is the menu's, not the picture's.
 *
 * The assets name themselves -- 0x435d78 and 0x435d87 load "BOMBTARG" and
 * "CROSS" into 0x4926e8 and 0x4926e4 -- and the arithmetic confirms the
 * reading rather than merely fitting it: the counter's right edge is predicted
 * at 660 + 472 = 1132 and measures 1130 on the screenshot.
 *
 * All three take the layout -> screen box map, exactly as the scope's own
 * elements do (K29): x = boxX0 + L*boxW/600, y = L*boxH/360.  That is the same
 * map the WORLD uses -- K10's cancellation makes layout 300 land on W/2 and
 * layout 0 on the box's left edge -- so the ring lands on screen centre by
 * construction and the cursor sweeps exactly the region the old view covered.
 *
 * Both hotspots are pinned on BOTH axes.  A target ring and a cursor are aim
 * points, not things standing on a floor, so K12's bottom-edge pivot is wrong
 * for them for the same reason it is wrong for the mouse cursor.
 *
 * The cursor's own globals are NOT touched: 0x538f0c has 68 readers and
 * 0x538f10 has 37, all of them the game's aiming maths.  Only the DRAWN
 * position moves, so cursor and aim point stay consistent by construction --
 * the same argument K22 makes for the menu pointer.  The consequence is that
 * the extra world Hor+ reveals outside the old 600-unit view cannot be
 * targeted; that is a limit of the game's own coordinate space, not of this.
 */
static const unsigned char mdk_bomb_sig[] =      /* 0x0046ae97 */
    { 0xa1, 0xe8, 0x26, 0x49, 0x00,              /* mov  0x4926e8,%eax  BOMBTARG */
      0x8b, 0x58, 0x04,                          /* mov  0x4(%eax),%ebx */
      0xba, 0xb4, 0x00, 0x00, 0x00,              /* mov  $180,%edx      y */
      0x01, 0xc3,                                /* add  %eax,%ebx      */
      0xb8, 0x2c, 0x01, 0x00, 0x00 };            /* mov  $300,%eax      x */
#define MDK_BOMB_TARG_AT    20                   /* call the sprite helper */
#define MDK_BOMB_CROSS_AT   69                   /* call the sprite helper */
#define MDK_BOMB_TEXT_AT   123                   /* call the font renderer */

/* The font renderer's own lock, so the counter can be told apart from every
   other string in the game by the caller it was invoked from. */
static const unsigned char mdk_font_sig[] =      /* 0x00414fc4 */
    { 0x8d, 0x55, 0xdc, 0x8d, 0x45, 0xe0, 0xe8 };
#define MDK_FONT_RET  (sizeof(mdk_font_sig) + 4)  /* past the call */

static unsigned int g_mdkBombTargRet  = 0;   /* 0x0046aeb0 */
static unsigned int g_mdkBombCrossRet = 0;   /* 0x0046aee1 */
static unsigned int g_mdkBombTextFrom = 0;   /* 0x0046af17 */
static unsigned int g_mdkFontRet      = 0;   /* 0x00414fcf */

/* K27.  The muzzle flash, and every other ATTACHMENT drawn on a sprite.

   0x40aa84 draws a sprite that may carry an attached one.  Read the two
   paths together and the whole bug is on the page:

       40aa90  mov  %ebx,-0xc(%ebp)      ; remember the PARENT's sprite
       40aa93  call 0x470f60             ; any attachment?
       40aa9a  jne  0x40aab6
       40aaa3  call 0x40aacc             ; <- 0x40aaa8, the parent
       ...
       40aab6  mov  0xc(%ebp),%edx       ; a fixed offset
       40aabe  add  %esi,%edx            ; parent position + offset
       40aac0  add  %edi,%eax
       40aac2  call 0x40aacc             ; <- 0x40aac7, the ATTACHMENT
       40aac7  jmp  0x40aa9c             ; then fall back and draw the parent

   So the attachment sits at a fixed offset from its parent, in layout units.
   The anchor correction below pivots each sprite on its OWN bottom edge, so
   parent and attachment are lifted by different amounts and drift apart by

       (h_parent - h_attach) * (S - 1)

   which at 1920x800 with the jumping frame is (146 - 57) * 1.222 = 109 px --
   the muzzle flash sitting just under the gun, measured at ~110 on hardware.

   Standing looked right for a reason worth recording: that pose is a single
   wide 347x294 frame with the flash already drawn into it, so there is no
   attachment at all and nothing to drift.  "Correct when grounded, wrong when
   jumping" was never about being airborne -- it was about which frames carry
   the flash as art and which draw it as a child.

   The fix is to lift the attachment by its PARENT's height, so the offset
   between them survives the magnification.  Exact no-op at S = 1. */
static const unsigned char mdk_attach_sig[] =    /* 0x0040aac0 */
    { 0x01, 0xf8, 0xe8, 0x05, 0x00, 0x00, 0x00 };  /* add %edi,%eax ; call */
#define MDK_ATTACH_RET   (sizeof(mdk_attach_sig))     /* past the call */

static unsigned int g_mdkAttachRet = 0;

//
// Anchor correction, at the shared helper's entry.  The helper is about to do
// `x -= hotX` / `y -= hotY`, so the incoming position is pre-adjusted and popad
// carries the change back into the game.
//
// **The two axes get different pivots, and the hardware numbers are why.**
// Logging the header during play gave:
//
//     52x146  hot 13,29     74x158  hot 29,29
//    120x160  hot 32,51     86x152  hot 43,28
//
// so the anchor sits about **20% down** the sprite -- up around the chest, not
// at the feet.  Magnifying about it therefore drives four fifths of the sprite
// downward: the feet went from y=965 to y=1199 on a 1080-tall screen, i.e.
// straight off the bottom.  That is the reported "a bit too down".
//
//   x: pivot on the hotspot.  It is the character's own horizontal position and
//      the observed error was purely vertical.
//   y: pivot on the sprite's BOTTOM edge, which is the ground contact point and
//      the physically meaningful invariant for something standing on a floor.
//      `dest_y + h` is held constant, so the feet stay exactly where the 1:1
//      build put them (965, 940, 989, 987 for the four frames above) and the
//      character grows upward as a taller character would.
//
extern "C" void __attribute__((cdecl, used, noinline))
MdkFixHotspot(unsigned int *f)
{
    const short *hdr;
    int          hy;
    unsigned int s = g_mdkSpriteScale;

    if (!f) return;

    g_mdkSprScale = 0;                           /* per-sprite, not sticky */

    /* K22.  The mouse cursor.
     *
     * The cursor is drawn through this same helper, so it lands in RAW screen
     * pixels -- K5 deliberately leaves this blitter's lock uncentred, because
     * the character's coordinates really are screen pixels.  The menu is not:
     * it is drawn 1:1 through the view lock and centred by (cx,cy).  So the
     * cursor sat in a 600x360 patch in the top-left while the menu it selects
     * sat in the middle, and only that patch was clickable.
     *
     * Offsetting the cursor by the same (cx,cy) puts it back over the menu.
     * Note this needs no change to MDK's hit-testing: that still happens in
     * 600x360 layout space, which is exactly the space the menu is drawn in,
     * so cursor and hit box move together and stay consistent by construction.
     * The reachable area maps onto the centred island, which is the only part
     * of the screen the menu occupies.
     *
     * Keyed on the caller (GAME-PATCHING section 6): the character reaches this
     * helper from elsewhere and must NOT be offset.
     *
     * It also returns before the anchor correction below, which is right rather
     * than lazy -- that correction pivots on the sprite's bottom edge because a
     * character stands on the ground.  A cursor's anchor is its tip, and its
     * header says so: hot=(0,0).  Applying the bottom pivot to an 8x17 cursor
     * would lift it 20 px away from the pixel it actually clicks. */
    if (g_mdkCursorRet && f[8] == g_mdkCursorRet) {
        f[7] += (unsigned int)g_mdkHudCx;        /* eax -- x */
        f[5] += (unsigned int)g_mdkHudCy;        /* edx -- y */
        return;
    }

    /* K25.  The ammo icon: anchored like the ammo counter beside it -- the
       left edge of the PICTURE, and the bottom.  Those are exactly the numbers
       K15 gives a left-band HUD element, so the two stay together by
       construction rather than by two tuned offsets.

       Its hotspot is the sprite's centre (24,24 of 48x48), so both axes pivot
       on the hotspot: that keeps the icon's middle where the maths puts it.
       The character's bottom-edge pivot below would be wrong here for the same
       reason it is wrong for the cursor -- it is not standing on anything. */
    if (g_mdkAmmoRet && f[8] == g_mdkAmmoRet) {
        /* Drawn 1:1, because the rest of the HUD is.  Measured off hardware:
           the health gauge comes out 79x59 -- exactly its size at 640x480, so
           the HUD drawer does not scale at all -- while this icon was being
           magnified by H/360 and looked far too big beside it.  Matching the
           HUD it belongs to matters more than matching the screen. */
        g_mdkSprScale = 0x10000u;                /* 1:1, like the HUD */
        f[7] += (unsigned int)g_mdkBoxX0;
        f[5] += (unsigned int)(g_mdkHudCy * 2);
        return;                                  /* 1:1, so no anchor to fix */
    }

    if (s == 0x10000u) return;

    hdr = (const short *)f[4];                   /* ebx -> sprite header */
    if (!hdr || IsBadReadPtr(hdr, 8)) return;

    /* Hotspot pinned on BOTH axes, through whichever box the sprite belongs
       to.  None of these is standing on anything, so the bottom-edge pivot
       below is wrong for all of them for the same reason it is wrong for
       the mouse cursor.

       The two boxes were the same thing until K38 and the four sprites
       shared one branch.  They are not the same any more:

         - the snipe crosshair and the three bullet-cam covers live INSIDE
           the scope, so they take the scope window AND the window's own
           magnification.  Leaving them on the world's H/360 is what made
           the covers 4/3 too wide, so the one over the middle cam reached
           across the first as well;
         - the bombing run is laid out in plain 600x360 with no composition
           around it, so it takes the layout box and the ordinary scale. */
    if (((g_mdkCrossRet && f[8] == g_mdkCrossRet) ||
         (g_mdkCamRet   && f[8] == g_mdkCamRet)) && g_mdkSnipeW > 0) {
        unsigned int sw = ((unsigned int)g_mdkSnipeW << 16) / MDK_SCOPE_W;
        int cxp = g_mdkSnipeX0 + ((int)f[7] * g_mdkSnipeW) / MDK_SCOPE_W;
        int cyp = g_mdkSnipeY0 + ((int)f[5] * g_mdkSnipeH) / MDK_SCOPE_H;

        g_mdkSprScale = sw;                  /* the WINDOW's scale */
        f[7] = (unsigned int)(cxp - (((int)hdr[2] * (int)sw >> 16) - (int)hdr[2]));
        f[5] = (unsigned int)(cyp - (((int)hdr[3] * (int)sw >> 16) - (int)hdr[3]));
        return;
    }

    if (((g_mdkBombTargRet  && f[8] == g_mdkBombTargRet) ||
         (g_mdkBombCrossRet && f[8] == g_mdkBombCrossRet)) && g_mdkLayoutW > 0) {
        int cxp = g_mdkLayoutX0 + ((int)f[7] * g_mdkLayoutW) / MDK_SCOPE_W;
        int cyp = g_mdkLayoutY0 + ((int)f[5] * g_mdkLayoutH) / MDK_SCOPE_H;

        f[7] = (unsigned int)(cxp - (((int)hdr[2] * (int)s >> 16) - (int)hdr[2]));
        f[5] = (unsigned int)(cyp - (((int)hdr[3] * (int)s >> 16) - (int)hdr[3]));
        return;
    }


    /* K27.  An attachment is lifted by its PARENT's height, not its own, so
       it keeps its offset from the body it is drawn on.  The parent's header
       is the one 0x40aa90 stashed at [ebp-0xc], and f[2] is still the
       caller's ebp -- the stub runs before the displaced `push %ebp`.

       Falls back to the sprite's own height if that cannot be read, which is
       exactly the behaviour of the build before this one. */
    hy = (int)hdr[1];

    if (g_mdkAttachRet && f[8] == g_mdkAttachRet && f[2] != 0
        && !IsBadReadPtr((const void *)(f[2] - 0xc), 4)) {
        const short *ph = (const short *)
                          (*(const unsigned int *)(f[2] - 0xc));

        if (ph && !IsBadReadPtr(ph, 8) && ph[1] > 0) {
            /* Map the attachment through the PARENT's transform, rather than
               giving it a pivot of its own.  The parent is not merely lifted:
               it is magnified about its bottom edge, so a point on it moves by
               an amount that grows with its distance from that edge.  An
               attachment has to follow the same map or it lands somewhere the
               body no longer is.

               `esi`/`edi` are the parent's position -- the attachment is built
               as `parent + offset` two instructions before the call -- and the
               parent's header is the one 0x40aa90 stashed at [ebp-0xc].  So
               everything needed is live at the hook, with no need to remember
               anything across calls (the child is drawn FIRST here, so there
               would be nothing to remember). */
            int px = (int)f[0];                  /* edi -- parent x */
            int py = (int)f[1];                  /* esi -- parent y */
            int pb = py - (int)ph[3] + (int)ph[1];    /* its pinned bottom */
            int tx = px + (((int)f[7] - px) * (int)s >> 16);
            int ty = pb - ((pb - (int)f[5]) * (int)s >> 16);


            /* Undo the helper's own hotspot magnification, so the hotspot
               itself lands on (tx,ty) rather than the sprite's corner. */
            f[7] = (unsigned int)(tx - (((int)hdr[2] * (int)s >> 16)
                                        - (int)hdr[2]));
            f[5] = (unsigned int)(ty - (((int)hdr[3] * (int)s >> 16)
                                        - (int)hdr[3]));
            return;
        }
    }

    /* x: keep the hotspot fixed. */
    f[7] -= (unsigned int)(((int)hdr[2] * (int)s >> 16) - (int)hdr[2]);

    /* y: keep the sprite's bottom edge -- the ground contact -- fixed. */
    f[5] += (unsigned int)(hy - (hy * (int)s >> 16));
}

static const unsigned char mdk_hotspot_stub[] = {
    0x83, 0xc4, 0x04,           // add   $0x4,%esp     (drop our return address)
    0x60,                       // pushad
    0x54,                       // push  %esp   -> &frame
    0xe8, 0, 0, 0, 0,           // call  MdkFixHotspot        rel32 <- @6
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0, 0, 0, 0, 0,              // <the helper's own first 5 bytes>   @14
    0xe9, 0, 0, 0, 0            // jmp   helper+5             rel32 <- @20
};
#define MDK_HOT_CALL_AT   6
#define MDK_HOT_DISP_AT  14
#define MDK_HOT_JMP_AT   20

//
// Reached by a JMP, not a call -- the original body is discarded entirely, so
// there is no return address to drop and nothing to displace.
//
static const unsigned char mdk_blit_stub[] = {
    0x60,                       // pushad
    0x54,                       // push  %esp   -> &frame
    0xe8, 0, 0, 0, 0,           // call  MdkBlitScaled        rel32 <- @3
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xe9, 0, 0, 0, 0            // jmp   the unlock+epilogue  rel32 <- @12
};
#define MDK_BLIT_CALL_AT  3
#define MDK_BLIT_JMP_AT  12

// ---------------------------------------------------------------------------
// K13.  The sky -- scale the panorama to the screen
// ---------------------------------------------------------------------------
//
// `0x471E80` draws the backdrop, and the reason the entry hook never logged it
// is instructive: its FIRST ARGUMENT IS NULL on this path.  `0x471EAA` tests
// exactly that pointer and, when it is null, jumps to `0x472172`, which
// substitutes a local rect copied from the zeroed constant at `0x471D20` --
// that is, position (0,0) -- and rejoins.  The diagnostic hook declined to
// dereference NULL and so said nothing, which read as "this routine never
// runs" when in fact it runs every frame.  Position (0,0) is also precisely why
// the sky sits in the corner.
//
// What it does, from `0x472206`-`0x472281`:
//
//     source   16bpp bitmap at [0x492CA8], row stride 0x552D90 = 1800 px,
//              height 0x552D94 = 360, horizontally scrolled by a byte offset
//              from the table at 0x552808 indexed by 0x552DA0
//     dest     lfbPtr + strideBytes*y + 2*x, straight `rep movsd` per row
//     size     w from *[ebp-0x28], rows from [ebp-0x38]..[ebp-0x24]
//
// So it is a 1:1 row copy of a 600x360 window onto an 1800x360 panorama, and it
// cannot fill 2560x1080 as it stands -- but the panorama is wide enough that a
// scaled copy can.  At S = H/360 = 3 the screen needs 2560/3 = 853 source
// columns of the 1800 available, and 1080/3 = 360 rows, which is the panorama's
// full height exactly.  Showing more of it horizontally is not a liberty: the
// world's field of view widened by the same factor, so the sky must widen with
// it or the two disagree.
//
// Hooked at `0x47220B`, immediately after the framebuffer lock, and rejoining
// at `0x472144`, the unlock both of the routine's blit loops jump to.  Same
// shape as K12 and for the same reason -- the lock, the scroll lookup and the
// unlock are all left to the game.
//
// Cost: this writes the whole screen rather than a 600x360 corner, ~5.5 MB a
// frame against 432 KB.  It is done as one built row per SOURCE row, memcpy'd S
// times, so the per-pixel scaling work happens 360 times a frame rather than
// 1080, and the rest is linear copies.  If it proves too slow on the Voodoo's
// write-combined aperture that will show as a frame-rate drop and nothing else.
//
#define MDK_SKY_SRC     0x00492ca8u     /* the panorama's pixels          */
#define MDK_SKY_STRIDE  0x00552d90u     /* its row stride, in pixels      */

static const unsigned char mdk_sky_sig[] =      /* 0x0047220b */
    "\x8b\x55\xbc\xd1\xfa\x8b";
static const unsigned char mdk_sky2_sig[] =     /* 0x00472094, wrapped case */
    "\x8b\x5d\xbc\xd1\xfb";
static const unsigned char mdk_skyend_sig[] =   /* 0x00472144 */
    "\xe8\xd7\xee\xff\xff\x80";

static unsigned short *g_mdkSkyRow = NULL;      /* one scaled row, W wide */

// ---------------------------------------------------------------------------
// K14.  The sky, stretched by the hardware
// ---------------------------------------------------------------------------
//
// Scaling the panorama on the CPU works and looks right, but it writes the
// whole screen every frame -- 5.3 MB against the 0.4 MB the shipped game wrote
// -- and the Voodoo's write-combined aperture is the bottleneck, not the
// arithmetic.  Stretching instead of scaling saves nothing: the cost is the
// bytes, and the bytes are the same however the pixels are worked out.
//
// So hand the stretch to the card.  The panorama is uploaded once as textures
// and drawn as a strip of quads; scrolling then costs two floats rather than a
// blit, and the CPU writes nothing per frame at all.
//
// MDK's own render state is already exactly what a decal quad wants -- its one
// `grTexCombine` is LOCAL/ZERO and its one `grColorCombine` is texture times
// one, and `grDepthMask` is set false once at init and never touched.  So the
// only state worth setting is clamping and filtering, and MDK's own state
// routine at 0x46F9D0 happens to re-issue precisely that set, which is what we
// call afterwards to put things back.
//
// Glide is reached through MDK's own import thunks (`jmp *[IAT]`).  Calling a
// thunk is identical to calling the function, and it keeps this file dependent
// on <windows.h> alone -- no Glide headers, no coupling to the wrapper.

#define MDK_T_TEXDOWNLOAD    0x00489750u   /* grTexDownloadMipMap  */
#define MDK_T_TEXSOURCE      0x004897b0u   /* grTexSource          */
#define MDK_T_DRAWTRIANGLE   0x004897a4u   /* grDrawTriangle       */
#define MDK_T_TEXCLAMPMODE   0x004897e0u   /* grTexClampMode       */
#define MDK_T_TEXFILTERMODE  0x004897e6u   /* grTexFilterMode      */
#define MDK_T_TEXMIPMAPMODE  0x004897dau   /* grTexMipMapMode      */
#define MDK_T_CHROMAKEYMODE  0x004897c8u   /* grChromakeyMode      */
#define MDK_T_COLORCOMBINE   0x004897ecu   /* grColorCombine       */
#define MDK_T_ALPHACOMBINE   0x004897c2u   /* grAlphaCombine       */
#define MDK_T_ALPHABLEND     0x004897bcu   /* grAlphaBlendFunction */
#define MDK_T_CONSTCOLOR     0x004897b6u   /* grConstantColorValue */
#define MDK_CACHE_CONST      0x00492c34u   /* MDK's cache of the above    */
#define MDK_CACHE_ACOMB      0x00492c40u   /* "alpha combine is set" flag */
#define MDK_CACHE_ABLEND     0x00492c44u   /* "alpha blend is set" flag   */

#define MDK_STATE_RESTORE    0x0046f9d0u   /* MDK re-establishes its own state */
#define MDK_CACHE_COMBINE    0x00492c3cu   /* ... once these are invalidated,  */
#define MDK_CACHE_TEX0       0x00492c30u   /* exactly as MDK does at 0x46FA51  */
#define MDK_CACHE_TEX1       0x00492c38u

#define MDK_SKY_ROWS_G       0x00552d94u   /* the panorama's height           */

/* MDK's import slots.  Patching the IAT rather than the call site means every
   consumer inside the game sees the reduced range, whichever path asks -- I
   was previously only shrinking the one argument handed to its cache
   initialiser, which assumes that is the only place the range is learnt. */
#define MDK_IAT_TEXMAX       0x0048a52cu   /* grTexMaxAddress      */
#define MDK_IAT_TEXMIN       0x0048a530u   /* grTexMinAddress      */
#define MDK_IAT_TEXDL        0x0048a520u   /* grTexDownloadMipMap  */

/* Glide2 texture constants.  GR_LOD_256 = 0, GR_ASPECT_1x1 = 3, 2x1 = 2, and
   GR_TEXFMT_RGB_565 = 0x0a -- 565 because MDK locks the framebuffer with
   writeMode 0, so its panorama is already in the framebuffer's format. */
#define MDK_TEX_LOD256       0u
#define MDK_TEX_1x1          3u
#define MDK_TEX_2x1          2u
#define MDK_TEXFMT_565       0x0au

/* 1800x360 as 8 columns of 256, in two bands: 256 tall then 128 tall.  Square
   tiles for the whole height would waste half the second band. */
#define MDK_SKY_TILE         256
#define MDK_SKY_B0BYTES      (MDK_SKY_TILE * 256 * 2)
#define MDK_SKY_B1BYTES      (MDK_SKY_TILE * 128 * 2)
#define MDK_SKY_COLBYTES     (MDK_SKY_B0BYTES + MDK_SKY_B1BYTES)

/* The panorama measured 1800 wide in the one level I logged, but that is a
   property of the level, not of the engine -- so the grid is sized from the
   stride the game reports rather than pinned to 8 columns.  Getting this
   wrong is not a graceful failure: `col` would run past the uploaded tiles
   and address texture memory belonging to MDK, which samples as whatever it
   last loaded there. */
#define MDK_SKY_MINCOLS      8
#define MDK_SKY_MAXCOLS      16

typedef struct {
    unsigned int smallLod, largeLod, aspect, format;
    void        *data;
} MdkTexInfo;

typedef struct { float sow, tow, oow; } MdkTmuVtx;

/* The Glide2 GrVertex, laid out here rather than included -- GLIDE_NUM_TMU is
   2, so this is the 60 bytes the wrapper's vertex layout describes. */
typedef struct {
    float     x, y, z, r, g, b, ooz, a, oow;
    MdkTmuVtx tmuvtx[2];
} MdkVtx;

typedef unsigned int (__stdcall *MdkTexAddrFn)(unsigned int);
typedef void (__stdcall *MdkTexDlFn)(unsigned int, unsigned int,
                                     unsigned int, MdkTexInfo *);

static MdkTexAddrFn          g_realTexMin     = NULL;
static MdkTexDlFn            g_realTexDl      = NULL;
static unsigned int          g_mdkTexLo       = 0;   /* the real range, as  */
static unsigned int          g_mdkTexHi       = 0;   /* the driver reports  */
static unsigned long         g_mdkDlCount     = 0;   /* MDK's own downloads */
static unsigned int          g_mdkDlHigh      = 0;   /* highest it wrote to */

static unsigned int          g_mdkSkyTexBase  = 0;
static int                   g_mdkSkyHW       = 0;
static int                   g_mdkSkyCols     = 0;   /* tiles actually held */
static int                   g_mdkSkyBusy     = 0;
static unsigned short       *g_mdkSkyTile     = NULL;   /* upload staging */
static const unsigned short *g_mdkSkyUploaded = NULL;

/* The last sky the game asked for, captured instead of blitted. */
static int                   g_skyPend = 0;
static unsigned int          g_mdkSkyHash = 0;
static int                   g_skyDx0, g_skyY0, g_skyRows;
static int                   g_skySrcRow, g_skyScroll, g_skySrcStride;
static const unsigned short *g_skySrc = NULL;

/* 0x471e80 is not only the sky.  The bullet cams call it too:
 *
 *     460ce2  mov  -0x10(%ebp),%eax     ; the cam viewport origin
 *     460ce5  mov  %ebx,%edx
 *     460ce7  call 0x471e80             ; <- 0x460cec
 *
 * and K14 replaced that routine's blit loops wholesale, taking its SOURCE
 * from the sky globals rather than from the call.  So a cam frame drew a
 * fragment of sky into the oval -- the yellow wedge -- and, worse, latched the
 * cam's own y0/rows/scroll into the sky state and re-armed its countdown,
 * every frame, from parameters that were never the sky's.
 *
 * A hook on a shared routine has to know which caller it is standing in for. */
static unsigned int g_mdkCamRotRet = 0;
static int          g_mdkSkyTookOver = 0;   /* stub reads this after popad */

static const unsigned char mdk_camrot_sig[] =    /* 0x00460ce2 */
    { 0x8b, 0x45, 0xf0, 0x89, 0xda, 0xe8 };
#define MDK_CAMROT_RET   (sizeof(mdk_camrot_sig) + 4)   /* past the call */

extern "C" void __attribute__((cdecl, used, noinline))
MdkSkyScaled(unsigned int *f)
{
    const unsigned short *src;
    const char           *fp;
    unsigned char        *lfb;
    const int            *size;
    unsigned short       *row = g_mdkSkyRow;
    int  stride, y0, y1, dx0, srcRow0, scrollPx, srcStride;
    int  dy, dx, rows, sy, prevSy = -1, W, H;
    unsigned int invS;

    g_mdkSkyTookOver = 1;         /* default: we stand in for the blit */

    if (!f || !row) return;

    fp   = (const char *)f[2];                    /* ebp */
    if (!fp) return;

    /* Not the sky -- this is a bullet cam asking 0x471e80 for its own overlay,
       the brown one that fades back to black after a hit.

       0149 declined and drew nothing, which turned it into a BLACK SQUARE:
       the stub jumped straight to the unlock, so declining meant the game did
       not get to draw either.  The stub now falls through to the original
       blit instead, so the cam draws its own overlay exactly as it always
       did, while we still keep our hands off the sky state -- which was the
       half that mattered, since these calls were latching the cam's geometry
       into it every frame. */
    if (g_mdkCamRotRet
        && !IsBadReadPtr(fp + 4, 4)
        && *(const unsigned int *)(fp + 4) == g_mdkCamRotRet) {
        /* It asked for a backdrop, so it is live: leave its oval alone.
           An IDLE cam never reaches here at all -- it asks 0x471e80 for
           nothing, which returns long before this hook -- and that silence
           is the only signal there is that it is idle. */
        int ci = MdkCamIndex((int)f[1] / 2);

        if (ci >= 0) g_mdkCamSky[ci] = 2;
        g_mdkSkyTookOver = 0;
        return;
    }

    lfb       = *(unsigned char *const *)(fp - 0x48);
    stride    = *(const int *)(fp - 0x44);        /* bytes */
    y0        = *(const int *)(fp - 0x38);
    y1        = *(const int *)(fp - 0x24);
    size      = *(const int *const *)(fp - 0x28);
    dx0       = (int)f[1] / 2;                    /* esi = 2*x            */
    srcRow0   = (int)f[6];                        /* ecx                  */
    scrollPx  = (int)f[0] / 2;                    /* edi = byte offset    */
    src       = *(const unsigned short *const *)MDK_SKY_SRC;
    srcStride = *(const int *)MDK_SKY_STRIDE;

    if (!lfb || !src || stride <= 0 || srcStride <= 0 || !size) return;

    /* K14.  With texture memory reserved the card does the stretching, so the
       whole point of this routine is to record what the game asked for and
       then draw nothing.  The blit below is the fallback for the case where
       there was not enough texture memory to reserve. */
    /* A panorama wider than the grid we hold takes the blit below instead --
       drawing it from tiles we never uploaded would sample MDK's own
       textures. */
    if (g_mdkSkyHW && srcStride <= g_mdkSkyCols * MDK_SKY_TILE) {
        g_skyDx0       = dx0;
        g_skyY0        = y0;
        g_skyRows      = y1 - y0;
        g_skySrcRow    = srcRow0;
        g_skyScroll    = scrollPx;
        g_skySrc       = src;
        g_skySrcStride = srcStride;
        /* A countdown rather than a flag: if some path I have not found ever
           skips a frame, the last sky is redrawn rather than the screen going
           black.  Two frames of grace, so it cannot leak into a menu. */
        g_skyPend      = 3;     /* frames of grace -- see the gate below.
                                   8 was enough to see the sky linger on the
                                   quit prompt; the game asks every frame, so
                                   3 is still slack. */
        return;
    }

    W = (int)g_targetW;
    H = (int)g_targetH;

    /* Source pixels per destination pixel, 16.16.  S = H/360. */
    invS = ((unsigned int)MDK_VIEW_H << 16) / (unsigned int)g_targetH;

    rows = (y1 - y0) * (int)g_targetH / MDK_VIEW_H;
    if (rows <= 0) return;
    if (y0 + rows > H) rows = H - y0;

    for (dy = 0; dy < rows; dy++) {
        int ry = y0 + dy;
        if (ry < 0) continue;

        sy = srcRow0 + (int)(((unsigned int)dy * invS) >> 16);

        /* Rebuild the scaled row only when the source row actually changes. */
        if (sy != prevSy) {
            const unsigned short *s = src + (unsigned int)sy * srcStride;

            for (dx = 0; dx + dx0 < W; dx++) {
                int sx = scrollPx + (int)(((unsigned int)dx * invS) >> 16);

                /* The panorama wraps; the shipped 600-column window never had
                   to, a 853-column one can. */
                while (sx >= srcStride) sx -= srcStride;
                row[dx] = s[sx];
            }
            prevSy = sy;
        }

        memcpy(lfb + ry * stride + dx0 * 2, row,
               (unsigned int)(W - dx0) * 2);
    }
}

static const unsigned char mdk_sky_stub[] = {
    0x60,                             // pushad
    0x54,                             // push  %esp   -> &frame
    0xe8, 0, 0, 0, 0,                 // call  MdkSkyScaled       rel32 <- @3
    0x83, 0xc4, 0x04,                 // add   $0x4,%esp
    0x61,                             // popad
    0x83, 0x3d, 0, 0, 0, 0, 0x00,     // cmpl  $0,g_mdkSkyTookOver addr <- @13
    0x74, 0x05,                       // je    +5   (we declined)
    0xe9, 0, 0, 0, 0,                 // jmp   the unlock         rel32 <- @21
    0, 0, 0, 0, 0,                    // the displaced bytes, from the SITE @25
    0xe9, 0, 0, 0, 0                  // jmp   back               rel32 <- @31
};
#define MDK_SKY_CALL_AT   3
#define MDK_SKY_FLAG_AT  13
#define MDK_SKY_JMP_AT   21
#define MDK_SKY_DISP_AT  25
#define MDK_SKY_BACK_AT  31

// --- K14a.  Reserve the texture memory -------------------------------------
//
// MDK works out how much texture memory it has as
// `grTexMaxAddress - grTexMinAddress`.  The first attempt shrank the size
// argument at the one call site that feeds its cache manager -- which works
// only if that is the sole place the range is learnt.  Answering short from
// grTexMaxAddress itself makes no such assumption, and it is a single IAT
// write rather than a code patch.

/* Reserve at the BOTTOM of texture memory, not the top.
 *
 * The top was the obvious place -- MDK allocates upward and never got near it
 * (its highest load was 1.1 MB against a reported 30.75 MB top).  But
 * "reported" is doing a lot of work in that sentence: this is a Voodoo 5 with
 * 32 MB a chip, and at 2560x1080 the framebuffer takes a large piece of it.
 * A texture address near the top of the reported range is not obviously backed
 * by texture memory at all, and sampling the framebuffer would look exactly
 * like what we were seeing -- the screen's own colours, changing with the
 * camera.
 *
 * The bottom carries no such doubt: it is where MDK's own textures live and
 * demonstrably works.  So we take the first slice and MDK starts above us --
 * `size = max - min` still comes out right because both ends come from here. */
static unsigned int __stdcall MdkTexMinAddress(unsigned int tmu)
{
    unsigned int max, min, size, need;
    int          cols;

    min = g_realTexMin ? g_realTexMin(tmu) : 0;
    max = (*(MdkTexAddrFn *)MDK_IAT_TEXMAX)(tmu);
    g_mdkTexLo = min;
    g_mdkTexHi = max;
    if (max <= min) return min;

    /* Take as wide a grid as fits, down to a floor of 8 columns, and only if
       MDK is left with plenty -- a sky is not worth starving its texture cache
       for, and without the reservation the fallback blit still works. */
    size = max - min;
    for (cols = MDK_SKY_MAXCOLS; cols >= MDK_SKY_MINCOLS; cols -= 4) {
        need = (unsigned int)cols * MDK_SKY_COLBYTES + 0x20000u;
        if (size >= need + 0x400000u) break;
    }
    if (cols < MDK_SKY_MINCOLS) return min;

    /* Deterministic in its inputs, so repeated calls give the same answer. */
    g_mdkSkyCols    = cols;
    g_mdkSkyTexBase = (min + 0x1ffffu) & ~0x1ffffu;
    g_mdkSkyHW      = 1;
    return min + need;
}

/* Measures the question the last build could not answer: does MDK ever write
   above the line?  Our own uploads bypass this and are not counted. */
static void __stdcall MdkTexDownload(unsigned int tmu, unsigned int addr,
                                     unsigned int evenOdd, MdkTexInfo *info)
{
    g_mdkDlCount++;
    if (addr > g_mdkDlHigh) g_mdkDlHigh = addr;
    if (g_realTexDl) g_realTexDl(tmu, addr, evenOdd, info);
}

static unsigned int MdkSkyTileAddr(int col, int band)
{
    /* Grouped by band, not interleaved by column.  A texture's base has to be
       aligned to its own size, and the tiles are two different sizes -- laid
       out column by column the stride is 0x30000, so every other 256x256 tile
       lands on a 0x10000 boundary instead of 0x20000 and does not bind, which
       leaves whatever was bound before.  Grouping keeps 0x20000-sized tiles in
       a 0x20000-aligned run and 0x10000-sized ones after it, so every tile is
       naturally aligned.  Same total size. */
    if (col < 0) col = 0;
    if (col >= g_mdkSkyCols) col = g_mdkSkyCols - 1;

    if (!band)
        return g_mdkSkyTexBase + (unsigned int)col * MDK_SKY_B0BYTES;

    return g_mdkSkyTexBase
         + (unsigned int)g_mdkSkyCols * MDK_SKY_B0BYTES
         + (unsigned int)col * MDK_SKY_B1BYTES;
}

/* Cut the panorama into tiles and hand each to the card.  The lower band is
   256x128 and the upper 256x256, which is why the two carry different texture
   info; the addresses come from MdkSkyTileAddr so upload and draw cannot
   disagree about where a tile lives. */
static void MdkSkyUpload(const unsigned short *src, int srcStride, int srcRows)
{
    MdkTexInfo info;
    int col, band, x, y;

    if (!g_mdkSkyTile || srcRows <= 0 || !g_realTexDl) return;

    for (col = 0; col < g_mdkSkyCols; col++) {
        for (band = 0; band < 2; band++) {
            int th = band ? 128 : 256;

            for (y = 0; y < th; y++) {
                const unsigned short *s;
                int sy = band * 256 + y;

                /* The panorama is 1800x360; the tiles cover 2048x384.  Clamp
                   rather than wrap so a tile's own edge never bleeds. */
                if (sy >= srcRows) sy = srcRows - 1;
                s = src + (unsigned int)sy * (unsigned int)srcStride;

                for (x = 0; x < MDK_SKY_TILE; x++) {
                    int sx = col * MDK_SKY_TILE + x;
                    if (sx >= srcStride) sx = srcStride - 1;
                    g_mdkSkyTile[y * MDK_SKY_TILE + x] = s[sx];
                }
            }

            info.smallLod = MDK_TEX_LOD256;
            info.largeLod = MDK_TEX_LOD256;
            info.aspect   = band ? MDK_TEX_2x1 : MDK_TEX_1x1;
            info.format   = MDK_TEXFMT_565;
            info.data     = g_mdkSkyTile;
            g_realTexDl(0, MdkSkyTileAddr(col, band), 3, &info);
        }
    }
}

static void MdkSkyQuad(unsigned int addr, unsigned int aspect,
                       float x0, float y0, float x1, float y1,
                       float s0, float t0, float s1, float t1)
{
    typedef void (__stdcall *TexSrc)(unsigned int, unsigned int,
                                     unsigned int, MdkTexInfo *);
    typedef void (__stdcall *DrawTri)(const MdkVtx *, const MdkVtx *,
                                      const MdkVtx *);
    MdkTexInfo info;
    MdkVtx     v[4];
    int        i;

    info.smallLod = MDK_TEX_LOD256;
    info.largeLod = MDK_TEX_LOD256;
    info.aspect   = aspect;
    info.format   = MDK_TEXFMT_565;
    info.data     = NULL;
    ((TexSrc)MDK_T_TEXSOURCE)(0, addr, 3, &info);

    for (i = 0; i < 4; i++) {
        v[i].z   = 0.0f;
        v[i].r   = 255.0f; v[i].g = 255.0f; v[i].b = 255.0f; v[i].a = 255.0f;
        /* Depth writes are off game-wide, so a near depth simply passes
           whatever test is in force and cannot occlude the world. */
        v[i].ooz = 0.0f;
        v[i].oow = 1.0f;
        v[i].tmuvtx[0].oow = 1.0f;
        v[i].tmuvtx[1].sow = 0.0f;
        v[i].tmuvtx[1].tow = 0.0f;
        v[i].tmuvtx[1].oow = 1.0f;
    }
    v[0].x = x0; v[0].y = y0; v[0].tmuvtx[0].sow = s0; v[0].tmuvtx[0].tow = t0;
    v[1].x = x1; v[1].y = y0; v[1].tmuvtx[0].sow = s1; v[1].tmuvtx[0].tow = t0;
    v[2].x = x1; v[2].y = y1; v[2].tmuvtx[0].sow = s1; v[2].tmuvtx[0].tow = t1;
    v[3].x = x0; v[3].y = y1; v[3].tmuvtx[0].sow = s0; v[3].tmuvtx[0].tow = t1;

    ((DrawTri)MDK_T_DRAWTRIANGLE)(&v[0], &v[1], &v[2]);
    ((DrawTri)MDK_T_DRAWTRIANGLE)(&v[0], &v[2], &v[3]);
}

static void MdkSkyDrawHW(int force)
{
    typedef void (__stdcall *Mode1)(unsigned int);
    typedef void (__stdcall *Mode3)(unsigned int, unsigned int, unsigned int);
    typedef void (__stdcall *Mode4)(unsigned int, unsigned int, unsigned int,
                                    unsigned int);
    typedef void (__stdcall *Mode5)(unsigned int, unsigned int, unsigned int,
                                    unsigned int, unsigned int);
    unsigned int savedConst;
    int   srcRows, sx, sx0, sx1, W;
    float S;

    /* The sky is a backdrop: it has to be under every frame, whether or not
       the game asked for one that frame.  Waiting to be asked is what left
       frames with nothing behind the world -- which is the black.  The
       parameters are latched, so an unasked frame simply redraws the last
       sky; only the scroll is a frame late, on something that takes seconds
       to cross the screen.
       Gated on the panorama stride, which is 0 outside a level, so this
       cannot paint over a menu or a loading screen. */
    /* CORRECTED.  The gate above was the panorama stride, on the reasoning
       that it is 0 outside a level so this could not paint over a menu.  The
       IN-GAME menu is inside a level -- the stride is still set -- so the
       quit prompt, which should be black, was drawn over a full sky.  The
       three bullet-cam ovals were the same fault seen from the other side.

       And the countdown never actually stopped anything: the exhausted branch
       fell through and drew anyway, and `force` skipped it outright, so the
       sky was redrawn after every clear for as long as a level was loaded --
       the diagnostic counted that branch firing once a frame.

       Now: the sky is drawn only while the game has recently ASKED for one.
       The countdown is what makes an unasked frame redraw the last sky rather
       than flash black, which is what it was for; it just has to expire. */
    if (g_skyPend > 0) {
        if (!force) g_skyPend--;        /* aged once a frame, by the tick */
    } else {
        return;
    }
    if (!g_skySrc || *(const int *)MDK_SKY_STRIDE == 0) return;

    if (!g_mdkSkyHW || !g_skySrc || g_skySrcStride <= 0 || g_skyRows <= 0)
        return;

    srcRows = *(const int *)MDK_SKY_ROWS_G;
    if (srcRows <= 0) return;

    /* Keyed on the CONTENT, not just the address.
     *
     * Load level 2, quit, load level 1, and level 2's sky was still on screen:
     * MDK frees and reallocates the panorama, the new one can land at the same
     * address, and an upload keyed on the pointer alone then decides there is
     * nothing to do.  A cheap spread hash costs 64 reads a frame and cannot be
     * fooled that way; the dimensions are folded in for the case where the
     * content matches but the shape does not. */
    {
        unsigned int hash = (unsigned int)g_skySrcStride * 2654435761u
                          + (unsigned int)srcRows;
        unsigned int span = (unsigned int)srcRows
                          * (unsigned int)g_skySrcStride;
        unsigned int i;

        for (i = 0; i < 64; i++)
            hash = hash * 131u + g_skySrc[(span / 64u) * i];

        if (g_mdkSkyUploaded != g_skySrc || g_mdkSkyHash != hash) {
            MdkSkyUpload(g_skySrc, g_skySrcStride, srcRows);
            g_mdkSkyUploaded = g_skySrc;
            g_mdkSkyHash     = hash;
        }
    }

    W = (int)g_targetW;
    S = (float)(int)g_targetH / (float)MDK_VIEW_H;   /* dest px per source px */
    if (S <= 0.0f) return;

    /* The snipe clamp that used to live here is GONE, and its reasoning was
       the mistake.  It raised the sky's first source row to the sniper
       view's top edge so the three cam ovals above it stayed black -- but
       black is not the ovals' own job to arrange, it is the COVER sprite's,
       and that is drawn over them every frame a shot is not in flight.
       Keeping the sky out of them as well meant a cam that WAS following a
       projectile showed its world geometry against nothing: right shapes,
       black sky.

       MDK's own model is one backdrop for the whole window with every
       viewport rendering over it, which is what this now reproduces.  The
       ordering makes it safe: the sky is drawn at the frame's first clear,
       the covers after it, and the scope body and overlay after those -- so
       the sky can only ever be seen through a hole nothing else filled. */

    /* The source window the screen needs: 2560/3 = 853 of the 1800 available
       at 2560x1080.  One spare column each side covers the rounding. */
    sx0 = g_skyScroll;
    sx1 = sx0 + (int)((float)(W - g_skyDx0) / S) + 2;

    /* Say what we want rather than inheriting it.  MDK's own colour combine
       is SCALE_OTHER(CONSTANT) -- flat constant colour, not texture -- and it
       reaches for the constant colour again for every spark and every splash
       of blood.  Drawing the sky in whatever state the last primitive left
       behind is why it wore their colours. */
    ((Mode5)MDK_T_COLORCOMBINE)(3, 8, 1, 1, 0);      /* SCALE_OTHER(TEXTURE) */
    ((Mode5)MDK_T_ALPHACOMBINE)(1, 0, 1, 2, 0);      /* LOCAL(CONSTANT)      */
    /* MDK CACHES the constant colour at 0x492c34 and skips the call when the
       cache already matches.  Its state-restore re-issues the colour combine,
       both texture modes, the chromakey, the alpha combine and the alpha
       blend -- but NOT this one.  So setting it here and walking away leaves
       the hardware white while MDK believes its own value is still loaded,
       and everything it draws afterwards wears our white until it happens to
       want a different colour.  Saved and put back below. */
    savedConst = *(const unsigned int *)MDK_CACHE_CONST;
    ((Mode1)MDK_T_CONSTCOLOR)(0xffffffffu);          /* opaque white         */
    ((Mode4)MDK_T_ALPHABLEND)(4, 0, 4, 0);           /* ONE/ZERO -- opaque   */
    ((Mode1)MDK_T_CHROMAKEYMODE)(0);                 /* GR_CHROMAKEY_DISABLE */
    ((Mode3)MDK_T_TEXCLAMPMODE)(0, 1, 1);            /* CLAMP both ways      */
    ((Mode3)MDK_T_TEXFILTERMODE)(0, 1, 1);           /* BILINEAR             */
    ((Mode3)MDK_T_TEXMIPMAPMODE)(0, 0, 0);           /* GR_MIPMAP_DISABLE    */

    for (sx = sx0; sx < sx1; ) {
        int wrapped = sx % g_skySrcStride;
        int col     = wrapped / MDK_SKY_TILE;
        int inTile  = wrapped - col * MDK_SKY_TILE;
        int avail   = MDK_SKY_TILE - inTile;               /* to tile's end   */
        int toWrap  = g_skySrcStride - wrapped;            /* to panorama end */
        int n       = avail < toWrap ? avail : toWrap;
        int band;
        float dx0, dx1;

        if (n > sx1 - sx) n = sx1 - sx;
        if (n <= 0) break;

        dx0 = (float)g_skyDx0 + (float)(sx     - sx0) * S;
        dx1 = (float)g_skyDx0 + (float)(sx + n - sx0) * S;

        for (band = 0; band < 2; band++) {
            int   bandTop  = band * 256;
            int   bandRows = band ? 128 : 256;
            int   ya = g_skySrcRow;
            int   yb = g_skySrcRow + g_skyRows;

            float dy0, dy1;

            if (yb > srcRows) yb = srcRows;
            if (ya < bandTop) ya = bandTop;
            if (yb > bandTop + bandRows) yb = bandTop + bandRows;
            if (yb <= ya) continue;

            dy0 = (float)g_skyY0 + (float)(ya - g_skySrcRow) * S;
            dy1 = (float)g_skyY0 + (float)(yb - g_skySrcRow) * S;

            MdkSkyQuad(MdkSkyTileAddr(col, band),
                       band ? MDK_TEX_2x1 : MDK_TEX_1x1,
                       dx0, dy0, dx1, dy1,
                       (float)inTile,     (float)(ya - bandTop),
                       (float)(inTile+n), (float)(yb - bandTop));
        }

        sx += n;
    }

    /* Put MDK's state back.  0x46F9D0 re-issues the filter, clamp, mipmap,
       texcombine and chromakey calls we touched -- exactly this set -- once
       its own caches say they are stale. */
    /* Put the constant colour back to what MDK's cache says it is, so the two
       agree again.  Restoring the value rather than invalidating the cache
       means MDK does not have to re-issue anything. */
    ((Mode1)MDK_T_CONSTCOLOR)(savedConst);

    /* The alpha pair are guarded by FLAGS, not by value:
     *
     *     46fa76  call grAlphaCombine(3,8,1,2,0)
     *     46fa7b  movl $1,0x492c40          <- "already set"
     *     46fa92  call grAlphaBlendFunction(4,0,4,0)
     *     46fa97  movl $1,0x492c44
     *
     * and the restore skips each call while its flag reads 1.  We change the
     * alpha combine to LOCAL(CONSTANT) and left both flags standing, so MDK
     * went on believing its own SCALE_OTHER was loaded and never re-issued
     * it.  Everything it drew afterwards took its alpha from the constant
     * colour instead -- which is why the enemy health fill came out almost
     * transparent instead of bright green.
     *
     * Clearing the flags is the same fix as invalidating the three caches
     * below, and the same lesson as the constant colour a few builds ago:
     * a cache we do not own has to be told when we have made it wrong. */
    *(unsigned int *)MDK_CACHE_ACOMB   = 0;
    *(unsigned int *)MDK_CACHE_ABLEND  = 0;

    *(unsigned int *)MDK_CACHE_COMBINE = 0;
    *(unsigned int *)MDK_CACHE_TEX0    = 0;
    *(unsigned int *)MDK_CACHE_TEX1    = 0xffffffffu;
    ((void (*)(void))MDK_STATE_RESTORE)();
}

static void InstallMdkTexReserve(unsigned char *code, unsigned int codeSize)
{
    unsigned char ptr[4];

    (void)code; (void)codeSize;

    g_mdkSkyTile = (unsigned short *)VirtualAlloc(
        NULL, MDK_SKY_TILE * 256 * 2, MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkSkyTile) return;

    /* The originals have to be captured before the slots are overwritten, and
       the download slot before the max-address one -- MDK can call the latter
       as soon as it has a context. */
    g_realTexDl  = *(MdkTexDlFn   *)MDK_IAT_TEXDL;
    g_realTexMin = *(MdkTexAddrFn *)MDK_IAT_TEXMIN;
    if (!g_realTexDl || !g_realTexMin) return;

    PutU32(ptr, (unsigned int)(unsigned long)&MdkTexDownload);
    if (!WriteCode((unsigned char *)MDK_IAT_TEXDL, ptr, 4)) return;

    PutU32(ptr, (unsigned int)(unsigned long)&MdkTexMinAddress);
    WriteCode((unsigned char *)MDK_IAT_TEXMIN, ptr, 4);
}

// ---------------------------------------------------------------------------
// K9.  Clear the frame
// ---------------------------------------------------------------------------
//
// MDK never clears the framebuffer.  It does not need to at 640x480, because
// its backdrop covers the whole 600x360 3D window -- so the clear routine it
// does have (`0x46E6E8`: clip, grBufferClear, clip) is called at transitions
// rather than per frame.
//
// Once the window is the whole screen that assumption fails: the backdrop is
// drawn by `0x471E80` from a 1800x360 source and covers only part of the
// screen, so everything else keeps whatever the previous frame left there.
// That is the smearing.
//
// The backdrop cannot be made to cover 2560x1080 -- there is no more image --
// so the honest fix is to clear what it does not reach.  `0x46E6E8` takes no
// arguments, returns normally, and after K1 its clip window is already the full
// screen, so it is called as-is from the frame boundary rather than
// reimplemented.  Its address is taken from the K1 span that overlaps it, which
// means it is only ever called if that span matched.
//
// (`g_mdkClear` is declared with the other MDK data, above.)

//
// K6: scale the main-view camera viewport by H/360.
//
// Every field is the shipped value times H/360, so the value is taken from the
// signature rather than from a table -- see the note above on why that is both
// shorter and safer.
//
static void InstallMdkCamera(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at[MDK_CAMERA_SPANS];
    unsigned char  repl[MDK_SPAN_MAX];
    unsigned int   i, j, found = 0;

    for (i = 0; i < MDK_CAMERA_SPANS; i++) {
        at[i] = FindUnique(code, codeSize, mdk_camera[i].sig, mdk_camera[i].len);
        if (at[i]) found++;
    }

    if (found == 0) return;                     // already applied
    if (found != MDK_CAMERA_SPANS) return;      // partial: refuse

    for (i = 0; i < MDK_CAMERA_SPANS; i++) {
        const MdkSpan *s = &mdk_camera[i];

        memcpy(repl, s->sig, s->len);
        for (j = 0; j < s->fieldCount; j++) {
            unsigned int at_ = s->fields[j].at;
            unsigned int v   = ReadU32(s->sig + at_);

            PutU32(repl + at_, v * g_targetH / MDK_VIEW_H);
        }
        WriteCode(at[i], repl, s->len);
    }
}

//
// K7: give the sprite clip the real screen width.
//
static void InstallMdkSpriteClip(unsigned char *code, unsigned int codeSize)
{
    unsigned char  repl[sizeof(mdk_clipw_sig) - 1];
    unsigned char *at;

    at = FindUnique(code, codeSize, mdk_clipw_sig, sizeof(mdk_clipw_sig) - 1);
    if (!at) return;

    repl[0] = 0xba;                     // mov $W,%edx  -- same length as the
    PutU32(repl + 1, g_targetW);        // 6-byte load it replaces, less one
    repl[5] = 0x90;                     // nop
    repl[6] = 0x29;                     // sub %esi,%edx  (kept)
    repl[7] = 0xf2;

    WriteCode(at, repl, sizeof(repl));
}

//
// K12: replace the 1:1 sprite blit with a magnifying one.
//
// Fails closed as a unit -- a hotspot correction without the magnified blit,
// or the reverse, is worse than neither.
//
static void InstallMdkSpriteScale(unsigned char *code, unsigned int codeSize)
{
    unsigned char *hot, *blit, *end, *stub;
    unsigned char  tmpl[sizeof(mdk_hotspot_stub)];
    unsigned char  patch[5];
    int            rel;

    g_mdkSpriteScale = ((unsigned int)g_targetH << 16) / MDK_VIEW_H;
    if (g_mdkSpriteScale <= 0x10000u) return;       // nothing to magnify

    hot  = FindUnique(code, codeSize, mdk_hotspot_sig, sizeof(mdk_hotspot_sig) - 1);
    blit = FindUnique(code, codeSize, mdk_blit_sig,    sizeof(mdk_blit_sig)    - 1);
    end  = FindUnique(code, codeSize, mdk_blitend_sig, sizeof(mdk_blitend_sig) - 1);
    if (!hot || !blit || !end) return;

    /* --- the magnified blit: jump in, and rejoin at the unlock ------------ */
    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_blit_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_blit_stub, sizeof(mdk_blit_stub));
    rel = (int)((unsigned char *)&MdkBlitScaled - (stub + MDK_BLIT_CALL_AT + 4));
    PutU32(stub + MDK_BLIT_CALL_AT, (unsigned int)rel);
    rel = (int)(end - (stub + MDK_BLIT_JMP_AT + 4));
    PutU32(stub + MDK_BLIT_JMP_AT, (unsigned int)rel);

    patch[0] = 0xe9;                                /* jmp rel32 -> stub */
    rel = (int)(stub - (blit + 5));
    PutU32(patch + 1, (unsigned int)rel);
    if (!WriteCode(blit, patch, 5)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return;
    }

    /* --- K22: which caller is the mouse cursor ---------------------------- */
    {
        unsigned char *cur = FindUnique(code, codeSize, mdk_cursor_sig,
                                        sizeof(mdk_cursor_sig) - 1);

        /* Not fail-closed: without it the cursor merely stays where it has
           been, which is the state this build inherits rather than a broken
           one.  Zero here disables the offset by itself. */
        g_mdkCursorRet = cur ? (unsigned int)(cur + MDK_CURSOR_RET) : 0;

        cur = FindUnique(code, codeSize, mdk_ammo_sig,
                         sizeof(mdk_ammo_sig) - 1);
        g_mdkAmmoRet = cur ? (unsigned int)(cur + MDK_AMMO_RET) : 0;

        cur = FindUnique(code, codeSize, mdk_attach_sig,
                         sizeof(mdk_attach_sig));
        g_mdkAttachRet = cur ? (unsigned int)(cur + MDK_ATTACH_RET) : 0;

    }

    /* --- the anchor correction ------------------------------------------- */
    memcpy(tmpl, mdk_hotspot_stub, sizeof(tmpl));
    memcpy(tmpl + MDK_HOT_DISP_AT, mdk_hotspot_sig, 5);

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(tmpl),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, tmpl, sizeof(tmpl));
    rel = (int)((unsigned char *)&MdkFixHotspot - (stub + MDK_HOT_CALL_AT + 4));
    PutU32(stub + MDK_HOT_CALL_AT, (unsigned int)rel);
    rel = (int)((hot + 5) - (stub + MDK_HOT_JMP_AT + 4));
    PutU32(stub + MDK_HOT_JMP_AT, (unsigned int)rel);

    patch[0] = 0xe8;                                /* call rel32 -> stub */
    rel = (int)(stub - (hot + 5));
    PutU32(patch + 1, (unsigned int)rel);
    if (!WriteCode(hot, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

//
// K13: scale the sky panorama to the screen.
//
/* One hook per blit loop.  It jumps to the shared unlock when we drew the sky
   ourselves, and falls through to the game's own blit when we declined --
   which is what the bullet cams need, since they call this routine for their
   own overlay.  The displaced bytes are copied from the SITE, per section 5b,
   because the two loops do not start with the same instructions. */
static int MdkHookSkyLoop(unsigned char *at, unsigned char *end)
{
    unsigned char *stub;
    unsigned char  patch[5];
    int            rel;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_sky_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return 0;

    memcpy(stub, mdk_sky_stub, sizeof(mdk_sky_stub));
    memcpy(stub + MDK_SKY_DISP_AT, at, 5);      /* from the site, not a table */
    rel = (int)((unsigned char *)&MdkSkyScaled - (stub + MDK_SKY_CALL_AT + 4));
    PutU32(stub + MDK_SKY_CALL_AT, (unsigned int)rel);
    PutU32(stub + MDK_SKY_FLAG_AT, (unsigned int)&g_mdkSkyTookOver);
    rel = (int)(end - (stub + MDK_SKY_JMP_AT + 4));
    PutU32(stub + MDK_SKY_JMP_AT, (unsigned int)rel);
    rel = (int)((at + 5) - (stub + MDK_SKY_BACK_AT + 4));
    PutU32(stub + MDK_SKY_BACK_AT, (unsigned int)rel);

    patch[0] = 0xe9;                            /* jmp rel32 -> stub */
    rel = (int)(stub - (at + 5));
    PutU32(patch + 1, (unsigned int)rel);

    if (!WriteCode(at, patch, 5)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return 0;
    }
    return 1;
}

//
// K16.  The sprite-path drawer still lays out in the old 600x360 view.
//
// 0x410308 positions through the sprite blitter, and every constant in it is
// in the shipped view's terms:
//
//     x = (v * -0.1 + 0.8) * scale + 300      300 = 600/2, the old centre
//     x default = 300                          (mov edx,0x12c)
//     y = v * -90 + 270,  and + 64             360-tall coordinates
//
// This is the same fault K10 corrected for the player character, at the two
// call sites that fix never revisited -- which is why health, the weapon icon,
// the intro and the in-game items all sit in a cluster left of middle at their
// original size.  Scaling the offsets by H/360 and replacing the centres with
// the real one makes the whole path follow the screen.
//
// Every constant is referenced by exactly one instruction, so they are patched
// by value; each write checks the shipped value first, which makes it
// idempotent and a no-op at 640x480.
//
static int MdkPutFloat(unsigned int va, float want, float expect)
{
    unsigned char buf[4];
    float cur;

    memcpy(&cur, (const void *)va, sizeof(cur));
    if (cur != expect) return 0;
    memcpy(buf, &want, sizeof(buf));
    return WriteCode((unsigned char *)va, buf, sizeof(buf)) ? 1 : 0;
}

static int MdkPutDouble(unsigned int va, double want, double expect)
{
    unsigned char buf[8];
    double cur;

    memcpy(&cur, (const void *)va, sizeof(cur));
    if (cur != expect) return 0;
    memcpy(buf, &want, sizeof(buf));
    return WriteCode((unsigned char *)va, buf, sizeof(buf)) ? 1 : 0;
}

//
// K17.  Pin two sprite-path elements to the corners.  OFF BY DEFAULT.
//
// Written to place health and the weapon icon, and aimed at the wrong thing:
// these two call sites are the INTRO FX WINDOW -- the flight into the map at
// the start of a level -- drawn as two halves.  Pinning them to opposite
// corners tore it down the middle.
//
// K16 already centres that window, by replacing the old 600-wide view's centre
// with the real one, so the correct treatment here is to leave it alone.  Kept
// behind patches=16383 because the mechanism is right even though the target
// was not; health is drawn somewhere else entirely and is still open.
//
// K16 makes this path scale, but these two are centre-relative in the game's
// own layout and health sits LEFT of centre there, so no scale factor puts it
// on the right.  Their positions are therefore replaced outright, at the two
// call sites, using the destination size the blitter is about to use so the
// element sits against the edge whatever its size.
//
// The struct the blitter is handed is {x, y, srcW, srcH, scaleX, scaleY} with
// the scales in 8.8, so the drawn size is (scale * src) >> 8.
//
#define MDK_SPR_MARGIN   24
#define MDK_SPR_ICON_X   64

static const unsigned char mdk_sprA_sig[] =   /* 0x0041039b, call at +3 */
    "\x8d\x45\xd0\xe8\x4d\x42\xff\xff";
static const unsigned char mdk_sprB_sig[] =   /* 0x00410453, call at +3 */
    "\x8d\x45\xd0\xe8\x95\x41\xff\xff";

/* The intro's inner parallax layers, as a percentage of stock.  267 is the
   world's own H/360 at 1920x800, which is what makes the moon and the Earth
   agree with the geometry behind them, and it is confirmed on hardware --
   measured, not eyeballed: Earth-to-moon comes out 5.77 against stock's 5.72.
   It was an ini key while it was being tuned; it is a constant now, so the
   compiler folds both multiplies below. */
static const int g_mdkIntroScale = 267;

extern "C" void __attribute__((cdecl, used, noinline))
MdkPlaceSprite(int *d, unsigned int which)
{
    int dw, dh;

    if (!d || IsBadWritePtr(d, 6 * sizeof(int))) return;

    dw = (d[4] * d[2]) >> 8;
    dh = (d[5] * d[3]) >> 8;
    if (dw < 0) dw = 0;
    if (dh < 0) dh = 0;

    d[0] = which ? MDK_SPR_ICON_X
                 : (int)g_targetW - MDK_SPR_MARGIN - dw;
    d[1] = (int)g_targetH - MDK_SPR_MARGIN - dh;
}

static const unsigned char mdk_spr_stub[] = {
    0x60,                       // pushad
    0x6a, 0x00,                 // push  $which                    <- @2
    0x50,                       // push  %eax   -> the arg struct
    0xe8, 0, 0, 0, 0,           // call  MdkPlaceSprite            <- @5
    0x83, 0xc4, 0x08,           // add   $0x8,%esp
    0x61,                       // popad
    0xe9, 0, 0, 0, 0            // jmp   the real blitter          <- @14
};
#define MDK_SPR_WHICH_AT   2
#define MDK_SPR_CALL_AT    5
#define MDK_SPR_JMP_AT    14

static void InstallMdkSprPin(unsigned char *code, unsigned int codeSize,
                             const unsigned char *sig, unsigned int len,
                             unsigned int which)
{
    unsigned char  t[sizeof(mdk_spr_stub)];
    unsigned char  patch[5];
    unsigned char *at, *stub, *target;
    int            rel;

    at = FindUnique(code, codeSize, sig, len);
    if (!at) return;
    at    += 3;                                   /* the call */
    target = at + 5 + (int)ReadU32(at + 1);

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(t), MEM_COMMIT,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(t, mdk_spr_stub, sizeof(t));
    t[MDK_SPR_WHICH_AT] = (unsigned char)which;
    rel = (int)((unsigned char *)&MdkPlaceSprite - (stub + MDK_SPR_CALL_AT + 4));
    PutU32(t + MDK_SPR_CALL_AT, (unsigned int)rel);
    rel = (int)(target - (stub + MDK_SPR_JMP_AT + 4));
    PutU32(t + MDK_SPR_JMP_AT, (unsigned int)rel);
    memcpy(stub, t, sizeof(t));

    patch[0] = 0xe8;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5)) VirtualFree(stub, 0, MEM_RELEASE);
}

//
// K19.  Centre the fixed cutscene / loading art.
//
// 0x46E4D4 draws MDK's 640x480 stills -- the intro fly-in background among
// them -- through the RAW framebuffer lock, which returns the buffer's origin
// with no viewport offset applied.  That is why it sits against the top-left
// corner while everything on the other 2D path is centred.
//
// This path was deliberately left alone until now because its lock had two
// other users whose extent came from level data.  Both turned out to be the
// sky, which is handled separately, so the remaining users of that lock are
// exactly this blitter's two calls -- and it can now be keyed on them safely.
//
// The clip window it sets (0,0,640,480) does not constrain this: the art is
// blitted by the CPU straight into the framebuffer, and grClipWindow governs
// rasterisation, not LFB writes.  So only the base has to move.
//
#define MDK_ART_W   640
#define MDK_ART_H   480

static const unsigned char mdk_rawbase_sig[] =   /* 0x00470ff8, load at +5 */
    "\xe8\x25\x88\x01\x00\xa1\x54\x6a";
static const unsigned char mdk_art1_sig[] =      /* 0x0046e52a, returns +5 */
    "\xe8\xa9\x2a\x00\x00\x8b";
static const unsigned char mdk_art2_sig[] =      /* 0x0046e5ad, returns +5 */
    "\xe8\x26\x2a\x00\x00\x89";
#define MDK_RAWBASE_AT   5

static unsigned int g_mdkArtRet1 = 0;
static unsigned int g_mdkArtRet2 = 0;
static int          g_mdkArtCx   = 0;
static int          g_mdkArtCy   = 0;

/* Returns the framebuffer base the caller should use.  Deliberately not a
   pushad stub: the instruction replaced is the load of that base into eax, so
   the value has to come back in eax.  ecx and edx are already dead here -- the
   grLfbLock call two instructions earlier clobbered them -- and everything the
   routine still needs is in callee-saved registers. */
extern "C" unsigned int __attribute__((cdecl, used, noinline))
MdkRawBase(unsigned int caller)
{
    unsigned int base;

    if (!g_mdkLfbPtr || !g_mdkStride) return 0;
    base = *g_mdkLfbPtr;

    /* K19.  Only the 640x480 art blitter is centred; the raw lock's other
       users -- the sky and the rotated-sprite drawer -- own the whole screen
       and must keep the buffer origin. */
    if (caller == g_mdkArtRet1 || caller == g_mdkArtRet2)
        base += (unsigned int)(g_mdkArtCy * (int)*g_mdkStride)
              + (unsigned int)(2 * g_mdkArtCx);
    return base;
}

static const unsigned char mdk_rawbase_stub[] = {
    0xff, 0x75, 0x04,           // push  0x4(%ebp)   -> the caller
    0xe8, 0, 0, 0, 0,           // call  MdkRawBase                <- @4
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0xc3                        // ret   (base left in eax)
};
#define MDK_RAWBASE_CALL_AT  4

static void InstallMdkArtCentre(unsigned char *code, unsigned int codeSize)
{
    unsigned char  t[sizeof(mdk_rawbase_stub)];
    unsigned char  patch[5];
    unsigned char *at, *a1, *a2, *stub;
    int            rel;

    g_mdkArtCx = ((int)g_targetW - MDK_ART_W) / 2;
    g_mdkArtCy = ((int)g_targetH - MDK_ART_H) / 2;
    if (g_mdkArtCx < 0) g_mdkArtCx = 0;
    if (g_mdkArtCy < 0) g_mdkArtCy = 0;
    if (!g_mdkArtCx && !g_mdkArtCy) return;

    /* Fails closed: without both callers the hook cannot tell the art from the
       sky, and offsetting the sky's base would put it somewhere arbitrary. */
    a1 = FindUnique(code, codeSize, mdk_art1_sig, sizeof(mdk_art1_sig) - 1);
    a2 = FindUnique(code, codeSize, mdk_art2_sig, sizeof(mdk_art2_sig) - 1);
    at = FindUnique(code, codeSize, mdk_rawbase_sig, sizeof(mdk_rawbase_sig) - 1);

    if (!a1 || !a2 || !at || !g_mdkLfbPtr || !g_mdkStride) return;

    g_mdkArtRet1 = (unsigned int)(a1 + 5);
    g_mdkArtRet2 = (unsigned int)(a2 + 5);
    at += MDK_RAWBASE_AT;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(t), MEM_COMMIT,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(t, mdk_rawbase_stub, sizeof(t));
    rel = (int)((unsigned char *)&MdkRawBase - (stub + MDK_RAWBASE_CALL_AT + 4));
    PutU32(t + MDK_RAWBASE_CALL_AT, (unsigned int)rel);
    memcpy(stub, t, sizeof(t));

    patch[0] = 0xe8;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5)) VirtualFree(stub, 0, MEM_RELEASE);
}

/* The width of the box the backdrop occupies -- the whole screen with
   backdrop=1, the letterboxed picture with backdrop=2.  Defined with the
   backdrop code below; declared here because the Earth must size itself
   against the same box or it draws across the bars. */
static int MdkBoxWidth(void);

//
// K16g.  Make the Earth reach the bottom of the screen, exactly.
//
// The Earth is a backdrop: it has to cover everything below its own top edge,
// or the starfield shows through underneath.  Its bottom is `posY + 2*srcH`,
// and BOTH terms move with the animation variable -- posY falls by 224 per
// unit while srcH grows by only 42 (i.e. 84 of destination).  So the bottom
// edge RISES by 140 per unit, and past a certain point it leaves the screen
// no matter what the constants are:
//
//     stock   bottom = 688 - 140*v   against a 480-tall screen
//     ours    bottom = 1147 - 233*v  against an 800-tall screen
//
// Those are the same relationship, which is why scaling the constants by H/480
// looked right and still ran out at the end of the travel: it reproduces stock
// faithfully, including stock's own limit. Measuring the stock reference frames
// settled which side of that limit the game actually uses -- the Earth is
// bright all the way to the last row of the window (y=419) in both of them --
// so stock never crosses it, and we should not either.
//
// Rather than guess a bigger constant and be back here at the next v, this
// computes the requirement directly at the draw call: grow srcH just enough
// that `posY + destH` reaches the bottom, and not at all when it already does.
// Exact for every v, and the smallest possible over-read of the source bitmap,
// which is the one risk here (these are source-rectangle dimensions, so the
// Earth is sampled further down rather than stretched -- that is what keeps
// the curve of the limb right).
//
// Hooked at the CALL rather than in the drawer (GAME-PATCHING 5b): the
// descriptor is already built and its address is in eax, the game's return
// address is already on the stack, and nothing moves.
//
static const unsigned char mdk_earth_sig[] =     /* 0x00410453 */
    "\x8d\x45\xd0\xe8\x95\x41\xff\xff";
#define MDK_EARTH_CALL_AT   3                    /* the call opcode           */

extern "C" void __attribute__((cdecl, used, noinline))
MdkEarthCover(int *d)
{
    int srcW, srcH, posY, kw, kh, k, boxW;

    /* d: posX, posY, srcW, srcH, scaleX, scaleY, pixels
     *
     * srcW and srcH are not source dimensions -- they set the DRAWN size.
     * `destW = (d[4]*srcW)>>8` while the source traversal is fixed by d[4], so
     * srcW:srcH IS the drawn aspect.  Stock is 428:142 = 3.01 at the end of
     * the travel; an earlier build pushed srcW alone to W/2, making it 7.66,
     * which is the horizontal stretch that came back from hardware.
     *
     * So scale BOTH by one factor.  The shape is then stock's by construction,
     * and the factor is whatever makes the Earth both span the screen and
     * reach the bottom:
     *
     *      kw = W / (2*srcW)                 fill the width
     *      kh = (H - posY) / srcH            reach the bottom  (posY is the
     *                                        sprite's CENTRE, so half the
     *                                        drawn height is srcH)
     *      k  = max(kw, kh)
     *
     * d[4] is never touched: it feeds the source ROW ADVANCE at 0x40473a, and
     * changing it walks the bitmap with the wrong stride -- the scrambling in
     * 0121 and 0122.  d[5] is left alone too, so the 2x magnification and
     * therefore the sampling stay exactly as the game intended. */
    if (!d || IsBadWritePtr(d, 7 * sizeof(int))) return;
    if (!g_targetW || !g_targetH) return;

    srcW = d[2];
    srcH = d[3];
    posY = d[1];
    if (srcW <= 0 || srcH <= 0) return;

    /* The Earth belongs inside whatever box the BACKDROP occupies.  With
       backdrop=2 that is a letterboxed 1333x800 at 1920x800, not the whole
       screen -- and sizing the Earth against the screen is what made it spill
       across the black bars.  Ask the same helper the backdrop uses, so the
       two cannot disagree. */
    boxW = MdkBoxWidth();

    kw = (int)(((unsigned int)boxW << 16) / (unsigned int)(2 * srcW));
    kh = (posY >= (int)g_targetH)
       ? 0
       : (int)(((unsigned int)((int)g_targetH - posY) << 16) /
               (unsigned int)srcH);

    k = kw > kh ? kw : kh;

    /* Deliberately NOT capped to the box.  Capping it keeps the whole planet
       inside the letterbox, but then the drawn height is capped too, and to go
       on covering the bottom the Earth has to stop rising -- a frozen
       animation, which is worse than the thing it fixes.  Instead it is sized
       freely and the BARS are painted last (GameFix_PreSwap), so anything
       outside the picture is cut off the way a letterbox actually works. */

    if (k <= (1 << 16)) return;                 /* already big enough */


    /* 32-bit is sufficient and avoids libgcc's 64-bit helpers: the largest
       product here is about 660 million (srcW 2228 * k 4.53<<16). */
    d[2] = (int)(((unsigned int)srcW * (unsigned int)k) >> 16);
    d[3] = (int)(((unsigned int)srcH * (unsigned int)k) >> 16);
}

static const unsigned char mdk_earth_stub[] = {
    0x60,                       // pushad
    0x50,                       // push  %eax   -> the descriptor
    0xe8, 0, 0, 0, 0,           // call  MdkEarthCover        rel32 <- @3
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xe9, 0, 0, 0, 0            // jmp   the real drawer      rel32 <- @12
};
#define MDK_EARTH_STUB_CALL_AT   3
#define MDK_EARTH_STUB_JMP_AT   12

static void InstallMdkEarthCover(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *target;
    unsigned char  patch[5];
    int            rel;

    /* The `lea -0x30(%ebp),%eax` in front of the call is shared with the MOON's
       drawer; only the call displacement separates them, so the signature has
       to include it.  Verified: this form matches once, the moon's once. */
    at = FindUnique(code, codeSize, mdk_earth_sig, sizeof(mdk_earth_sig) - 1);
    if (!at) return;
    at += MDK_EARTH_CALL_AT;

    /* Resolve the drawer from the very call we are displacing, rather than from
       a constant.  The first version hardcoded it and got it wrong: the address
       was read off a RAW-BINARY objdump, whose addresses are FILE OFFSETS, not
       VAs -- 0x39f0 there is 0x4045f0 here, and the stub jumped into garbage.
       Derived this way it is the same instruction either way and cannot
       disagree with itself. */
    target = (at + 5) + (int)ReadU32(at + 1);

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_earth_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_earth_stub, sizeof(mdk_earth_stub));
    rel = (int)((unsigned char *)&MdkEarthCover
                - (stub + MDK_EARTH_STUB_CALL_AT + 4));
    PutU32(stub + MDK_EARTH_STUB_CALL_AT, (unsigned int)rel);
    rel = (int)(target - (stub + MDK_EARTH_STUB_JMP_AT + 4));
    PutU32(stub + MDK_EARTH_STUB_JMP_AT, (unsigned int)rel);


    patch[0] = 0xe8;                            /* call rel32 -> stub */
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

/* K28.  The selectable item's BACKGROUND square.
 *
 * The slot box is drawn twice by 0x46bdd3 -- a filled background, then the
 * white frame K26 already relocates:
 *
 *     46bdfb  lea (,%edx,4),%esi ; sub %edx,%esi ; shl $4,%esi   ; slot * 48
 *     46be07  push $0x15e        ; y1 = 350
 *     46be0f  mov  $0x131,%ebx   ; y0 = 305
 *     46be14  lea  0x36(%esi),%ecx                               ; x1
 *     46be19  lea  0x9(%esi),%edx                                ; x0
 *     46be1c  call 0x46f920      ; <- the FILL
 *     46be21  push $0x15f        ; and then the frame, via 0x416d30
 *
 * The two go to completely different drawers, which is why K26 moved one and
 * left the other in the top-left corner.  0x46f920 is not an LFB blitter at
 * all: it builds Glide QUADS, offsetting them by the viewport origin globals
 * 0x492be0/0x492be4 -- which K1 forces to (0,0).  So this is a fourth 2D path,
 * and no amount of work on the lock helpers could ever have reached it.
 *
 * Its arguments, read off its own prologue: edx/ecx are x0/x1, ebx and the
 * pushed dword are y0/y1, eax a mode flag.  The stub adds K26's offsets to all
 * four and tail-jumps, so the drawer returns straight to the game and the
 * pushed argument is still at [ebp+8] where it expects it.
 *
 * Scoped to this one call site.  The same drawer is used elsewhere, and a
 * blanket offset would move whatever else it draws.
 */
static const unsigned char mdk_itemfill_sig[] =  /* 0x0046be14 */
    { 0x8d, 0x4e, 0x36, 0x31, 0xc0, 0x8d, 0x56, 0x09, 0xe8 };
#define MDK_ITEMFILL_CALL_AT   8

static const unsigned char mdk_itemfill_stub[] = {
    0x81, 0xc2, 0, 0, 0, 0,             // add   $cx,%edx        x0   <- @2
    0x81, 0xc1, 0, 0, 0, 0,             // add   $cx,%ecx        x1   <- @8
    0x81, 0xc3, 0, 0, 0, 0,             // add   $cy,%ebx        y0   <- @14
    0x81, 0x44, 0x24, 0x04, 0, 0, 0, 0, // addl  $cy,0x4(%esp)   y1   <- @22
    0xe9, 0, 0, 0, 0                    // jmp   drawer               <- @27
};
#define MDK_FILL_X0_AT   2
#define MDK_FILL_X1_AT   8
#define MDK_FILL_Y0_AT   14
#define MDK_FILL_Y1_AT   22
#define MDK_FILL_JMP_AT  27

/* K32.  The enemy health bar's FILL.
 *
 * The bar is two calls, both at the same hardcoded layout position (0,4):
 *
 *     4388a2  push $0xa
 *     4388a7  mov  $0x4,%ebx          ; y = 4
 *     4388ac  mov  $0x3,%eax          ; colour index
 *     4388b1  xor  %edx,%edx          ; x = 0
 *     4388b3  call 0x416dd0           ; <- the FILL
 *     4388c6  call 0x416d30           ; <- the frame, via the LFB rect drawer
 *
 * The frame goes through the view lock, so it is centred with the rest of the
 * 2D layer and lands at (660,224).  The fill does not: 0x416dd0 looks a
 * colour out of the table at 0x546448 and draws GLIDE QUADS through 0x46f098,
 * offset by the viewport origin globals -- which K1 forces to (0,0).  So it
 * stays at raw (0,4), in the screen's top-left corner, while its own frame
 * sits at the centred window's origin.  That is exactly what the marked
 * screenshot shows: yellow at (660,220), pink at (0,0).
 *
 * Same class as K28, the item background, and the same fix: the two
 * coordinates are in registers at the call site, so a stub adds the centring
 * and tail-jumps.  Entered by `call` and leaving by `jmp`, so the drawer
 * returns straight to the game and the pushed argument is still where it
 * expects it.
 *
 * Offset by g_mdkHudCx/Cy -- the DEFAULT 2D centring, which is precisely what
 * the frame gets -- rather than by K15's band numbers, so the two cannot
 * drift apart.
 */
static const unsigned char mdk_hpfill_sig[] =    /* 0x004388a2 */
    { 0x6a, 0x0a, 0x8b, 0x4d, 0xe8, 0xbb, 0x04, 0x00, 0x00, 0x00,
      0xb8, 0x03, 0x00, 0x00, 0x00, 0x31, 0xd2 };
#define MDK_HPFILL_CALL_AT  (sizeof(mdk_hpfill_sig))   /* the call follows */

/* The drawer takes a RECTANGLE, not a point.  Read off its own tail:
 *
 *     416e1c  add %esi,%eax     x0 = edx arg
 *     416e1a  add %edi,%edx     y0 = ebx arg
 *     416e18  add %eax,%ebx     x1 = ecx arg      <- the health WIDTH
 *     416e1e  call 0x471d28     rect(x0,y0,x1,y1)
 *
 * 0158 offset x0 to 293 and left x1 at the bar's width, which inverted the
 * rectangle -- so the fill did not move badly, it stopped existing.  Both
 * edges have to move, which is obvious once the call is read as a rect and
 * invisible while it is read as a position. */
static const unsigned char mdk_hpfill_stub[] = {
    0x81, 0xc2, 0, 0, 0, 0,             // add  $cx,%edx      x0    <- @2
    0x81, 0xc1, 0, 0, 0, 0,             // add  $cx,%ecx      x1    <- @8
    0x81, 0xc3, 0, 0, 0, 0,             // add  $cy,%ebx      y0    <- @14
    0x81, 0x44, 0x24, 0x04, 0, 0, 0, 0, // addl $cy,0x4(%esp) y1    <- @22
    0xe9, 0, 0, 0, 0                    // jmp  0x416dd0            <- @27
};
#define MDK_HPF_X0_AT   2
#define MDK_HPF_X1_AT   8
#define MDK_HPF_Y0_AT  14
#define MDK_HPF_Y1_AT  22
#define MDK_HPF_JMP_AT 27

static void InstallMdkHealthFill(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *target;
    unsigned char  patch[5];

    /* Read per install, and installed late for that reason: these are set by
       the 2D centring installer, and K28 already cost a build by baking in
       two zeroes because it ran first. */
    if (g_mdkBoxX0 == 0 && g_mdkHudCy == 0) {
        return;
    }

    at = FindUnique(code, codeSize, mdk_hpfill_sig, sizeof(mdk_hpfill_sig));
    if (!at) return;
    at += MDK_HPFILL_CALL_AT;

    target = (at + 5) + (int)ReadU32(at + 1);   /* from the displaced call */

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_hpfill_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_hpfill_stub, sizeof(mdk_hpfill_stub));
    /* The top-left of the PICTURE, not of the centred window.
     *
     * 0157 used the centring, on the reasoning that it is what the frame
     * gets.  Two things were wrong with that.  The frame's position was not
     * right either -- it only looked like a target because it was the one of
     * the two that had moved.  And 660 is outside the 600-wide layout this
     * drawer culls against, so the fill did not merely land badly, it
     * vanished: the same bounds check that hid the player character until K11
     * raised it.
     *
     * g_mdkBoxX0 keeps it inside that limit (293) and puts it where the rest
     * of the left-hand HUD lives. */
    PutU32(stub + MDK_HPF_X0_AT, (unsigned int)g_mdkBoxX0);
    PutU32(stub + MDK_HPF_X1_AT, (unsigned int)g_mdkBoxX0);
    PutU32(stub + MDK_HPF_Y0_AT, 0);
    PutU32(stub + MDK_HPF_Y1_AT, 0);
    PutU32(stub + MDK_HPF_JMP_AT,
           (unsigned int)(int)(target - (stub + MDK_HPF_JMP_AT + 4)));


    patch[0] = 0xe8;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

/* K37.  The loading screen's two progress bars -- K32's bug at two more call
 * sites, and the third time this exact pair of drawers has come apart.
 *
 * 0x41ae00 draws the loading screen, and each bar is a fill and a frame drawn
 * back to back by two UNRELATED routines:
 *
 *     41af15  call 0x416dd0   the FILL   -- Glide quads, raw layout space
 *     41af33  call 0x416d30   the FRAME  -- LFB rect, centred by K5
 *     41af7a  call 0x416dd0   the second bar's fill
 *     41af98  call 0x416d30   the second bar's frame
 *
 * with the arguments in registers: eax = colour index, edx = x0, ebx = y0,
 * ecx = x1, and y1 pushed.  Both bars span layout x 10..594; bar one is
 * y 290..310 and bar two y 330..350, and only the fill's x1 moves with the
 * progress.
 *
 * The frames therefore land at screen (670,510)..(1254,530) and (670,550)..
 * (1254,570), and the fills stayed at raw layout in the top-left corner --
 * which is exactly what the screenshot measures and exactly what K28 (the item
 * background) and K32 (the enemy health bar) each were.
 *
 * Offset by K5's DEFAULT centring rather than by any band rule, because that
 * is what the frame beside it gets: neither the item nor the health keying
 * matches these call sites, so 0x416d30 falls through to (g_mdkHudCx,
 * g_mdkHudCy).  Matching the frame is the whole requirement -- a fill that is
 * merely "somewhere sensible" is still wrong if its own frame is elsewhere.
 *
 * One stub serves both sites: the offsets are identical and the drawer returns
 * straight to the game, since the stub is entered by `call` and leaves by
 * `jmp` with the pushed y1 untouched on the stack.
 *
 * The signature pins the routine and the two call sites are then VERIFIED
 * rather than trusted -- each must be an `e8`, and both must name the same
 * drawer (section 5b).
 *
 * The other thirteen callers of 0x416dd0 are left alone.  Some of them are
 * very likely misplaced in the same way; each needs its own frame identified
 * first, which is a five-minute job once one is reported.
 */
static const unsigned char mdk_loadbar_sig[] = { /* 0x0041aef0 */
    0x68, 0x36, 0x01, 0x00, 0x00,                /* push $0x136   y1 = 310  */
    0xbb, 0x22, 0x01, 0x00, 0x00,                /* mov  $0x122,%ebx  y0    */
    0xba, 0x0a, 0x00, 0x00, 0x00,                /* mov  $0xa,%edx    x0    */
    0xd8, 0x05, 0x8c, 0xc5, 0x48, 0x00,          /* fadds 0x48c58c          */
    0xb8, 0x03, 0x00, 0x00, 0x00,                /* mov  $0x3,%eax  colour  */
    0xe8, 0xe7, 0xa0, 0x05, 0x00,                /* call 0x474ff6   round   */
    0xdb, 0x5d, 0xe0,                            /* fistpl -0x20(%ebp)      */
    0x8b, 0x4d, 0xe0 };                          /* mov  -0x20(%ebp),%ecx x1 */
#define MDK_LOADBAR_A   (sizeof(mdk_loadbar_sig))       /* 0x0041af15 */
#define MDK_LOADBAR_B   (MDK_LOADBAR_A + 0x65)          /* 0x0041af7a */

static void InstallMdkLoadBars(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *a, *b, *target;
    unsigned char  patch[5];
    int            i;

    /* Read per install and installed late for the same reason K28 is: these
       are baked into the stub, and both are computed after MdkApply starts.
       A silent decline is the failure mode that costs a build, so it says so. */
    if (g_mdkHudCx == 0 && g_mdkHudCy == 0) {
        return;
    }

    at = FindUnique(code, codeSize, mdk_loadbar_sig, sizeof(mdk_loadbar_sig));
    if (!at) return;

    a = at + MDK_LOADBAR_A;
    b = at + MDK_LOADBAR_B;

    if (*a != 0xe8 || *b != 0xe8) {
        return;
    }

    target = (a + 5) + (int)ReadU32(a + 1);      /* from the displaced call */
    if (((b + 5) + (int)ReadU32(b + 1)) != target) {
        return;
    }

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_hpfill_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_hpfill_stub, sizeof(mdk_hpfill_stub));
    PutU32(stub + MDK_HPF_X0_AT, (unsigned int)g_mdkHudCx);
    PutU32(stub + MDK_HPF_X1_AT, (unsigned int)g_mdkHudCx);
    PutU32(stub + MDK_HPF_Y0_AT, (unsigned int)g_mdkHudCy);
    PutU32(stub + MDK_HPF_Y1_AT, (unsigned int)g_mdkHudCy);
    PutU32(stub + MDK_HPF_JMP_AT,
           (unsigned int)(int)(target - (stub + MDK_HPF_JMP_AT + 4)));


    for (i = 0; i < 2; i++) {
        unsigned char *site = i ? b : a;

        patch[0] = 0xe8;
        PutU32(patch + 1, (unsigned int)(int)(stub - (site + 5)));
        WriteCode(site, patch, 5);
    }
}

/* K42.  The Score-O-matic screen's category bars -- the fill/frame split for
 * the FOURTH time, and the third site to want this exact stub.
 *
 * MDK's idiom for a filled rectangle is two unrelated drawers: 0x416dd0 builds
 * Glide quads offset by the viewport origin globals (which K1 forces to (0,0),
 * so its coordinates are raw layout), and the frame goes through the LFB rect
 * drawer, which K5 centres.  The enemy health bar (K32), the loading screen's
 * two progress bars (K37) and now the statistics screen are all the same bug.
 *
 * 0x42a0e1 draws one category's bar and returns; it is called once per row,
 * which is why every category showed its own stray fill:
 *
 *     42a11a  push %esi        y1
 *     42a11b  mov  $0x3f,%eax  colour 63 -- white
 *     42a120  mov  %edi,%edx   x0        (ebx = y0, ecx = x1 already)
 *     42a122  call 0x416dd0
 *
 * MEASURED, so this is an identification rather than a match by shape.  The
 * five bars sit at screen (114,80), (360,80), (114,130), (360,130), (236,184).
 * Add the default 2D centring (660,220) that the screen's TEXT gets -- the
 * font renderer's lock matches no keyed branch, so it falls through to it --
 * and the first lands at (774,300) against "13663" measured at x 716..776,
 * y 308, and the second at (1020,300) against "43%" at x 984..1020.  Both
 * come out immediately to the right of their own number.
 *
 * MdkPatchQuadFill is new and shared.  K32 and K37 predate it and each carry
 * their own copy of the same twenty lines; folding them in belongs in the
 * cleanup pass, not in a build whose point is to test this one.
 */
static unsigned char *MdkPatchQuadFill(unsigned char *at, int cx, int cy,
                                       const char *tag)
{
    unsigned char *stub, *target;
    unsigned char  patch[5];

    if (*at != 0xe8) {
        return NULL;
    }
    target = (at + 5) + (int)ReadU32(at + 1);   /* from the displaced call */

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_hpfill_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return NULL;

    memcpy(stub, mdk_hpfill_stub, sizeof(mdk_hpfill_stub));
    PutU32(stub + MDK_HPF_X0_AT, (unsigned int)cx);
    PutU32(stub + MDK_HPF_X1_AT, (unsigned int)cx);
    PutU32(stub + MDK_HPF_Y0_AT, (unsigned int)cy);
    PutU32(stub + MDK_HPF_Y1_AT, (unsigned int)cy);
    PutU32(stub + MDK_HPF_JMP_AT,
           (unsigned int)(int)(target - (stub + MDK_HPF_JMP_AT + 4)));


    patch[0] = 0xe8;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

static const unsigned char mdk_statbar_sig[] = { /* 0x0042a10d */
    0x8b, 0x55, 0xf4,                            /* mov  -0xc(%ebp),%edx     */
    0x01, 0xf8,                                  /* add  %edi,%eax           */
    0x01, 0xd6,                                  /* add  %edx,%esi           */
    0x8d, 0x48, 0xff,                            /* lea  -0x1(%eax),%ecx  x1 */
    0x4e,                                        /* dec  %esi             y1 */
    0x89, 0xd3,                                  /* mov  %edx,%ebx        y0 */
    0x56,                                        /* push %esi                */
    0xb8, 0x3f, 0x00, 0x00, 0x00,                /* mov  $0x3f,%eax   white  */
    0x89, 0xfa };                                /* mov  %edi,%edx        x0 */
#define MDK_STATBAR_AT  (sizeof(mdk_statbar_sig))    /* the call follows */

static void InstallMdkStatBars(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at;

    /* Read per install, and installed late for it: the offsets are baked into
       the stub and both are computed after MdkApply starts.  K28 cost a build
       by declining in silence here, so this says so. */
    if (g_mdkHudCx == 0 && g_mdkHudCy == 0) {
        return;
    }

    at = FindUnique(code, codeSize, mdk_statbar_sig, sizeof(mdk_statbar_sig));
    if (!at) return;

    MdkPatchQuadFill(at + MDK_STATBAR_AT, g_mdkHudCx, g_mdkHudCy, "statbar");
}

static void InstallMdkItemFill(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *target;
    unsigned char  patch[5];

    /* Loud, not silent: a zero here means this ran before the offsets were
       computed, which is a build mistake rather than a 4:3 no-op. */
    if (g_mdkBoxX0 == 0 && g_mdkHudCy == 0) {
        return;
    }

    at = FindUnique(code, codeSize, mdk_itemfill_sig,
                    sizeof(mdk_itemfill_sig));
    if (!at) return;
    at += MDK_ITEMFILL_CALL_AT;

    /* From the displaced call, never a constant -- section 5b. */
    target = (at + 5) + (int)ReadU32(at + 1);

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_itemfill_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_itemfill_stub, sizeof(mdk_itemfill_stub));
    PutU32(stub + MDK_FILL_X0_AT, (unsigned int)g_mdkBoxX0);
    PutU32(stub + MDK_FILL_X1_AT, (unsigned int)g_mdkBoxX0);
    PutU32(stub + MDK_FILL_Y0_AT, (unsigned int)(g_mdkHudCy * 2));
    PutU32(stub + MDK_FILL_Y1_AT, (unsigned int)(g_mdkHudCy * 2));
    PutU32(stub + MDK_FILL_JMP_AT,
           (unsigned int)(int)(target - (stub + MDK_FILL_JMP_AT + 4)));


    patch[0] = 0xe8;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

static void InstallMdkSpritePos(unsigned char *code, unsigned int codeSize)
{
    /* Anchored on the singly-referenced 256.0 load, because `mov edx,300`
       alone occurs eight times in the image. */
    static const unsigned char sig[] =
        "\xd8\x0d\x1c\xbc\x48\x00\xba\x2c\x01\x00\x00";
    static const unsigned char sig2[] =      /* the second drawer, 0x4103E8 */
        "\xd8\x0d\x4c\xbc\x48\x00\xba\x2c\x01\x00\x00";
    unsigned char *at;
    double s      = (double)(int)g_targetH / (double)MDK_VIEW_H;
    int    centre = (int)g_targetW / 2;

    /* K16c.  The intro's inner parallax layers.
     *
     * The layer's 8.8 scale is this constant times the parallax value the
     * caller passes -- so it IS the layer's size, and it is referenced by one
     * instruction.  The outermost layer (the Earth) already fills the screen,
     * but the moon and starfield were still sized for 640x480: measured 192
     * wide against the log's dest=189, exactly src/2.
     *
     * Left as a percentage because this one is a matter of framing rather than
     * correctness -- the parallax still works at any value, so it is quicker
     * for you to dial in than for me to guess. */
    MdkPutFloat (0x0048bc1cu, (float)(256.0 * g_mdkIntroScale / 100.0), 256.0f);

    /* K16d, REMOVED.  0x48C0E8 was scaled here on the theory that 0x415314
       drew the starfield -- its logged height doubled to exactly the height
       measured on screen.  It does not: that routine renders the main menu
       text, and scaling it corrupted the menu.  A matching dimension is not an
       identification, which is the same trap as the health widget earlier.
       Left as a comment rather than deleted so the constant is not tried a
       second time on the same reasoning. */

    /* K16e.  The intro's layout space is 640x480 SCREEN pixels, not the
     * 600x360 render window -- so its POSITION constants scale by H/480, not
     * by H/360.
     *
     * Proved rather than assumed.  Reading the two drawers:
     *
     *     moon  0x410308:  posY = v*(-90) + 270      size = v*256 + 64 (square)
     *     earth 0x4103b0:  posY = 488 - v*224        srcW/srcH separate
     *
     * 264..488 is the lower half of a 480-tall screen, and the stock reference
     * frame measures the Earth's top edge at screen y=266 against a formula
     * minimum of 264.  Both are screen coordinates.
     *
     * Two symptoms fall out of getting this wrong, and both were reported:
     * the Earth sat mid-screen instead of low (its constants were never scaled
     * at all -- K16 scaled 0x48bc14/18, which belong to the MOON), and the moon
     * sat BELOW the Earth's limb instead of above it, because H/360 over-scales
     * it by exactly 480/360 = 1.33.
     *
     * `sSize` stays on H/360: 0x48bc20 is the moon's base SIZE, not a position,
     * and the intro scale is tuned against it. */
    {
        double sPos = (double)(int)g_targetH / 480.0;

        MdkPutFloat (0x0048bc14u, (float)(-90.0 * sPos), -90.0f);   /* moon  */
        MdkPutFloat (0x0048bc18u, (float)(270.0 * sPos), 270.0f);
        MdkPutFloat (0x0048bc3cu, (float)(224.0 * sPos), 224.0f);   /* earth */
        MdkPutFloat (0x0048bc40u, (float)(488.0 * sPos), 488.0f);

        /* K16f, the Earth's HEIGHT, is deliberately NOT done by scaling its
           constants -- see K16g below.  Scaling them by H/480 made the bottom
           edge 1147 - 233*v, which is proportionally identical to stock's
           688 - 140*v against a 480-tall screen: correct in shape, and still
           short once v runs far enough.  Chasing it with a bigger constant just
           moves the value of v at which it fails. */

    }

    /* The moon's base SIZE.  It takes the same intro scale as its growth
       rate at 0x48bc1c -- these two are srcW/srcH offsets and so set the
       DRAWN size, and moving them together is what keeps the moon round.
       It used to scale by H/360 while the other did not, which changed only
       how fast the moon grew and not how big it started. */
    MdkPutFloat (0x0048bc20u,
                 (float)(64.0 * g_mdkIntroScale / 100.0), 64.0f);

    MdkPutDouble(0x0048bc24u, -0.1 * s, -0.1);
    MdkPutDouble(0x0048bc2cu,  0.8 * s,  0.8);
    MdkPutDouble(0x0048bc54u, -0.1 * s, -0.1);
    MdkPutDouble(0x0048bc5cu,  0.6 * s,  0.6);

    MdkPutDouble(0x0048bc34u, (double)centre, 300.0);
    MdkPutDouble(0x0048bc64u, (double)centre, 300.0);

    at = FindUnique(code, codeSize, sig, sizeof(sig) - 1);
    if (at) {
        unsigned char imm[4];
        PutU32(imm, (unsigned int)centre);
        WriteCode(at + 7, imm, 4);
    }

    /* 0x48bc48 is DELIBERATELY left at its stock 300.
     *
     * It was rewritten to `centre` here for a long time, on the reading that it
     * was the Earth's horizontal position.  It is not -- it is the offset of
     * the Earth's srcW, and srcW is what sets the DRAWN WIDTH.  Pushing it from
     * 300 to W/2 therefore widened the Earth without touching its height:
     * stock's srcW:srcH of 428:142 (3.01) became 1088:142 (7.66), which is a
     * 2.5x horizontal stretch and exactly the "it is horizontally stretched"
     * that came back from hardware.
     *
     * The width is now handled in MdkEarthCover, which scales srcW and srcH
     * TOGETHER so the ratio -- and therefore the planet's shape -- is stock's
     * at every point of the animation.  Leaving this at 300 is what lets that
     * start from the correct ratio. */

    at = FindUnique(code, codeSize, sig2, sizeof(sig2) - 1);
    if (at) {
        unsigned char imm[4];
        PutU32(imm, (unsigned int)centre);
        WriteCode(at + 7, imm, 4);
    }
}

//
// K21.  Ask Glide for a PLAIN framebuffer pointer instead of a pixel-pipeline
// one.  One byte at each of MDK's three write locks.
//
// MDK calls grLfbLock(GR_LFB_WRITE_ONLY, buf, GR_LFBWRITEMODE_565,
// GR_ORIGIN_UPPER_LEFT, pixelPipeline=FXTRUE, info).  glide3x has a fast path
// (glfb.c, the !textureBuffer.on branch) that hands back the LINEAR buffer and
// bufLfbStride whenever a write lock is native-format, upper-left AND
// !pixelPipeline.  MDK fails only that last condition, so every one of its 2D
// writes goes through the 3D LFB aperture instead.  That costs three things:
//
//   * SPEED.  Each write is a pixel-pipeline operation rather than a store to
//     write-combined memory.  MDK's own 600x360 blit could afford it; K20's
//     full-screen one could not, and the game ran at 3 fps.
//   * ACCURACY.  The pipeline is still free to dither.  On the near-black
//     areas K20 newly covers, a +1 blue LSB reads as a distinctly blue speckle,
//     which is what appeared all over the menu -- 6 blue-dominant pixels before
//     K20, 55,535 after, in a regular lattice rather than at random.
//   * REACH.  The 3D aperture decodes (y*2048 + x), which is the wrap that put
//     the health gauge in the wrong corner at 2304.  The linear path uses
//     bufLfbStride = 0x2000, i.e. 4096 pixels.
//
// Nothing is lost that MDK uses: its blitters do their own colour-keying in
// software (the RLE decoder skips index 0, the backdrop writes every pixel),
// and depth is disabled for 2D.  The game reads lfbPtr and strideInBytes out of
// the info struct either way, so it adapts to the new geometry by itself.
//
// The FOURTH site that pushes the same info struct, 0x427b0c, is a READ lock
// (`type` is 0, not 1) -- pixelPipeline means nothing there and read locks
// already get bufLfbStride, so it is deliberately excluded.  The two patterns
// below differ only in `buffer` (BACK for the first two, FRONT for the third)
// and both end at the `call`, which is what keeps the read lock out.
//
static const unsigned char mdk_lfbpp_a[] =      /* 0x470f8e, 0x470fee -- x2 */
    "\x6a\x01\x6a\x00\x6a\x00\x6a\x01\x6a\x01\xe8";
static const unsigned char mdk_lfbpp_b[] =      /* 0x47106a          -- x1 */
    "\x6a\x01\x6a\x00\x6a\x00\x6a\x00\x6a\x01\xe8";
#define MDK_LFBPP_IMM_AT  1        /* the pixelPipeline imm8 */


/* Patch every occurrence, and say how many there were.  FindUnique cannot be
   used: pattern A is two sites by construction (the view lock and the raw
   lock are identical up to their call target). */
static unsigned int MdkPatchAll(unsigned char *code, unsigned int codeSize,
                                const unsigned char *sig, unsigned int len,
                                unsigned int immAt, unsigned char want)
{
    unsigned int i, n = 0;

    if (codeSize < len) return 0;
    for (i = 0; i + len <= codeSize; i++) {
        if (memcmp(code + i, sig, len) != 0) continue;
        WriteCode(code + i + immAt, &want, 1);
        n++;
    }
    return n;
}

static void InstallMdkLfbLinear(unsigned char *code, unsigned int codeSize)
{
    unsigned char zero = 0x00;

    MdkPatchAll(code, codeSize, mdk_lfbpp_a, sizeof(mdk_lfbpp_a) - 1,
                MDK_LFBPP_IMM_AT, zero);
    MdkPatchAll(code, codeSize, mdk_lfbpp_b, sizeof(mdk_lfbpp_b) - 1,
                MDK_LFBPP_IMM_AT, zero);
}

//
// K20.  The full-window backdrop -- the starfield, and the terrain under the
// intro dive.  Scaled to the screen instead of blitted 1:1.
//
// 0x46E7E0 is a flat 8bpp -> 16bpp blitter and nothing more:
//
//     lock the view LFB   -> base [ebp-0x1c], strideBytes [ebp-0x18], src in esi
//     rowAdvance = strideBytes - 0x4b0            ; 1200 == 600 px * 2
//     mov $0x168,%ecx                             ; 360 rows
//       mov $0x96,%ecx                            ; 150 iterations x 4 px = 600
//         dst[0..3] = pal[src[0..3]]              ; palette at 0x546848
//
// No RLE, no transparency, source contiguous at 600 bytes a row.  So unlike
// the character blitter (K12) there is nothing to decode -- only to resample.
//
// Found from the log rather than by searching: `viewlock caller=0046e7f8` was
// the one caller in that module nothing had accounted for, and the routine it
// sits in turned out to be the element that had been looked for under the name
// "starfield" for several builds.  It is the same routine for both scenes; only
// the image differs, which is why the space approach and the dive both show it.
//
// Entered after the lock and rejoined at the unlock, per GAME-PATCHING 5c, so
// the lock, the pointer, the stride and the teardown all stay with the game.
// The five bytes replaced are `mov -0x18(%ebp),%edx ; sar %edx`, which only
// recompute the stride we read ourselves.
//
#define MDK_BD_W    600
#define MDK_BD_H    360
#define MDK_BD_PAL  0x00546848u

/* 0 = leave it 1:1 (centred, as before), 1 = stretch to fill.
   A uniform-scale-and-CROP mode was offered alongside and is gone: on hardware
   it was "stretched too much by its height", which is what cropping to a 2.4:1
   screen has to look like when the source is 1.67:1.  Mode 2 was then rewritten
   as FIT -- the whole picture, centred, with bars on the surplus axis -- and
   that is what the install runs and what the HUD anchoring is measured
   against, so it is the default.  Mode 1 (stretch) is kept because which way
   the error should go is a genuine preference: into the shape, or into the
   framing.  Neither shows more of the WORLD; the game renders 600x360 either
   way. */
static int g_mdkBackdrop = 2;   /* 0 off, 1 stretch, 2 original aspect + bars */

/* One scaled row, in system memory, reused every row and every frame.  Never
   freed -- it lives as long as the hook that uses it.  Same reason K14 has one:
   the framebuffer must only ever be written, never read back. */
static unsigned short *g_mdkBdRow = NULL;

/* Work out where a srcW x srcH picture goes on a W x H screen, and the 16.16
   source steps to get it there.

   mode 1 STRETCH  fills the screen exactly; a 600x360 source on 2.4:1 comes
                   out 44% too wide.
   mode 2 FIT      one magnification for both axes, so the picture keeps its
                   original shape, centred, with bars on the surplus.  At
                   1920x800 that is 1333x800 with 293 px of bar each side.

   Returns the destination rectangle; the caller fills anything outside it. */
static void MdkFitRect(int srcW, int srcH, int W, int H, int fit,
                       int *dx0, int *dy0, int *dw, int *dh,
                       unsigned int *sxStep, unsigned int *syStep)
{
    int w = W, h = H;

    if (fit) {
        /* Largest whole-pixel box of the source's shape that fits.  Compared
           as a cross-product so there is no divide and no rounding drift. */
        if (srcW * H > srcH * W) h = (W * srcH) / srcW;   /* wider: bars top/bottom */
        else                     w = (H * srcW) / srcH;   /* taller: bars each side */
        if (w < 1) w = 1;
        if (h < 1) h = 1;
    }

    *dw  = w;
    *dh  = h;
    *dx0 = (W - w) / 2;
    *dy0 = (H - h) / 2;
    *sxStep = ((unsigned int)srcW << 16) / (unsigned int)w;
    *syStep = ((unsigned int)srcH << 16) / (unsigned int)h;
}

/* Set by either backdrop when it draws, cleared once the bars are painted.
   Without it the bars would also appear during in-level play, where the 3D
   legitimately owns the whole screen and there is no letterbox. */
static int g_mdkBoxDrew = 0;

/* Paint everything outside the picture black, as the last thing in the frame.
   Doing it here rather than up front is what makes the letterbox real: sprites
   drawn after the backdrop -- the Earth especially -- would otherwise run
   across the bars. */
static void MdkPaintBars(void)
{
    unsigned short *base;
    int             W, H, strideP, dx0, dy0, dw, dh, y;
    unsigned int    sx, sy;

    if (!g_mdkBoxDrew) return;
    g_mdkBoxDrew = 0;

    if (g_mdkBackdrop != 2) return;             /* only mode 2 has bars */
    if (!g_mdkLfbPtr || !*g_mdkLfbPtr || !g_mdkStride) return;

    W = (int)g_targetW;
    H = (int)g_targetH;
    strideP = (int)(*g_mdkStride / 2);
    if (W <= 0 || H <= 0 || strideP <= 0) return;
    if (W > strideP) W = strideP;

    MdkFitRect(MDK_BD_W, MDK_BD_H, W, H, 1, &dx0, &dy0, &dw, &dh, &sx, &sy);

    base = (unsigned short *)*g_mdkLfbPtr;
    for (y = 0; y < H; y++) {
        unsigned short *dst = base + (unsigned int)y * (unsigned int)strideP;

        if (y < dy0 || y >= dy0 + dh) {
            memset(dst, 0, (unsigned int)W * 2);
        } else {
            if (dx0 > 0)      memset(dst, 0, (unsigned int)dx0 * 2);
            if (dx0 + dw < W) memset(dst + dx0 + dw, 0,
                                     (unsigned int)(W - dx0 - dw) * 2);
        }
    }
}

static int MdkBoxWidth(void)
{
    int dx0, dy0, dw, dh;
    unsigned int sx, sy;

    if (!g_targetW || !g_targetH) return (int)g_targetW;
    MdkFitRect(MDK_BD_W, MDK_BD_H, (int)g_targetW, (int)g_targetH,
               g_mdkBackdrop == 2, &dx0, &dy0, &dw, &dh, &sx, &sy);
    return dw;
}

extern "C" void __attribute__((cdecl, used, noinline))
MdkBackdrop(unsigned int *f)
{
    const unsigned char  *src;
    const unsigned short *pal = (const unsigned short *)MDK_BD_PAL;
    unsigned short       *base, *dst, *row;
    unsigned int          ebp;
    int                   strideB, strideP, W, H, x, y, lastRow = -1;
    int                   dx0, dy0, dw, dh;
    unsigned int          sxStep, syStep, sy;

    if (!f) return;
    src = (const unsigned char *)f[1];          /* esi -- the source image  */
    ebp = f[2];                                 /* the routine's own frame  */
    if (!src || !ebp) return;

    row = g_mdkBdRow;
    if (!row) return;

    base    = *(unsigned short **)(ebp - 0x1c);
    strideB = *(const int *)(ebp - 0x18);
    if (!base || strideB <= 0) return;

    /* The base the lock handed back is ALREADY offset by K5's centring -- at
       1920x800 that is (660,220).  Drawing a full-screen image from there ran
       660+1920 = 2580 into a 2048-pixel row, spilling 532 pixels onto the next
       one every row, which is what turned the picture into a diagonal.
       A backdrop wants the true framebuffer origin, so take it from the global
       the lock helper reads rather than from the centred result. */
    if (g_mdkLfbPtr && *g_mdkLfbPtr)
        base = (unsigned short *)*g_mdkLfbPtr;

    strideP = strideB / 2;
    W = (int)g_targetW;
    H = (int)g_targetH;
    if (W <= 0 || H <= 0) return;

    /* The LFB row is 2048 pixels on this driver (glfb.c hardcodes 0x1000 for
       the pixel-pipeline aperture, and MDK asks for that path).  Writing past
       it wraps onto the next scanline -- which is what put the health gauge in
       the wrong corner at 2304.  Clamp rather than bail: a backdrop that
       covers 2048 of 2304 columns is a degraded picture, whereas returning
       here would leave the screen black, which is worse than the 1:1 blit
       this replaced. */
    if (W > strideP) W = strideP;

    /* 16.16 throughout: at 1920x800 the vertical factor is 2.22, and whole
       pixel steps would be 10% wrong. */
    MdkFitRect(MDK_BD_W, MDK_BD_H, W, H, g_mdkBackdrop == 2,
               &dx0, &dy0, &dw, &dh, &sxStep, &syStep);

    /* Everything is built in a system-memory row and then copied out.
     *
     * The first version resampled straight into the framebuffer and reused an
     * already-written framebuffer row when the source row repeated -- which
     * meant memcpy READING BACK from video memory.  Uncached VRAM reads across
     * PCI are orders of magnitude slower than writes, and ~1.7 MB of them a
     * frame took the game to about 1 fps.  Nothing here touches the LFB except
     * to write to it, sequentially, once per output row. */
    g_mdkBoxDrew = 1;

    sy = 0;
    for (y = 0; y < H; y++) {
        int srcRow;

        dst = base + (unsigned int)y * (unsigned int)strideP;

        /* Outside the picture: a bar.  Written rather than left alone, so a
           letterbox cannot show whatever the last frame put there. */
        if (y < dy0 || y >= dy0 + dh) {
            memset(dst, 0, (unsigned int)W * 2);
            continue;
        }

        srcRow = (int)(sy >> 16);
        sy += syStep;
        if (srcRow >= MDK_BD_H) srcRow = MDK_BD_H - 1;

        /* Vertical magnification is 2.22x at 1920x800, so most output rows
           repeat the previous source row and cost only the copy out.  The
           bars are built into the row, so they cost nothing extra. */
        if (srcRow != lastRow) {
            const unsigned char *s = src + (unsigned int)srcRow * MDK_BD_W;
            unsigned int         sx = 0;

            for (x = 0; x < dx0; x++) row[x] = 0;
            for (; x < dx0 + dw; x++, sx += sxStep) {
                unsigned int col = sx >> 16;

                if (col >= MDK_BD_W) col = MDK_BD_W - 1;
                row[x] = pal[s[col]];
            }
            for (; x < W; x++) row[x] = 0;
            lastRow = srcRow;
        }

        memcpy(dst, row, (unsigned int)W * 2);
    }
}

/* ==========================================================================
 * K29.  SNIPE MODE.
 *
 * Two halves, on two unrelated mechanisms, and the screenshot measured both.
 *
 * The 3D view came out as a 384x280 box at (108,80).  That is PROJECTION MODE
 * 1 exactly -- one of the four inset projections this project deliberately
 * left alone when K2/K3 widened mode 0, on the reasoning that the inset
 * projections were never widened either so each inset stays internally
 * consistent.  True for the three 140x70 panels; wrong for this one, because
 * snipe mode is not an inset at all, it is the whole view.
 *
 * The 2D scope frame is a 600x360 8bpp image blitted 1:1 through the view
 * lock -- the same shape as K20's backdrop -- so K5 centred it and it stayed
 * 600x360 in the middle of the screen.
 *
 * ---- the 3D half -------------------------------------------------------
 *
 * Mode 1 is drawn by 0x46ca28, and its five constants are singly-referenced
 * doubles, exactly like mode 0:
 *
 *     0x48fc9c Kx 191.95    0x48fca4 Cx 108.0
 *     0x48fcb4 Ky 139.95    0x48fcbc Cy  80.0     0x48fcac 0.05 (both axes)
 *
 * screen = (coord/z + 1) * K + C, so K is the half extent and C the origin:
 * 108 .. 108+2*191.95 is 384 wide, and 80 .. 80+2*139.95 is 280 tall.  Setting
 * K to half the screen and C to zero makes it the whole screen.
 *
 * Extents alone would STRETCH it, which is the K2-without-K3 failure.  Mode 1
 * carries its own frustum aspect group -- three sites, each A=0.5, B=280.0,
 * C=1/384, giving yscale/xscale = 384/280 -- and each A is singly referenced.
 * A' = 0.5*(W*280)/(H*384) makes the ratio W/H, exactly as K3 does for mode 0.
 *
 * Note this is deliberately NOT a no-op at 4:3: the mode-1 frustum is 384:280
 * and the screen is 4:3, so they genuinely differ.  Nothing runs at all unless
 * an override is active.
 *
 * ---- the 2D half -------------------------------------------------------
 *
 * The overlay decoder 0x46f520 is run-length encoded WITH TRANSPARENCY -- a
 * u16 token, values >= 0x8000 meaning skip -- and the transparency is
 * load-bearing: the hole in the middle of the scope is how the 3D shows
 * through once both halves are full screen.
 *
 * Rather than reimplement that format (K12 work, and the expensive kind to get
 * subtly wrong), the decoder is left to do it: its destination is redirected
 * to a 600x360 buffer of ours with a row advance of 0, and the result is then
 * scaled out.  Transparency is recovered WITHOUT knowing the format at all --
 * the buffer is pre-filled with a 16-bit value that appears nowhere in the
 * 256-entry palette, so any pixel still holding it was never written.  The
 * decoder only ever stores palette entries, so the test is exact, and a free
 * value always exists among 65536 for a 256-entry table.
 */
/* ==========================================================================
 * K38.  THE SNIPER SCREEN IS 640x480, NOT 600x360.
 *
 * K29 letterboxed the scope and every viewport inside it on one uniform k, and
 * the mapping was right -- warping a patched frame back through it reproduces
 * the stock 640x480 frame's 600x360 window pixel for pixel.  The SPACE was
 * wrong.
 *
 * The sniper screen is TWO images:
 *
 *     SNIPERS1   640x480   raw lock, 0x46e4d4      the scope BODY
 *     SNIPERS2   600x360   view lock, 0x40dce0     the overlay with the holes
 *
 * -- the loader names them, at 0x435d17 and 0x435d25 -- and 0x437a56 blits the
 * body immediately before the state that draws the overlay every frame.
 * Brightening a stock frame shows the body running to all four edges of the
 * 640x480 screen, straight across the window boundary with no seam.
 *
 * So K29 scaled the inner 600x360 to the full screen height, k = 800/360 =
 * 2.222, where the composition's own scale is 800/480 = 1.667.  Exactly 4/3
 * too big, and the 60 rows of body above and below the window became 133 px
 * off-screen each: 94% of the width, 75% of the height, 70% of the area.
 *
 * K38 letterboxes the COMPOSITION instead.  At 1920x800 that is 1067x800 at
 * x=427, and the window is the composition's own (20,60)..(620,420) inside it
 * -- 1000x600 at (460,100).  Everything else follows for free, because every
 * viewport, the crosshair, the cam covers, the HUD and the barrel props all
 * map through g_mdkSnipeX0/Y0/W/H already; only the box they are given
 * changes.
 *
 * THE BODY IS CAPTURED, NOT JUST SCALED IN PLACE.  0x46e4d4 is a one-shot: it
 * paints both buffers and swaps, then never runs again while the scope is up.
 * Anything drawn full-screen afterwards -- the sky especially -- would paint
 * over the ring between the composition box and the window, which the old
 * full-height overlay used to cover.  So the body is decoded once into a
 * buffer of ours and the ring is repainted from it every frame, immediately
 * before the overlay.  That makes the whole composition ours per frame and
 * nothing can get underneath it.
 *
 * Keyed on the CALLER (0x437a5b), so the other three users of the art blitter
 * -- loading screens and cutscene stills -- keep K19's centring untouched.
 * The stub falls through to the game's own loop for them, which is the shape
 * 0149 had to learn: declining by jumping to the unlock draws nothing at all.
 *
 * FAILS CLOSED.  If the body hook does not install, g_mdkCompW stays 0 and
 * InstallMdkSnipe keeps K29's confirmed 600x360 box -- a smaller window on a
 * black ring would be worse than the crop it replaces.
 */
#define MDK_BODY_W  640
#define MDK_BODY_H  480
#define MDK_WIN_X    20                 /* the window's origin inside it */
#define MDK_WIN_Y    60

static unsigned short *g_mdkBodyBuf = NULL;     /* 640x480, 16bpp        */
static unsigned short *g_mdkBodyRow = NULL;     /* one expanded row      */
static int            *g_mdkBodyCol = NULL;     /* source column per x   */
static int             g_mdkBodyCols = 0;
static int             g_mdkBodyHave = 0;       /* the buffer holds art  */
static int             g_mdkBodyTook = 0;       /* stub reads after popad */
static unsigned int    g_mdkBodyFrom = 0;       /* the snipe caller      */

/* The composition box.  Zero until the body hook installs, which is what
   makes the fallback to K29's box automatic rather than a second switch. */
static int g_mdkCompX0 = 0, g_mdkCompY0 = 0, g_mdkCompW = 0, g_mdkCompH = 0;

/* Paint the composition from the captured body.  ringOnly leaves the window
   alone, because the 3D and the overlay own it. */
static void MdkBodyBlit(int ringOnly)
{
    unsigned short *base, *dst, *row = g_mdkBodyRow;
    int  *col = g_mdkBodyCol;
    int   strideP, W, H, x, y, lastRow = -1;
    unsigned int sxStep, syStep, sy;

    if (!g_mdkBodyHave || !g_mdkBodyBuf || !row || !col) return;
    if (g_mdkCompW <= 0 || g_mdkCompH <= 0) return;
    if (!g_mdkLfbPtr || !*g_mdkLfbPtr || !g_mdkStride) return;

    base    = (unsigned short *)*g_mdkLfbPtr;
    strideP = (int)(*g_mdkStride) / 2;
    W = (int)g_targetW;
    H = (int)g_targetH;
    if (!base || strideP <= 0 || W <= 0 || H <= 0) return;
    if (W > strideP) W = strideP;                /* the LFB row limit */
    if (W > 4096) W = 4096;                      /* our row/col buffers */

    sxStep = ((unsigned int)MDK_BODY_W << 16) / (unsigned int)g_mdkCompW;
    syStep = ((unsigned int)MDK_BODY_H << 16) / (unsigned int)g_mdkCompH;

    if (g_mdkBodyCols != W) {
        unsigned int sx = 0;

        for (x = 0; x < W; x++) {
            int c = -1;

            if (x >= g_mdkCompX0 && x < g_mdkCompX0 + g_mdkCompW) {
                c = (int)(sx >> 16);
                if (c >= MDK_BODY_W) c = MDK_BODY_W - 1;
                sx += sxStep;
            }
            col[x] = c;
        }
        g_mdkBodyCols = W;
    }

    sy = 0;
    for (y = 0; y < H; y++) {
        int srcRow, inWin;

        dst = base + (unsigned int)y * (unsigned int)strideP;

        if (y < g_mdkCompY0 || y >= g_mdkCompY0 + g_mdkCompH) {
            memset(dst, 0, (unsigned int)W * 2);  /* outside: a bar */
            continue;
        }

        srcRow = (int)(sy >> 16);
        sy += syStep;
        if (srcRow >= MDK_BODY_H) srcRow = MDK_BODY_H - 1;

        if (srcRow != lastRow) {
            const unsigned short *s = g_mdkBodyBuf
                                    + (unsigned int)srcRow * MDK_BODY_W;

            for (x = 0; x < W; x++)
                row[x] = (col[x] < 0) ? 0 : s[col[x]];
            lastRow = srcRow;
        }

        inWin = ringOnly && g_mdkSnipeW > 0
             && y >= g_mdkSnipeY0 && y < g_mdkSnipeY0 + g_mdkSnipeH;

        if (!inWin) {
            memcpy(dst, row, (unsigned int)W * 2);
            continue;
        }

        for (x = 0; x < g_mdkSnipeX0 && x < W; x++)     dst[x] = row[x];
        for (x = g_mdkSnipeX0 + g_mdkSnipeW; x < W; x++) dst[x] = row[x];
    }
}

extern "C" void __attribute__((cdecl, used, noinline))
MdkBodyCapture(unsigned int *f)
{
    const unsigned char  *src;
    const unsigned short *pal = (const unsigned short *)MDK_BD_PAL;
    unsigned int          ebp;
    int                   x, y;

    g_mdkBodyTook = 0;                  /* default: let the game draw it */

    if (!f || !g_mdkBodyBuf || g_mdkCompW <= 0) return;

    ebp = f[2];
    if (!ebp || IsBadReadPtr((const void *)(ebp - 0x30), 0x14)) return;
    if (IsBadReadPtr((const void *)(ebp + 4), 4)) return;

    /* Only the sniper screen.  The other three callers are loading screens and
       cutscene stills, and they keep K19's centring exactly as it is. */
    if (!g_mdkBodyFrom || *(const unsigned int *)(ebp + 4) != g_mdkBodyFrom)
        return;

    src = *(const unsigned char *const *)(ebp - 0x28);
    if (!src || IsBadReadPtr(src, MDK_BODY_W * MDK_BODY_H)) return;

    for (y = 0; y < MDK_BODY_H; y++) {
        const unsigned char  *s = src + (unsigned int)y * MDK_BODY_W;
        unsigned short       *d = g_mdkBodyBuf + (unsigned int)y * MDK_BODY_W;

        for (x = 0; x < MDK_BODY_W; x++) d[x] = pal[s[x]];
    }
    g_mdkBodyHave = 1;
    g_mdkBodyTook = 1;

    /* "installed but never fired" and "fired" look the same on screen until
       you know which, so say it. */

    /* Put it up now as well.  The game blits into both buffers around a swap,
       so leaving the first one to the next frame's overlay would show one
       blank frame on the way in. */
    MdkBodyBlit(0);
}

/* Both loops are the same shape and 0x67 bytes from their own unlock, but they
   sar a DIFFERENT register (ebx in the first, esi in the second) -- so the five
   displaced bytes are copied FROM THE SITE, never from a template. */
static const unsigned char mdk_body1_sig[] =     /* 0x0046e521 */
    { 0x8d, 0x55, 0xd0, 0x8d, 0x45, 0xd4, 0x89, 0x75, 0xdc, 0xe8 };
#define MDK_BODY1_SITE  (sizeof(mdk_body1_sig) + 4 + 3)

static const unsigned char mdk_body2_sig[] =     /* 0x0046e5a5 */
    { 0x8d, 0x55, 0xd0, 0x8d, 0x45, 0xd4, 0x31, 0xff, 0xe8 };
#define MDK_BODY2_SITE  (sizeof(mdk_body2_sig) + 4 + 6)

#define MDK_BODY_REJOIN 0x67                     /* site -> its unlock call */

/* The caller that is the sniper screen's:
       437a4c  mov  0x544f10,%eax      SNIPERS1
       437a51  mov  $0x1,%ecx
       437a56  call 0x46e4d4           <- return 0x0046e5b... 0x00437a5b   */
static const unsigned char mdk_bodyfrom_sig[] =  /* 0x00437a4c */
    { 0xa1, 0x10, 0x4f, 0x54, 0x00, 0xb9, 0x01, 0x00, 0x00, 0x00, 0xe8 };
#define MDK_BODYFROM_RET (sizeof(mdk_bodyfrom_sig) + 4)

static const unsigned char mdk_body_stub[] = {
    0x60,                             // pushad
    0x54,                             // push  %esp
    0xe8, 0, 0, 0, 0,                 // call  MdkBodyCapture     rel32 <- @3
    0x83, 0xc4, 0x04,                 // add   $0x4,%esp
    0x61,                             // popad
    0x83, 0x3d, 0, 0, 0, 0, 0x00,     // cmpl  $0,g_mdkBodyTook   addr  <- @13
    0x74, 0x05,                       // je    +5   (not ours -- draw it)
    0xe9, 0, 0, 0, 0,                 // jmp   the unlock         rel32 <- @21
    0, 0, 0, 0, 0,                    // <the five displaced bytes>      @25
    0xe9, 0, 0, 0, 0                  // jmp   back               rel32 <- @31
};
#define MDK_BODYS_CALL_AT  3
#define MDK_BODYS_FLAG_AT 13
#define MDK_BODYS_SKIP_AT 21
#define MDK_BODYS_DISP_AT 25
#define MDK_BODYS_BACK_AT 31

static int InstallMdkBodyOne(unsigned char *code, unsigned int codeSize,
                             const unsigned char *sig, unsigned int sigLen,
                             unsigned int siteAt, const char *tag)
{
    unsigned char *at, *site, *rejoin, *stub;
    unsigned char  patch[5];

    at = FindUnique(code, codeSize, sig, sigLen);
    if (!at) return 0;

    site   = at + siteAt;
    rejoin = site + MDK_BODY_REJOIN;
    if (*rejoin != 0xe8) {
        return 0;
    }

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_body_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return 0;

    memcpy(stub, mdk_body_stub, sizeof(mdk_body_stub));
    memcpy(stub + MDK_BODYS_DISP_AT, site, 5);   /* FROM THE SITE */
    PutU32(stub + MDK_BODYS_CALL_AT,
           (unsigned int)(int)((unsigned char *)&MdkBodyCapture
                               - (stub + MDK_BODYS_CALL_AT + 4)));
    PutU32(stub + MDK_BODYS_FLAG_AT, (unsigned int)&g_mdkBodyTook);
    PutU32(stub + MDK_BODYS_SKIP_AT,
           (unsigned int)(int)(rejoin - (stub + MDK_BODYS_SKIP_AT + 4)));
    PutU32(stub + MDK_BODYS_BACK_AT,
           (unsigned int)(int)((site + 5) - (stub + MDK_BODYS_BACK_AT + 4)));


    patch[0] = 0xe9;
    PutU32(patch + 1, (unsigned int)(int)(stub - (site + 5)));
    if (!WriteCode(site, patch, 5)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return 0;
    }
    return 1;
}

static void InstallMdkBody(unsigned char *code, unsigned int codeSize)
{
    unsigned char *c;
    int            W = (int)g_targetW, H = (int)g_targetH;
    int            dx0, dy0, dw, dh;
    unsigned int   sx, sy;

    if (W < 16 || H < 16) return;

    c = FindUnique(code, codeSize, mdk_bodyfrom_sig, sizeof(mdk_bodyfrom_sig));
    if (!c) return;
    g_mdkBodyFrom = (unsigned int)(c + MDK_BODYFROM_RET);

    g_mdkBodyBuf = (unsigned short *)
        VirtualAlloc(NULL, MDK_BODY_W * MDK_BODY_H * 2,
                     MEM_COMMIT, PAGE_READWRITE);
    g_mdkBodyRow = (unsigned short *)
        VirtualAlloc(NULL, 4096 * 2, MEM_COMMIT, PAGE_READWRITE);
    g_mdkBodyCol = (int *)
        VirtualAlloc(NULL, 4096 * 4, MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkBodyBuf || !g_mdkBodyRow || !g_mdkBodyCol) return;

    if (!InstallMdkBodyOne(code, codeSize, mdk_body1_sig,
                           sizeof(mdk_body1_sig), MDK_BODY1_SITE, "1"))
        return;
    if (!InstallMdkBodyOne(code, codeSize, mdk_body2_sig,
                           sizeof(mdk_body2_sig), MDK_BODY2_SITE, "2"))
        return;                                  /* half-hooked is still safe:
                                                    the box below stays K29's */

    MdkFitRect(MDK_BODY_W, MDK_BODY_H, W, H, 1,
               &dx0, &dy0, &dw, &dh, &sx, &sy);
    if (dw <= 0 || dh <= 0) return;

    g_mdkCompX0 = dx0;  g_mdkCompY0 = dy0;
    g_mdkCompW  = dw;   g_mdkCompH  = dh;

}

#define MDK_SCOPE_ROWS 400              /* slack, in case the image is taller */

static unsigned short *g_mdkScopeBuf = NULL;    /* 600 x MDK_SCOPE_ROWS      */
static unsigned short *g_mdkScopeRow = NULL;    /* one expanded output row   */
static int            *g_mdkScopeCol = NULL;    /* precomputed source column */
static unsigned short  g_mdkScopeKey = 0;       /* the transparent sentinel  */
static int             g_mdkScopeCols = 0;      /* columns already built     */

/* A 16-bit value the palette cannot produce, so "never written" is exact. */
static unsigned short MdkScopeSentinel(void)
{
    const unsigned short *pal = (const unsigned short *)MDK_BD_PAL;
    unsigned int v, i;

    if (IsBadReadPtr(pal, 512)) return 0xf81fu;      /* magenta, best effort */

    for (v = 0; v < 0x10000u; v++) {
        for (i = 0; i < 256; i++)
            if (pal[i] == (unsigned short)v) break;
        if (i == 256) return (unsigned short)v;
    }
    return 0xf81fu;                                  /* cannot happen */
}

extern "C" void __attribute__((cdecl, used, noinline))
MdkScopeBefore(unsigned int *f)
{
    unsigned int n;

    if (!f || !g_mdkScopeBuf) return;

    n = (unsigned int)MDK_SCOPE_W * MDK_SCOPE_ROWS;
    while (n--) g_mdkScopeBuf[n] = g_mdkScopeKey;

    f[5] = (unsigned int)g_mdkScopeBuf;    /* edx -- destination            */
    f[4] = 0;                              /* ebx -- row advance; we are    */
                                           /*        exactly 600 wide       */
}

extern "C" void __attribute__((cdecl, used, noinline))
MdkScopeAfter(unsigned int *f)
{
    const unsigned short *src = g_mdkScopeBuf;
    unsigned short       *base, *dst, *row = g_mdkScopeRow;
    int                  *col = g_mdkScopeCol;
    int    strideP, W, H, x, y, lastRow = -1;
    int    haveBody = (g_mdkBodyHave && g_mdkCompW > 0);
    int    dx0, dy0, dw, dh;
    unsigned int sxStep, syStep, sy;

    if (!f || !src || !row || !col) return;
    if (!g_mdkLfbPtr || !*g_mdkLfbPtr || !g_mdkStride) return;

    base    = (unsigned short *)*g_mdkLfbPtr;   /* the TRUE origin, not the  */
    strideP = (int)(*g_mdkStride) / 2;          /* K5-centred one            */
    W = (int)g_targetW;
    H = (int)g_targetH;
    if (!base || strideP <= 0 || W <= 0 || H <= 0) return;
    if (W > strideP) W = strideP;               /* the 2048 LFB limit        */
    if (W > 4096) W = 4096;                     /* our row/col buffers       */

    /* The destination is the WINDOW InstallMdkSnipe computed, which every
       viewport, the crosshair and the HUD are already mapped through -- so
       there is one box, not two that have to be kept in step. */
    dx0 = g_mdkSnipeX0;  dy0 = g_mdkSnipeY0;
    dw  = g_mdkSnipeW;   dh  = g_mdkSnipeH;
    if (dw <= 0 || dh <= 0) return;
    sxStep = ((unsigned int)MDK_SCOPE_W << 16) / (unsigned int)dw;
    syStep = ((unsigned int)MDK_SCOPE_H << 16) / (unsigned int)dh;

    /* K38.  The scope BODY, and the bars outside it, repainted from the
       captured 640x480 image every frame.  It has to be every frame: the
       game blits the body ONCE on the way in, and the sky is drawn full
       width afterwards -- the old full-height overlay used to cover that,
       and with a window smaller than the picture it no longer would.
       When the body was never captured this does nothing, and the loop
       below paints its own black bars as before. */
    MdkBodyBlit(1);

    /* K41's blackout used to be here and has MOVED to the frame's first
       clear -- see MdkCamBlank.  Painted at the overlay it also painted over
       the hit FX, which the cam draws without asking 0x471e80 for anything
       and which therefore looked idle.  Doing it before the game draws into
       the ovals removes the need to recognise the FX at all. */

    /* Build the column table once per band, not per frame. */
    if (g_mdkScopeCols != W) {
        unsigned int sx = 0;

        for (x = 0; x < W; x++) {
            int c = -1;

            if (x >= dx0 && x < dx0 + dw) {
                c = (int)(sx >> 16);
                if (c >= MDK_SCOPE_W) c = MDK_SCOPE_W - 1;
                sx += sxStep;
            }
            col[x] = c;
        }
        g_mdkScopeCols = W;
    }

    g_mdkInScope   = 1;         /* the scope is up: see the HUD branch in K5 */
    GameFix_TriHot = 1;
    g_mdkScopeHold = 2;         /* ...and still up at the NEXT frame's clear.
                                   TWO is the minimum that survives the swap
                                   decrement, and the minimum still matters:
                                   it is how many frames K31 goes on moving
                                   the barrel props after snipe mode ends. */

    sy = 0;
    for (y = 0; y < H; y++) {
        int srcRow;

        dst = base + (unsigned int)y * (unsigned int)strideP;

        /* Outside the WINDOW.  MdkBodyBlit has already put the scope body
           and the bars there, so this only paints when there is no body --
           the K29 fallback, where the window is the whole picture and
           anything outside it is whatever the previous frame left. */
        if (y < dy0 || y >= dy0 + dh) {
            if (!haveBody) memset(dst, 0, (unsigned int)W * 2);
            continue;
        }

        srcRow = (int)(sy >> 16);
        sy += syStep;
        if (srcRow >= MDK_SCOPE_H) srcRow = MDK_SCOPE_H - 1;

        /* Resample horizontally once per SOURCE row -- the vertical
           magnification is 2.22x at 1920x800, so most output rows repeat. */
        if (srcRow != lastRow) {
            const unsigned short *s = src + (unsigned int)srcRow * MDK_SCOPE_W;

            for (x = 0; x < W; x++)
                row[x] = (col[x] < 0) ? g_mdkScopeKey : s[col[x]];
            lastRow = srcRow;
        }

        if (!haveBody) {
            for (x = 0; x < dx0; x++)      dst[x] = 0;
            for (x = dx0 + dw; x < W; x++) dst[x] = 0;
        }

        /* Per pixel, because transparency means this cannot be a memcpy: the
           holes have to keep whatever the 3D drew underneath. */
        for (x = dx0; x < dx0 + dw; x++)
            if (row[x] != g_mdkScopeKey) dst[x] = row[x];
    }
}

static const unsigned char mdk_scope_sig[] =     /* 0x0040dd07 */
    { 0x89, 0xf0, 0x8b, 0x55, 0xec, 0x81, 0xeb, 0xb0, 0x04, 0x00, 0x00 };
#define MDK_SCOPE_CALL_AT  (sizeof(mdk_scope_sig))   /* the call follows */

static const unsigned char mdk_scope_stub[] = {
    0x60,                       // pushad
    0x54,                       // push  %esp
    0xe8, 0, 0, 0, 0,           // call  MdkScopeBefore       rel32 <- @3
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad          (carries edx/ebx back)
    0xe8, 0, 0, 0, 0,           // call  the game decoder     rel32 <- @12
    0x60,                       // pushad
    0x54,                       // push  %esp
    0xe8, 0, 0, 0, 0,           // call  MdkScopeAfter        rel32 <- @19
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xc3                        // ret   -> 0x40dd17, the unlock
};
#define MDK_SCOPE_BEFORE_AT  3
#define MDK_SCOPE_DEC_AT    12
#define MDK_SCOPE_AFTER_AT  19

/* Every viewport that lives inside the scope, scaled by the SAME factor as
   the scope art itself.

   The reference shot settled what mode 1 actually is.  Unpatched at 640x480
   the 3D view is a 384x280 window at (108,80) in layout space, sitting behind
   the big hole in the scope frame -- it was never the whole screen.  Build
   0140 made it the whole screen, which is why the world showed through the
   three ovals at the top as well as through the hole.

   So the right model is: the scope art defines the picture, and everything
   drawn inside it -- the sniper view and the three bullet cams -- is a
   rectangle in that same 600x360 layout.  Scale the art into a letterboxed
   box, scale every viewport by the same factor into the same box, and the
   whole assembly stays exactly as designed, only bigger.

   Two consequences worth stating, because they are why this is simpler than
   0140 rather than more complicated:

   - THE ASPECT PATCH IS GONE.  0140 rescaled the three mode-1 frustum
     constants because a stretched viewport needs a stretched frustum.  A
     uniform scale does not: 384:280 stays 384:280, so the frustum is already
     right and touching it would be the bug.

   - Nothing here is resolution-dependent beyond one number, k.  At 1920x800
     the box is 1333x800 at x=293 and k = 2.2222, which is exactly H/360 --
     the same scale the rest of the game already uses.

   The three cams are modes 2/3/4, selected from one site that loops over the
   three (0x460b7c does lea 0x2(%edx),%eax).  They are the three ovals along
   the top, and they stay black until a bullet is in flight.

   Mode 3 has no Cy constant at all -- its viewport starts at y=0, so the
   compiler folded the add away.  That means its vertical origin cannot take
   dy0.  Harmless while the bars are vertical (dy0 = 0), which is every
   widescreen override; a taller-than-4:3 mode would put that one cam a little
   high. */
static void MdkScaleViewport(unsigned int kx, unsigned int cx,
                             unsigned int ky, unsigned int cy,
                             double oKx, double oCx, double oKy, double oCy,
                             double k, double dx0, double dy0)
{
    MdkPatchDouble((unsigned char *)kx, oKx, oKx * k);
    MdkPatchDouble((unsigned char *)cx, oCx, dx0 + oCx * k);
    MdkPatchDouble((unsigned char *)ky, oKy, oKy * k);
    if (cy) MdkPatchDouble((unsigned char *)cy, oCy, dy0 + oCy * k);
}

/* The scope: its art, the sniper view, and the three bullet cams. */
/* K38's other half: mode 3's projection has no Cy, and now it needs one.
 *
 * The middle bullet cam's viewport starts at layout y=0, so Watcom folded the
 * `+ Cy` away and its routine is six bytes shorter than its three siblings.
 * That was harmless while the window's own y0 was zero -- which it was for
 * every widescreen override under K29's box.  K38's window starts 100 px down
 * inside the composition, so without a Cy that cam would draw a hundred pixels
 * above its own hole, on top of the scope body.
 *
 * Mode 4 is the SAME routine with the add present -- byte for byte apart from
 * its five constants -- so it is the template.  Copy its 66 bytes, point four
 * of the five at mode 3's own constants (which MdkScaleViewport goes on
 * scaling exactly as before), and give the fifth a double of ours.  The body
 * contains no relative branch, only x87 with absolute operands, so it
 * relocates unchanged.
 *
 * Switching to the copy is one imm32: `0x46cab0` appears exactly ONCE in the
 * image, as the operand of the dispatcher's own `mov $mode3,%edx`.  The mode 3
 * body is read out of that instruction rather than named here, so the two
 * cannot disagree.
 *
 * Fails closed: on any check failing, mode 3 keeps the game's own routine and
 * K38's box is still correct for everything else.
 */
static const unsigned char mdk_mode4_sig[] =     /* 0x0046caec */
    { 0x55, 0x89, 0xe5,                          /* push %ebp ; mov %esp,%ebp */
      0xd9, 0x00, 0xd8, 0x40, 0x08, 0xd8, 0x70, 0x08,  /* (x/z + 1)           */
      0xdc, 0x0d, 0x0c, 0xfd, 0x48, 0x00 };      /* fmull mode4's Kx          */

static const unsigned char mdk_mode3arm_sig[] =  /* 0x0046cb67 */
    { 0xba, 0xb0, 0xca, 0x46, 0x00,              /* mov $0x46cab0,%edx */
      0xeb, 0xdf };                              /* jmp the common store */

#define MDK_MODE_LEN   66
#define MDK_MODE_KX    13
#define MDK_MODE_CX    19
#define MDK_MODE_BIAS  25
#define MDK_MODE_KY    47
#define MDK_MODE_CY    53

static unsigned int InstallMdkMode3Cy(unsigned char *code, unsigned int codeSize)
{
    unsigned char *m4, *arm, *m3, *stub;
    double        *cy;
    unsigned char  imm[4];

    m4  = FindUnique(code, codeSize, mdk_mode4_sig, sizeof(mdk_mode4_sig));
    arm = FindUnique(code, codeSize, mdk_mode3arm_sig, sizeof(mdk_mode3arm_sig));
    if (!m4 || !arm) return 0;

    m3 = (unsigned char *)ReadU32(arm + 1);      /* from the dispatcher itself */
    if (IsBadReadPtr(m3, MDK_MODE_LEN)) return 0;

    /* The two bodies have to be the shape this depends on: mode 3's Ky is an
       fmull at the same offset, and mode 4's Cy is an faddl six bytes later. */
    if (m3[MDK_MODE_KY - 2] != 0xdc || m3[MDK_MODE_KY - 1] != 0x0d
        || m4[MDK_MODE_CY - 2] != 0xdc || m4[MDK_MODE_CY - 1] != 0x05) {
        return 0;
    }

    stub = (unsigned char *)VirtualAlloc(NULL, MDK_MODE_LEN, MEM_COMMIT,
                                         PAGE_EXECUTE_READWRITE);
    cy   = (double *)VirtualAlloc(NULL, 8, MEM_COMMIT, PAGE_READWRITE);
    if (!stub || !cy) return 0;
    *cy = 0.0;                        /* MdkScaleViewport writes dy0 into it */

    memcpy(stub, m4, MDK_MODE_LEN);              /* the template, from the site */
    PutU32(stub + MDK_MODE_KX,   ReadU32(m3 + MDK_MODE_KX));
    PutU32(stub + MDK_MODE_CX,   ReadU32(m3 + MDK_MODE_CX));
    PutU32(stub + MDK_MODE_BIAS, ReadU32(m3 + MDK_MODE_BIAS));
    PutU32(stub + MDK_MODE_KY,   ReadU32(m3 + MDK_MODE_KY));
    PutU32(stub + MDK_MODE_CY,   (unsigned int)cy);

    PutU32(imm, (unsigned int)stub);
    if (!WriteCode(arm + 1, imm, 4)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return 0;
    }

    return (unsigned int)cy;
}
static void InstallMdkSnipe(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *target;
    unsigned char  patch[5];
    int            W = (int)g_targetW, H = (int)g_targetH;
    int            dx0, dy0, dw, dh;
    unsigned int   sx, sy;
    double         k, fx, fy;

    if (W < 16 || H < 16) return;

    /* K38.  The window is the composition's own (20,60)..(620,420), mapped
       into the 640x480 letterbox -- so the scope body around it is part of
       the picture instead of being pushed off the top and bottom.

       Falls back to K29's 600x360 fit when the body hook did not install:
       a smaller window on a black ring would be worse than the crop it
       replaces, so this fails to the confirmed behaviour. */
    /* The plain layout box first, and unconditionally: the bombing run is
       laid out in the game's 600x360 space with no composition around it,
       so it must not inherit the scope's inset.  It also serves as K29's
       fallback window if the body hook declined. */
    MdkFitRect(MDK_SCOPE_W, MDK_SCOPE_H, W, H, 1,
               &dx0, &dy0, &dw, &dh, &sx, &sy);
    g_mdkLayoutX0 = dx0;  g_mdkLayoutY0 = dy0;
    g_mdkLayoutW  = dw;   g_mdkLayoutH  = dh;

    if (g_mdkCompW > 0 && g_mdkCompH > 0) {
        dx0 = g_mdkCompX0 + MDK_WIN_X * g_mdkCompW / MDK_BODY_W;
        dy0 = g_mdkCompY0 + MDK_WIN_Y * g_mdkCompH / MDK_BODY_H;
        dw  = MDK_SCOPE_W * g_mdkCompW / MDK_BODY_W;
        dh  = MDK_SCOPE_H * g_mdkCompH / MDK_BODY_H;
    }
    if (dw <= 0 || dh <= 0) return;

    k  = (double)dw / (double)MDK_SCOPE_W;
    fx = (double)dx0;
    fy = (double)dy0;

    g_mdkSnipeX0 = dx0;  g_mdkSnipeY0 = dy0;
    g_mdkSnipeW  = dw;   g_mdkSnipeH  = dh;

    {
        unsigned char *c = FindUnique(code, codeSize, mdk_cross_sig,
                                      sizeof(mdk_cross_sig));
        g_mdkCrossRet = c ? (unsigned int)(c + MDK_CROSS_RET) : 0;

        c = FindUnique(code, codeSize, mdk_cam_sig, sizeof(mdk_cam_sig));
        g_mdkCamRet = c ? (unsigned int)(c + MDK_CAM_RET) : 0;

        c = FindUnique(code, codeSize, mdk_camrot_sig,
                       sizeof(mdk_camrot_sig));
        g_mdkCamRotRet = c ? (unsigned int)(c + MDK_CAMROT_RET) : 0;

        c = FindUnique(code, codeSize, mdk_hpframe_sig,
                       sizeof(mdk_hpframe_sig));
        g_mdkHpFrom = c ? (unsigned int)(c + MDK_HPFRAME_RET) : 0;
    }


    /* mode 1 -- the sniper view, 384x280 at (108,80) */
    MdkScaleViewport(0x0048fc9cu, 0x0048fca4u, 0x0048fcb4u, 0x0048fcbcu,
                     191.95, 108.0, 139.95, 80.0, k, fx, fy);

    /* modes 2/3/4 -- the three bullet cams, 140x70 along the top */
    MdkScaleViewport(0x0048fcc4u, 0x0048fcccu, 0x0048fcdcu, 0x0048fce4u,
                     69.95, 72.0, 34.95, 10.0, k, fx, fy);
    /* Mode 3's Cy is ours, because the game folded its own away -- see
       InstallMdkMode3Cy.  Zero if that declined, and MdkScaleViewport then
       skips the write exactly as it always did. */
    MdkScaleViewport(0x0048fcecu, 0x0048fcf4u, 0x0048fd04u,
                     InstallMdkMode3Cy(code, codeSize),
                     69.95, 228.0, 34.95, 0.0, k, fx, fy);
    MdkScaleViewport(0x0048fd0cu, 0x0048fd14u, 0x0048fd24u, 0x0048fd2cu,
                     69.95, 384.0, 34.95, 10.0, k, fx, fy);

    /* --- the 2D scope frame: scaled out of a buffer of our own ---------- */
    at = FindUnique(code, codeSize, mdk_scope_sig, sizeof(mdk_scope_sig));
    if (!at) return;
    at += MDK_SCOPE_CALL_AT;

    target = (at + 5) + (int)ReadU32(at + 1);   /* the decoder -- section 5b */

    g_mdkScopeBuf = (unsigned short *)
        VirtualAlloc(NULL, MDK_SCOPE_W * MDK_SCOPE_ROWS * 2,
                     MEM_COMMIT, PAGE_READWRITE);
    g_mdkScopeRow = (unsigned short *)
        VirtualAlloc(NULL, 4096 * 2, MEM_COMMIT, PAGE_READWRITE);
    g_mdkScopeCol = (int *)
        VirtualAlloc(NULL, 4096 * 4, MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkScopeBuf || !g_mdkScopeRow || !g_mdkScopeCol) return;

    g_mdkScopeKey = MdkScopeSentinel();

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_scope_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_scope_stub, sizeof(mdk_scope_stub));
    PutU32(stub + MDK_SCOPE_BEFORE_AT,
           (unsigned int)(int)((unsigned char *)&MdkScopeBefore
                               - (stub + MDK_SCOPE_BEFORE_AT + 4)));
    PutU32(stub + MDK_SCOPE_DEC_AT,
           (unsigned int)(int)(target - (stub + MDK_SCOPE_DEC_AT + 4)));
    PutU32(stub + MDK_SCOPE_AFTER_AT,
           (unsigned int)(int)((unsigned char *)&MdkScopeAfter
                               - (stub + MDK_SCOPE_AFTER_AT + 4)));


    patch[0] = 0xe8;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

/* K30.  The snipe HUD is drawn at 1:1 into a scope magnified 2.22x.
 *
 * 0142 put these elements in the right PLACE and left them the wrong SIZE,
 * and for the bullets in the barrel that reads as a position error rather
 * than a size one: a 15x35 bullet anchored at the top-left of a slot drawn
 * for a 33x78 one looks like a small bullet in the wrong corner.
 *
 * Measured off the reference: the two bullets sit at layout (83,245) and
 * (111,288), and the log has this drawer at 112,304 -- the second one, to the
 * pixel in x.  So the anchor was already right in 0142.
 *
 * The drawer turns out to be trivially replaceable.  Read off its own loop:
 *
 *     eax  source pixels, flat 8bpp        [ebp+8]  transparent INDEX
 *     ecx  -> { width, height }            0x546848 the palette
 *     edx  -> { source stride }            ebx -> { x, y }
 *
 * so it is a colour-keyed 8bpp blit with the dimensions handed to it -- no
 * format to reverse, and the key is given rather than inferred (K29 needed a
 * palette sentinel precisely because it was not).
 *
 * Hooked after its lock and rejoining at its unlock, so the lock, the clip
 * and the unlock all stay the game's.  It draws scaled only while the scope
 * is up; in ordinary play it declines and the game's own loop runs, so the
 * normal HUD is byte-for-byte untouched.
 *
 * The anchor needs no work here: the base this drawer is handed has already
 * been offset by the box transform (see the HUD branch in K5), so
 * base + Y*stride + X is the correct scaled position by construction.  Only
 * the extent is ours.
 */
static int g_mdkHudDrew = 0;         /* the stub reads this after popad */

extern "C" void __attribute__((cdecl, used, noinline))
MdkHudScale(unsigned int *f)
{
    const unsigned short *pal = (const unsigned short *)MDK_BD_PAL;
    const unsigned char  *src;
    const int            *dims, *pos, *srcStridePtr;
    unsigned short       *base;
    unsigned int          ebp, sx, sy, sxStep, syStep;
    int  w, h, X, Y, strideP, srcStride, dw, dh, x, y, key;

    g_mdkHudDrew = 0;
    if (!f || !g_mdkInScope || g_mdkSnipeW <= 0) return;

    src  = (const unsigned char *)f[1];          /* esi */
    dims = (const int *)f[0];                    /* edi */
    pos  = (const int *)f[4];                    /* ebx */
    ebp  = f[2];
    if (!src || !dims || !pos || !ebp) return;
    if (IsBadReadPtr(dims, 8) || IsBadReadPtr(pos, 8)) return;
    if (IsBadReadPtr((const void *)(ebp - 0x18), 0x20)) return;

    base         = *(unsigned short **)(ebp - 0x14);
    strideP      = *(const int *)(ebp - 0x18) / 2;
    srcStridePtr = *(const int **)(ebp - 0x10);
    key          = *(const unsigned char *)(ebp + 8);
    if (!base || strideP <= 0 || !srcStridePtr) return;
    if (IsBadReadPtr(srcStridePtr, 4)) return;

    w = dims[0];  h = dims[1];
    X = pos[0];   Y = pos[1];
    srcStride = *srcStridePtr;
    if (w <= 0 || h <= 0 || srcStride < w) return;

    dw = (w * g_mdkSnipeW) / MDK_SCOPE_W;
    dh = (h * g_mdkSnipeH) / MDK_SCOPE_H;
    if (dw <= 0 || dh <= 0) return;              /* let the game draw it */
    if (IsBadReadPtr(src, (unsigned int)srcStride * (unsigned int)h)) return;

    /* Clip to the framebuffer.  The row is 4096 pixels with the linear LFB
       and 2048 without, and running off the end wraps onto the next scanline
       rather than faulting -- section 7c, which cost a whole round trip to
       diagnose once already. */
    if (X < 0 || Y < 0) return;
    if (X + dw > strideP)        dw = strideP - X;
    if (Y + dh > (int)g_targetH) dh = (int)g_targetH - Y;
    if (dw <= 0 || dh <= 0) return;

    sxStep = ((unsigned int)w << 16) / (unsigned int)dw;
    syStep = ((unsigned int)h << 16) / (unsigned int)dh;

    sy = 0;
    for (y = 0; y < dh; y++) {
        const unsigned char *s = src + (sy >> 16) * (unsigned int)srcStride;
        unsigned short      *d = base + (unsigned int)(Y + y)
                                        * (unsigned int)strideP + X;

        sy += syStep;
        sx = 0;
        for (x = 0; x < dw; x++, sx += sxStep) {
            unsigned char v = s[sx >> 16];

            if ((int)v != key) d[x] = pal[v];
        }
    }

    g_mdkHudDrew = 1;
}

static const unsigned char mdk_hudscale_sig[] =  /* 0x00419411 */
    { 0xe8, 0x62, 0x7b, 0x05, 0x00, 0x8b, 0x55, 0xe8, 0xd1, 0xfa };
#define MDK_HUDSCALE_AT   5                      /* past the lock call */
#define MDK_HUDSCALE_JOIN 0x0041948bu            /* the unlock call    */

static const unsigned char mdk_hudscale_stub[] = {
    0x60,                             // pushad
    0x54,                             // push  %esp
    0xe8, 0, 0, 0, 0,                 // call  MdkHudScale       rel32 <- @3
    0x83, 0xc4, 0x04,                 // add   $0x4,%esp
    0x61,                             // popad
    0x83, 0x3d, 0, 0, 0, 0, 0x00,     // cmpl  $0,g_mdkHudDrew   addr  <- @13
    0x74, 0x05,                       // je    +5   (we did not draw)
    0xe9, 0, 0, 0, 0,                 // jmp   the unlock        rel32 <- @21
    0x8b, 0x55, 0xe8, 0xd1, 0xfa,     // the displaced pair
    0xe9, 0, 0, 0, 0                  // jmp   back              rel32 <- @31
};
#define MDK_HS_CALL_AT  3
#define MDK_HS_FLAG_AT  13
#define MDK_HS_SKIP_AT  21
#define MDK_HS_BACK_AT  31

static void InstallMdkHudScale(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub;
    unsigned char  patch[5];

    if (g_mdkSnipeW <= 0) return;

    at = FindUnique(code, codeSize, mdk_hudscale_sig,
                    sizeof(mdk_hudscale_sig));
    if (!at) return;
    at += MDK_HUDSCALE_AT;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_hudscale_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_hudscale_stub, sizeof(mdk_hudscale_stub));
    PutU32(stub + MDK_HS_CALL_AT,
           (unsigned int)(int)((unsigned char *)&MdkHudScale
                               - (stub + MDK_HS_CALL_AT + 4)));
    PutU32(stub + MDK_HS_FLAG_AT, (unsigned int)&g_mdkHudDrew);
    PutU32(stub + MDK_HS_SKIP_AT,
           (unsigned int)(int)((unsigned char *)MDK_HUDSCALE_JOIN
                               - (stub + MDK_HS_SKIP_AT + 4)));
    PutU32(stub + MDK_HS_BACK_AT,
           (unsigned int)(int)((at + 5) - (stub + MDK_HS_BACK_AT + 4)));


    patch[0] = 0xe9;                  /* jmp, not call -- the stub returns
                                         control with its own jmp either way */
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

/* K35's installer.  It writes no code at all -- everything it does is record
   four return addresses that the two existing hooks then key on, so a failure
   here leaves the bombing run exactly as it is today rather than moving
   something it should not.

   The signature is short and pins the function; the three call sites inside it
   are then VERIFIED rather than trusted, which is what makes an offset into a
   routine safe (section 5b).  The last check is the strongest: the lock the
   counter is keyed on has to live inside the very routine the counter's own
   call goes to. */
static void InstallMdkBomb(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *f, *sprA, *sprB, *font;

    at = FindUnique(code, codeSize, mdk_bomb_sig, sizeof(mdk_bomb_sig));
    if (!at) return;

    if (at[MDK_BOMB_TARG_AT]  != 0xe8 ||
        at[MDK_BOMB_CROSS_AT] != 0xe8 ||
        at[MDK_BOMB_TEXT_AT]  != 0xe8) {
        return;
    }

    sprA = (at + MDK_BOMB_TARG_AT  + 5) + (int)ReadU32(at + MDK_BOMB_TARG_AT  + 1);
    sprB = (at + MDK_BOMB_CROSS_AT + 5) + (int)ReadU32(at + MDK_BOMB_CROSS_AT + 1);
    font = (at + MDK_BOMB_TEXT_AT  + 5) + (int)ReadU32(at + MDK_BOMB_TEXT_AT  + 1);

    if (sprA != sprB) {
        return;
    }

    f = FindUnique(code, codeSize, mdk_font_sig, sizeof(mdk_font_sig));
    if (!f || f < font || f > font + 0x80) {
        return;
    }

    g_mdkBombTargRet  = (unsigned int)(at + MDK_BOMB_TARG_AT  + 5);
    g_mdkBombCrossRet = (unsigned int)(at + MDK_BOMB_CROSS_AT + 5);
    g_mdkBombTextFrom = (unsigned int)(at + MDK_BOMB_TEXT_AT  + 5);
    g_mdkFontRet      = (unsigned int)(f + MDK_FONT_RET);

}

/* K34.  The brown band -- MDK's own leftover sky fill, and the last thing on
 * this screen still laid out in 600x360.
 *
 * The backdrop routine 0x471e80 draws the panorama for as many rows as it
 * has and paints the REST of the window with a flat colour out of the table
 * at 0x552808.  Two sites do it, one for each end:
 *
 *     471ff6   the panorama runs out below   -> fill (x, 360-skyTop)..(x+w, y+h)
 *     4721d8   it starts below the window top -> fill the strip above it
 *
 * with skyTop = [0x552dac], the source row the panorama is scrolled to.  Both
 * are FillRect(eax=x0, edx=y0, ebx=x1, ecx=y1, colour) through 0x471d28 --
 * which is grClipWindow followed by grBufferClear, so it is a hard-edged
 * solid rectangle and NO 2D blitter hook could ever have seen it.  Nor could
 * the triangle probe: it is not geometry either.
 *
 * The arithmetic identifies it rather than fitting it.  The log carries
 * `live rotAC = 165`, and the band measured off the screenshot starts at
 * y = 195 = 360 - 165 exactly; a second frame gave 236 against a skyTop of
 * 124.  It is 600 wide because the size struct 0x471e80 is handed still
 * describes the original window, and it slides vertically with the camera
 * PITCH while staying put horizontally -- which is exactly "it moves with the
 * camera, always in the same place whichever way we look".
 *
 * K14 draws the panorama itself at S = H/360 across the whole screen, so the
 * fill has to be scaled to match or it covers a strip of the corner instead
 * of the band under the sky.  Same two factors K33 gives the damage overlay,
 * which comes out of the same routine.
 *
 * Why it only turned up with the health-fill build: until then our sky was
 * redrawn on EVERY grBufferClear, and this fill goes through one -- so the
 * sky was painted straight back over it every time.  Making the sky draw once
 * a frame stopped hiding it.  The band has been there since K14.
 *
 * 0x471e80 is SHARED with the bullet cams, whose own black/brown overlay must
 * not be stretched to the screen, so this declines unless the rectangle
 * really is the full-width window and the routine was not entered from the
 * cam site.  Declining leaves the game's own fill -- today's behaviour --
 * rather than moving something that should stay put.
 */
extern "C" void __attribute__((cdecl, used, noinline))
MdkSkyFill(unsigned int *f)
{
    int x0, y0, x1, y1, W, H;

    if (!f || !g_targetW || !g_targetH) return;

    x0 = (int)f[7];  y0 = (int)f[5];        /* eax, edx */
    x1 = (int)f[4];  y1 = (int)f[6];        /* ebx, ecx */

    if (x1 - x0 < MDK_VIEW_W - 60) return;  /* a cam oval, not the window */
    if (y1 <= y0) return;

    /* f[2] is still 0x471e80's own ebp -- the stub runs inside it -- so
       [ebp+4] names whoever asked for a backdrop. */
    if (g_mdkCamRotRet && f[2]
        && !IsBadReadPtr((const void *)(f[2] + 4), 4)
        && *(const unsigned int *)(f[2] + 4) == g_mdkCamRotRet)
        return;

    W = (int)g_targetW;  H = (int)g_targetH;


    x0 = x0 * W / MDK_VIEW_W;   x1 = x1 * W / MDK_VIEW_W;
    y0 = y0 * H / MDK_VIEW_H;   y1 = y1 * H / MDK_VIEW_H;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;

    f[7] = (unsigned int)x0;  f[5] = (unsigned int)y0;
    f[4] = (unsigned int)x1;  f[6] = (unsigned int)y1;
}

/* Both sites end the same way -- the colour looked up out of 0x552808 and
   pushed -- and differ only in which locals they load next, which is what
   makes each of them unique in the image. */
static const unsigned char mdk_skyfillA_sig[] =  /* 0x00471feb */
    { 0x8b, 0x3c, 0x95, 0x08, 0x28, 0x55, 0x00,  /* mov 0x552808(,%edx,4),%edi */
      0x57,                                      /* push %edi                  */
      0x8b, 0x55, 0xc8 };                        /* mov  -0x38(%ebp),%edx      */

static const unsigned char mdk_skyfillB_sig[] =  /* 0x004721ca */
    { 0x8b, 0x3c, 0x95, 0x08, 0x28, 0x55, 0x00,
      0x57,
      0x8b, 0x4d, 0xdc,                          /* mov  -0x24(%ebp),%ecx      */
      0x8b, 0x55, 0xc8 };

static const unsigned char mdk_skyfill_stub[] = {
    0x60,                       // pushad
    0x54,                       // push  %esp   -> &frame
    0xe8, 0, 0, 0, 0,           // call  MdkSkyFill           rel32 <- @3
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xe9, 0, 0, 0, 0            // jmp   the filler           rel32 <- @12
};
#define MDK_SF_CALL_AT   3
#define MDK_SF_JMP_AT   12

/* K41b.  Blanking an idle oval has to happen BEFORE the game draws into it,
 * not after.
 *
 * 0176 painted the black in MdkScopeAfter, which runs at the overlay -- the
 * last thing in the frame.  That is after the cams have drawn, so it also
 * painted over the hit FX: the sprite that plays in an oval when a projectile
 * lands is drawn by the cam and asks 0x471e80 for nothing, so it looked idle.
 *
 * Doing it at the frame's first clear instead, right after the sky, makes the
 * question go away rather than needing a third signal: whatever the game draws
 * in an oval afterwards -- world, fill, FX sprite, anything found later --
 * lands on top of the black by construction.  An oval nothing draws into stays
 * black, which is precisely what "idle" means and exactly what the game itself
 * relies on (it leaves an idle oval untouched and expects the cleared frame
 * underneath).
 *
 * Only the LIVE cams have to be spared, because their sky comes from K14's
 * one full-screen pass and blanking would take it away.  That is the one thing
 * the grace counter is for, and it is the one thing it can actually observe.
 *
 * grClipWindow + grBufferClear rather than LFB writes, because there is no
 * lock held here -- and it is the same pair the game's own 0x471d28 uses for
 * exactly this, so both thunks are read out of that routine (section 5b)
 * rather than named.  The nested grBufferClear re-enters GameFix_AfterClear,
 * which g_mdkSkyBusy already guards.
 */
static unsigned int g_mdkClipFn  = 0;      /* grClipWindow, from 0x471d28+7  */
static unsigned int g_mdkClearFn = 0;      /* grBufferClear, from +20        */

static void MdkCamBlank(void)
{
    typedef void (__stdcall *ClipFn)(unsigned int, unsigned int,
                                     unsigned int, unsigned int);
    typedef void (__stdcall *ClearFn)(unsigned int, unsigned int, unsigned int);
    int i, W, H, blanked = 0;

    if (!g_mdkClipFn || !g_mdkClearFn) return;
    if (g_mdkScopeHold <= 0 || g_mdkSnipeW <= 0 || g_mdkSnipeH <= 0) return;

    W = (int)g_targetW;
    H = (int)g_targetH;
    if (W <= 0 || H <= 0) return;

    for (i = 0; i < 3; i++) {
        int x0, y0, x1, y1;

        if (g_mdkCamSky[i]) continue;        /* live: its sky must survive */

        x0 = g_mdkSnipeX0 + mdk_cam_box[i][0] * g_mdkSnipeW / MDK_SCOPE_W;
        y0 = g_mdkSnipeY0 + mdk_cam_box[i][1] * g_mdkSnipeH / MDK_SCOPE_H;
        x1 = g_mdkSnipeX0 + mdk_cam_box[i][2] * g_mdkSnipeW / MDK_SCOPE_W;
        y1 = g_mdkSnipeY0 + mdk_cam_box[i][3] * g_mdkSnipeH / MDK_SCOPE_H;

        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > W) x1 = W;
        if (y1 > H) y1 = H;
        if (x1 <= x0 || y1 <= y0) continue;

        ((ClipFn)g_mdkClipFn)((unsigned int)x0, (unsigned int)y0,
                              (unsigned int)x1, (unsigned int)y1);
        ((ClearFn)g_mdkClearFn)(0, 0, 0);
        blanked = 1;
    }

    /* Put the clip back, exactly as 0x471d28 does after its own fill. */
    if (blanked)
        ((ClipFn)g_mdkClipFn)(0, 0, (unsigned int)W, (unsigned int)H);
}

/* The sky pass and the blackout are ONE operation and must not be separable.
   0177 called MdkCamBlank from GameFix_AfterClear and missed that the tick
   draws the sky a second time straight afterwards -- which painted over the
   ovals again, so the sky was back in them.  Both callers go through here
   now, and a third one could not get it wrong. */
static void MdkSkyPass(int force)
{
    MdkSkyDrawHW(force);
    MdkCamBlank();
}

static void InstallMdkSkyFillOne(unsigned char *code, unsigned int codeSize,
                                 const void *body,
                                 const unsigned char *sig, unsigned int sigLen,
                                 const char *tag)
{
    unsigned char *at, *stub, *target;
    unsigned char  patch[5];

    at = FindUnique(code, codeSize, sig, sigLen);
    if (!at) return;
    at += sigLen;                              /* the call follows the load */

    target = (at + 5) + (int)ReadU32(at + 1);  /* from the displaced call   */

    /* K41b needs grClipWindow and grBufferClear, and THIS routine is where
       the game itself keeps them: clip, clear, clip.  Read out of the very
       routine we are already redirecting into, so they cannot be a pair of
       guessed addresses. */
    if (target[7] == 0xe8 && target[20] == 0xe8) {
        g_mdkClipFn  = (unsigned int)((target + 7 + 5)
                                      + (int)ReadU32(target + 8));
        g_mdkClearFn = (unsigned int)((target + 20 + 5)
                                      + (int)ReadU32(target + 21));
    }

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_skyfill_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_skyfill_stub, sizeof(mdk_skyfill_stub));
    PutU32(stub + MDK_SF_CALL_AT,
           (unsigned int)(int)((const unsigned char *)body
                               - (stub + MDK_SF_CALL_AT + 4)));
    PutU32(stub + MDK_SF_JMP_AT,
           (unsigned int)(int)(target - (stub + MDK_SF_JMP_AT + 4)));


    patch[0] = 0xe8;
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

/* K40.  The bullet cams' own black, and what actually draws it.
 *
 * 0174 gave the cams their sky back by deleting K14's clamp, and that exposed
 * the thing the clamp had been standing in for: with nothing in flight the
 * ovals showed sky instead of black.  The note I left with 0174 said the cover
 * sprite blanks them.  It does not.
 *
 * 0x471e80 is not "draw the sky" -- it is "draw the backdrop for THIS
 * viewport", called once per viewport with that viewport's own rect, and the
 * cams call it too (`rot caller=00460cec pos=72,10` in the capture, which is
 * cam one's viewport exactly).  Its first act is to read 0x5396f8:
 *
 *     0   blit the panorama          the sniper view, and a live cam
 *     1   FILL THE RECT FLAT         an idle cam -- the black, and the brown
 *                                    that fades after a hit
 *     any other value: draw nothing
 *
 * So the black is the fill at 0x472367, through the same grClipWindow +
 * grBufferClear helper K34 hooks twice for the sky's leftovers -- and K34
 * deliberately left this third site alone, on the reasoning that it was "the
 * cams' own overlay".  True, and exactly why it needed the same treatment: it
 * is a rectangle in the game's 600x360 layout, so with the viewport origin
 * forced to (0,0) it was being cleared in the top-left corner of the screen,
 * nowhere near the oval it belongs to.  Behind the scope body, where nobody
 * could see it -- which is why it read as "the cams are just never blanked".
 *
 * Mapped through the SCOPE WINDOW, the same box the cam's own projection was
 * given, so the fill lands on the viewport it is meant to cover: cam one's
 * (72,10)..(212,80) becomes (579,116)..(813,233) against an oval measured at
 * 579..812, 117..234.
 *
 * Keyed on the caller of 0x471e80, which is how K34's own body already tells
 * this case apart -- there it declines for the cam, here it acts only for it.
 * The two halves of one question, in one place.
 */
extern "C" void __attribute__((cdecl, used, noinline))
MdkCamFill(unsigned int *f)
{
    unsigned int ebp;
    int          x0, y0, x1, y1, W, H;

    if (!f || g_mdkSnipeW <= 0 || g_mdkSnipeH <= 0) return;

    ebp = f[2];                                  /* 0x471e80's own frame */
    if (!ebp || IsBadReadPtr((const void *)(ebp + 4), 4)) return;
    if (!g_mdkCamRotRet || *(const unsigned int *)(ebp + 4) != g_mdkCamRotRet)
        return;                                  /* not a bullet cam */

    x0 = (int)f[7];  y0 = (int)f[5];             /* eax, edx */
    x1 = (int)f[4];  y1 = (int)f[6];             /* ebx, ecx */
    if (x1 <= x0 || y1 <= y0) return;

    {   /* the game is painting this oval itself -- hands off it */
        int ci = MdkCamIndex(x0);

        if (ci >= 0) g_mdkCamSky[ci] = 2;
    }

    W = (int)g_targetW;
    H = (int)g_targetH;

    x0 = g_mdkSnipeX0 + x0 * g_mdkSnipeW / MDK_SCOPE_W;
    x1 = g_mdkSnipeX0 + x1 * g_mdkSnipeW / MDK_SCOPE_W;
    y0 = g_mdkSnipeY0 + y0 * g_mdkSnipeH / MDK_SCOPE_H;
    y1 = g_mdkSnipeY0 + y1 * g_mdkSnipeH / MDK_SCOPE_H;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x1 <= x0 || y1 <= y0) return;


    f[7] = (unsigned int)x0;  f[5] = (unsigned int)y0;
    f[4] = (unsigned int)x1;  f[6] = (unsigned int)y1;
}

/* The third fill site: the colour looked up out of 0x552848 and pushed, then
   the call.  Unique image-wide, and the call is at the same offset past the
   signature as K34's two, so it installs through the same helper. */
static const unsigned char mdk_camfill_sig[] =   /* 0x00472360 */
    { 0x8b, 0x35, 0x48, 0x28, 0x55, 0x00,        /* mov 0x552848,%esi  colour */
      0x56 };                                    /* push %esi                 */

static void InstallMdkSkyFill(unsigned char *code, unsigned int codeSize)
{
    /* All three flat fills 0x471e80 can issue, through one helper: two for
       the sky's leftovers (K34) and one for the bullet cams' own black
       (K40).  Same drawer, same shape, opposite caller test. */
    InstallMdkSkyFillOne(code, codeSize, (const void *)&MdkSkyFill,
                         mdk_skyfillA_sig,
                         sizeof(mdk_skyfillA_sig), "below");
    InstallMdkSkyFillOne(code, codeSize, (const void *)&MdkSkyFill,
                         mdk_skyfillB_sig,
                         sizeof(mdk_skyfillB_sig), "above");
    InstallMdkSkyFillOne(code, codeSize, (const void *)&MdkCamFill,
                         mdk_camfill_sig,
                         sizeof(mdk_camfill_sig), "cam");
}

static const unsigned char mdk_bd_sig[] =        /* 0x0046e7f3 */
    "\xe8\x80\x27\x00\x00\x8b\x55\xe8\xd1\xfa";
#define MDK_BD_SITE_AT   5                       /* the 5 bytes we replace   */
#define MDK_BD_REJOIN    0x0046e872u             /* the unlock call          */

static const unsigned char mdk_bd_stub[] = {
    0x60,                       // pushad
    0x54,                       // push  %esp   -> &frame
    0xe8, 0, 0, 0, 0,           // call  MdkBackdrop          rel32 <- @3
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xe9, 0, 0, 0, 0            // jmp   the unlock call      rel32 <- @12
};
#define MDK_BD_CALL_AT    3
#define MDK_BD_JMP_AT    12

//
// K23.  The intro GAMEPLAY backdrop -- K20's twin, with horizontal scrolling.
//
// K20 fixed the menu and the intro sequence's starfield.  The dive still drew
// 1:1, because it is a DIFFERENT routine: 0x4101cc, in the intro module right
// beside the moon (0x410308) and Earth (0x4103b0) drawers, gated on the same
// 0x53970c flag they are, and using the same 8bpp palette at 0x546848.  Same
// 600x360, but scrolled horizontally with a wrap:
//
//     lock view LFB   -> base, strideBytes ; stride /= 2
//     for (row = 0; row < 0x168; row++) {
//         0x46e884(dst, src + 600-scroll, scroll)      ; the wrapped part
//         0x46e884(dst + 2*scroll, src, 600-scroll)    ; the rest
//         src += 600 ; dst += stride
//     }
//
// It has TWO of those loops -- one per sign of the scroll -- so both are
// hooked, sharing one body.  The two branches' formulas reduce to the same
// thing, which is what makes that possible:
//
//     scroll >= 0 :  shown[i] = src[(600 - scroll + i) mod 600]
//     scroll <  0 :  shown[i] = src[(i - scroll) mod 600]
//     and (600 - scroll + i) mod 600 == (i - scroll) mod 600
//
// Entered after the lock and rejoined at each branch's unlock (section 5c), so
// the lock, the pointer and the teardown all stay with the game.  As in K20 the
// origin is taken from the framebuffer global rather than the value the lock
// returned, because K5 has already offset that one by the 2D centring -- using
// it here is what turned K20's first build into a diagonal.
//
#define MDK_IBG_SRC     0x004e5cecu       /* pointer to the 600x360 source   */
#define MDK_IBG_SCROLL  0x00492510u       /* the horizontal scroll, signed   */

extern "C" void __attribute__((cdecl, used, noinline))
MdkIntroBg(unsigned short *base, int strideP)
{
    const unsigned char  *src;
    const unsigned short *pal = (const unsigned short *)MDK_BD_PAL;
    unsigned short       *row = g_mdkBdRow, *dst;
    int                   scroll, W, H, x, y, lastRow = -1;
    unsigned int          sxStep, syStep, sy, col0;

    (void)base;

    src    = *(const unsigned char * const *)MDK_IBG_SRC;
    scroll = *(const int *)MDK_IBG_SCROLL;

    /* Log the FIRST call whatever happens, including every early-out.
     *
     * 0119 hooked both branches of this routine -- the header proved that --
     * and the dive background did not change.  "Hooked but never runs" and
     * "runs and declines" are then indistinguishable, which is precisely the
     * failure GAME-PATCHING section 6 warns about: a guarded hook that logs
     * nothing is still saying something, so say it. */

    if (!row || !g_mdkLfbPtr || !*g_mdkLfbPtr || strideP <= 0) return;
    if (!src) return;

    W = (int)g_targetW;
    H = (int)g_targetH;
    if (W <= 0 || H <= 0) return;
    if (W > strideP) W = strideP;              /* never run off the row */

    sxStep = ((unsigned int)MDK_BD_W << 16) / (unsigned int)W;
    syStep = ((unsigned int)MDK_BD_H << 16) / (unsigned int)H;

    /* Start column, wrapped into 0..599, in 16.16.  Kept as a running value so
       the inner loop needs no modulo -- one subtract when it passes the end. */
    scroll %= MDK_BD_W;
    if (scroll < 0) scroll += MDK_BD_W;
    col0 = (unsigned int)((MDK_BD_W - scroll) % MDK_BD_W) << 16;

    base = (unsigned short *)*g_mdkLfbPtr;

    sy = 0;
    for (y = 0; y < H; y++, sy += syStep) {
        int srcRow = (int)(sy >> 16);

        if (srcRow >= MDK_BD_H) srcRow = MDK_BD_H - 1;

        if (srcRow != lastRow) {
            const unsigned char *s = src + (unsigned int)srcRow * MDK_BD_W;
            unsigned int         sx = col0;

            for (x = 0; x < W; x++) {
                row[x] = pal[s[sx >> 16]];
                sx += sxStep;
                if (sx >= ((unsigned int)MDK_BD_W << 16))
                    sx -= (unsigned int)MDK_BD_W << 16;
            }
            lastRow = srcRow;
        }

        dst = base + (unsigned int)y * (unsigned int)strideP;
        memcpy(dst, row, (unsigned int)W * 2);
    }
}

/* Two sites, identical shape, different frame slots and rejoin.  `base` and
   `stride` are pushed by the stub from the routine's own locals, which is why
   each site needs its own displacements. */
struct MdkIbgSite {
    const unsigned char *sig;
    unsigned char        baseDisp;    /* push -X(%ebp) -> the LFB base   */
    unsigned char        strideDisp;  /* push -X(%ebp) -> stride, PIXELS */
    unsigned int         rejoin;      /* the branch's unlock call        */
};

static const unsigned char mdk_ibgA_sig[] = "\x8b\x75\xe8\x31\xff";   /* 0x410213 */
static const unsigned char mdk_ibgB_sig[] = "\x8d\x04\x3f\x31\xf6";   /* 0x410296 */

static const MdkIbgSite mdk_ibg[] = {
    { mdk_ibgA_sig, 0xdc, 0xe0, 0x00410262u },   /* base [-0x24] stride [-0x20] */
    { mdk_ibgB_sig, 0xd4, 0xd8, 0x004102e4u }    /* base [-0x2c] stride [-0x28] */
};
#define MDK_IBG_SITES (sizeof(mdk_ibg) / sizeof(mdk_ibg[0]))

static const unsigned char mdk_ibg_stub[] = {
    0x60,                       // pushad
    0xff, 0x75, 0x00,           // push  -X(%ebp)   stride        <- disp @3
    0xff, 0x75, 0x00,           // push  -X(%ebp)   base          <- disp @6
    0xe8, 0, 0, 0, 0,           // call  MdkIntroBg        rel32  <- @8
    0x83, 0xc4, 0x08,           // add   $0x8,%esp
    0x61,                       // popad
    0xe9, 0, 0, 0, 0            // jmp   the unlock        rel32  <- @17
};
#define MDK_IBG_STRIDE_AT   3
#define MDK_IBG_BASE_AT     6
#define MDK_IBG_CALL_AT     8
#define MDK_IBG_JMP_AT     17

//
// K24.  The intro-GAMEPLAY (dive) background -- the perspective floor.
//
// This is NOT a blit, which is why K23 was aimed at the wrong thing.  0x412978
// locks the framebuffer and calls a span renderer at 0x46f6a0 whose control
// data is SIXTEEN PRECOMPUTED TABLES at 0x4e5d74[frame] -- span lengths plus a
// per-pixel shading byte, generated for a 600-column view.  Regenerating those
// for W columns, with the perspective and the shading still right, is a large
// and risky job.
//
// So instead: let it render at its native 600x360 into a buffer of ours, and
// scale that out.  Exactly K20's mechanism, which is confirmed working and
// fast, and it needs to understand nothing about the span format.
//
// The renderer takes one parameter block in eax:
//
//      +0x28  destination row advance   (the game sets stride - 1200,
//                                        i.e. "next row after 600 pixels")
//      +0x2c  destination pointer       (written by the framebuffer lock)
//
// Pointing +0x2c at a 600-pixel-wide buffer and setting +0x28 to zero is the
// whole redirection: the renderer already advances 2 bytes per pixel across
// 600 pixels, so a zero row advance lands exactly on the next row.
//
// Hooked at the CALL (section 5b) so the lock, the unlock and everything else
// stay with the game, and the renderer target is read out of the displaced
// instruction rather than hardcoded -- the mistake that crashed 0116.
//
#define MDK_DIVE_W   600
#define MDK_DIVE_H   360

static unsigned short *g_mdkDiveBuf  = NULL;
static unsigned int    g_mdkDiveDest = 0;
static unsigned int    g_mdkDiveAdv  = 0;

extern "C" void __attribute__((cdecl, used, noinline))
MdkDiveBefore(unsigned int *p)
{
    if (!p || !g_mdkDiveBuf || IsBadWritePtr(p, 0x40)) return;

    g_mdkDiveDest = p[0x2c / 4];
    g_mdkDiveAdv  = p[0x28 / 4];
    p[0x2c / 4]   = (unsigned int)g_mdkDiveBuf;
    p[0x28 / 4]   = 0;                          /* rows are exactly 600 wide */
}

extern "C" void __attribute__((cdecl, used, noinline))
MdkDiveAfter(unsigned int *p)
{
    const unsigned short *src = g_mdkDiveBuf;
    unsigned short       *row = g_mdkBdRow, *dst, *base;
    int                   W, H, x, y, strideP, lastRow = -1;
    int                   dx0, dy0, dw, dh;
    unsigned int          sxStep, syStep, sy;

    if (!p || !g_mdkDiveBuf || IsBadWritePtr(p, 0x40)) return;

    /* Put the game's own values back before anything can go wrong below. */
    p[0x2c / 4] = g_mdkDiveDest;
    p[0x28 / 4] = g_mdkDiveAdv;

    if (!row || !g_mdkLfbPtr || !*g_mdkLfbPtr || !g_mdkStride) return;

    strideP = (int)(*g_mdkStride / 2);
    W = (int)g_targetW;
    H = (int)g_targetH;
    if (W <= 0 || H <= 0 || strideP <= 0) return;
    if (W > strideP) W = strideP;

    /* The true framebuffer origin, not the centred one the lock handed back --
       K5 has already offset that, and using it is what made K20's first build
       a diagonal. */
    base = (unsigned short *)*g_mdkLfbPtr;
    MdkFitRect(MDK_DIVE_W, MDK_DIVE_H, W, H, g_mdkBackdrop == 2,
               &dx0, &dy0, &dw, &dh, &sxStep, &syStep);

    g_mdkBoxDrew = 1;

    sy = 0;
    for (y = 0; y < H; y++) {
        int srcRow;

        dst = base + (unsigned int)y * (unsigned int)strideP;

        if (y < dy0 || y >= dy0 + dh) {         /* a bar */
            memset(dst, 0, (unsigned int)W * 2);
            continue;
        }

        srcRow = (int)(sy >> 16);
        sy += syStep;
        if (srcRow >= MDK_DIVE_H) srcRow = MDK_DIVE_H - 1;

        if (srcRow != lastRow) {
            const unsigned short *s = src + (unsigned int)srcRow * MDK_DIVE_W;
            unsigned int          sx = 0;

            for (x = 0; x < dx0; x++) row[x] = 0;
            for (; x < dx0 + dw; x++, sx += sxStep) {
                unsigned int col = sx >> 16;

                if (col >= MDK_DIVE_W) col = MDK_DIVE_W - 1;
                row[x] = s[col];
            }
            for (; x < W; x++) row[x] = 0;
            lastRow = srcRow;
        }

        memcpy(dst, row, (unsigned int)W * 2);
    }
}

static const unsigned char mdk_dive_sig[] =      /* 0x00412d21 */
    "\xe8\x7a\xc9\x05\x00\xe8";
#define MDK_DIVE_CALL_AT   0                     /* the call we replace */

static const unsigned char mdk_dive_stub[] = {
    0x60,                       // pushad
    0x50,                       // push  %eax   -> the parameter block
    0xe8, 0, 0, 0, 0,           // call  MdkDiveBefore        rel32 <- @3
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xe8, 0, 0, 0, 0,           // call  the real renderer    rel32 <- @12
    0x60,                       // pushad
    0x50,                       // push  %eax
    0xe8, 0, 0, 0, 0,           // call  MdkDiveAfter         rel32 <- @19
    0x83, 0xc4, 0x04,           // add   $0x4,%esp
    0x61,                       // popad
    0xc3                        // ret   (back to the game)
};
#define MDK_DIVE_B_AT    3
#define MDK_DIVE_R_AT   12
#define MDK_DIVE_A_AT   19

static void InstallMdkDive(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *target, patch[5];
    int            rel;

    if (!g_mdkBackdrop) return;                  /* shares K20's switch */

    at = FindUnique(code, codeSize, mdk_dive_sig, sizeof(mdk_dive_sig) - 1);
    if (!at) return;

    /* Read the renderer out of the call being displaced (GAME-PATCHING 5b). */
    target = (at + 5) + (int)ReadU32(at + 1);

    if (!g_mdkBdRow)
        g_mdkBdRow = (unsigned short *)VirtualAlloc(NULL, (g_targetW + 8) * 2,
                                                    MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkDiveBuf)
        g_mdkDiveBuf = (unsigned short *)VirtualAlloc(
                           NULL, MDK_DIVE_W * MDK_DIVE_H * 2,
                           MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkBdRow || !g_mdkDiveBuf) return;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_dive_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_dive_stub, sizeof(mdk_dive_stub));
    rel = (int)((unsigned char *)&MdkDiveBefore - (stub + MDK_DIVE_B_AT + 4));
    PutU32(stub + MDK_DIVE_B_AT, (unsigned int)rel);
    rel = (int)(target - (stub + MDK_DIVE_R_AT + 4));
    PutU32(stub + MDK_DIVE_R_AT, (unsigned int)rel);
    rel = (int)((unsigned char *)&MdkDiveAfter - (stub + MDK_DIVE_A_AT + 4));
    PutU32(stub + MDK_DIVE_A_AT, (unsigned int)rel);

    patch[0] = 0xe8;                             /* call rel32 -> stub */
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return;
    }
}

static void InstallMdkIntroBg(unsigned char *code, unsigned int codeSize)
{
    unsigned int i;

    if (!g_mdkBackdrop) return;                 /* shares K20's switch */

    /* K20 normally owns the scratch row, but it allocates only after its own
       signature matches -- so do not depend on that having happened. */
    if (!g_mdkBdRow)
        g_mdkBdRow = (unsigned short *)VirtualAlloc(NULL, (g_targetW + 8) * 2,
                                                    MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkBdRow) return;

    for (i = 0; i < MDK_IBG_SITES; i++) {
        const MdkIbgSite *s = &mdk_ibg[i];
        unsigned char    *at, *stub, patch[5];
        int               rel;

        at = FindUnique(code, codeSize, s->sig, 5);
        if (!at) continue;

        stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_ibg_stub),
                                             MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!stub) return;

        memcpy(stub, mdk_ibg_stub, sizeof(mdk_ibg_stub));
        stub[MDK_IBG_STRIDE_AT] = s->strideDisp;
        stub[MDK_IBG_BASE_AT]   = s->baseDisp;
        rel = (int)((unsigned char *)&MdkIntroBg - (stub + MDK_IBG_CALL_AT + 4));
        PutU32(stub + MDK_IBG_CALL_AT, (unsigned int)rel);
        rel = (int)((unsigned char *)s->rejoin - (stub + MDK_IBG_JMP_AT + 4));
        PutU32(stub + MDK_IBG_JMP_AT, (unsigned int)rel);

        patch[0] = 0xe9;                        /* jmp rel32 -> stub */
        PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
        if (!WriteCode(at, patch, 5)) {
            VirtualFree(stub, 0, MEM_RELEASE);
            continue;
        }
    }
}

static void InstallMdkBackdrop(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub;
    unsigned char  patch[5];
    int            rel;

    if (!g_mdkBackdrop) return;

    /* The 5 bytes at the site are `mov -0x18(%ebp),%edx ; sar %edx`, which
       occur FIVE times in the image -- 0x40dcf7, 0x419416, 0x41e68c, 0x46e7f8
       and 0x46e942.  The signature therefore starts at the `call` to the lock
       helper, which makes it unique.  GAME-PATCHING section 4: extend until it
       is unique and verify by count, never by eye. */
    at = FindUnique(code, codeSize, mdk_bd_sig, sizeof(mdk_bd_sig) - 1);
    if (!at) return;
    at += MDK_BD_SITE_AT;

    /* Fails closed: without the scratch row the body would have to resample
       into the framebuffer and read it back, which is the 1 fps this replaced. */
    g_mdkBdRow = (unsigned short *)VirtualAlloc(NULL, (g_targetW + 8) * 2,
                                                MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkBdRow) return;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_bd_stub), MEM_COMMIT,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_bd_stub, sizeof(mdk_bd_stub));
    rel = (int)((unsigned char *)&MdkBackdrop - (stub + MDK_BD_CALL_AT + 4));
    PutU32(stub + MDK_BD_CALL_AT, (unsigned int)rel);
    rel = (int)((unsigned char *)MDK_BD_REJOIN - (stub + MDK_BD_JMP_AT + 4));
    PutU32(stub + MDK_BD_JMP_AT, (unsigned int)rel);

    patch[0] = 0xe9;                            /* jmp rel32 -> stub */
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

//
// K36.  The OUTRO gameplay backdrop -- K20's third twin, scrolling on BOTH
// axes, and the last 600x360 island left on screen.
//
// 0x42fe00 is a self-contained scrolling background with exactly one caller
// (0x42ccd4).  It loads its image once, by name: 0x42afff passes the string
// "BG" at 0x48dd1c and stores the result in 0x4e5c4c, which nothing outside
// this routine ever reads.  Its two scroll values are computed at the head of
// the routine from the camera's own direction vectors (0x538d74/0x538d84 and
// 0x538d8c/0x538d9c), so the picture drifts with where you are looking:
//
//     0x492570   horizontal scroll, wrapped to 0..599
//     0x492574   vertical   scroll, wrapped to 0..359
//
// then it locks the view LFB and blits the 600x360 image 1:1 through the same
// shared 8bpp->16bpp row helper (0x46e884, palette 0x546848) K20 and K23 use.
// Four loops -- two for the vertical wrap, and two more per row at 0x430013
// when the horizontal scroll is non-zero -- but they all reduce to one thing:
//
//     shown[r][c] = src[(vscroll + r) mod 360][(hscroll + c) mod 600]
//
// which is K23's cancellation in both axes at once, so one body serves every
// path and there is no need to reproduce the four loops.
//
// MEASURED, not inferred.  The marked rectangle is 600x360 with its right edge
// at 1260 and its bottom at 580 -- an edge-energy scan puts both to the pixel
// -- and 660+600 = 1260, 220+360 = 580 are exactly K5's default 2D centring.
// So it is a full-window backdrop drawn 1:1 and then centred, which is the
// same thing K20 was written for.
//
// It shares K20's destination maths (MdkFitRect against `backdrop=`), and that
// is the part that matters: the OTHER backdrop in this scene goes through K20,
// and the two have to agree or there is a seam between them.  It shares the
// scratch row too, for the reason K20 learnt the hard way -- the framebuffer is
// only ever written, never read back.
//
// Entered after the lock and rejoined at the unlock (section 5c).  The five
// bytes replaced are `mov -0x24(%ebp),%edx ; sar %edx`, which only recompute
// the stride we read ourselves.
//
#define MDK_STAR_SRC     0x004e5c4cu   /* the "BG" image, 600x360 8bpp */
#define MDK_STAR_HSCROLL 0x00492570u   /* 0..599 */
#define MDK_STAR_VSCROLL 0x00492574u   /* 0..359 */

extern "C" void __attribute__((cdecl, used, noinline))
MdkStarfield(unsigned int *f)
{
    const unsigned char  *src;
    const unsigned short *pal = (const unsigned short *)MDK_BD_PAL;
    unsigned short       *base, *dst, *row = g_mdkBdRow;
    unsigned int          ebp;
    int  strideB, strideP, W, H, x, y, hs, vs, lastRow = -1;
    int  dx0, dy0, dw, dh;
    unsigned int sxStep, syStep, sy;

    if (!f || !row) return;

    ebp = f[2];                                  /* the routine's own frame */
    if (!ebp || IsBadReadPtr((const void *)(ebp - 0x30), 0x10)) return;

    src = *(const unsigned char *const *)MDK_STAR_SRC;
    if (!src || IsBadReadPtr(src, MDK_BD_W * MDK_BD_H)) return;

    base    = *(unsigned short **)(ebp - 0x30);
    strideB = *(const int *)(ebp - 0x24);        /* still BYTES at the hook */
    if (!base || strideB <= 0) return;

    /* K20's lesson, and it cost a build there: the base the lock hands back is
       ALREADY offset by K5's centring, so a full-screen image drawn from it
       runs off the end of the row and comes out as a diagonal.  A backdrop
       wants the true framebuffer origin. */
    if (g_mdkLfbPtr && *g_mdkLfbPtr)
        base = (unsigned short *)*g_mdkLfbPtr;

    strideP = strideB / 2;
    W = (int)g_targetW;
    H = (int)g_targetH;
    if (W <= 0 || H <= 0 || strideP <= 0) return;
    if (W > strideP) W = strideP;                /* the 2048/4096 row limit */

    hs = *(const int *)MDK_STAR_HSCROLL;
    vs = *(const int *)MDK_STAR_VSCROLL;
    if (hs < 0 || hs >= MDK_BD_W) hs = 0;        /* the game wraps them itself,
                                                    but a hook runs in more
                                                    states than the code it
                                                    watches */
    if (vs < 0 || vs >= MDK_BD_H) vs = 0;

    MdkFitRect(MDK_BD_W, MDK_BD_H, W, H, g_mdkBackdrop == 2,
               &dx0, &dy0, &dw, &dh, &sxStep, &syStep);

    g_mdkBoxDrew = 1;

    sy = 0;
    for (y = 0; y < H; y++) {
        int srcRow;

        dst = base + (unsigned int)y * (unsigned int)strideP;

        if (y < dy0 || y >= dy0 + dh) {          /* outside the picture: bar */
            memset(dst, 0, (unsigned int)W * 2);
            continue;
        }

        srcRow = (int)(sy >> 16);
        sy += syStep;
        if (srcRow >= MDK_BD_H) srcRow = MDK_BD_H - 1;
        srcRow += vs;                            /* one subtract, not a modulo:
                                                    both terms are below 360 */
        if (srcRow >= MDK_BD_H) srcRow -= MDK_BD_H;

        /* The scroll is constant across a frame, so a row's content depends on
           srcRow alone -- and the 2.22x vertical magnification means most
           output rows cost only the copy out. */
        if (srcRow != lastRow) {
            const unsigned char *s = src + (unsigned int)srcRow * MDK_BD_W;
            unsigned int         sx = 0;

            for (x = 0; x < dx0; x++) row[x] = 0;
            for (; x < dx0 + dw; x++, sx += sxStep) {
                int col = (int)(sx >> 16);

                if (col >= MDK_BD_W) col = MDK_BD_W - 1;
                col += hs;
                if (col >= MDK_BD_W) col -= MDK_BD_W;
                row[x] = pal[s[col]];
            }
            for (; x < W; x++) row[x] = 0;
            lastRow = srcRow;
        }

        memcpy(dst, row, (unsigned int)W * 2);
    }
}

/* The five bytes at the site happen to be unique here, unlike K20's, but the
   signature still starts at the two `lea`s and the `call` to the lock helper:
   that is what K20's five-way collision taught, and it is also what lets the
   two stack slots the body reads be part of the match rather than an
   assumption. */
static const unsigned char mdk_star_sig[] =      /* 0x0042ff09 */
    { 0x8d, 0x55, 0xdc,                          /* lea -0x24(%ebp),%edx  stride */
      0x8d, 0x45, 0xd0,                          /* lea -0x30(%ebp),%eax  base   */
      0xe8 };                                    /* call the lock helper         */
#define MDK_STAR_SITE_AT   (sizeof(mdk_star_sig) + 4)      /* past the call */
#define MDK_STAR_REJOIN_AT (MDK_STAR_SITE_AT + 0xb4)       /* its unlock call */

static void InstallMdkStarfield(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *rejoin;
    unsigned char  patch[5];
    int            rel;

    if (!g_mdkBackdrop) return;

    at = FindUnique(code, codeSize, mdk_star_sig, sizeof(mdk_star_sig));
    if (!at) return;

    rejoin = at + MDK_STAR_REJOIN_AT;

    /* The rejoin is an offset into a routine the signature has pinned, so it
       is checked rather than trusted: it has to be the unlock CALL, and it has
       to name the same helper the lock's own module unlocks through. */
    if (*rejoin != 0xe8) {
        return;
    }

    at += MDK_STAR_SITE_AT;

    if (!g_mdkBdRow)
        g_mdkBdRow = (unsigned short *)VirtualAlloc(NULL, (g_targetW + 8) * 2,
                                                    MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkBdRow) return;                     /* fails closed, like K20 */

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_bd_stub), MEM_COMMIT,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_bd_stub, sizeof(mdk_bd_stub));
    rel = (int)((unsigned char *)&MdkStarfield - (stub + MDK_BD_CALL_AT + 4));
    PutU32(stub + MDK_BD_CALL_AT, (unsigned int)rel);
    rel = (int)(rejoin - (stub + MDK_BD_JMP_AT + 4));
    PutU32(stub + MDK_BD_JMP_AT, (unsigned int)rel);


    patch[0] = 0xe9;                             /* jmp rel32 -> stub */
    PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
    if (!WriteCode(at, patch, 5))
        VirtualFree(stub, 0, MEM_RELEASE);
}

static void InstallMdkSky(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *at2, *end, *stub;
    unsigned char  patch[5];
    int            rel;

    if (g_targetH <= (unsigned int)MDK_VIEW_H) return;

    /* The routine has TWO row-copy loops and the test at 0x472071 picks
       between them: one `rep movsd` per row while the scroll window fits
       inside the panorama, two per row once it crosses the end of it.  Both
       have to be caught -- hooking only the first left the wrapped case doing
       its original 1:1 blit into the corner, and drawing no quads at all for
       that frame because nothing was captured.

       At 0x472094 the wrapped loop's registers and frame slots are identical
       to the other's at 0x47220B -- edi = 2*scroll, esi = 2*destX, ecx = the
       source row -- so one capture body serves both. */
    at  = FindUnique(code, codeSize, mdk_sky_sig,    sizeof(mdk_sky_sig)    - 1);
    at2 = FindUnique(code, codeSize, mdk_sky2_sig,   sizeof(mdk_sky2_sig)   - 1);
    end = FindUnique(code, codeSize, mdk_skyend_sig, sizeof(mdk_skyend_sig) - 1);
    if (!at || !at2 || !end) return;

    /* One scaled row, reused every frame.  Never freed -- it lives as long as
       the hook that uses it. */
    g_mdkSkyRow = (unsigned short *)VirtualAlloc(NULL, (g_targetW + 8) * 2,
                                                 MEM_COMMIT, PAGE_READWRITE);
    if (!g_mdkSkyRow) return;

    /* Both loops go through the same helper.  This site used to be a copy of
       it, which is how it would have been left behind by the fall-through
       change below. */
    if (!MdkHookSkyLoop(at2, end)) return;
    if (!MdkHookSkyLoop(at,  end)) return;
}

//
// K11: give the entity drawer the real screen as its destination surface.
//
static void InstallMdkEntity(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at[MDK_ENTITY_SPANS];
    unsigned char  repl[MDK_SPAN_MAX];
    unsigned int   i, j, found = 0;

    for (i = 0; i < MDK_ENTITY_SPANS; i++) {
        at[i] = FindUnique(code, codeSize, mdk_entity[i].sig, mdk_entity[i].len);
        if (at[i]) found++;
    }

    if (found == 0) return;                     // already applied
    if (found != MDK_ENTITY_SPANS) return;      // partial: refuse

    for (i = 0; i < MDK_ENTITY_SPANS; i++) {
        const MdkSpan *s = &mdk_entity[i];

        memcpy(repl, s->sig, s->len);
        for (j = 0; j < s->fieldCount; j++)
            PutU32(repl + s->fields[j].at,
                   s->fields[j].kind == MDK_W ? g_targetW : g_targetH);
        WriteCode(at[i], repl, s->len);
    }
}

//
// K10: put the menu/intro sprite where the world is.
//
static void InstallMdkPlayer(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *blitter;
    unsigned char  call[5];
    int            rel;

    at = FindUnique(code, codeSize, mdk_player_sig, sizeof(mdk_player_sig) - 1);
    if (!at) return;

    // Resolve the blitter from the call we are displacing, rather than from a
    // second signature: it is the same instruction either way, and this cannot
    // disagree with itself.
    at     += MDK_PLAYER_CALL_AT;
    rel     = (int)ReadU32(at + 1);
    blitter = at + 5 + rel;

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_player_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_player_stub, sizeof(mdk_player_stub));
    rel = (int)((unsigned char *)&MdkFixPlayer
                - (stub + MDK_PLAYER_STUB_CALL_AT + 4));
    PutU32(stub + MDK_PLAYER_STUB_CALL_AT, (unsigned int)rel);
    rel = (int)(blitter - (stub + MDK_PLAYER_STUB_JMP_AT + 4));
    PutU32(stub + MDK_PLAYER_STUB_JMP_AT, (unsigned int)rel);

    call[0] = 0xe8;
    rel = (int)(stub - (at + 5));
    PutU32(call + 1, (unsigned int)rel);

    if (!WriteCode(at, call, 5)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return;
    }
    g_mdkBlitter = blitter;
}

//
// K5/K8: centre the 2D island, except for the sprite blitter.
//
static void InstallMdkHud(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at, *stub, *spriteLock;
    unsigned char  repl[sizeof(mdk_lfb_view_sig)];
    unsigned int   i;
    int            rel;

    at = FindUnique(code, codeSize, mdk_lfb_view_sig, sizeof(mdk_lfb_view_sig));
    if (!at) return;

    // Fail closed.  Centring every caller -- including the one whose
    // coordinates are already real screen pixels -- is not a partial fix, it is
    // the bug this replaces.
    spriteLock = FindUnique(code, codeSize, mdk_spritelock_sig,
                            sizeof(mdk_spritelock_sig));
    if (!spriteLock) return;
    g_mdkSpriteRet = (unsigned int)(spriteLock + sizeof(mdk_spritelock_sig));

    // The entity drawer's lock, for the same reason.  Located here rather than
    // in K11 because this is where the exclusion table lives.
    spriteLock = FindUnique(code, codeSize, mdk_entlock_sig,
                            sizeof(mdk_entlock_sig) - 1);
    if (!spriteLock) return;
    g_mdkEntityRet = (unsigned int)(spriteLock + sizeof(mdk_entlock_sig) - 1);

    /* K15.  Not fail-closed: without it the HUD merely stays centred, which is
       where it has been all along rather than a broken state. */
    spriteLock = FindUnique(code, codeSize, mdk_hudlock_sig,
                            sizeof(mdk_hudlock_sig) - 1);
    g_mdkHudRet = spriteLock ? (unsigned int)(spriteLock + MDK_HUDLOCK_RET) : 0;

    {
        unsigned char *r = FindUnique(code, codeSize, mdk_rectlock_sig,
                                      sizeof(mdk_rectlock_sig));
        unsigned char *i = FindUnique(code, codeSize, mdk_itemrect_sig,
                                      sizeof(mdk_itemrect_sig));

        g_mdkRectRet  = r ? (unsigned int)(r + MDK_RECT_RET) : 0;
        g_mdkItemFrom = i ? (unsigned int)(i + MDK_ITEM_RET) : 0;
    }

    spriteLock = FindUnique(code, codeSize, mdk_numdraw_sig,
                            sizeof(mdk_numdraw_sig));
    g_mdkNumRet = spriteLock
                ? (unsigned int)(spriteLock + MDK_NUMDRAW_RET) : 0;

    /* Record which call site is drawing, from the drawer's own entry. */
    {
        unsigned char  t[sizeof(mdk_huddraw_stub)];
        unsigned char  patch[5];
        unsigned char *at, *stub;
        int            rel;

        at = FindUnique(code, codeSize, mdk_huddraw_sig,
                        sizeof(mdk_huddraw_sig));
        stub = at ? (unsigned char *)VirtualAlloc(NULL, sizeof(t), MEM_COMMIT,
                                                  PAGE_EXECUTE_READWRITE)
                  : NULL;
        if (stub) {
            memcpy(t, mdk_huddraw_stub, sizeof(t));
            memcpy(t + MDK_HUDDRAW_DISP_AT, mdk_huddraw_sig, 5);
            rel = (int)((unsigned char *)&MdkHudCaller
                        - (stub + MDK_HUDDRAW_CALL_AT + 4));
            PutU32(t + MDK_HUDDRAW_CALL_AT, (unsigned int)rel);
            rel = (int)((at + 5) - (stub + MDK_HUDDRAW_JMP_AT + 4));
            PutU32(t + MDK_HUDDRAW_JMP_AT, (unsigned int)rel);
            memcpy(stub, t, sizeof(t));

            patch[0] = 0xe9;
            PutU32(patch + 1, (unsigned int)(int)(stub - (at + 5)));
            if (!WriteCode(at, patch, 5)) VirtualFree(stub, 0, MEM_RELEASE);
        }
    }

    g_mdkHudCx = ((int)g_targetW - MDK_VIEW_W) / 2;
    g_mdkHudCy = ((int)g_targetH - MDK_VIEW_H) / 2;
    if (g_mdkHudCx < 0) g_mdkHudCx = 0;
    if (g_mdkHudCy < 0) g_mdkHudCy = 0;
    if (!g_mdkHudCx && !g_mdkHudCy) return;     // nothing to move

    g_mdkStride = (const unsigned int *)ReadU32(at + MDK_LFB_STRIDE_AT);
    g_mdkOrgY   = (const int          *)ReadU32(at + MDK_LFB_ORGY_AT);
    g_mdkLfbPtr = (const unsigned int *)ReadU32(at + MDK_LFB_PTR_AT);
    g_mdkOrgX   = (const int          *)ReadU32(at + MDK_LFB_ORGX_AT);

    stub = (unsigned char *)VirtualAlloc(NULL, sizeof(mdk_lfb_stub),
                                         MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!stub) return;

    memcpy(stub, mdk_lfb_stub, sizeof(mdk_lfb_stub));
    rel = (int)((unsigned char *)&MdkLfbBaseView
                - (stub + MDK_LFB_STUB_CALL_AT + 4));
    PutU32(stub + MDK_LFB_STUB_CALL_AT, (unsigned int)rel);

    repl[0] = 0xe8;                             // call rel32 -> stub
    rel = (int)(stub - (at + 5));
    PutU32(repl + 1, (unsigned int)rel);
    for (i = 5; i < sizeof(repl); i++)
        repl[i] = 0x90;

    if (!WriteCode(at, repl, sizeof(repl)))
        VirtualFree(stub, 0, MEM_RELEASE);
}

extern "C" void __attribute__((cdecl, used, noinline))
MdkLfbBaseView(unsigned int *out, unsigned int caller,
               unsigned int callerEbx, unsigned int callerEbp)
{
    int cx, cy;

    if (!out || !g_mdkLfbPtr) return;

    cx = g_mdkHudCx;
    cy = g_mdkHudCy;

    /* The sprite blitter and the entity drawer are EXCLUDED, not centred:
       their coordinates are already real screen pixels.  Centring them was
       the bug this replaces, so the installer fails closed if either lock
       cannot be found. */
    if (caller == g_mdkSpriteRet) {
        cx = 0;
        cy = 0;
    }
    else if (g_mdkEntityRet && caller == g_mdkEntityRet) {
        cx = 0;
        cy = 0;
    }
    /* K26.  The item frame: anchored with the ammo icon it belongs to, using
       K15's left-band numbers so the three stay together.  Keyed on the
       drawer's own caller, because its other 21 callers must stay put. */
    else if (g_mdkRectRet && caller == g_mdkRectRet && callerEbp
             && g_mdkItemFrom
             && !IsBadReadPtr((const void *)(callerEbp + 4), 4)
             && *(const unsigned int *)(callerEbp + 4) == g_mdkItemFrom) {
        cx = g_mdkBoxX0;
        cy = g_mdkHudCy * 2;
    }
    /* The enemy health bar's frame: the picture's top-left, matching its
       fill.  Keyed on the caller, like the item frame above, because this
       drawer has 22 of them. */
    else if (g_mdkRectRet && caller == g_mdkRectRet && callerEbp
             && g_mdkHpFrom
             && !IsBadReadPtr((const void *)(callerEbp + 4), 4)
             && *(const unsigned int *)(callerEbp + 4) == g_mdkHpFrom) {
        cx = g_mdkBoxX0;
        cy = 0;
    }
    /* K35.  The bombing run's bomb counter.  The font renderer has seven
       callers and draws every string in the game, so this is keyed on the one
       that is the bomb sight's -- 0x46af17, the return address 0x414fa8 was
       entered with, which is live at [ebp+4] exactly as the two rect drawers
       above use it.

       Its own arguments are still in its frame at the lock: x at [ebp-0x28]
       and y at [ebp-0x18], stored by the first three instructions of the
       function.  Reading them is what lets one offset express a per-element
       map, which is all this hook can do. */
    else if (g_mdkFontRet && caller == g_mdkFontRet && callerEbp
             && g_mdkBombTextFrom && g_mdkLayoutW > 0
             && !IsBadReadPtr((const void *)(callerEbp - 0x28), 4)
             && !IsBadReadPtr((const void *)(callerEbp + 4), 4)
             && *(const unsigned int *)(callerEbp + 4) == g_mdkBombTextFrom) {
        int lx = *(const int *)(callerEbp - 0x28);
        int ly = *(const int *)(callerEbp - 0x18);

        cx = g_mdkLayoutX0 + (lx * g_mdkLayoutW) / MDK_SCOPE_W - lx;
        cy = g_mdkLayoutY0 + (ly * g_mdkLayoutH) / MDK_SCOPE_H - ly;

    }
    else if (g_mdkHudRet && callerEbx && caller == g_mdkHudRet
             && !IsBadReadPtr((const void *)callerEbx, 8)) {
        const int *p = (const int *)callerEbx;

        /* K29.  In snipe mode there are no HUD corners to anchor to: every
           element on that screen is laid out in the SCOPE's own 600x360
           space, the same space as the art and the four viewports, so they
           take the same box transform.  Expressed as an offset because that
           is all this hook can do, but it is per element, so it is the box
           map exactly. */
        if (g_mdkInScope && g_mdkSnipeW > 0) {
            cx = g_mdkSnipeX0 + (p[0] * g_mdkSnipeW) / MDK_SCOPE_W - p[0];
            cy = g_mdkSnipeY0 + (p[1] * g_mdkSnipeH) / MDK_SCOPE_H - p[1];
        }
        /* Anchored to the edges of the PICTURE, not of the screen: with a
           letterboxed backdrop those differ, and the bars are painted last,
           so a HUD anchored to the screen corners was drawn correctly and
           then blacked out. */
        else if (p[0] < MDK_HUD_LEFT_X) {
            cx = g_mdkBoxX0;                    /* ammo counter, far left */
            cy = cy * 2;
        } else {
            cx = g_mdkBoxX0 + g_mdkBoxW - MDK_VIEW_W;   /* right cluster  */
            cy = cy * 2;
        }

    }

    *out = *g_mdkLfbPtr
         + (unsigned int)((*g_mdkOrgY + cy) * (int)*g_mdkStride)
         + (unsigned int)(2 * (*g_mdkOrgX + cx));
}

/* Called from the wrapper for every grBufferClear, from anywhere.  MDK clears
 * from 30 different sites and the sky has to be re-laid after the frame one;
 * catching them all here is what removes the need to know which is which.
 *
 * ONCE PER FRAME, not once per clear.
 *
 * grBufferClear is not only the frame clear.  MDK also paints small filled
 * rectangles with it -- 0x471d28 is grClipWindow followed by grBufferClear,
 * and that is how the enemy health bar's fill is drawn.  Firing this hook on
 * every clear meant we drew the whole sky again inside whatever clip window
 * the game had just set, so the fill came out full of SKYBOX.  That is
 * exactly what it looked like, and why it differed between targets: different
 * clip rectangles, different piece of sky.
 *
 * The frame clear is the first of the frame, so drawing on the first one and
 * ignoring the rest is both the fix and the original intent.  Cleared at the
 * swap, so it is one flag with one setter and one clearer. */
static int g_mdkSkyDrawn = 0;

void GameFix_AfterClear(void)
{
    if (!g_mdkClear || g_mdkSkyBusy) return;    /* MDK only, and no re-entry */
    if (g_mdkSkyDrawn) return;                  /* not a frame clear         */
    g_mdkSkyDrawn = 1;
    g_mdkSkyBusy = 1;
    MdkSkyPass(1);          /* K41b -- and the ovals blanked with it */
    g_mdkSkyBusy = 0;
}

/* Defined in the Driver block below; declared here only because GameFix_Tick
   happens to sit above it. */
static void DrvTick(void);

/* Called from the wrapper's grBufferSwap BEFORE it forwards, so the letterbox
   bars are the last thing written to the frame and everything drawn outside
   the picture is cut off -- with no per-element rule. */
/* K31.  The bullets in the barrel -- GLIDE GEOMETRY, which is why nine
 * captures of the 2D blitters came back clean: they never touch one.
 *
 * The probe that found them logged 137 triangles in the barrel area, every
 * one from the same emitter, in two clusters that match the two missiles I
 * measured off the screenshot to within a few pixels.
 *
 * Their screen x is layout * W/600 and their y is layout * H/360 -- the main
 * projection's extents after the world was widened.  Everything else inside
 * the scope is on a UNIFORM k with the letterbox offset, so the two disagree.
 * In the original they agreed, because the projection and the scope art were
 * both 600x360.
 *
 * The projection cannot be corrected from its constants: it has a horizontal
 * scale but no horizontal offset -- its one offset is shared between both
 * axes, so it cannot say "shift x, leave y alone".  But the vertices reach us
 * already in screen space, so the correction is exact and one line:
 *
 *     undo x * W/600, apply dw/600, add dx0   ==   dx0 + x * dw/W
 *
 * y needs nothing: layout * H/360 is already the scope's own vertical scale,
 * and the letterbox has no vertical bar at any widescreen override.
 *
 * SCOPE OF THE TEST, and it is why this is safe: only while the scope is up,
 * and only triangles wholly left of the sniper view (x=533) and below the cam
 * ovals.  The scope art is 2D, the view is to the right, the cams are above --
 * nothing else draws in that box, and the 137 logged triangles are the
 * evidence rather than an assumption.  The test uses the ORIGINAL coordinates,
 * so a triangle is moved whole or not at all; it cannot tear.
 */
extern "C" void GameFix_Tri(const void *a, const void *b, const void *c,
                            unsigned int caller)
{
    float *v[3];
    float  minx, maxx, miny, maxy;
    int    i;

    if (!g_targetW || !a || !b || !c) return;

    v[0] = (float *)a;  v[1] = (float *)b;  v[2] = (float *)c;
    minx = maxx = v[0][0];
    miny = maxy = v[0][1];
    for (i = 1; i < 3; i++) {
        if (v[i][0] < minx) minx = v[i][0];
        if (v[i][0] > maxx) maxx = v[i][0];
        if (v[i][1] < miny) miny = v[i][1];
        if (v[i][1] > maxy) maxy = v[i][1];
    }

    /* K33.  The damage overlay.  Its producer is 0x472306/0x472333 -- two
       triangles making one quad, inside the same routine K14 hooks for the
       sky but on a geometry path rather than a blit loop.  It is laid out in
       the game's 600x360 space, so with the viewport origin forced to (0,0)
       it covers 600 of 1920 and sits in the corner.

       Scaled the way the game would if it knew the screen size.  Keyed on the
       PRODUCER, which is why the emitter probe had to exist: the shared
       emitter draws world geometry in the same region and must not move. */
    if (g_mdkEmitFrom == 0x00472306u || g_mdkEmitFrom == 0x00472333u) {
        float kx = (float)(int)g_targetW / (float)MDK_VIEW_W;
        float ky = (float)(int)g_targetH / (float)MDK_VIEW_H;

        for (i = 0; i < 3; i++) {
            v[i][0] *= kx;
            v[i][1] *= ky;
        }
        return;
    }

    if (!g_mdkScopeHold || g_mdkSnipeW <= 0) return;

    if (minx < 0.0f  || maxx > 520.0f) return;   /* left of the view only */
    if (miny < 380.0f || maxy > (float)(int)g_targetH) return;  /* below the ovals */

    /* K38: the window no longer starts at y=0 nor fills the height, so the
       vertical needs the same undo-and-remap the horizontal always did.
       Under K29's box this was the identity, which is why it was absent. */
    for (i = 0; i < 3; i++) {
        v[i][0] = (float)g_mdkSnipeX0
                + v[i][0] * (float)g_mdkSnipeW / (float)(int)g_targetW;
        v[i][1] = (float)g_mdkSnipeY0
                + v[i][1] * (float)g_mdkSnipeH / (float)(int)g_targetH;
    }

}

void GameFix_PreSwap(void)
{
    MdkPaintBars();
    g_mdkInScope = 0;       /* K29: true only within the frame the scope drew */
    if (g_mdkScopeHold > 0) g_mdkScopeHold--;
    g_mdkSkyDrawn = 0;      /* the next frame's first clear draws the sky */

    {   /* K41: the one place the cams' grace is aged. */
        int i;

        for (i = 0; i < 3; i++)
            if (g_mdkCamSky[i]) g_mdkCamSky[i]--;
    }
}

void GameFix_Tick(void)
{
    /* V6's viewing-distance ceiling.  Inert for every other game, and inert
       for Driver until DriverApply has run. */
    DrvTick();

    // K9.  Called from the wrapper's grBufferSwap AFTER the swap, so this
    // clears the buffer the game is about to draw into, not the one being
    // shown.  Inert unless MDK's window patch matched, and therefore inert
    // whenever the resolution override is disabled.
    if (g_mdkClear) g_mdkClear();

    /* K14.  The backdrop, drawn right after the clear and before MDK starts
       the frame -- which is what a backdrop wants, and the one point in the
       frame where the framebuffer is certain not to be LFB-locked. */
    MdkSkyPass(0);
}


//
// Everything MDK needs, in one place.
//
// K1 fails closed, and takes the rest with it: a projection scaled to the full
// screen while the clip window is still 600x360 would draw a correctly framed
// picture and then throw three quarters of it away, which reads as a projection
// bug rather than as the missing patch it would be.
//
// No ini keys.  A game rendering into a corner of the screen, rendering it
// stretched, or leaving its HUD in a corner is not a preference; and the one
// thing here that IS a judgement call -- deleting the window shake -- has no
// second behaviour anyone could choose between, because the alternative hands
// Glide a clip rectangle off the end of the framebuffer.
//
//
// `patches` is one bit per installer.  It was scaffolding -- built so the
// hardware could bisect which patch hid the player, in one session instead of
// one build per candidate -- and everything it could bisect is now confirmed,
// so the ini key is gone and `mask` below is a constant.
//
// The BITS are kept, for the same reason Driver's are: the compiler folds
// every test away, so they cost nothing, and a future bisect is then a
// one-line edit rather than a redesign.  Do not renumber them -- the values
// are quoted in CLAUDE.md and in shipped READMEs.
//
#define MDK_P_WINDOW  0x01u
#define MDK_P_PROJ    0x02u
#define MDK_P_ASPECT  0x04u
#define MDK_P_SHAKE   0x08u
#define MDK_P_CAMERA  0x10u
#define MDK_P_SPRCLIP 0x20u
#define MDK_P_HUD     0x40u
#define MDK_P_CLEAR   0x80u
#define MDK_P_PLAYER  0x100u
#define MDK_P_ENTITY  0x200u
#define MDK_P_SPRSCALE 0x400u
#define MDK_P_SKY     0x800u
#define MDK_P_SPRPOS  0x1000u
#define MDK_P_SPRPIN  0x2000u
#define MDK_P_ART     0x4000u
#define MDK_P_BACKDR  0x8000u
#define MDK_P_LINEAR  0x10000u
#define MDK_P_ALL     0x1dfffu

static void MdkApply(const char *exePath, const char *ini, BOOL haveIni)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0;
    /* One bit per installer.  It was an ini key for bisecting on hardware and
       everything it could bisect is now confirmed, so it is a constant -- the
       compiler folds every test below away.  The bits are KEPT rather than
       deleted because they cost nothing and make a future bisect a one-line
       edit instead of a redesign.  Same reasoning as Driver's DRV_P_*. */
    const unsigned int mask = MDK_P_ALL;

    if (!PathEndsWith(exePath, MDK_EXE)) return;

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if ((unsigned int)mod != MDK_IMAGE_BASE) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    /* The last key.  K20/K24/K36: 0 off, 1 stretch to fill, 2 the original
       600x360 shape centred with bars -- the default, and the only one this
       install has been tested on.  Every other MDK key became unconditional
       once it was confirmed, so with no ini at all the game gets exactly the
       tested configuration and no file needs to be shipped. */
    if (haveIni) {
        g_mdkBackdrop = GetPrivateProfileIntA("MDK", "backdrop",
                                              g_mdkBackdrop, ini);
        if (g_mdkBackdrop < 0 || g_mdkBackdrop > 2) g_mdkBackdrop = 2;
    }

    /* The picture's horizontal extent, for the HUD to anchor against.
       Computed with the same helper the backdrops use, so the HUD and the
       bars cannot disagree about where the picture ends. */
    {
        int dy0, dh;
        unsigned int sx, sy;

        MdkFitRect(MDK_BD_W, MDK_BD_H, (int)g_targetW, (int)g_targetH,
                   g_mdkBackdrop == 2,
                   &g_mdkBoxX0, &dy0, &g_mdkBoxW, &dh, &sx, &sy);
    }

    /* K21 first: it changes the geometry every other 2D patch works against,
       and it is a plain immediate rewrite that depends on nothing. */
    if (mask & MDK_P_LINEAR) InstallMdkLfbLinear(code, codeSize);

    if (mask & MDK_P_WINDOW) {
        if (!InstallMdkWindow(code, codeSize)) return;
    }
    if (!(mask & MDK_P_CLEAR)) g_mdkClear = NULL;

    if (mask & MDK_P_PROJ)    InstallMdkProjection(code, codeSize);
    if (mask & MDK_P_ASPECT)  InstallMdkAspect(code, codeSize);
    if (mask & MDK_P_SHAKE)   InstallMdkShake(code, codeSize);
    if (mask & MDK_P_CAMERA)  InstallMdkCamera(code, codeSize);
    if (mask & MDK_P_SPRCLIP) InstallMdkSpriteClip(code, codeSize);
    if (mask & MDK_P_HUD)     InstallMdkHud(code, codeSize);
    if (mask & MDK_P_PLAYER)  InstallMdkPlayer(code, codeSize);
    if (mask & MDK_P_ENTITY)  InstallMdkEntity(code, codeSize);
    if (mask & MDK_P_SPRSCALE) InstallMdkSpriteScale(code, codeSize);
    if (mask & MDK_P_ART)      InstallMdkArtCentre(code, codeSize);
    if (mask & MDK_P_SPRPOS)   InstallMdkSpritePos(code, codeSize);
    if (mask & MDK_P_SPRPOS)   InstallMdkEarthCover(code, codeSize);
    if (mask & MDK_P_SPRPIN) {
        InstallMdkSprPin(code, codeSize, mdk_sprA_sig,
                         sizeof(mdk_sprA_sig) - 1, 0);
        InstallMdkSprPin(code, codeSize, mdk_sprB_sig,
                         sizeof(mdk_sprB_sig) - 1, 1);
    }
    if (mask & MDK_P_SKY)      InstallMdkTexReserve(code, codeSize);
    if (mask & MDK_P_SKY)      InstallMdkSky(code, codeSize);
    if (mask & MDK_P_BACKDR)   InstallMdkBackdrop(code, codeSize);
    if (mask & MDK_P_BACKDR)   InstallMdkIntroBg(code, codeSize);
    if (mask & MDK_P_BACKDR)   InstallMdkStarfield(code, codeSize);
    if (mask & MDK_P_BACKDR)   InstallMdkDive(code, codeSize);



    /* K28 runs HERE, not with the other item work, and the ordering is the
       whole reason: it bakes the offsets into its stub, and g_mdkBoxX0 is not
       computed until MdkApply runs while g_mdkHudCy is set inside the 2D
       centring installer.  Placed with K26 it read two zeroes and declined in
       silence.  K26 itself is immune because it reads them per draw. */
    if (mask & MDK_P_HUD)    InstallMdkItemFill(code, codeSize);
    if (mask & MDK_P_HUD)    InstallMdkHealthFill(code, codeSize);
    if (mask & MDK_P_HUD)    InstallMdkLoadBars(code, codeSize);
    if (mask & MDK_P_HUD)    InstallMdkStatBars(code, codeSize);

    /* Armed for the whole of MDK, not just snipe mode: K33's damage overlay is
       geometry, so GameFix_Tri is the only thing that can see it, and K31's
       barrel props need the emitter's caller too.  It costs MDK one call per
       triangle.  Other games never set this, so they still pay only a load
       and a not-taken branch. */
    InstallMdkEmitProbe(code, codeSize);
    GameFix_TriHot = 1;
    /* K38 first: InstallMdkSnipe picks the composition box only if the body
       hook installed, and falls back to K29's otherwise. */
    if (mask & MDK_P_PROJ)   InstallMdkBody(code, codeSize);
    if (mask & MDK_P_PROJ)   InstallMdkSnipe(code, codeSize);
    if (mask & MDK_P_PROJ)   InstallMdkHudScale(code, codeSize);

    /* K35.  The bombing run: three elements in the same layout space the
       scope uses, so it goes with the projection bit and after the box
       has been computed. */
    if (mask & MDK_P_PROJ)   InstallMdkBomb(code, codeSize);

    /* K34.  The sky's own leftover fill -- part of the backdrop, so it goes
       with the sky bit.  Reads its offsets per draw, so ordering is free. */
    if (mask & MDK_P_SKY)    InstallMdkSkyFill(code, codeSize);

}


// ==========================================================================
// Driver (Reflections, 1999) -- Game.exe
// ==========================================================================
//
// The smallest job in this file so far: four patches, all in memory, nothing
// on disk modified.  Three unrelated things make it small.
//
//  - The Game.exe in this install is ALREADY DECRYPTED.  Driver retail is
//    SafeDisc (GAME.ICD, clcd32.dll, secdrv.sys, and the 249 KB loader stub
//    left behind as "Copy of Game.exe"), but the Game.exe actually in place
//    has .text entropy 5.89 against GAME.ICD's 7.38.  So everything below was
//    read off the file and no runtime dump was needed.  GAME.ICD is matched
//    too, in case the protected image is ever the one that runs -- by
//    grGlideInit it is decrypted, and every pattern here is verified unique
//    before it is written, so a build that does not match is left alone.
//
//  - The engine is resolution-parametric.  The render size lives in two
//    globals (0x12eff08 W, 0x12de6d0 H) read ~100 and ~88 times; grClipWindow
//    is built as (0,0,W,H); the projection centre is W/2, H/2; and the only
//    two W*H products in the whole image are in the TGA screen grab, which
//    mallocs.  There is no fixed-size static framebuffer to relocate (part
//    one, section 4), which is the check that has to come first.
//
//  - Requirement 1 is a real mode list, but a tidy one.  0x54d790 holds seven
//    28-byte records -- { u32 w; u32 h; char name[12]; u32 flags; u32 enum } --
//    320x240, 512x384, 640x480, 800x600, 1024x768, 1280x1024, 1600x1200, and a
//    -1 terminator.  CONFIG.DAT's dword at +0x0c is the index into it.
//
// Requirement 2 is free in the same sense: the game already asks for whatever
// that row says, so rewriting a row is the whole of "the game must ask".
//
// WHICH row, and why two of them.  GlideSetup (0x4264eb) does not read the
// enum out of the table -- it re-derives it from the live width through a
// binary-search if-else chain:
//
//      cmp W,800 ; jg upper                     upper: cmp W,1024 -> 0x0c
//      cmp W,800 -> 0x08                               cmp W,1280 -> 0x0d
//      cmp W,320 -> 0x01                               cmp W,1600 -> 0x0e
//      cmp W,512 -> 0x03
//      cmp W,640 -> 0x07
//
// Any widescreen width is > 800, so it lands in the UPPER branch.  Repurposing
// the 800x600 arm would therefore be wrong twice over: the arm would never be
// reached, and rewriting the FIRST compare to route it there would send 1024
// and 1280 down the lower branch, where nothing matches and grSstWinOpen fails.
// So the arm taken over is 1600x1200's, the one mode this hardware cannot use
// anyway, and both the 800x600 and 1600x1200 rows are given the new size so it
// does not matter which of the two the user picks in Config.exe.  Every other
// mode in the list keeps working exactly as before.
//
// Requirement 3 is one instruction, and it is the same disguised constant GTA2
// had.  The world projection is
//
//      recip[z] = S / z                         (table at 0xc24520, 345 readers)
//      screenX  = camX * focal * recip[z] + W/2
//      screenY  = camY * focal * recip[z] + H/2
//
// with `focal` a plain constant (384.0 normally; 1152.0 and 2112.0 for two
// other cameras) and the table built once at 0x528277 from
//
//      S = (W / 640.0) * uiZoom                 <- 0x4260a8
//
// One scale for both axes, so the picture is never distorted -- but deriving
// it from the WIDTH means the vertical field of view shrinks as the screen
// gets wider.  At 2560x1080 S is 4.0 where the aspect wants 2.25, and the
// result is a correctly-shaped image zoomed in by 1.78.  Taking the scale from
// the HEIGHT instead makes the vertical FOV independent of resolution and the
// extra width becomes extra world: Hor+, by construction (part one, section 7).
//
// The patch is an exact no-op at every 4:3 mode the game ships, because
// H/480 == W/640 at all of them:
//
//      320x240 0.5   512x384 0.8   640x480 1.0
//      800x600 1.25  1024x768 1.6  1600x1200 2.5
//
// Only 1280x1024 (5:4, 2.133 against 2.0) and an override differ.  480.0
// already sits in .rdata at 0x54805c, one dword below the 640.0 being
// replaced -- the same happy accident Ignition had with 240.0 next to 320.0.
//
// What is NOT fixed yet: the 2D layer.  Roughly two dozen paired sites divide
// an x by 640.0 and a y by 480.0, then multiply by W and H -- so HUD positions
// stretch to the full screen (which keeps corner elements in the corners, and
// is right) while element SIZE comes from W/640 (which at 21:9 is 1.78x too
// big).  That needs a screenshot before it needs code.
//

/* The video mode table, .data, seven 28-byte records. */
#define DRV_ROW_800   0x0054d7e4u      /* 800 x 600,   enum 0x08 */

/* The size the game latched out of that table at start-up, before we ran. */
#define DRV_CFG_W     0x0054bcc0u
#define DRV_CFG_H     0x0054bcc4u

/* The live render size, and the UI zoom (1.0 in normal play). */
#define DRV_LIVE_W    0x012eff08u
#define DRV_LIVE_H    0x012de6d0u
#define DRV_UI_ZOOM   0x012de6c0u

/* The game's own 480.0f, used as the projection divisor at virtual_height=480
   so the default path needs no allocation of ours at all. */
#define DRV_REF_480   0x0054805cu

/* Viewing distance and mirror viewing distance, both floats, both saved in
   CONFIG.DAT (+0x80 and +0x84) and both settable from the in-game options. */
#define DRV_VIEW_DIST 0x012eff04u
#define DRV_MIRR_DIST 0x012de6d8u

/* Their ceilings, in .rdata: 45000.0f and 42000.0f.  NOT patched in place --
   45000.0f alone has 29 references and only five of them are this. */



/* One bit per installer, in install order.  `mask` is a constant, so the
   compiler folds every test away and these cost nothing; they are kept so a
   future bisect is a one-line edit rather than a redesign.

   0x100 is retired (V6's draw_distance installer, deleted -- only its
   ceiling survives, unconditionally, in DrvTick).  The gap is deliberate:
   renumbering would invalidate every bit value recorded in CLAUDE.md and in
   the shipped README. */
#define DRV_P_MODES   0x01u
#define DRV_P_ENUM    0x02u
#define DRV_P_CONFIG  0x04u
#define DRV_P_RES2    0x08u
#define DRV_P_PROJ    0x10u
#define DRV_P_TEXT    0x20u
#define DRV_P_BAR     0x40u
#define DRV_P_SIZES   0x80u
#define DRV_P_FRUSP   0x200u
#define DRV_P_UISCL   0x400u
#define DRV_P_RANCH   0x800u
#define DRV_P_NEEDLE  0x1000u
#define DRV_P_STRIKE  0x2000u
#define DRV_P_MENU    0x4000u
#define DRV_P_LOAD    0x8000u
#define DRV_P_BTN     0x10000u
#define DRV_P_TIMER   0x20000u
#define DRV_P_SPRSZ   0x40000u
#define DRV_P_FLARE   0x80000u
#define DRV_P_ALL     0xfffffu

//
// Signatures are byte arrays rather than string literals on purpose: two
// signatures elsewhere in this file were silently destroyed by a UTF-8
// round-trip that turned every byte >= 0x80 into EF BF BD, and an array
// initialiser cannot suffer that.
//

/* V2.  0x4265d2 -- the tail of the width -> Glide enum chain's upper branch:
   `cmp [ebp-0xc],1600 ; je <1600 arm> ; jmp <fail>`.
   Only the five-byte `jmp` at offset 13 is rewritten, so the 1600x1200 arm
   itself is left completely intact. */
static const unsigned char drv_enumtail_sig[] = {
    0x81,0x7d,0xf4, 0x40,0x06,0x00,0x00,
    0x0f,0x84,0xe6,0x00,0x00,0x00,
    0xe9,0x05,0x01,0x00,0x00
};
#define DRV_ENUMTAIL_JMP 13

/* Used only to LOCATE the arm's `call` instruction, which the stub rejoins.
   Not patched.  The six-byte tail is shared with every other arm; the
   `push 0xe` at offset 13 is what makes this unique. */
static const unsigned char drv_arm1600_sig[] = {
    0x6a,0x01, 0x6a,0x02, 0x8b,0x4d,0xfc, 0x51, 0x6a,0x00, 0x6a,0x00,
    0x6a,0x0e, 0x8b,0x55,0x08, 0x52, 0xe8
};
#define DRV_ARM1600_CALL 18

/* V8.  The text drawers' `SetGlyphScale(sx*zoom, sy*zoom)`.  sy is loaded
   from [ebp-0xc] and sx from [ebp-0x4]; changing the second load's
   displacement to sy's makes the glyphs uniform.  One byte, at offset 15.
   Three text routines share the shape (0x5193f3, 0x51950f, 0x519640). */
static const unsigned char drv_txtscale_sig[] = {
    0xd9,0x45,0xf4,                      /* fld  [ebp-0xc]   sy         */
    0xd8,0x0d,0xc0,0xe6,0x2d,0x01,       /* fmul ds:uiZoom              */
    0x51, 0xd9,0x1c,0x24,                /* push ecx ; fstp [esp]       */
    0xd9,0x45,0xfc,                      /* fld  [ebp-0x4]   sx         */
    0xd8,0x0d,0xc0,0xe6,0x2d,0x01,       /* fmul ds:uiZoom              */
    0x51, 0xd9,0x1c,0x24,                /* push ecx ; fstp [esp]       */
    0xe8                                 /* call SetGlyphScale          */
};
#define DRV_TXTSCALE_SLOT   15
#define DRV_TXTSCALE_COUNT  3

//
// V9/V10.  `k * W` sizes -- the other way this engine expresses an extent.
//
// Each is `fild W ; fstp <local> ; fld <k> ; fmul <local>`, and each `k` is a
// fraction of the screen width standing in for a fraction of the height,
// because at 4:3 they are the same thing.  The correction is the same one V5
// and V7 apply: multiply by (H*640)/(W*480), which is 1.0 at 4:3 and 1/1.8 at
// 21:9.  Only the constant's disp32 moves, so nothing changes length.
//
// Measured against the 800x600 reference frame, to the pixel:
//   mirror  0.363636*W x 0.133333*H  ->  291x80 at 800x600, 698x107 at 1920x800
//   map     r = 0.1*W, drawn as centre +/- r  ->  160 square / 384 square
//
struct DrvSizeSite {
    const unsigned char *sig;
    unsigned int         len;
    unsigned int         at;        /* offset of the constant's disp32 */
    float                expect;    /* the constant it must currently hold */
    const char          *what;
};

static const unsigned char drv_mirrorw_sig[] = {
    0xdb,0x05,0x08,0xff,0x2e,0x01,       /* fild ds:W                   */
    0xd9,0x5d,0x88, 0xd9,0x45,0x88,      /* fstp/fld [ebp-0x78]         */
    0xd8,0x0d,0xb0,0x59,0x56,0x00        /* fmul ds:0x5659b0  (0.36363) */
};
static const unsigned char drv_mapr_sig[] = {
    0xdb,0x05,0x08,0xff,0x2e,0x01,       /* fild ds:W                   */
    0xd9,0x9d,0x74,0xfe,0xff,0xff,       /* fstp [ebp-0x18c]            */
    0xd9,0x05,0xc4,0x81,0x54,0x00        /* fld  ds:0x5481c4  (0.1)     */
};
static const unsigned char drv_mapr2_sig[] = {
    0xdb,0x05,0x08,0xff,0x2e,0x01,       /* fild ds:W                   */
    0xd9,0x9d,0x70,0xfe,0xff,0xff,       /* fstp [ebp-0x190]            */
    0xd9,0x05,0x14,0x8b,0x54,0x00        /* fld  ds:0x548b14  (0.14219) */
};

/* V32.  The radar BLIP scale -- what positions the police cars on the minimap.
 *
 *      0x5044a6   [0xbb1c30] = 0.00222168 * W * uiZoom      <- width-derived
 *      0x5044c4   [0x5762d0] = (W/640) * 540                <- centre X, correct
 *      0x5044e2   [0x5762d4] = (H/480) * 380                <- centre Y, correct
 *
 * The radar's CENTRE is placed correctly, and V10 already made the radar's
 * FRAME height-derived -- but the scale the blips are plotted with was left on
 * the width.  So a blip is drawn `1/AR` = 1.8x too far from the centre at
 * 1920x800, in both axes: exact at the middle, worsening outward.  That is the
 * reported symptom precisely ("correct near the player, driving off the roads
 * further out").
 *
 * The blip math itself is isotropic and needs nothing else -- 0x508f85 is
 * `x = delta*scale + centreX` with the SAME scale on both axes -- so correcting
 * this one constant corrects the whole radar overlay.
 *
 * `0xbb1c30` has exactly one writer and eight readers, all blip-position sites,
 * so nothing else can be disturbed.  Cross-check that it belongs with the two
 * map-radius entries above: 0.00222168 * 640 = 1.42188, exactly ten times the
 * 0.142187 they already correct. */
static const unsigned char drv_radar_sig[] = {
    0xdb,0x05,0x08,0xff,0x2e,0x01,       /* fild ds:W                     */
    0xd9,0x5d,0xfc,                      /* fstp [ebp-0x4]                */
    0xd9,0x05,0x04,0x8b,0x54,0x00        /* fld  ds:0x548b04  (0.0022217) */
};

static const DrvSizeSite drv_size_sites[] = {
    { drv_mirrorw_sig, sizeof(drv_mirrorw_sig), 14, 0.363636374f, "mirror" },
    { drv_mapr_sig,    sizeof(drv_mapr_sig),    14, 0.100000001f, "map r"  },
    { drv_mapr2_sig,   sizeof(drv_mapr2_sig),   14, 0.142187506f, "map r2" },
    { drv_radar_sig,   sizeof(drv_radar_sig),   11, 0.00222167978f, "radar" }
};

//
// V12.  The Damage / Felony fill bars, at 0x50495a.
//
// Named by V11's log in one run: `mapx caller=00504939 x=256/16`, which is the
// return address of the MapX call at 0x504934 -- so the bar's routine is
// 0x5048bd, and reading it showed the same shape as the mirror and the sprite
// drawer for the third time:
//
//      [ebp-0x8] = W/640        [ebp-0xc] = H/480
//      MapX(rect->x) -> screen x            position, correct
//      MapY(rect->y) -> screen y            position, correct
//      width  = [ebp-0x8] * rect->w * zoom  <- sx, STRETCHED
//      height = [ebp-0xc] * rect->h * zoom  <- sy, already right
//
// So the earlier worry that both edges were positions was wrong: only the
// left edge goes through MapX, and the extent is a separate multiply. One byte,
// swapping which local the width is taken from.
//
static const unsigned char drv_barw_sig[] = {
    0xd9,0x45,0xf8,                      /* fld  [ebp-0x8]   sx = W/640 */
    0xd8,0x48,0x08,                      /* fmul [eax+0x8]   rect width */
    0xd8,0x0d,0xc0,0xe6,0x2d,0x01        /* fmul ds:uiZoom              */
};
#define DRV_BARW_SLOT  2
#define DRV_BARW_COUNT 1

/* V4.  0x4260a8 -- `fild ds:W ; fstp [ebp-0x24] ; fld [ebp-0x24] ;
   fdiv ds:640.0`.  Two disp32s to rewrite, at offsets 2 and 14. */
static const unsigned char drv_proj_sig[] = {
    0xdb,0x05, 0x08,0xff,0x2e,0x01,
    0xd9,0x5d,0xdc,
    0xd9,0x45,0xdc,
    0xd8,0x35, 0x60,0x80,0x54,0x00
};
#define DRV_PROJ_SRC 2
#define DRV_PROJ_DIV  14

static BOOL         g_drvActive   = FALSE;
static unsigned int g_drvVirtualH = 480;
//
// V21's overdraw, in percent.  100 = the side clip planes sit exactly on the
// screen edge, which is correct in principle and still leaves a visible seam:
// the engine culls tightly, so a polygon whose plane test fails by a hair is
// dropped even though part of it belongs on screen.
//
// Not an ini key.  It is not a preference -- nobody wants the seam -- and per
// the project's own rule a confirmed correctness fix becomes unconditional.
// Change it here if a future resolution ever needs more.
//
static const unsigned int g_drvFrusMargin = 110;

//
// V6's Viewing Distance knob, a percent of the game's own 45000 maximum.
// Permanently 0 = "leave the game's setting alone", and no longer an ini
// key: it was only ever a diagnostic, and a knob that writes a value the
// game PERSISTS into CONFIG.DAT can outlive the build that set it.
//
// The CEILING in DrvTick is unaffected and still applies unconditionally --
// that half is a correctness limit, not a preference.
//

static unsigned int g_drvDivisor  = DRV_REF_480;   /* where the fdiv points */

/* Is this address inside the main image's committed range?  Cheap sanity for
   the fixed .data/.bss addresses used below. */
static BOOL DrvReadable(unsigned int va, unsigned int len)
{
    return !IsBadReadPtr((const void *)va, len);
}

//
// A page of our own for the floats the patched instructions point at.
//
// x86 disp32 operands are absolute, so any address works and a VirtualAlloc'd
// page is as good as .rdata.  This is how a resolution-dependent constant gets
// into an instruction without lengthening it.  Never freed -- the game reads
// these for its whole run.
//
static float *DrvConst(float v)
{
    static float *page  = NULL;
    static int    used  = 0;

    if (!page) {
        page = (float *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE,
                                     PAGE_READWRITE);
        if (!page) return NULL;
    }
    if (used >= 1024) return NULL;
    page[used] = v;
    return &page[used++];
}

//
// V1.  Rewrite the 800x600 mode-table row to the override resolution.
//
// The name field is 12 bytes including its terminator and the widest string
// that can be produced is "1920 x 1080" -- 11 characters -- so it always fits.
// Game.exe never reads it (only +0x00 and +0x04 are referenced, four times in
// the whole image); it is written so a dump of the table still reads as what
// it now means.
//
//
// V1/V3/V17.  The mode plumbing -- EVERY entry, not just one.
//
// This reverses the requirement of 2026-08-18, which was "with an override
// active, patch the in-game 800x600 entry and nothing else".  The reasoning
// then was that the other modes should keep meaning what they say.  It does not
// survive contact with what the override actually does:
//
//   the driver forces its override resolution inside grSstWinOpen whatever the
//   game asked for, so EVERY mode already displays at the override size.
//
// An unpatched selection therefore does not mean "1024x768 as labelled" -- it
// means the game believes 1024x768 while the screen is 1920x800, which is the
// corner-island failure with the projection, the HUD and the front end all
// laid out for the wrong size.  There is no selection that behaves correctly
// unpatched, so leaving six of the seven alone protected nothing.  Taking all
// seven means whatever is already in CONFIG.DAT works, and nobody has to be
// told which entry to pick.
//
// With the override Disabled, g_targetRes is 0, GameFix_Apply returns before
// any of this, and the mode list is untouched -- so the game is stock.
//
// The three places a mode lives, and all three have to agree:
//
//   0x54d790   the mode TABLE, 7 records of 28 bytes
//              { u32 w; u32 h; char name[12]; u32 flags; u32 glideEnum }
//   0x54bcc0   the size WinMain already latched out of it at 0x524477,
//              before we ran -- what 0x40c431 and 0x40c6b6 re-apply
//   0x41f411   the jump table's 7 arms, 16 bytes apart, each
//              `mov [ebp-0x4],w ; mov [ebp-0x10],h` -- a second, unrelated
//              latch that took six builds to find (see V17's history)
//
// Every row and every arm is verified against the stock value it must still
// hold before anything is written, so the seven checks double as the build
// check and a second application is a no-op.
//
#define DRV_MODE_TAB   0x0054d790u   /* 7 x 28 bytes                      */
#define DRV_MODE_N     7
#define DRV_MODE_STRIDE 28
#define DRV_ARM_TAB    0x0041f411u   /* 7 x 16 bytes, 14 of them ours     */
#define DRV_ARM_STRIDE 16

struct DrvMode { unsigned int w, h, glideEnum; };

static const struct DrvMode drv_modes[DRV_MODE_N] = {
    {  320,  240, 0x01 },
    {  512,  384, 0x03 },
    {  640,  480, 0x07 },
    {  800,  600, 0x08 },
    { 1024,  768, 0x0c },
    { 1280, 1024, 0x0d },
    { 1600, 1200, 0x0e }
};

static BOOL InstallDrvModeRow(unsigned char *data, unsigned int dataSize)
{
    unsigned char name[12];
    char          buf[16];
    int           i, n, wrote = 0;

    (void)data; (void)dataSize;

    /* Verify all seven first: a half-rewritten table would leave some
       selections working and others not, which is worse than none. */
    for (i = 0; i < DRV_MODE_N; i++) {
        unsigned int at = DRV_MODE_TAB + (unsigned int)i * DRV_MODE_STRIDE;
        if (!DrvReadable(at, DRV_MODE_STRIDE)) return FALSE;
        if (ReadU32((const unsigned char *)at)      != drv_modes[i].w ||
            ReadU32((const unsigned char *)at + 4)  != drv_modes[i].h ||
            ReadU32((const unsigned char *)at + 24) != drv_modes[i].glideEnum) {
            return FALSE;
        }
    }

    memset(name, 0, sizeof(name));
    n = wsprintfA(buf, "%u x %u", g_targetW, g_targetH);
    if (n > 0 && n < (int)sizeof(name)) memcpy(name, buf, (unsigned int)n);

    for (i = 0; i < DRV_MODE_N; i++) {
        unsigned char *at = (unsigned char *)(DRV_MODE_TAB +
                            (unsigned int)i * DRV_MODE_STRIDE);
        unsigned char  rep[DRV_MODE_STRIDE];

        memcpy(rep, at, DRV_MODE_STRIDE);
        PutU32(rep + 0, g_targetW);
        PutU32(rep + 4, g_targetH);
        memcpy(rep + 8, name, 12);
        PutU32(rep + 24, g_targetRes);
        if (WriteCode(at, rep, DRV_MODE_STRIDE)) wrote++;
    }

    return wrote == DRV_MODE_N;
}

//
// V2.  Teach the width -> Glide enum chain one extra width, without taking
// anything away from it.
//
// The chain is a binary search and any widescreen width is > 800, so it always
// falls out of the UPPER branch (1024 / 1280 / 1600) having matched nothing,
// into a five-byte `jmp <fail>` at 0x4265df.  Replacing THAT jump costs no
// existing mode: the failure path is the only thing lost, and only for widths
// that would have failed anyway.
//
// An earlier version took over the 1600x1200 arm in place.  That worked, but
// it silently removed 1600x1200 from the mode list, and the point of this is
// that ONLY the 800x600 entry changes meaning.
//
// The stub re-tests the width, and on a match rebuilds the arm's argument
// pushes with our own enum and jumps into the game's own `call` -- so the call,
// its result test and both exits stay the game's.  It is entered by `jmp`, not
// `call`, so esp and ebp are exactly what the surrounding function expects and
// `[ebp-0xc]` still names the width.
//
static BOOL InstallDrvEnum(unsigned char *code, unsigned int codeSize)
{
    unsigned char *tail, *arm, *stub;
    unsigned char  rep[18];
    unsigned int   jmpAt, failTarget, callTarget, s;

    tail = FindUnique(code, codeSize, drv_enumtail_sig,
                      sizeof(drv_enumtail_sig));
    if (!tail) {  return FALSE; }

    arm = FindUnique(code, codeSize, drv_arm1600_sig, sizeof(drv_arm1600_sig));
    if (!arm) {  return FALSE; }

    // Both targets are read back out of the very instructions being displaced,
    // so they cannot disagree with what is really there (part one, section 5b).
    jmpAt      = (unsigned int)(tail + DRV_ENUMTAIL_JMP);
    failTarget = jmpAt + 5 + ReadU32(tail + DRV_ENUMTAIL_JMP + 1);
    callTarget = (unsigned int)(arm + DRV_ARM1600_CALL);

    stub = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) {  return FALSE; }
    s = (unsigned int)stub;

    stub[0] = 0x81; stub[1] = 0x7d; stub[2] = 0xf4;   /* cmp [ebp-0xc], W */
    PutU32(stub + 3, g_targetW);
    stub[7] = 0x0f; stub[8] = 0x85;                   /* jne failTarget   */
    PutU32(stub + 9, failTarget - (s + 13));
    stub[13] = 0x6a; stub[14] = 0x01;                 /* push 1  nAux     */
    stub[15] = 0x6a; stub[16] = 0x02;                 /* push 2  nColBuf  */
    stub[17] = 0x8b; stub[18] = 0x4d; stub[19] = 0xfc;/* mov ecx,[ebp-4]  */
    stub[20] = 0x51;                                  /* push ecx  origin */
    stub[21] = 0x6a; stub[22] = 0x00;                 /* push 0  colfmt   */
    stub[23] = 0x6a; stub[24] = 0x00;                 /* push 0  refresh  */
    stub[25] = 0x6a; stub[26] = (unsigned char)g_targetRes;
    stub[27] = 0x8b; stub[28] = 0x55; stub[29] = 0x08;/* mov edx,[ebp+8]  */
    stub[30] = 0x52;                                  /* push edx  hWnd   */
    stub[31] = 0xe9;                                  /* jmp the game's call */
    PutU32(stub + 32, callTarget - (s + 36));

    memcpy(rep, drv_enumtail_sig, sizeof(drv_enumtail_sig));
    rep[DRV_ENUMTAIL_JMP] = 0xe9;
    PutU32(rep + DRV_ENUMTAIL_JMP + 1, s - (jmpAt + 5));


    return WriteCode(tail, rep, sizeof(drv_enumtail_sig));
}

//
// V3.  The size WinMain already latched, before we ran.
//
// LoadConfig has six call sites and 0x40c431 / 0x40c6b6 compare these against
// the live size and re-apply the mode when they differ, so these decide what
// the first race runs at.  Accepting any of the seven stock sizes rather than
// only 800x600 is the whole of this change; the test also serves as the
// idempotency check, since the target is never one of them.
//
static BOOL InstallDrvConfig(void)
{
    unsigned int w, h;
    int i;

    if (!DrvReadable(DRV_CFG_W, 8)) return FALSE;

    w = *(volatile unsigned int *)DRV_CFG_W;
    h = *(volatile unsigned int *)DRV_CFG_H;

    if (w == g_targetW && h == g_targetH) return TRUE;   /* already done */

    for (i = 0; i < DRV_MODE_N; i++)
        if (w == drv_modes[i].w && h == drv_modes[i].h) {
            *(volatile unsigned int *)DRV_CFG_W = g_targetW;
            *(volatile unsigned int *)DRV_CFG_H = g_targetH;
            return TRUE;
        }

    return FALSE;
}

//
// V4.  Derive the world scale from the height instead of the width.
//
// `virtual_height` is literally the number of the game's own vertical units
// that stay visible, so 480 reproduces the original framing exactly and larger
// values pull the camera back.  At the default the fdiv is pointed at the
// game's own 480.0f and nothing is allocated.
//
static BOOL InstallDrvProjection(unsigned char *at)
{
    unsigned char rep[18];

    if (!at) {  return FALSE; }

    g_drvDivisor = DRV_REF_480;
    if (g_drvVirtualH != 480) {
        float *f = DrvConst((float)g_drvVirtualH);
        /* Falling back to the game's own 480.0 is the right failure: the
           projection stays correct for the aspect and only the knob is lost. */
        if (f) g_drvDivisor = (unsigned int)f;
    }

    memcpy(rep, drv_proj_sig, sizeof(drv_proj_sig));
    PutU32(rep + DRV_PROJ_SRC, DRV_LIVE_H);     /* fild H, not W */
    PutU32(rep + DRV_PROJ_DIV, g_drvDivisor);   /* fdiv virtual_height */
    return WriteCode(at, rep, sizeof(drv_proj_sig));
}


//
// Apply a fixed-length edit at EVERY occurrence of a pattern, but only if
// there are exactly as many as were counted when the patch was derived.
//
// The count is the build check.  `FindUnique` cannot be used for a shape a
// game repeats deliberately -- two sprite drawers, three text routines -- and
// "patch the first one" would be a guess.  Two passes so nothing is written
// unless the whole set is present.
//
static int DrvPatchAll(unsigned char *code, unsigned int codeSize,
                       const unsigned char *sig, unsigned int len,
                       int expected, unsigned int at, const unsigned char *rep,
                       unsigned int repLen, const char *what)
{
    unsigned int i;
    int          n = 0;

    if (len == 0 || codeSize < len) return 0;

    for (i = 0; i <= codeSize - len; i++)
        if (memcmp(code + i, sig, len) == 0) n++;

    if (n != expected) {
        return 0;
    }

    n = 0;
    for (i = 0; i <= codeSize - len; i++) {
        if (memcmp(code + i, sig, len) != 0) continue;
        if (WriteCode(code + i + at, rep, repLen)) n++;
    }
    return n;
}


//
// V8.  The text drawers pass `SetGlyphScale(sx*zoom, sy*zoom)` with sx = W/640
// and sy = H/480, so glyphs come out 1.8x wider than tall at 21:9.
//
// One byte per site: the second `fld` reads sx from [ebp-0x4]; making it read
// sy from [ebp-0xc] instead gives square glyphs.  Positions are computed
// separately, earlier in the same routines, and are untouched.
//
static int InstallDrvTextScale(unsigned char *code, unsigned int codeSize)
{
    unsigned char rep[1];

    rep[0] = 0xf4;      /* [ebp-0x4] -> [ebp-0xc] */
    return DrvPatchAll(code, codeSize, drv_txtscale_sig,
                       sizeof(drv_txtscale_sig), DRV_TXTSCALE_COUNT,
                       DRV_TXTSCALE_SLOT, rep, 1, "text");
}

//
// V12.  The fill bars' width: take it from H/480 rather than W/640.  See the
// signature above -- one byte, and the height multiply beside it already uses
// the right one, which is what makes this unambiguous.
//
static int InstallDrvBarScale(unsigned char *code, unsigned int codeSize)
{
    unsigned char rep[1];

    rep[0] = 0xf4;      /* [ebp-0x8] -> [ebp-0xc] */
    return DrvPatchAll(code, codeSize, drv_barw_sig, sizeof(drv_barw_sig),
                       DRV_BARW_COUNT, DRV_BARW_SLOT, rep, 1, "bar");
}

//
// V9/V10.  The `k * W` sizes: mirror width and map radius.
//
// Each site's constant is verified by VALUE before anything is written, so a
// pattern that matched the wrong place cannot corrupt anything -- and the
// disp32 is repointed rather than the constant edited, because 0.1f has three
// references and only one of them is the map (part one, section 7e).
//
static int InstallDrvSizes(unsigned char *code, unsigned int codeSize)
{
    unsigned int i;
    int          n = 0;
    float        ar;

    if (!g_targetW || !g_targetH) return 0;

    /* (H/480) / (W/640) -- 1.0 at 4:3, 1/1.8 at 21:9. */
    ar = ((float)g_targetH * 640.0f) / ((float)g_targetW * 480.0f);

    for (i = 0; i < sizeof(drv_size_sites) / sizeof(drv_size_sites[0]); i++) {
        const DrvSizeSite *s = &drv_size_sites[i];
        unsigned char     *at, rep[4];
        unsigned int       konst;
        float             *ref;

        at = FindUnique(code, codeSize, s->sig, s->len);
        if (!at) {  continue; }

        konst = ReadU32(at + s->at);
        if (!DrvReadable(konst, 4) ||
            *(volatile float *)konst != s->expect) {
            continue;
        }

        ref = DrvConst(s->expect * ar);
        if (!ref) return n;

        PutU32(rep, (unsigned int)ref);
        if (WriteCode(at + s->at, rep, 4)) {
            n++;
        }
    }
    return n;
}




/* The reciprocal table at 0xc24520 covers z in [0, 56249].  45000 * 124% is
   55800, the most the far clip can be without indexing off the end of it. */

#define DRV_DRAW_MAX_Z   55800.0f










//
// V17.  The SECOND resolution dispatch, at 0x41f411.
//
// Found by reading AuToMaNiAk005's DriverWidescreenFix, which the user
// supplied.  Its patcher prints the offsets it writes, and the whole of its
// Game.exe patch is five 32-bit integers:
//
//      0x0041F474 width   0x0041F47B height     <- THIS site
//      0x004265D5 width                         <- the enum chain's cmp
//      0x0054D838 width   0x0054D83C height     <- mode table row 6
//
// Two of those three sites I already had.  This one I had never seen: a jump
// table at 0x41f40a, indexed by the CONFIG.DAT mode index, whose seven arms
// each load a hardcoded (width, height) pair into locals and pass them to
// 0x4215ba.  It is a completely separate latch from the mode table at
// 0x54d790 -- the game stores its resolution twice, in two unrelated forms,
// and patching only one leaves a subsystem configured for 1600x1200 (or
// 800x600) while the screen is 1920x800.
//
// That is what all the horizon artefacts were.  Every theory that "worked" --
// a narrower FOV, a shorter draw distance, a tighter frustum table -- reduced
// the disagreement between the two numbers or hid its consequences, which is
// why each looked plausible and none was the cause.
//
// Their fix repurposes mode row 6 and tells the user to select 1600x1200.  We
// take all seven rows and all seven arms instead, so no selection is special
// and CONFIG.DAT can say whatever it already says -- see V1 above.
//
static int InstallDrvRes2(unsigned char *code, unsigned int codeSize)
{
    /* mov DWORD PTR [ebp-0x4], imm32 ; mov DWORD PTR [ebp-0x10], imm32 */
    static const unsigned char head1[3] = { 0xc7, 0x45, 0xfc };
    static const unsigned char head2[3] = { 0xc7, 0x45, 0xf0 };
    int i, n = 0;

    (void)code; (void)codeSize;

    /* All seven verified against stock before any is written. */
    for (i = 0; i < DRV_MODE_N; i++) {
        const unsigned char *at = (const unsigned char *)
            (DRV_ARM_TAB + (unsigned int)i * DRV_ARM_STRIDE);
        if (!DrvReadable((unsigned int)(unsigned long)at, 14)) return 0;
        if (memcmp(at, head1, 3) != 0 || memcmp(at + 7, head2, 3) != 0 ||
            ReadU32(at + 3)  != drv_modes[i].w ||
            ReadU32(at + 10) != drv_modes[i].h) {
            return 0;
        }
    }

    for (i = 0; i < DRV_MODE_N; i++) {
        unsigned char *at = (unsigned char *)
            (DRV_ARM_TAB + (unsigned int)i * DRV_ARM_STRIDE);
        unsigned char  rep[14];

        memcpy(rep, at, 14);
        PutU32(rep + 3,  g_targetW);
        PutU32(rep + 10, g_targetH);
        if (WriteCode(at, rep, 14)) n++;
    }
    return n;
}







//
// V6.  Raise the ceiling on the game's own Viewing Distance setting.
//
// A wider field of view reaches further sideways, so the distance at which the
// game stops drawing -- invisible at 4:3, where it sat outside the frustum --
// comes into the corners of a 21:9 screen.  The game's own slider is the right
// control; it simply stops at 45000.
//
// The ceiling constants are NOT edited in place.  45000.0f is a pooled literal
// with 29 references and only five of them are this setting; the other 24 are
// unrelated comparisons in the mission and replay code.  So each of the five
// (and each of the ten for the 42000.0f used by the mirror and the in-race
// adjust) has its disp32 repointed instead, after verifying that the two opcode
// bytes and the operand are exactly what is expected -- which turns a wrong
// address into a no-op rather than corruption.
//
// The live value is SET, not scaled, so the setting takes effect without a trip
// through the menu; raising the ceiling is what stops the game clamping it
// straight back the moment the options screen is opened.
//
// `draw_distance` is a percent of the game's own maximum (45000), and 0 -- the
// default -- means "do not touch anything at all".  It was a percent of the
// CURRENT value in build 2, which turned out to be unreadable: the value in
// CONFIG.DAT was 30000, so 150% produced 45000, i.e. exactly the number the
// slider already offered, and the run reported "does nothing".  Anchoring the
// knob to a fixed reference makes what it asks for unambiguous.
//
// Both ceilings get the same value on purpose: the in-race clamp at 0x511c5c
// tests the VIEW distance against the 42000 constant, so leaving that one lower
// would pull the view distance back down the moment a race started.
//
//
// V21.  The left/right view-frustum clip planes.
//
// THIS IS THE FIX for "surfaces missing at the screen edges with the skybox
// showing through".  Confirmed on hardware.
//
// Driver builds its side clip planes from a hardcoded 4:3 aspect.  Two sites
// have the identical shape:
//
//      0x12733a0 = <aspect>            <- 0.75, i.e. 480/640
//      0x1288924 = <y>
//      fld <aspect> ; fchs             ; -aspect
//      fld <y>      ; fchs
//      lea edx,[ebp-0x10] ; call 0x4019a1     ; normalise a 3-vector
//      -> 0x1288950 / 0x1288954 / 0x1288958   ; middle component is 0
//
// Two normalised vectors of the form (+/-aspect, 0, -y).  A zero Y component
// makes them VERTICAL planes: the left and right edges of the view frustum.
//
// Widening the projection -- by V4, by V15, or by rewriting the shared 640.0
// the way AuToMaNiAk005's fix does -- moves the picture wider and leaves these
// two planes at 4:3.  Everything outside the old frustum is then clipped, and
// it gets worse the further you widen.  It is a CLIP, not a cull, which is why
// it scaled smoothly with field of view instead of switching on at a
// threshold, and why it shows on the Direct3D renderer too.
//
// The correction is to divide the aspect by the same factor the projection was
// widened by:
//
//      F     = (W * virtual_height) / (640 * H)      the widening factor
//      value = 0.75 / F = 480 * H / (W * virtual_height)
//
// 0.75 at the AUTO default, so this is an EXACT NO-OP at 4:3 and whenever the
// view is not widened; H/W at virtual_height=480.  It is correct for any
// virtual_height because it is derived from the same factor the projection
// uses, and it does not care whether the widening came from V4 or V15.
//
// WHY THIS TOOK SO LONG TO FIND, worth recording:
//
// In OUR build the aspect is read from 0xc14b6c, which is past the end of raw
// .data -- BSS, written at run time through some pointer, so a search for
// direct writes finds none.  In the 1999-11-18 build the compiler folded the
// same value to a literal in .rdata.  When the two binaries were compared
// instruction by instruction, every address was normalised away first, so
//
//      mov eax,ds:0x547064      (his)
//      mov eax,ds:0xc14b6c      (ours)
//
// compared EQUAL, and four rounds of comparison concluded "identical program".
// The lesson is narrow and worth keeping: normalising operands to compare code
// across builds hides exactly the class of difference that matters when the
// bug IS an operand.  Compare shapes to find candidates, then compare the
// operands of the candidates.
//
// The base 0.75 is taken from the other build rather than read from 0xc14b6c,
// because that global is still zero when grGlideInit runs.
//
static const unsigned char drv_frusplane_sig[5] = {
    0xa1, 0x6c, 0x4b, 0xc1, 0x00        /* mov eax, ds:0xc14b6c */
};
#define DRV_FRUSPLANE_DISP 1
#define DRV_FRUSPLANE_N    2            /* exactly two, and that is the check */

static int InstallDrvFrusPlane(unsigned char *code, unsigned int codeSize)
{
    unsigned char rep[5];
    float        *ref;
    float         value, widen;
    unsigned int  margin;

    value = (480.0f * (float)g_targetH) /
            ((float)g_targetW * (float)g_drvVirtualH);

    //
    // The margin.  Exactly matching the frustum to the screen is correct in
    // principle and still visible in practice: the engine culls tightly, so a
    // polygon whose plane test fails by a hair is dropped even though part of
    // it belongs on screen.  Pushing the side planes out a few percent draws a
    // little outside the viewport and moves the seam off the edge.
    //
    // Applied ONLY when the view is actually widened.  At 4:3, and at
    // virtual_height=0, the aspect stays exactly 0.75 so the patch keeps its
    // exact-no-op property and cannot be blamed for anything there.
    //
    widen  = 0.75f / value;
    /* 1.01, not 1.001: AUTO rounds virtual_height to a whole number, which
       leaves a 0.1%% residue -- that must still count as "not widened". */
    margin = (widen > 1.01f) ? g_drvFrusMargin : 100u;
    if (margin > 100u) value = value * 100.0f / (float)margin;

    ref = DrvConst(value);
    if (!ref) return 0;

    memcpy(rep, drv_frusplane_sig, sizeof(drv_frusplane_sig));
    PutU32(rep + DRV_FRUSPLANE_DISP, (unsigned int)ref);


    return DrvPatchAll(code, codeSize, drv_frusplane_sig,
                       sizeof(drv_frusplane_sig), DRV_FRUSPLANE_N,
                       0, rep, sizeof(rep), "frusplane");
}

//
// V22.  The 2D image scale -- uniform, but taken from the WIDTH.
//
// Found by the corner probe, then read back up the call chain:
//
//   0x40b92d   the 2D image drawer.  Position is `u * scale + origin`, so both
//              the origin and the scale come from its caller.  It ends in
//              grDrawPolygonVertexList, which is how the probe caught it
//              (caller=0x40bcef, box 0,0 .. 283,379 -- the stopwatch).
//
//   0x4e940c   one of its four callers, and the only one that pushes the SAME
//              local for both axes:
//
//                  4e9380  fild  W                    ; 0x12eff08
//                  4e938c  fdiv  640.0                ; 0x548060
//                  4e9392  fmul  uiZoom               ; 0x12de6c0
//                  4e9398  [ebp-0x8] = (W/640)*uiZoom ; sx
//                  ...
//                  4e93fc  push [ebp-0x8]             ; scaleY  <-- sx
//                  4e9400  push [ebp-0x8]             ; scaleX  <-- sx
//
// So the image is scaled UNIFORMLY -- the stopwatch dial stays round, which is
// why this never looked like the stretched-HUD bug -- but from the width.  At
// 1920x800 that is 3.0 where the aspect-preserving value is H/480 = 1.667, so
// every 2D image is 1.8x too big.
//
// The overlays on those images -- the stopwatch needle, the red strikeout
// scribble on the to-do list -- are positioned by the ordinary proportional
// mapping, which is correct.  So they land where the CORRECTLY sized image
// would put them: up and left of an oversized dial.  That is the whole of the
// reported symptom, and it is why "make the image smaller and the overlay will
// line up" was the right instinct.
//
// Fixed the same way as V5/V6/V9/V10: repoint the divisor at a constant of
// ours, so `W / K == H / 480`:
//
//      K = 480 * W / H
//
// 640 at 4:3 -- an EXACT NO-OP there -- and 1152 at 1920x800.
//
// ONE SITE ONLY, deliberately.  Three sites in the image compute
// `fild W ; fdiv 640 ; fmul uiZoom` and no site anywhere computes the H/480
// equivalent, so the engine simply has no height-derived uniform scale.  Of
// the three:
//
//      0x4260b4   the WORLD scale -- V4 owns it, and V4 replaces the 18 bytes
//                 that contain it.  Patching it here would double up.
//      0x4b75bd   a world-space billboard sizer: the same routine reads
//                 recip[z] at 0xc24520 and the focal at 0x12d0d44.  Different
//                 domain, unconfirmed, left alone.
//      0x4e938c   this one.
//
// Patched by explicit address rather than by scanning for the byte pattern,
// because V4 may or may not have already consumed 0x4260b4 depending on the
// patch mask -- a scan would then find two sites or three and could not tell
// which.
//
//
// The full site list.  V22 originally patched only 0x4e938c -- the one the
// corner probe had confirmed -- and that fixed the stopwatch and the paper
// while leaving the to-do list's strikeouts untouched, because they are
// drawn by a DIFFERENT routine that computes the same scale at its own
// site.  AuToMaNiAk005's fix does not have that problem: it rewrites the
// shared 640.0 itself, so all 36 readers move together.  Ours repoints
// individual divisors, which is safer but means the list has to be
// complete.
//
// The five below all compute `uiZoom * W / 640` and use it as a SIZE:
//
//      0x4e938c   stopwatch and to-do paper   (via 0x4e940c / 0x4e9411)
//      0x4e94c8   X scale, and
//      0x4e94e8   Y scale -- both from the WIDTH, in the same routine
//      0x4e978a   the strikeout marks         (via 0x4e983e / 0x4e9843)
//      0x4e98b7   same shape, same module
//
// Deliberately NOT in the list, though they read the same constant:
//
//      0x4e9317, 0x4e944a   `fld x ; fdiv 640` immediately followed by
//                           `fld y ; fdiv 480` -- POSITION mappers, which
//                           are correct as they stand.  That pairing is
//                           the discriminator: a size divides only by 640,
//                           a position divides x by 640 and y by 480.
//      0x4260b4             the world scale (V4 owns it).
//      0x4b75bd             a world-space billboard sizer -- the same
//                           routine reads recip[z] and the focal.
//
static const unsigned int drv_uiscale_sites[] = {
    0x004e938cu, 0x004e94c8u, 0x004e94e8u, 0x004e978au, 0x004e98b7u
};

static int InstallDrvUiScale(void)
{
    static const unsigned char sig[6] = { 0xd8,0x35, 0x60,0x80,0x54,0x00 };
    const int n = (int)(sizeof(drv_uiscale_sites) /
                        sizeof(drv_uiscale_sites[0]));
    float *ref;
    float  k;
    int    i, ok = 0, wrote = 0;

    for (i = 0; i < n; i++) {
        const unsigned char *at = (const unsigned char *)drv_uiscale_sites[i];
        if (DrvReadable(drv_uiscale_sites[i], 6) && memcmp(at, sig, 6) == 0)
            ok++;
    }
    if (ok != n) {
        return 0;
    }

    k = 480.0f * (float)g_targetW / (float)g_targetH;
    ref = DrvConst(k);
    if (!ref) return 0;

    for (i = 0; i < n; i++) {
        unsigned char *at = (unsigned char *)drv_uiscale_sites[i];
        unsigned char  rep[6];
        memcpy(rep, sig, 6);
        PutU32(rep + 2, (unsigned int)ref);
        if (WriteCode(at, rep, 6)) wrote++;
    }

    return wrote;
}

//
// V23.  Re-anchor the right-anchored 2D images.
//
// V22 made 2D image SIZES uniform (H/480) while positions still map with
// W/640.  For a top-left-anchored image that is fine -- its origin is at the
// left edge either way.  For a RIGHT-anchored one it is not, and the to-do
// list showed it: the image got smaller but its left edge stayed put, so it
// pulled away from the right edge of the screen.
//
// The engine expresses a right anchor in 640-space, at two sites of identical
// shape:
//
//      4e7cb3  cl = [0xd82aa0 + 0xb]      the image's width, a byte
//      4e7cbf  fld  640.0
//      4e7cc5  fsub w
//      4e7cc8  [ebp-0x8] = 640 - w        origin, top-right anchored
//
// That origin maps to `W - w*W/640`, but the image is now `w*H/480` wide, so
// its right edge falls short of the screen by exactly `w*(W/640 - H/480)`.
//
// The origin has to become `640 - w*K` with
//
//      K = (640 * H) / (480 * W)
//
// because then `(640 - w*K) * W/640` is exactly `W - w*H/480`.  K is 1.0 at
// 4:3, so both sites are an EXACT NO-OP there, and 0.5556 at 1920x800.
//
// Twelve bytes at each site, and the replacement needs eighteen, so each gets
// a small stub reached by `call rel32` + nops.  Entered by CALL, so ebp is
// untouched and the stub can read and write the same locals the original did;
// their displacements are copied out of the site rather than hardcoded,
// because the two sites use different ones (-0x1c/-0x8 and -0x10/-0x4).
//
static const unsigned int drv_ranchor_sites[2] = { 0x004e7cbfu, 0x004e811fu };

static int InstallDrvRightAnchor(void)
{
    static const unsigned char head[6] = { 0xd9,0x05, 0x60,0x80,0x54,0x00 };
    float *k;
    int    i, n = 0;

    k = DrvConst((640.0f * (float)g_targetH) /
                 (480.0f * (float)g_targetW));
    if (!k) return 0;

    /* Two passes: a half-applied pair would leave one right-anchored element
       correct and the other not, which is harder to read than neither. */
    for (i = 0; i < 2; i++) {
        const unsigned char *at = (const unsigned char *)drv_ranchor_sites[i];
        if (!DrvReadable(drv_ranchor_sites[i], 12)) return 0;
        if (memcmp(at, head, 6) != 0)    return 0;   /* fld ds:0x548060   */
        if (at[6] != 0xd8 || at[7] != 0x65) return 0; /* fsub [ebp-disp8] */
        if (at[9] != 0xd9 || at[10] != 0x5d) return 0;/* fstp [ebp-disp8] */
    }

    for (i = 0; i < 2; i++) {
        unsigned char *at = (unsigned char *)drv_ranchor_sites[i];
        unsigned char *stub;
        unsigned char  rep[12];
        unsigned char  src = at[8];      /* fsub's ebp disp8 */
        unsigned char  dst = at[11];     /* fstp's ebp disp8 */

        stub = (unsigned char *)VirtualAlloc(NULL, 32,
                   MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!stub) return n;

        stub[0] = 0xd9; stub[1] = 0x45; stub[2] = src;      /* fld  [ebp-src] */
        stub[3] = 0xd8; stub[4] = 0x0d;                     /* fmul ds:K      */
        PutU32(stub + 5, (unsigned int)k);
        stub[9] = 0xd8; stub[10] = 0x2d;                    /* fsubr ds:640.0 */
        PutU32(stub + 11, 0x00548060u);
        stub[15] = 0xd9; stub[16] = 0x5d; stub[17] = dst;   /* fstp [ebp-dst] */
        stub[18] = 0xc3;                                    /* ret            */

        rep[0] = 0xe8;
        PutU32(rep + 1, (unsigned int)stub - (drv_ranchor_sites[i] + 5));
        memset(rep + 5, 0x90, 7);
        if (WriteCode(at, rep, sizeof(rep))) n++;
    }

    return n;
}

//
// V24.  The stopwatch needle.
//
// Named by the corner probe once it recorded the caller's caller: every 2D
// line goes through the shared rect mapper at 0x42683e, so depth 0 is always
// that mapper and identifies nothing.  Depth 1 was 0x4e7c8e -- the call at
// 0x4e7c89 -- and that routine reads in one go:
//
//      4e7c02  [ebp-0x4]  = 47.0        x0, the dial centre in 640-space
//      4e7c09  [ebp-0x10] = 87.0        y0
//      4e7c10  [ebp-0x8]  = 32.0        radius, X
//      4e7c17  [ebp-0x14] = -32.0       radius, Y
//      4e7c42  [ebp-0x8]  = cos[a] * 32         cos table at 0xc13400
//      4e7c52  [ebp-0x14] = sin[a] * -32        sin table at 0xc14400
//      4e7c62  [ebp-0x8]  += ds:0x548a98        = 47.0
//      4e7c6e  [ebp-0x14] += ds:0x548a94        = 87.0
//      4e7c89  call the rect mapper             draws centre -> centre+radius
//
// The mapper sends x through W/640 and y through H/480.  Y is therefore
// already right -- 87 * H/480 = 145, which is where the probe found it -- and
// only X is wrong: 47 * W/640 = 141 where the dial's centre now sits at
// 47 * H/480 = 78.  Hence a needle pinned to the right edge of the dial.
//
// So every X quantity is pre-multiplied by the same factor V23 uses:
//
//      K = (640 * H) / (480 * W)
//
// because (x*K) * W/640 == x * H/480 exactly.  Three in-place, same-length
// edits: the two immediates and the additive centre.  K is 1.0 at 4:3, so all
// three are an EXACT NO-OP there.
//
// The Y immediates (87.0, -32.0) and 0x548a94 are deliberately untouched.
//
// 0x548a94 and 0x548a98 have exactly ONE reference each -- they are private to
// this routine -- but the disp32 is repointed at a constant of ours rather
// than rewriting .rdata in place, so nothing else can ever be affected by it.
//
#define DRV_NEEDLE_CX   0x004e7c02u     /* mov [ebp-0x4], 47.0  */
#define DRV_NEEDLE_RX   0x004e7c10u     /* mov [ebp-0x8], 32.0  */
#define DRV_NEEDLE_ADD  0x004e7c62u     /* fadd ds:0x548a98     */

static int InstallDrvNeedle(void)
{
    unsigned char *cx  = (unsigned char *)DRV_NEEDLE_CX;
    unsigned char *rx  = (unsigned char *)DRV_NEEDLE_RX;
    unsigned char *add = (unsigned char *)DRV_NEEDLE_ADD;
    unsigned char  rep[7];
    float         *ref;
    float          k;
    int            n = 0;

    if (!DrvReadable(DRV_NEEDLE_CX, 7) || !DrvReadable(DRV_NEEDLE_RX, 7) ||
        !DrvReadable(DRV_NEEDLE_ADD, 6)) return 0;

    /* verify all three are exactly what the analysis found */
    if (cx[0] != 0xc7 || cx[1] != 0x45 || cx[2] != 0xfc ||
        ReadU32(cx + 3) != FloatBits(47.0f))  {  return 0; }
    if (rx[0] != 0xc7 || rx[1] != 0x45 || rx[2] != 0xf8 ||
        ReadU32(rx + 3) != FloatBits(32.0f))  {  return 0; }
    if (add[0] != 0xd8 || add[1] != 0x05 ||
        ReadU32(add + 2) != 0x00548a98u)      {  return 0; }

    k = (640.0f * (float)g_targetH) / (480.0f * (float)g_targetW);

    memcpy(rep, cx, 7);
    PutU32(rep + 3, FloatBits(47.0f * k));
    if (WriteCode(cx, rep, 7)) n++;

    memcpy(rep, rx, 7);
    PutU32(rep + 3, FloatBits(32.0f * k));
    if (WriteCode(rx, rep, 7)) n++;

    ref = DrvConst(47.0f * k);
    if (ref) {
        memcpy(rep, add, 6);
        PutU32(rep + 2, (unsigned int)ref);
        if (WriteCode(add, rep, 6)) n++;
    }

    return n;
}

//
// V25.  The strikeout x offsets.
//
// Each crossed-off line is drawn at `paperOrigin + record[0]`, where the
// record comes from a 16-byte table at 0x564df0 holding
// {x_offset, width, y0, y1} for each of the NINE list rows -- which is
// exactly the maximum the list can show:
//
//      4e8108  [ebp-0x8] = 0x564df0 + index*16
//      4e811f  [ebp-0x4] = 640 - w          the paper origin (V23)
//      4e8161  fld  [ebp-0x4]
//      4e8164  fadd [eax]                   + record[0]
//
// The paper origin is already right and the y goes through MapY, which
// divides by 480 and is therefore uniform already.  Only the x OFFSET is
// still in stretched units: MapX multiplies it by W/640 where the paper's
// own content is now drawn at H/480.
//
// Scaling the offsets by the usual K = (640*H)/(480*W) puts them back:
// the first row goes from (570+21)*3.0 = 1773 to (570+21*0.5556)*3.0 =
// 1745, which is the ~27 px the marks were out by at 1920x800.
//
// A pure DATA patch -- no code changes -- and the table has exactly one
// reference in .text, so nothing else can be reading it.  The widths and
// the y fields are deliberately untouched: the strokes are already the
// right size and the right height.
//
// K is 1.0 at 4:3, so this is an EXACT NO-OP there.
//
#define DRV_STRIKE_TAB 0x00564df0u
#define DRV_STRIKE_N   9

static int InstallDrvStrikeX(void)
{
    static const float want[DRV_STRIKE_N] = {
        21.0f, 17.0f, 18.0f, 21.0f, 23.0f, 20.0f, 25.0f, 25.0f, 31.0f
    };
    float k;
    int   i, n = 0;

    if (!DrvReadable(DRV_STRIKE_TAB, DRV_STRIKE_N * 16)) return 0;

    /* Two passes -- a partly-scaled table would stagger the rows. */
    for (i = 0; i < DRV_STRIKE_N; i++) {
        float v = *(const float *)(DRV_STRIKE_TAB + i * 16);
        if (v < want[i] - 0.01f || v > want[i] + 0.01f) {
            return 0;
        }
    }

    k = (640.0f * (float)g_targetH) / (480.0f * (float)g_targetW);

    for (i = 0; i < DRV_STRIKE_N; i++) {
        unsigned char rep[4];
        PutU32(rep, FloatBits(want[i] * k));
        if (WriteCode((unsigned char *)(DRV_STRIKE_TAB + i * 16), rep, 4)) n++;
    }

    return n;
}

//
// V26.  The front-end menu -- scaled into a centred 4:3 band.
//
// THE SHAPE OF DRIVER'S FRONT END, which is what makes this small.
//
// The menu is not drawn with Glide at all.  The whole front end -- background
// art, logo, every menu item, all the text -- is rendered into ONE plain
// 640x480 EIGHT-BIT paletted framebuffer in system memory, and a single leaf
// function copies it to the screen once a frame:
//
//      0x57d9b4   the 640x480 8bpp buffer.  0x407ee8 plots into it:
//                 `imul edx,0x280` for the row, base 0x57d9b4, one byte/pixel
//      0x12f0ec0  a 256-entry palette, entries ALREADY in the screen's pixel
//                 format (filled at 0x407e57 with a 0x400-byte copy)
//      0x407e7c   the per-frame present: grLfbLock, call [0x57d9b0], unlock.
//                 One caller (0x40ceca), and it is front-end only.
//      0x57d9b0   the blit pointer, chosen by depth at 0x407b4e / 0x407b5a
//      0x52b0a3   Blit8to16(src, dst, w, h)   <- what we replace
//      0x52b121   Blit8to32, same shape, a full palette dword per pixel
//
// So the entire menu reaches the screen through one function with a known
// signature, and everything upstream of it stays in its own 640x480 world.
// That is why this is a function replacement rather than the ~20-site
// per-element job GTA2 needed: there is nothing to re-lay-out.
//
// Worth knowing about the routine being replaced: it IGNORES its `w` argument.
// The inner count is a hardcoded 0xa0 = 160 iterations of 4 pixels, and the row
// advance is `stride - 0x500` with 0x500 = 640*2.  It is hardwired to 640 wide,
// which is exactly why the game could never have been asked politely for a
// bigger menu.
//
// WHAT THE REPLACEMENT DOES.  Nearest-neighbour scale of the 640x480 source
// into the largest 4:3 box that fits, centred, with the rest painted black:
//
//      s    = min(W/640, H/480)         uniform, so the art is not distorted
//      band = 640*s x 480*s, centred
//
// At 1920x800 that is 1066x800 at x=427 -- full height, 427px bars either side.
//
// It is a NO-OP AT 640x480 in the strongest sense: the installer declines
// outright when the band comes out exactly 640x480 at the origin, so the game
// keeps its own hand-tuned assembly and we add nothing at all.
//
// The bars are repainted every frame rather than once.  Once would do within a
// single visit to the menu -- nothing else writes there -- but coming back from
// a race leaves that race's pixels in them and there is no signal for it.  Two
// sequential fills of the non-band area is not a cost worth reasoning about on
// a menu.
//
// This is a patch to Driver's own code and data.  Nothing here touches the
// wrapper, so it applies just as well baked into the exe and run under any
// other Glide implementation.
//

#define DRV_FE_BUF      0x0057d9b4u   /* 640x480 8bpp front-end framebuffer   */
#define DRV_FE_PAL      0x012f0ec0u   /* 256 dwords, already in screen format  */
#define DRV_FE_STRIDE   0x005c89d8u   /* the framebuffer's strideInBytes       */
#define DRV_FE_BPP      0x005c89b8u   /* 16 or 32, captured at 0x407b64        */
#define DRV_FE_SET16    0x00407b4eu   /* mov [0x57d9b0], 0x402324              */
#define DRV_FE_SET32    0x00407b5au   /* mov [0x57d9b0], 0x402301              */
#define DRV_FE_ORIG16   0x00402324u
#define DRV_FE_ORIG32   0x00402301u
#define DRV_FE_SRC_W    640
#define DRV_FE_SRC_H    480

typedef void (__cdecl *DrvFeBlit_t)(const unsigned char *src, void *dst,
                                    int w, int h);

static DrvFeBlit_t g_drvFeOrig16 = NULL;
static DrvFeBlit_t g_drvFeOrig32 = NULL;
static int         g_drvFeW  = 0, g_drvFeH  = 0;   /* the band, in pixels */
static int         g_drvFeX0 = 0, g_drvFeY0 = 0;

//
// Bilinear, done in the DESTINATION's own packed format.
//
// The palette at 0x12f0ec0 already holds screen-format pixels, so blending
// there costs no unpacking and no repacking -- which is the whole reason this
// is affordable on a 1997 CPU.  Both passes use the standard two-field trick:
// spread the components so they cannot carry into each other, take a weighted
// sum, shift back.
//
//      565: (A*(32-t) + B*t) >> 5   with the fields spread to 0x07E0F81F
//      888: (A*(256-t) + B*t) >> 8  with the fields split 0x00ff00ff / 0xff00
//
// Both are checked for overflow at their widest: 63<<21 times 32 is 4227858432
// and 255<<16 times 256 is 4278190080, each just inside 32 bits, and the
// weights sum to exactly one step so a flat area stays flat.
//
// SEPARABLE, which is what keeps it to roughly two or three times the cost of
// the nearest-neighbour version rather than seven.  Each SOURCE row is scaled
// horizontally once into a two-row cache -- 480 of those -- and every
// DESTINATION row is then one vertical blend of two cached rows.  The cache
// slots rotate: the vertical source index only ever advances by zero or one,
// because the band is never smaller than the source.
//
//
// 565 is handled in three pieces rather than one, because the row cache holds
// pixels in the SPREAD form.  The horizontal pass then blends and stores
// without folding, and the vertical pass folds once on the way out -- which
// takes two spreads and a fold per pixel out of the inner loop.
//
static unsigned int Spread565(unsigned int p)
{
    return (p | (p << 16)) & 0x07E0F81Fu;
}

static unsigned int LerpSpread(unsigned int A, unsigned int B, unsigned int t)
{
    return (((A * (32u - t)) + (B * t)) >> 5) & 0x07E0F81Fu;
}

static unsigned int Fold565(unsigned int C)
{
    return (C | (C >> 16)) & 0xffffu;
}

static unsigned int Lerp888(unsigned int a, unsigned int b, unsigned int t)
{
    unsigned int rb = ((((a & 0x00ff00ffu) * (256u - t)) +
                        ((b & 0x00ff00ffu) * t)) >> 8) & 0x00ff00ffu;
    unsigned int g  = ((((a & 0x0000ff00u) * (256u - t)) +
                        ((b & 0x0000ff00u) * t)) >> 8) & 0x0000ff00u;
    return (a & 0xff000000u) | rb | g;
}

/* Scratch, all sized together and only when the band changes. */
static unsigned char *g_drvFeMem     = NULL;
static int            g_drvFeMemCap  = 0;
static unsigned int  *g_drvFeCol     = NULL;  /* per dest column: sx<<8 | wx */
static unsigned char *g_drvFeCache   = NULL;  /* two horizontally-scaled rows */
static unsigned char *g_drvFeOut     = NULL;  /* one destination row          */
static int            g_drvFeCacheA  = -1;    /* which source row is in slot 0 */
static int            g_drvFeTabW    = 0;     /* the band the tables are for   */
static int            g_drvFeTabSrcW = 0;

static void __cdecl DrvFrontEndBlit(const unsigned char *src, void *dst,
                                    int w, int h)
{
    const unsigned int *pal    = (const unsigned int *)DRV_FE_PAL;
    int                 bpp    = (int)*(volatile unsigned int *)DRV_FE_BPP;
    int                 stride = (int)*(volatile unsigned int *)DRV_FE_STRIDE;
    int   bandW = g_drvFeW, bandH = g_drvFeH;
    int   x0 = g_drvFeX0, y0 = g_drvFeY0;
    int   px, dy;
    unsigned int dxStep, dyStep, sy16;

    //
    // Anything not the exact shape this was written against goes to the game's
    // own routine.  Delegating is always safe; guessing is not.
    //
    // The 16bpp case additionally requires the framebuffer to really be 565 in
    // the usual bit order, because Lerp565 is written against that layout.
    // The game publishes its own answer at 0x5c89bc..0x5c89d0 (the shift-right
    // and shift-left counts its blit uses), so this is asked rather than
    // assumed.
    //
    if (!src || !dst || w != DRV_FE_SRC_W || h <= 1 ||
        bandW <= 0 || bandH <= 0 || stride <= 0 ||
        (bpp != 16 && bpp != 32)) {
        DrvFeBlit_t o = (bpp == 32) ? g_drvFeOrig32 : g_drvFeOrig16;
        if (o) o(src, dst, w, h);
        return;
    }
    if (bpp == 16) {
        const volatile unsigned int *f = (const volatile unsigned int *)0x005c89bcu;
        if (f[0] != 3 || f[1] != 2 || f[2] != 3 ||
            f[3] != 11 || f[4] != 5 || f[5] != 0) {
            if (g_drvFeOrig16) g_drvFeOrig16(src, dst, w, h);
            return;
        }
    }

    px = bpp >> 3;

    /* One allocation for the column table, the two-row cache and the output
       row, resized only when the band does -- which is never, in practice. */
    {
        int need = bandW * (int)sizeof(unsigned int)   /* column table   */
                 + bandW * (int)sizeof(unsigned int) * 2 /* two cached rows,
                                                            u32 either way */
                 + bandW * px;                         /* output row     */
        if (need > g_drvFeMemCap) {
            unsigned char *p = (unsigned char *)realloc(g_drvFeMem,
                                                        (unsigned int)need);
            if (!p) {
                DrvFeBlit_t o = (bpp == 32) ? g_drvFeOrig32 : g_drvFeOrig16;
                if (o) o(src, dst, w, h);
                return;
            }
            g_drvFeMem = p; g_drvFeMemCap = need;
            g_drvFeTabW = 0;                 /* force the tables rebuilt */
        }
        g_drvFeCol   = (unsigned int *)g_drvFeMem;
        g_drvFeCache = g_drvFeMem + bandW * (int)sizeof(unsigned int);
        g_drvFeOut   = g_drvFeCache + bandW * (int)sizeof(unsigned int) * 2;
    }

    dxStep = ((unsigned int)w << 16) / (unsigned int)bandW;
    dyStep = ((unsigned int)h << 16) / (unsigned int)bandH;

    /* The column table never changes while the band does not, so it is built
       once rather than 800 times a frame. */
    if (g_drvFeTabW != bandW || g_drvFeTabSrcW != w) {
        unsigned int fx = 0;
        int dx;
        for (dx = 0; dx < bandW; dx++, fx += dxStep) {
            int sx = (int)(fx >> 16);
            unsigned int wt = (fx >> 8) & 0xffu;
            /* Clamp so sx+1 is always inside the row: the last destination
               pixels then blend towards the last source pixel instead of
               reading past it. */
            if (sx > w - 2) { sx = w - 2; wt = 0xffu; }
            g_drvFeCol[dx] = ((unsigned int)sx << 8) | wt;
        }
        g_drvFeTabW = bandW; g_drvFeTabSrcW = w;
        g_drvFeCacheA = -1;
    }

    /* The bars.  Black is 0 in both output formats, so memset serves. */
    {
        int x1 = x0 + bandW, y1 = y0 + bandH;
        int W  = (int)g_targetW, H = (int)g_targetH;
        int y;

        for (y = 0; y < H; y++) {
            unsigned char *p = (unsigned char *)dst + y * stride;
            if (x0 > 0) memset(p, 0, (unsigned int)(x0 * px));
            if (x1 < W) memset(p + x1 * px, 0, (unsigned int)((W - x1) * px));
        }
        for (y = 0; y < y0; y++)
            memset((unsigned char *)dst + y * stride + x0 * px, 0,
                   (unsigned int)(bandW * px));
        for (y = y1; y < H; y++)
            memset((unsigned char *)dst + y * stride + x0 * px, 0,
                   (unsigned int)(bandW * px));
    }

    g_drvFeCacheA = -1;          /* the source buffer changed under us */
    sy16 = 0;

    for (dy = 0; dy < bandH; dy++, sy16 += dyStep) {
        int          sy = (int)(sy16 >> 16);
        unsigned int wy = (sy16 >> 8) & 0xffu;
        int          slot;

        if (sy > h - 2) { sy = h - 2; wy = 0xffu; }

        //
        // Make sure the cache holds source rows sy and sy+1.  sy only ever
        // advances by 0 or 1, so the common case is to keep the second row and
        // build one new one.
        //
        if (g_drvFeCacheA != sy) {
            int first = 0;
            if (g_drvFeCacheA == sy - 1) {
                /* shift: old slot 1 becomes slot 0 */
                memcpy(g_drvFeCache,
                       g_drvFeCache + bandW * (int)sizeof(unsigned int),
                       (unsigned int)(bandW * (int)sizeof(unsigned int)));
                first = 1;
            }
            for (slot = first; slot < 2; slot++) {
                const unsigned char *s = src + (unsigned int)(sy + slot) * DRV_FE_SRC_W;
                unsigned int *o = (unsigned int *)(g_drvFeCache
                                  + slot * bandW * (int)sizeof(unsigned int));
                int dx;

                if (bpp == 16) {
                    for (dx = 0; dx < bandW; dx++) {
                        unsigned int c  = g_drvFeCol[dx];
                        unsigned int sx = c >> 8;
                        o[dx] = LerpSpread(Spread565(pal[s[sx]] & 0xffffu),
                                           Spread565(pal[s[sx + 1]] & 0xffffu),
                                           (c & 0xffu) >> 3);
                    }
                } else {
                    for (dx = 0; dx < bandW; dx++) {
                        unsigned int c  = g_drvFeCol[dx];
                        unsigned int sx = c >> 8;
                        o[dx] = Lerp888(pal[s[sx]], pal[s[sx + 1]], c & 0xffu);
                    }
                }
            }
            g_drvFeCacheA = sy;
        }

        /* Vertical blend of the two cached rows into the output row. */
        {
            const unsigned int *a = (const unsigned int *)g_drvFeCache;
            const unsigned int *b = (const unsigned int *)(g_drvFeCache
                                    + bandW * (int)sizeof(unsigned int));
            int dx;

            if (bpp == 16) {
                unsigned short *o16 = (unsigned short *)g_drvFeOut;
                unsigned int t = wy >> 3;
                for (dx = 0; dx < bandW; dx++)
                    o16[dx] = (unsigned short)Fold565(LerpSpread(a[dx], b[dx], t));
            } else {
                unsigned int *o32 = (unsigned int *)g_drvFeOut;
                for (dx = 0; dx < bandW; dx++)
                    o32[dx] = Lerp888(a[dx], b[dx], wy);
            }
        }

        memcpy((unsigned char *)dst + (y0 + dy) * stride + x0 * px,
               g_drvFeOut, (unsigned int)(bandW * px));
    }
}

//
// The band: the largest 4:3 box that fits the screen, centred.
//
// Shared by the menu (V26) and the pre-menu screens (V27).  The whole front end
// has to agree on one rectangle or its pieces would not line up with each
// other, so it is computed once here rather than twice.
//
// Returns 0 when the band comes out exactly the source size at the origin,
// which is the signal to leave the game completely alone.
//
static int DrvFeBand(void)
{
    float sx = (float)g_targetW / (float)DRV_FE_SRC_W;
    float sy = (float)g_targetH / (float)DRV_FE_SRC_H;
    float s  = (sx < sy) ? sx : sy;
    int bandW = (int)((float)DRV_FE_SRC_W * s);
    int bandH = (int)((float)DRV_FE_SRC_H * s);
    int x0 = ((int)g_targetW - bandW) / 2;
    int y0 = ((int)g_targetH - bandH) / 2;

    if (bandW <= 0 || bandH <= 0 || x0 < 0 || y0 < 0) return 0;
    if (bandW == DRV_FE_SRC_W && bandH == DRV_FE_SRC_H && x0 == 0 && y0 == 0)
        return 0;

    g_drvFeW  = bandW; g_drvFeH  = bandH;
    g_drvFeX0 = x0;    g_drvFeY0 = y0;
    return 1;
}

static int InstallDrvMenu(void)
{
    /* mov DWORD PTR ds:0x57d9b0, imm32 */
    static const unsigned char head[6] = { 0xc7,0x05, 0xb0,0xd9,0x57,0x00 };
    unsigned char *s16 = (unsigned char *)DRV_FE_SET16;
    unsigned char *s32 = (unsigned char *)DRV_FE_SET32;

    if (!DrvReadable(DRV_FE_SET16, 10) || !DrvReadable(DRV_FE_SET32, 10) ||
        memcmp(s16, head, 6) != 0 || memcmp(s32, head, 6) != 0) {
        return 0;
    }
    if (ReadU32(s16 + 6) != DRV_FE_ORIG16 ||
        ReadU32(s32 + 6) != DRV_FE_ORIG32) {
        return 0;
    }

    //
    // Exactly the source size at the origin means the game's own hand-written
    // assembly already does the right thing, faster.  Decline rather than
    // install a slower copy of it.
    //
    if (!DrvFeBand()) {
        return 0;
    }

    g_drvFeOrig16 = (DrvFeBlit_t)DRV_FE_ORIG16;
    g_drvFeOrig32 = (DrvFeBlit_t)DRV_FE_ORIG32;

    {
        unsigned char rep[4];
        PutU32(rep, (unsigned int)(void *)DrvFrontEndBlit);
        if (!WriteCode(s16 + 6, rep, 4)) return 0;
        if (!WriteCode(s32 + 6, rep, 4)) return 0;
    }

    return 1;
}

//
// V27.  The pre-menu screens -- the copyright / GT / Reflections logos.
//
// A THIRD draw path, and nothing it shares with the menu but the band.
//
// The start-up sequence at 0x412cd0 is:
//
//      clear x4 ; ShowImage("DATA\USCOPYRIGHT.BMP" or "DATA\COPYRIGHT.BMP")
//      clear x4 ; ShowImage("DATA\GT.BMP")
//      clear x4 ; ShowImage("DATA\REFLECT.BMP")
//      front-end init (0x40c180)
//
// ShowImage is 0x40c8f1: it allocates 0x12c000 = 640*480*4, loads the bitmap,
// and fades it in and out through 0x40de21, which tiles the 640x480 image as a
// 3x2 grid of <=256x256 textured quads and draws each through 0x40df13 ->
// grDrawPolygonVertexList.  So unlike the menu this really is Glide geometry,
// in ABSOLUTE PIXEL COORDINATES, laid out against a hardcoded 640x480:
//
//      for (y = 0; y < 480; y += 256)  { h = (y+256 > 480) ? 224 : 256;
//      for (x = 0; x < 640; x += 256)  { w = (x+256 > 640) ? 128 : 256;
//          DrawQuad(x, y, w, h, tile++, colour); } }
//
// THE TRAP, and it is section 7b exactly: `w` and `h` are TWO QUANTITIES.
// 0x40df13 reads them at 0x40df3b and 0x40df56 as the TEXTURE extent (it
// stores w-0.5 and h-0.5, later scaled by 1.0 or 1/64 into the s/t fields),
// and then again at 0x40e00d, 0x40e0b5, 0x40e0ca and 0x40e157 as the SCREEN
// extent, via x+w and y+h.  Scaling the arguments at entry would stretch the
// geometry and the texture coordinates together, which samples off the end of
// the tile.
//
// The two uses are cleanly separated in time, which is what makes this a
// one-site patch: both texture reads happen before 0x40df71, and every read
// after it is screen geometry.  Verified by listing all 30 argument reads in
// the function.  So the hook goes at 0x40df71 -- the first instruction that
// touches a screen coordinate -- and rewrites the four arguments there.
//
// (The vertex array is [ebp-0xf0]: 4 vertices x 60 bytes = 0xf0, which is also
// how the four corner computations were told apart from the texture ones.)
//
// Both edges are mapped and the extent is taken as the DIFFERENCE, rather than
// scaling w and h directly.  That is what keeps the 3x2 grid seamless: the
// right edge of one tile and the left edge of the next round to the same pixel
// by construction.  The map is exact at the boundaries too -- 0 -> band origin
// and 640 -> band origin + band width -- so the image reaches the band edges.
//
// THE CLIP.  0x40c8f1 opens with grClipWindow(0, 0, liveW, liveH), and liveW /
// liveH are 640x480 in the front end, so band-space quads would be clipped
// away at x=640.  The same shape appears at 0x40c7f4 (the clear-and-swap loop
// the sequence above calls between images) and 0x40c846 (the LFB image path).
// All three read the two globals through a disp32, so all six reads are simply
// repointed at our own copies of the REAL screen size.
//
// Repointing rather than rewriting the immediates has a property worth having:
// during a race liveW/liveH already hold the real screen size, so the patch is
// an EXACT NO-OP everywhere except the front end.  And widening 0x40c7f4 is
// what paints the bars -- its clear-to-black now covers the whole screen
// instead of the top-left 640x480, and it swaps between iterations, so both
// buffers get one.
//
// 0x40de21 has ten callers, all in 0x40c97f..0x40ca31 -- the fade loops of the
// two image routines and nothing else.  0x40df13 has one.  So this cannot
// reach anything but a full-screen front-end image.
//

/* The game's own grTexFilterMode pointer, from its GetProcAddress table. */
#define DRV_GR_TEXFILTER 0x005cac4cu
typedef void (__stdcall *DrvTexFilter_t)(int tmu, int minFilter, int magFilter);

#define DRV_LS_QUAD     0x0040df71u   /* fild [ebp+8] ; fstp [ebp-0xf0] */
#define DRV_LS_QUAD_LEN 9

/* The six `mov reg, ds:live{W,H}` reads that feed a full-screen grClipWindow. */
struct DrvLsClip {
    unsigned int  at;       /* the instruction                        */
    unsigned char op0, op1; /* its opcode bytes                       */
    int           opLen;
    int           isHeight;
};

static const struct DrvLsClip drv_ls_clips[6] = {
    { 0x0040c7f8u, 0xa1, 0x00, 1, 1 },   /* the clear-and-swap loop */
    { 0x0040c7feu, 0x8b, 0x0d, 2, 0 },
    { 0x0040c84au, 0xa1, 0x00, 1, 1 },   /* the LFB image path      */
    { 0x0040c850u, 0x8b, 0x0d, 2, 0 },
    { 0x0040c8f7u, 0xa1, 0x00, 1, 1 },   /* the geometry image path */
    { 0x0040c8fdu, 0x8b, 0x0d, 2, 0 }
};

/* Our own copies of the real screen size, for those disp32s to point at. */
static int g_drvScrW = 0;
static int g_drvScrH = 0;

//
// Map one quad from the front end's 640x480 space into the band.
//
// Integer throughout: v is at most 640, so v*bandW tops out around 682,000 and
// the division is exact at both ends.  Called with the game's frame pointer, so
// the arguments are reached at the displacements 0x40df13 itself uses.
//
static void __cdecl DrvLoadQuadFix(unsigned char *frame)
{
    int *px = (int *)(frame + 0x08);
    int *py = (int *)(frame + 0x0c);
    int *pw = (int *)(frame + 0x10);
    int *ph = (int *)(frame + 0x14);
    int x0, y0, x1, y1;

    if (g_drvFeW <= 0 || g_drvFeH <= 0) return;

    //
    // Bilinear, on the first tile of each image.
    //
    // Glide's own default from grSstWinOpen is POINT_SAMPLED (gsst.c:1018),
    // and the only two places Driver sets bilinear -- 0x418790, the in-game
    // renderer init, and 0x42b056, deep in the world code -- run long after
    // the front end.  So these logos were being magnified nearest-neighbour by
    // the HARDWARE, and one state call fixes it for free.
    //
    // Called through the game's own imported pointer rather than by patching a
    // call site, and keyed on the first tile (the tiler always starts at 0,0)
    // rather than done once per process: a mode change re-opens the Glide
    // window and resets the filter, so once would not survive one.
    //
    // The drawer's texture coordinates already run 0.5 .. w-0.5 -- the
    // half-texel inset bilinear wants -- so the 3x2 grid does not seam.
    //
    if (*px == 0 && *py == 0) {
        DrvTexFilter_t f = *(DrvTexFilter_t *)DRV_GR_TEXFILTER;
        if (f) f(0 /* GR_TMU0 */, 1 /* BILINEAR */, 1 /* BILINEAR */);
    }

    x1 = *px + *pw;
    y1 = *py + *ph;

    x0 = g_drvFeX0 + (*px * g_drvFeW) / DRV_FE_SRC_W;
    y0 = g_drvFeY0 + (*py * g_drvFeH) / DRV_FE_SRC_H;
    x1 = g_drvFeX0 + (x1  * g_drvFeW) / DRV_FE_SRC_W;
    y1 = g_drvFeY0 + (y1  * g_drvFeH) / DRV_FE_SRC_H;

    *px = x0;
    *py = y0;
    *pw = x1 - x0;
    *ph = y1 - y0;
}

static int InstallDrvLoad(void)
{
    unsigned char *at = (unsigned char *)DRV_LS_QUAD;
    unsigned char  displaced[DRV_LS_QUAD_LEN];
    unsigned char *stub;
    unsigned char  rep[DRV_LS_QUAD_LEN];
    int i, clips = 0;

    if (!DrvFeBand()) {
        return 0;
    }

    g_drvScrW = (int)g_targetW;
    g_drvScrH = (int)g_targetH;

    /* Two passes over the clip sites: a half-applied set would leave one
       full-screen image clipped and another not, which is harder to read than
       neither being touched. */
    for (i = 0; i < 6; i++) {
        const struct DrvLsClip *c = &drv_ls_clips[i];
        const unsigned char *p = (const unsigned char *)c->at;

        if (!DrvReadable(c->at, (unsigned int)(c->opLen + 4))) return 0;
        if (p[0] != c->op0) return 0;
        if (c->opLen == 2 && p[1] != c->op1) return 0;
        if (ReadU32(p + c->opLen) !=
            (c->isHeight ? DRV_LIVE_H : DRV_LIVE_W)) return 0;
    }
    for (i = 0; i < 6; i++) {
        const struct DrvLsClip *c = &drv_ls_clips[i];
        unsigned char *p = (unsigned char *)c->at;
        unsigned char  d32[4];

        PutU32(d32, (unsigned int)(void *)(c->isHeight ? &g_drvScrH
                                                       : &g_drvScrW));
        if (WriteCode(p + c->opLen, d32, 4)) clips++;
    }

    /* The quad hook. */
    if (!DrvReadable(DRV_LS_QUAD, DRV_LS_QUAD_LEN)) return 0;
    if (at[0] != 0xdb || at[1] != 0x45 || at[2] != 0x08 ||
        at[3] != 0xd9 || at[4] != 0x9d) {
        return 0;
    }
    memcpy(displaced, at, DRV_LS_QUAD_LEN);

    stub = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return 0;

    stub[0] = 0x60;                                   /* pushad            */
    stub[1] = 0x55;                                   /* push ebp          */
    stub[2] = 0xe8;                                   /* call DrvLoadQuadFix */
    PutU32(stub + 3, (unsigned int)(void *)DrvLoadQuadFix
                     - ((unsigned int)stub + 7));
    stub[7] = 0x83; stub[8] = 0xc4; stub[9] = 0x04;   /* add esp,4         */
    stub[10] = 0x61;                                  /* popad             */
    memcpy(stub + 11, displaced, DRV_LS_QUAD_LEN);    /* the displaced work */
    stub[11 + DRV_LS_QUAD_LEN] = 0xc3;                /* ret               */

    rep[0] = 0xe8;
    PutU32(rep + 1, (unsigned int)stub - (DRV_LS_QUAD + 5));
    memset(rep + 5, 0x90, DRV_LS_QUAD_LEN - 5);
    if (!WriteCode(at, rep, DRV_LS_QUAD_LEN)) return 0;

    return 1;
}

//
// V28.  The tape-recorder menu buttons -- round again.
//
// The four selection buttons (rewind / play / forward / accept) were drawn as
// ellipses 1.80x wider than tall, which is exactly (W/640) / (H/480) at
// 1920x800.  Measured off the screenshot: 151 x 84, three of them identical.
//
// It is the same bug V12 fixed for the fill bars, in a different routine.
// 0x506b2a builds a size pair from two constants:
//
//      0x506c0d   fld ds:0x548b0c (22.5) ; fmul [ebp-0xc0] (W/640) ; fmul zoom
//      0x506c27   fld ds:0x5481f8 (45.0) ; fmul [ebp-0xc4] (H/480) ; fmul zoom
//
// 22.5 is a HALF-width and 45.0 a full height, so at uiZoom 1.12 that is
// 2*22.5*3.0*1.12 = 151.2 wide and 45*1.6667*1.12 = 84.0 tall -- both matching
// the measurement to the pixel, which is what identified the site.
//
// One byte: point the width multiply at the height scale, so a 45-unit square
// sprite comes out square.  [ebp-0xc0] -> [ebp-0xc4], i.e. the disp32's low
// byte 0x40 -> 0x3c.  At 4:3 the two scales are equal and this is an EXACT
// NO-OP.
//
// 0x548b0c (22.5) has exactly one reference in the image, so the 18-byte
// signature is unambiguous; verified unique in .text.
//
static const unsigned char drv_btnw_sig[] = {
    0xd9,0x05, 0x0c,0x8b,0x54,0x00,      /* fld  ds:0x548b0c   22.5      */
    0xd8,0x8d, 0x40,0xff,0xff,0xff,      /* fmul [ebp-0xc0]    sx=W/640  */
    0xd8,0x0d, 0xc0,0xe6,0x2d,0x01       /* fmul ds:uiZoom               */
};
#define DRV_BTNW_SLOT 8      /* the disp32's low byte */

static int InstallDrvButton(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at = FindUnique(code, codeSize, drv_btnw_sig,
                                   sizeof(drv_btnw_sig));
    unsigned char  rep[sizeof(drv_btnw_sig)];

    if (!at) {  return 0; }

    memcpy(rep, drv_btnw_sig, sizeof(rep));
    rep[DRV_BTNW_SLOT] = 0x3c;           /* [ebp-0xc0] -> [ebp-0xc4] */
    if (!WriteCode(at, rep, sizeof(rep))) return 0;

    return 1;
}

//
// V29.  The race timer's milliseconds.
//
// The clock reads "01:13" bottom-left with ".82" beside it, and the ".82" sat
// far out to the right.  Measured: the clock's anchor is 640-space x=10 and
// the milliseconds' is x=176, both landing on exact integers through the x3.0
// position map.
//
// THE GAME IS SELF-CONSISTENT AND STILL WRONG, which is the interesting part.
// 0x4fdb44 draws the clock, then measures the string it just drew and places
// the milliseconds one advance-width later:
//
//      0x4fdba4   DrawShadowText(10, 424, "01:13", ...)
//      0x4fdbb2   width = MeasureString(buf, 1)
//      0x4fdbd9   msX   = 10 + (int)width
//      0x4fdc17   DrawShadowText(msX, 426, ".82", ...)
//
// and MeasureString (0x40b3fc) sums glyph advances straight out of the font
// table **with no glyph-scale multiply at all** -- unlike the renderer's own
// centring pass at 0x40aee4, which multiplies each advance by ds:0x54cad4.
// So `width` is in NATIVE 640-space units and `10 + width` is a correct
// 640-space x.
//
// It comes apart at the map: positions go through MapX at W/640 = 3.0 while
// the glyphs, since V8, are drawn at H/480 = 1.6667.  The advance is therefore
// tripled while the digits it is meant to step over only grow by 1.667, and
// the milliseconds land 1.8x too far along.  Exactly the shape of V23.
//
// So the measured advance is multiplied by
//
//      K = (640 * H) / (480 * W)
//
// which is 1.0 at 4:3 -- an EXACT NO-OP there -- and 0.5556 at 1920x800.  The
// milliseconds' anchor then sits one CORRECTLY SCALED advance after the
// clock's: 30 + 166*0.5556*3.0 = 306.7 screen pixels, and 166 * 1.6667 = 276.7
// is the same distance, so it lands exactly where the digits end.
//
// PATCHED AT THE MEASURE, not at either use, because MeasureString has exactly
// TWO callers and both are this timer -- 0x4fdbb2 for the bottom-left layout
// and 0x4fdc6f for the centred one, which does `320 + width/2` and needs the
// same correction on its half-width.  One patch, both layouts, and no other
// code can be affected.
//
// The thunk is redirected rather than the function edited: 0x40b3fc ends
// `fld [ebp-0x4] ; mov esp,ebp ; pop ebp ; ret` in seven bytes with the next
// function immediately after, so there is no room for a six-byte fmul.  Both
// callers reach it through the thunk, so one displacement covers them.
//
#define DRV_MEAS_THUNK 0x00401839u   /* jmp 0x40b3fc */
#define DRV_MEAS_FUNC  0x0040b3fcu

static int InstallDrvTimer(void)
{
    unsigned char *th = (unsigned char *)DRV_MEAS_THUNK;
    unsigned char *stub;
    unsigned char  rep[5];
    float         *k;

    if (!DrvReadable(DRV_MEAS_THUNK, 5)) return 0;
    if (th[0] != 0xe9 ||
        (unsigned int)(DRV_MEAS_THUNK + 5 + (int)ReadU32(th + 1)) != DRV_MEAS_FUNC) {
        return 0;
    }

    k = DrvConst((640.0f * (float)g_targetH) / (480.0f * (float)g_targetW));
    if (!k) return 0;

    stub = (unsigned char *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!stub) return 0;

    //
    // Entered by JMP, so the frame is the original caller's: [esp] is its
    // return address and the two arguments sit above it.  They have to be
    // pushed again for the real call, which is why this is not simply a
    // `call ; fmul ; ret`.
    //
    stub[0] = 0xff; stub[1] = 0x74; stub[2] = 0x24; stub[3] = 0x08; /* push [esp+8]  font */
    stub[4] = 0xff; stub[5] = 0x74; stub[6] = 0x24; stub[7] = 0x08; /* push [esp+8]  str  */
    stub[8] = 0xe8;                                                 /* call 0x40b3fc      */
    PutU32(stub + 9, DRV_MEAS_FUNC - ((unsigned int)stub + 13));
    stub[13] = 0x83; stub[14] = 0xc4; stub[15] = 0x08;              /* add esp,8          */
    stub[16] = 0xd8; stub[17] = 0x0d;                               /* fmul ds:K          */
    PutU32(stub + 18, (unsigned int)k);
    stub[22] = 0xc3;                                                /* ret                */

    rep[0] = 0xe9;
    PutU32(rep + 1, (unsigned int)stub - (DRV_MEAS_THUNK + 5));
    if (!WriteCode(th, rep, 5)) return 0;

    return 1;
}

//
// V30.  The HUD sprite drawer's SIZE -- this is V7, reinstated.
//
// V7 was written on 2026-08-18, applied cleanly at both its sites, produced no
// visible difference across two hardware runs, and was deleted in the cleanup
// under the project's own rule that a patch never observed to do anything is
// scaffolding.  That deletion was wrong, and the reason is worth recording:
// **both of those runs were in-race**, and the screen that exercises this
// drawer is the tape-recorder menu, which nobody had looked at yet.  "Never
// observed to do anything" was really "never yet shown a screen that uses it".
//
// The bug is plain in the code.  0x419340 computes four quantities from the
// same pair of globals that 0x418790 fills with W/640 and H/480:
//
//      0x419495  fld [ebp+0x8]   ; fmul 0x550660 (W/640)   POSITION x  correct
//      0x4194a1  fld [ebp+0xc]   ; fmul 0x550664 (H/480)   POSITION y  correct
//      0x4194ad  fld [ebp-0x11c] ; fmul 0x550660 (W/640)   SIZE w      WRONG
//      0x4194bc  fld [ebp-0x120] ; fmul 0x550664 (H/480)   SIZE h      correct
//
// where [ebp-0x11c] and [ebp-0x120] are the sprite's own width and height
// bytes (sprite[+0xb] and sprite[+0xc], each less 0.5).  Using the width scale
// for the width and the height scale for the height is correct for a POSITION
// and wrong for a SIZE: at 4:3 the two are equal and it cannot be seen, and at
// 21:9 every sprite comes out (W/640)/(H/480) = 1.80x wider than tall.
//
// That is exactly the reported symptom -- the four selection buttons on the
// tape-recorder menu measured 151 x 84, a ratio of 1.80 to three digits.
//
// Two sites, 0x4194b3 and 0x4199d3, in a drawer and its sibling.  Four bytes
// each: repoint the disp32 from the width scale to the height scale.  The
// count of two is the build check, and it is an EXACT NO-OP at 4:3.
//
// The position multiplies are deliberately untouched -- those are right, and
// they are what keeps corner-anchored HUD sprites in the corners.
//
static const unsigned char drv_sprsz_sig[] = {
    0xd9,0x85, 0xe4,0xfe,0xff,0xff,      /* fld  [ebp-0x11c]  sprite width */
    0xd8,0x0d, 0x60,0x06,0x55,0x00       /* fmul ds:0x550660  W/640        */
};
#define DRV_SPRSZ_SLOT  8                /* the disp32 */
#define DRV_SPRSZ_COUNT 2

static int InstallDrvSpriteSize(unsigned char *code, unsigned int codeSize)
{
    unsigned char rep[4];

    PutU32(rep, 0x00550664u);                    /* -> H/480 */

    return DrvPatchAll(code, codeSize, drv_sprsz_sig, sizeof(drv_sprsz_sig),
                       DRV_SPRSZ_COUNT, DRV_SPRSZ_SLOT, rep, sizeof(rep),
                       "sprite size");
}

//
// V31.  The sun flare's falloff radius -- the "severely overbright" effect.
//
// Reported as: driving toward the sun washes the whole screen out.  Measured
// off a matched pair of 1920x800 screenshots with the effect on and off (the
// car parked, so ON minus OFF *is* the additive contribution):
//
//      mean luma  163.7 -> 217.1        pixels at >= 250:  1.0% -> 54.9%
//      delta is +73..76 across the WHOLE screen, peaking at 156 at the sun
//
// So it is one very large additive glow centred on the sun, not a sprite in
// the wrong place -- and the user's own observation pins the mechanism
// exactly: "very subtle at 4:3, a bit more visible at 16:9, and just very
// overbright at 21:9".  That progression is 1.0 / 1.33 / 1.8, which is the
// project's recurring factor (W/640) / (H/480).
//
// The drawer is 0x4b594d (it owns the SUN2 texture handle by way of 0x117a3b0
// and sets additive blending three times).  Its intensity is a plain linear
// falloff in SCREEN PIXELS:
//
//      0x4b5a75   sunOff = camOff * focal * recip[32000]     ; pixels
//      0x4b5ead   d      = sqrt(sunOffX^2 + sunOffY^2)       ; pixels
//      0x4b5996   R      = screenWidth * uiZoom              ; <-- THE BUG
//      0x4b5ef9   I      = (R - min(d,R)) / R * scale
//
// R and d are both in pixels, so this looks self-consistent -- and at 4:3 it
// is.  The projection scale S multiplies d, so
//
//      d / R  =  k * theta * S / W
//
// and at 4:3 S is W/640, which cancels W outright: d/R = k*theta/640,
// identical at 320x240 and at 1600x1200.  That invariance is exactly what
// makes it look right on every mode the game shipped.
//
// V4 breaks it.  Hor+ derives S from the HEIGHT (H/480) so the vertical field
// stays put and the extra width buys world, which means W no longer cancels:
//
//      d / R  =  k * theta / (640 * AR)      AR = (W/640)/(H/480) = 1.8
//
// so d/R comes out 1.8x too small and the flare stays near full intensity far
// off-axis.  A sun that gives I = 0.1 at 4:3 -- "very subtle" -- gives 0.5 at
// 21:9.  It is worse the further off-axis the sun is, which is why the report
// is "severely" overbright rather than a fixed amount, and it is why nothing
// about the sprite's SIZE could have explained it (V28 narrows that sprite and
// was rightly exonerated: an additive quad's peak does not rise when it
// shrinks).
//
// The fix is to give R the same units d is measured in.  Stock says R = W when
// S = W/640, so for any S the intended radius is
//
//      R = 640 * S = 640 * H / virtual_height
//
// which is 4H/3 at the default virtual_height of 480.  That is EXACTLY W at
// every 4:3 mode the game offers -- 640, 800, 1024, 1600 all come out as whole
// numbers equal to their own width -- so this is an exact no-op there, and
// 1067 instead of 1920 at 1920x800.
//
// Four bytes: the `fild` reads a DWORD, so pointing its disp32 at an int32 of
// ours is the whole patch.  Nothing lengthens, nothing moves.  The 27-byte
// signature is unique in the image, and every other use of [ebp-0x48] in the
// routine is the same R in the same falloff, so one edit covers all of them.
//
// NOT patched, and recorded rather than guessed at: 0x4b5f45 computes
// (sunX << 10) / (640 * uiZoom) from the sun's real screen x -- another
// hardcoded 640 where the screen width belongs, feeding 0x40106e.  It is a
// texture or table coordinate of some kind, its element has not been
// identified, and it is not the reported symptom.
//
static const unsigned char drv_flare_sig[] = {
    0xdb,0x05, 0x08,0xff,0x2e,0x01,      /* fild ds:0x12eff08   screen width */
    0xd9,0x9d, 0x30,0xfe,0xff,0xff,      /* fstp [ebp-0x1d0]                 */
    0xd9,0x05, 0xc0,0xe6,0x2d,0x01,      /* fld  ds:0x12de6c0   uiZoom       */
    0xd8,0x8d, 0x30,0xfe,0xff,0xff,      /* fmul [ebp-0x1d0]                 */
    0xd9,0x5d, 0xb8                      /* fstp [ebp-0x48]     R            */
};
#define DRV_FLARE_SLOT 2                 /* the fild's disp32 */

static unsigned int *DrvConstI(unsigned int v)
{
    static unsigned int *page = NULL;
    static int           used = 0;

    if (!page) {
        page = (unsigned int *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE,
                                            PAGE_READWRITE);
        if (!page) return NULL;
    }
    if (used >= 1024) return NULL;
    page[used] = v;
    return &page[used++];
}

static int InstallDrvFlare(unsigned char *code, unsigned int codeSize)
{
    unsigned char *at;
    unsigned char  rep[sizeof(drv_flare_sig)];
    unsigned int  *r;
    unsigned int   radius;

    if (!g_drvVirtualH) return 0;

    /* R = 640 * S, S being the projection scale V4 installs.  Rounded, though
       at every 4:3 mode it is already a whole number equal to the width. */
    radius = (640u * g_targetH + g_drvVirtualH / 2u) / g_drvVirtualH;

    at = FindUnique(code, codeSize, drv_flare_sig, sizeof(drv_flare_sig));
    if (!at) {  return 0; }

    r = DrvConstI(radius);
    if (!r) return 0;

    memcpy(rep, drv_flare_sig, sizeof(rep));
    PutU32(rep + DRV_FLARE_SLOT, (unsigned int)r);
    if (!WriteCode(at, rep, sizeof(rep))) return 0;

    return 1;
}

//
// V6's ceiling -- the only per-frame work Driver needs.
//
// The viewing distance must be re-asserted every frame because LoadConfig
// (0x405618, SIX call sites, two of them on the mode-change path that runs as
// a race starts) puts the CONFIG.DAT value back underneath us.
//
// This is a CORRECTNESS limit, not a preference.  The projection is
// `screenX = camX * focal * recip[z] + W/2`, and `recip` is a TABLE at
// 0xc24520 built at 0x528277 for indices 0..0xdbba-1 -- z up to 56249 and no
// further.  A vertex past that reads off the end of the array, so its
// reciprocal is garbage and the polygon streaks toward a vanishing point.  The
// game's own 45000 maximum is that table's capacity with margin.
//
// Enforced unconditionally, and that is deliberate: an earlier build raised
// the value to 90000 and **the game saved it into CONFIG.DAT**, so the file
// restores 90000 on every load regardless of what we do now.  Reverting the
// code that set it does not revert the save file.
//
static void DrvTick(void)
{
    if (!g_drvActive) return;

    if (DrvReadable(DRV_VIEW_DIST, 4)) {
        float *v = (float *)DRV_VIEW_DIST;
        if (*v > DRV_DRAW_MAX_Z) *v = DRV_DRAW_MAX_Z;
    }

    /* The mirror's distance shares the same table, so it needs the same cap. */
    if (DrvReadable(DRV_MIRR_DIST, 4)) {
        float *v = (float *)DRV_MIRR_DIST;
        if (*v > DRV_DRAW_MAX_Z) *v = DRV_DRAW_MAX_Z;
    }
}

static void DriverApply(const char *exePath, const char *ini, BOOL haveIni)
{
    HMODULE        mod;
    unsigned char *code = NULL, *data = NULL, *proj = NULL;
    unsigned int   codeSize = 0, dataSize = 0;
    unsigned int   mask = DRV_P_ALL;

    if (!PathEndsWith(exePath, "Game.exe") &&
        !PathEndsWith(exePath, "GAME.ICD")) return;

    mod = GetModuleHandleA(NULL);

    // Every address here is absolute.  Game.exe does carry a .reloc, so check
    // that it landed where it was linked rather than assuming it.
    if ((unsigned int)(unsigned long)mod != 0x00400000u) return;

    if (!GetCodeRange(mod, &code, &codeSize)) return;
    if (!GetSectionRange(mod, ".data", &data, &dataSize)) return;

    //
    // NO INI.  Driver has no preferences left: every patch here is either a
    // correctness fix or an exact no-op, and both knobs that used to exist are
    // settled.
    //
    //   virtual_height  fixed at 480 = true Hor+, the full vertical view plus
    //                   extra world at the sides.  It was 0 (AUTO / Vert-) for
    //                   a long time only because widening the view made distant
    //                   geometry vanish at the screen edges -- and that turned
    //                   out to be V21, the 4:3 frustum clip planes, not the
    //                   width of the view.  With V21 in there is no reason to
    //                   ship the narrower picture.
    //
    //   draw_distance   gone, and its installer with it.  It was only ever a
    //                   diagnostic, and worse, it wrote a value the game
    //                   PERSISTS into CONFIG.DAT, so it could outlive the build
    //                   that set it.  The ceiling it needed lives on in DrvTick
    //                   and is enforced unconditionally -- see the comment
    //                   there for why that half is a correctness limit.
    //
    //   patches         one bit per installer, for bisecting on hardware.
    //                   Scaffolding by the project's own rule, and everything
    //                   it could bisect is confirmed.  The bits are kept
    //                   because they cost nothing -- mask is a constant, so the
    //                   compiler folds every test away -- and because a future
    //                   bisect is then one edit rather than a redesign.
    //
    (void)ini; (void)haveIni;
    g_drvVirtualH = 480;

    // The enum has to fit a `push imm8`; every enum in the wide driver's list
    // does, but a sign-extended push would be a silent disaster.
    if (g_targetRes > 0x7f) return;

    // Located BEFORE anything is written: the patches consume their own find
    // patterns, so a later search would come back empty.
    proj = FindUnique(code, codeSize, drv_proj_sig, sizeof(drv_proj_sig));

    if (mask & DRV_P_MODES)  InstallDrvModeRow(data, dataSize);
    if (mask & DRV_P_ENUM)   InstallDrvEnum(code, codeSize);
    if (mask & DRV_P_CONFIG) InstallDrvConfig();
    if (mask & DRV_P_RES2)   InstallDrvRes2(code, codeSize);
    if (mask & DRV_P_PROJ)   InstallDrvProjection(proj);
    if (mask & DRV_P_TEXT)   InstallDrvTextScale(code, codeSize);
    if (mask & DRV_P_BAR)    InstallDrvBarScale(code, codeSize);
    if (mask & DRV_P_SIZES)  InstallDrvSizes(code, codeSize);
    if (mask & DRV_P_FRUSP)  InstallDrvFrusPlane(code, codeSize);
    if (mask & DRV_P_UISCL)  InstallDrvUiScale();
    if (mask & DRV_P_RANCH)  InstallDrvRightAnchor();
    if (mask & DRV_P_NEEDLE) InstallDrvNeedle();
    if (mask & DRV_P_STRIKE) InstallDrvStrikeX();
    if (mask & DRV_P_MENU)   InstallDrvMenu();
    if (mask & DRV_P_LOAD)   InstallDrvLoad();
    if (mask & DRV_P_BTN)    InstallDrvButton(code, codeSize);
    if (mask & DRV_P_TIMER)  InstallDrvTimer();
    if (mask & DRV_P_SPRSZ)  InstallDrvSpriteSize(code, codeSize);
    if (mask & DRV_P_FLARE)  InstallDrvFlare(code, codeSize);

    g_drvActive = TRUE;
}


// ==========================================================================
// Public entry points
// ==========================================================================


int GameFix_SetResolutionEnum(unsigned int glideEnum)
{
    unsigned int i;

    //
    // The driver ignores anything <= 1 (gsst.c:1558 tests `> 1`), which is how
    // "Disabled" is expressed, so we must treat those the same way or we would
    // patch the game for a resolution the driver is not going to set.
    //
    if (glideEnum <= 1) return 0;

    for (i = 0; i < sizeof(g_glideRes) / sizeof(g_glideRes[0]); i++) {
        if (g_glideRes[i].res != glideEnum) continue;

        g_targetW   = g_glideRes[i].w;
        g_targetH   = g_glideRes[i].h;
        g_targetRes = glideEnum;

        Gta2AdoptResolution();
        return 1;
    }
    return 0;
}

void GameFix_Apply(void)
{
    char         exePath[MAX_PATH];
    char         ini[MAX_PATH];
    DWORD        len;
    BOOL         haveIni;

    //
    // Nothing is patched until the target resolution is known.
    //
    // Two reasons, and the second is a real hazard rather than tidiness:
    //
    //  - With the override disabled there is nothing to fix.  The game runs at
    //    a mode it was designed for, so it should be left completely alone
    //    rather than patched with values that merely happen to be inert.
    //
    //  - GameFix_Apply is also called from DLL_PROCESS_ATTACH, which happens
    //    long before glide3x is loaded and the resolution can be read.  Without
    //    this test that early call would bake 640x480-derived constants into
    //    the image AND consume the `find` patterns, so the later, correct call
    //    from grGlideInit would find nothing left to patch and silently leave
    //    the wrong values in place.  Idempotency protects against applying a
    //    patch twice; it cannot protect against applying the wrong one first.
    //
    exePath[0] = '\0';
    len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;

    /* Driver: one line before the guard below, so "ran with no override" and
       "never ran at all" do not both look like an absent log file.  Writes
       nothing to the game. */

    if (!g_targetRes) return;

    haveIni = PathBesideExe(GAMEFIX_INI, ini);

    Gta2Apply(exePath, ini, haveIni);
    IgnitionApply(exePath);
    TurokApply(exePath, ini, haveIni);
    MdkApply(exePath, ini, haveIni);
    DriverApply(exePath, ini, haveIni);
}
