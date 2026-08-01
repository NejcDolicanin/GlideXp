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

/* Little-endian store, so the byte tables above stay readable as x86. */
static void PutU32(unsigned char *at, unsigned int v)
{
    at[0] = (unsigned char)(v      );
    at[1] = (unsigned char)(v >>  8);
    at[2] = (unsigned char)(v >> 16);
    at[3] = (unsigned char)(v >> 24);
}

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

        /* Bake the chosen mode into the DMAGlide replacement bytes. */
        PutU32(gta2_mode_replace + MODE_H_AT,  g_targetH);
        PutU32(gta2_mode_replace + MODE_W_AT,  g_targetW);
        PutU32(gta2_enum_replace + ENUM_W_AT,  g_targetW);
        PutU32(gta2_enum_replace + ENUM_H_AT,  g_targetH);
        PutU32(gta2_enum_replace + ENUM_RES_AT, g_targetRes);
        return 1;
    }
    return 0;
}

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

void GameFix_Apply(void)
{
    char         exePath[MAX_PATH];
    char         ini[MAX_PATH];
    DWORD        len;
    unsigned int i, vh = GTA2_VHEIGHT_DEFAULT;
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
    if (haveIni)
        vh = ReadVirtualHeight(ini);

    for (i = 0; i < g_profileCount; i++) {
        if (!PathEndsWith(exePath, g_profiles[i].exeName)) continue;

        ApplyProfile(&g_profiles[i]);
    }

    //
    // The camera hook is not a BytePatch: its replacement contains a rel32 to
    // memory allocated at runtime, so it cannot live in a static find/replace
    // table.  The manager process never runs this code.
    //
    if (haveIni &&
        (PathEndsWith(exePath, "gta2.exe") || PathEndsWith(exePath, "gta2.icd")) &&
        GetPrivateProfileIntA("GTA2", "aspect_fix", 1, ini))
    {
        InstallCameraHook(
            (int)GetPrivateProfileIntA("GTA2", "extra_zoom", 0, ini), vh);
    }

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
    if (PathEndsWith(exePath, "gta2.exe") || PathEndsWith(exePath, "gta2.icd")) {
        InstallHudHook(vh);
        InstallPopupScale();
        ApplyMsgSites(vh);
    }
}
