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

/* The static 8bpp overlay buffer, and how many times .text names it. */
#define IGN_FB_ADDR      0x547CA0u
#define IGN_FB_SITES     64

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

/* The per-view focal lengths 0x43C8A0 hardcodes, in 640x400 pixels. */
#define IGN_FOCAL_X      425.0
#define IGN_FOCAL_Y      348.0

/* The double 240.0 already sitting in .rdata -- 320.0 * IGN_PAR. */
#define IGN_K240         0x46A568u

//
// F1.  The 8bpp overlay buffer is a fixed-size static array.
//
// Ignition's 2D layer -- HUD, text, menus, the loading screen -- is
// software-rendered into an 8bpp buffer and colour-key blitted into the Glide
// LFB through a 16-bit palette (the blit is at 0x458780; note its
// `or %al,%al ; je skip`, which is what makes it an overlay rather than a
// background).  That buffer is not allocated: it is the static array at
// 0x547CA0, and the engine memsets W*H bytes of it every time the screen size
// changes.  640x400 is 256,000 bytes; the next referenced global sits at
// 0x5BEEE0, so there is room for 487,488 and no more.  2560x1080 wants
// 2,764,800.
//
// So the buffer has to move before the screen size may be raised, and if it
// cannot move then nothing else may be applied either -- a larger W*H over the
// old array would memset straight through the rest of .data.
//
// Moving it is mechanical and, unusually for this kind of edit, provably safe.
// The four bytes A0 7C 54 00 occur exactly 64 times in .text, and
// disassembling the whole section shows all 64 are genuine instruction
// operands: immediates of push/mov/add/sub, and disp32s of forms like
// `mov 0x547ca0(%esi,%ecx,1),%dl`.  Not one is a mid-instruction coincidence,
// and the value appears in no other section.  A flat scan-and-replace is
// therefore exactly equivalent to rewriting the 64 operands one at a time, and
// the count doubles as the build check -- any other build fails it and the
// game runs unpatched.
//
static unsigned char *g_ignFrameBuffer = NULL;

static BOOL InstallIgnFrameBuffer(unsigned char *code, unsigned int codeSize)
{
    unsigned char find[4], repl[4];
    unsigned int  i, count, need;

    if (g_ignFrameBuffer) return TRUE;          // already relocated

    PutU32(find, IGN_FB_ADDR);

    count = 0;
    for (i = 0; i + 4 <= codeSize; ) {
        if (memcmp(code + i, find, 4) == 0) { count++; i += 4; }
        else                                          i += 1;
    }
    if (count != IGN_FB_SITES) return FALSE;    // not the build this describes

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
static const unsigned char ign_size_main[] =        /* 0x43C8AB */
    "\xbf\x80\x02\x00\x00\xbd\x01\x00\x00\x00"
    "\x89\x3d\x38\xeb\x48\x00\xc7\x05\x3c\xeb"
    "\x48\x00\x90\x01\x00\x00";
#define IGN_SIZE_MAIN_W_AT   1
#define IGN_SIZE_MAIN_H_AT  22

static const unsigned char ign_size_global[] =      /* 0x43C907 */
    "\x89\x3d\xec\x6e\x53\x00\xc7\x05\x18\x7b"
    "\x54\x00\x90\x01\x00\x00";
#define IGN_SIZE_GLOBAL_H_AT 12

static const unsigned char ign_size_clip[] =        /* 0x412510 */
    "\xc7\x05\x38\xeb\x48\x00\x80\x02\x00\x00"
    "\xc7\x05\x3c\xeb\x48\x00\x90\x01\x00\x00"
    "\xc7\x05\x40\xeb\x48\x00\x08\x00\x00\x00"
    "\xc7\x05\x44\xeb\x48\x00\x01\x00\x00\x00"
    "\xe8\x43\x1e\x04\x00";
#define IGN_SIZE_CLIP_W_AT   6
#define IGN_SIZE_CLIP_H_AT  16

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
    IgnPatchSize(code, codeSize, ign_size_main,   sizeof(ign_size_main)   - 1,
                 IGN_SIZE_MAIN_W_AT,   IGN_SIZE_MAIN_H_AT);
    IgnPatchSize(code, codeSize, ign_size_global, sizeof(ign_size_global) - 1,
                 -1,                   IGN_SIZE_GLOBAL_H_AT);
    IgnPatchSize(code, codeSize, ign_size_clip,   sizeof(ign_size_clip)   - 1,
                 IGN_SIZE_CLIP_W_AT,   IGN_SIZE_CLIP_H_AT);
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
static const unsigned char ign_focal[] =            /* 0x43C91B */
    "\xbf\xa9\x01\x00\x00\xba\x00\x00\x24\x40"
    "\x8b\x2d\xe4\xee\x5b\x00\x41\x89\x7c\x28"
    "\x58\x8b\x2d\xe4\xee\x5b\x00\xc7\x44\x28"
    "\x5c\x5c\x01\x00\x00";
#define IGN_FOCAL_X_AT    1
#define IGN_FOCAL_Y_AT   31

static void InstallIgnFocal(unsigned char *code, unsigned int codeSize)
{
    unsigned char  repl[sizeof(ign_focal)];
    unsigned char *at;
    double         scale;
    unsigned int   fx, fy;

    at = FindUnique(code, codeSize, ign_focal, sizeof(ign_focal) - 1);
    if (!at) return;

    scale = (double)g_targetH / IGN_NATIVE_H;
    fy    = (unsigned int)(IGN_FOCAL_Y * scale + 0.5);
    fx    = (unsigned int)(IGN_FOCAL_X * scale * IGN_PAR + 0.5);

    memcpy(repl, ign_focal, sizeof(ign_focal) - 1);
    PutU32(repl + IGN_FOCAL_X_AT, fx);
    PutU32(repl + IGN_FOCAL_Y_AT, fy);

    WriteCode(at, repl, sizeof(ign_focal) - 1);
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
static const unsigned char ign_sprite_scale[] =     /* 0x43CAC3 */
    "\xdd\x44\x24\x10\xdc\x0d\x58\xa8\x46\x00"
    "\xa3\x30\x6e\x53\x00\xdc\x35\x50\xa8\x46"
    "\x00\xe8\xb7\x1a\x02\x00";
#define IGN_SPRITE_DISP_AT    3     /* fldl 0x10(%esp) -> 0x18(%esp) */
#define IGN_SPRITE_DIV_AT    17     /* fdivl 320.0     -> 240.0      */

static const unsigned char ign_menu_focal[] =       /* 0x44AB43 */
    "\xd9\x44\x24\x00\xdc\x35\x50\xa8\x46\x00"
    "\xdd\x5c\x24\x04\xdd\x44\x24\x04\xdc\x0d"
    "\xf8\xa8\x46\x00";
#define IGN_MENU_DISP_AT      3     /* flds 0x0(%esp)  -> 0x10(%esp) */
#define IGN_MENU_DIV_AT       6     /* fdivl 320.0     -> 240.0      */

static void InstallIgnAspect(unsigned char *code, unsigned int codeSize)
{
    unsigned char  repl[sizeof(ign_sprite_scale)];
    unsigned char *at;

    at = FindUnique(code, codeSize, ign_sprite_scale,
                    sizeof(ign_sprite_scale) - 1);
    if (at) {
        memcpy(repl, ign_sprite_scale, sizeof(ign_sprite_scale) - 1);
        repl[IGN_SPRITE_DISP_AT] = 0x18;            /* the height slot */
        PutU32(repl + IGN_SPRITE_DIV_AT, IGN_K240);
        WriteCode(at, repl, sizeof(ign_sprite_scale) - 1);
    }

    at = FindUnique(code, codeSize, ign_menu_focal,
                    sizeof(ign_menu_focal) - 1);
    if (at) {
        memcpy(repl, ign_menu_focal, sizeof(ign_menu_focal) - 1);
        repl[IGN_MENU_DISP_AT] = 0x10;              /* the height slot */
        PutU32(repl + IGN_MENU_DIV_AT, IGN_K240);
        WriteCode(at, repl, sizeof(ign_menu_focal) - 1);
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
static const unsigned char ign_wipe[] =             /* 0x436C10 */
    "\x55\x8b\xec\x83\xe4\xf8\x83\xec\x24\x8b"
    "\x0d\xec\x6e\x53\x00\xb8";

static void InstallIgnWipe(unsigned char *code, unsigned int codeSize)
{
    unsigned char  repl[sizeof(ign_wipe)];
    unsigned char *at;

    at = FindUnique(code, codeSize, ign_wipe, sizeof(ign_wipe) - 1);
    if (!at) return;                    // already applied, or not this build

    memcpy(repl, ign_wipe, sizeof(ign_wipe) - 1);
    repl[0] = 0xC3;                     // ret

    WriteCode(at, repl, sizeof(ign_wipe) - 1);
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
#define IGN_RVA_VT_BLIT     0x0F3348u   /* the device blit vtable slot   */
#define IGN_RVA_VT_LOCK     0x0F3358u   /* Lock(surface, type)           */
#define IGN_RVA_VT_UNLOCK   0x0F335Cu   /* Unlock(surface)               */
#define IGN_RVA_SCREEN_A    0x0F2F40u   /* the two screen surfaces       */
#define IGN_RVA_SCREEN_B    0x0F2E50u
#define IGN_RVA_PALETTE     0x21E520u   /* 256 x u16, index -> 16bpp     */

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
    int                   screenW = (int)g_targetW;
    int                   screenH = (int)g_targetH;

    // Unreachable -- the hook is written only after g_ignImage is set -- but
    // the alternative to checking is a wild read at 0x000F3348.
    if (!g_ignImage) return 0;

    orig   = *(IgnBlitFn *)  (g_ignImage + IGN_RVA_VT_BLIT);
    lock   = *(IgnLockFn *)  (g_ignImage + IGN_RVA_VT_LOCK);
    unlock = *(IgnUnlockFn *)(g_ignImage + IGN_RVA_VT_UNLOCK);

    //
    // Not ours: hand it straight back.  Note this also covers the case where
    // the vtable is not populated yet, since `orig` is then NULL and the game
    // would have crashed on the original instruction too.
    //
    if (!dst || !orig || w <= 0 || h <= 0 ||
        ((unsigned int)dst != g_ignImage + IGN_RVA_SCREEN_A &&
         (unsigned int)dst != g_ignImage + IGN_RVA_SCREEN_B) ||
        (w >= screenW && h >= screenH))
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
        pal    = (const unsigned short *)(g_ignImage + IGN_RVA_PALETTE);
        srcTop = src + (unsigned int)pitch * (unsigned int)sy + sx;

        for (y = 0; y < screenH; y++) {
            unsigned short      *row = (unsigned short *)(base + stride * (unsigned int)y);
            const unsigned char *srow;
            int                  acc;

            if (y < oy || y >= oy + outH) {          /* top / bottom bar */
                for (x = 0; x < screenW; x++) row[x] = 0;
                continue;
            }

            srow = srcTop + (unsigned int)pitch
                          * (unsigned int)((((y - oy) * stepY) >> 16));

            for (x = 0; x < ox; x++) row[x] = 0;     /* left bar */

            acc = 0;
            for (x = 0; x < outW; x++) {
                unsigned char b = srow[acc >> 16];
                row[ox + x] = b ? pal[b] : 0;
                acc += stepX;
            }

            for (x = ox + outW; x < screenW; x++) row[x] = 0;   /* right bar */
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
static const unsigned char ign_present[] =          /* 0x454336 */
    "\x8b\x54\x24\x1c\x50\x51\x52\xff\x15\x48"
    "\x33\x4f\x00\x83\xc4\x24\xc3";
#define IGN_PRESENT_CALL_AT   7     /* offset of the `ff 15` within it */

static void InstallIgnPresent(unsigned char *code, unsigned int codeSize,
                              unsigned int imageBase)
{
    unsigned char  repl[sizeof(ign_present)];
    unsigned char *at;
    int            rel;

    at = FindUnique(code, codeSize, ign_present, sizeof(ign_present) - 1);
    if (!at) return;                    // already applied, or not this build

    g_ignImage = imageBase;

    memcpy(repl, ign_present, sizeof(ign_present) - 1);

    rel = (int)((unsigned char *)&IgnPresentScaled
                - (at + IGN_PRESENT_CALL_AT + 5));

    repl[IGN_PRESENT_CALL_AT] = 0xE8;
    PutU32(repl + IGN_PRESENT_CALL_AT + 1, (unsigned int)rel);
    repl[IGN_PRESENT_CALL_AT + 5] = 0x90;       // pad the sixth byte

    WriteCode(at, repl, sizeof(ign_present) - 1);
}

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
    unsigned int   codeSize = 0;

    if (!PathEndsWith(exePath, IGN_EXE)) return;

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    if (!InstallIgnFrameBuffer(code, codeSize)) return;

    InstallIgnScreenSize(code, codeSize);
    InstallIgnFocal(code, codeSize);
    InstallIgnAspect(code, codeSize);
    InstallIgnWipe(code, codeSize);
    InstallIgnPresent(code, codeSize, (unsigned int)mod);

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
    if (!g_targetRes) return;

    exePath[0] = '\0';
    len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;

    haveIni = PathBesideExe(GAMEFIX_INI, ini);

    Gta2Apply(exePath, ini, haveIni);
    IgnitionApply(exePath);
    TurokApply(exePath, ini, haveIni);
}