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

// Target resolution.  The DMAGlide bytes below encode it literally, so these
// two must agree with them.
#define GTA2_TARGET_W 2560
#define GTA2_TARGET_H 1080


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
static const unsigned char gta2_mode_replace[] = {
    0x68, 0x38, 0x04, 0x00, 0x00,   // push $0x438   (1080)
    0x68, 0x00, 0x0a, 0x00, 0x00,   // push $0xa00   (2560)
    0x56,
    0xe8
};

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
static const unsigned char gta2_enum_replace[] = {
    0x81, 0xf9, 0x00, 0x0a, 0x00, 0x00,        // cmp  $0xa00,%ecx    (2560)
    0x75, 0x13,
    0x81, 0xfe, 0x38, 0x04, 0x00, 0x00,        // cmp  $0x438,%esi    (1080)
    0x75, 0x0b,
    0xc7, 0x40, 0x3c, 0x26, 0x00, 0x00, 0x00   // movl $0x26  (2560x1080)
};

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
#define HUD_MAX_ENTRIES 16

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

        { 0x005ced73, 1 },
        { 0x005cefe2, 1 },
        { 0x005cf02b, 1 },

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

static void InstallHudHook(int mode, unsigned int virtualHeight)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0, i;
    int            hudWidth, delta, offSpread, offCentre;

    if (mode <= 0) return;
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
// This is known to work: an earlier build rewrote all 15 at once and the ESC
// quit prompt and the area-name text both came out correctly centred.  The
// reason it was reverted is that the same sweep also hit whatever the
// world-anchored crash popup uses, dragging it off position, and left the
// area-name frame behind so frame and text disagreed.
//
// So the sweep was right in kind and wrong in scope.  Each site is now
// selectable by bit, so a single element can be fixed without touching the
// rest, and a site that misbehaves can be dropped without losing the others.
//
//   bit  0  0x5ca685 |
//   bit  1  0x5ca6e5 |  (640-w)/2 then draw -- a family of three centred-text
//   bit  2  0x5ca747 |  routines reading the width global 0x700768
//   bit  3  0x5caad6    clamp: x > 640 -> 640
//   bit  4  0x5caadd    640 - w
//   bit  5  0x5cade3    640 - w
//   bit  6  0x5cb088    push $640
//   bit  7  0x5cb0f0    640 - w
//   bit  8  0x5ce4c6 |
//   bit  9  0x5ce624 |  (640-w)/2 -- a second family of three, working from a
//   bit 10  0x5ce714 |  rect at esi+0x44/+0x48 (frame-like)
//   bit 11  0x5cea62    (640-w)/2
//   bit 12  0x5cf4d1    (640-w)/2
//   bit 13  0x5cf578    640-a-b   right-anchored
//   bit 14  0x5cf5ce    (640-w)/2
//
// Every signature below is unique image-wide, and the 32-bit immediate is at
// offset 1 in all of them (they are all `mov reg,imm32` / `push imm32` /
// `cmp eax,imm32`).
//
//
// Two kinds of constant appear here:
//
//   WIDTH  -- a 640 used as the layout width, e.g. `(640-w)/2` or `640-w`.
//             Becomes the virtual width that fills the screen: W*vh/H = 1137.
//   CENTRE -- a 320 pushed directly as a centre point.  Becomes half of that,
//             568, which lands at screen x 1278 against a true centre of 1280.
//
// The mission title is the CENTRE case and is why it resisted every earlier
// probe: it is not laid out from a width at all, so none of the 640 bits could
// ever have moved it.  Measuring it off a screenshot (virtual centre 319,
// y 114..165) matched `push $0x140 / push $0x88` = (320, 136) exactly.
//
#define MSG_WIDTH  0
#define MSG_CENTRE 1

struct MsgSite {
    const char   *note;
    unsigned char kind;
    unsigned char sigLen;
    unsigned char sig[18];
};

static const MsgSite g_msgSites[] = {
 { "0x5ca685", MSG_WIDTH, 18, {0xb8,0x80,0x02,0x00,0x00,0x51,0x2b,0xc5,0x8b,0xcc,0x99,0x2b,0xc2,0xd1,0xf8,0x50,0xe8,0xf6} },
 { "0x5ca6e5", MSG_WIDTH, 18, {0xb8,0x80,0x02,0x00,0x00,0x51,0x2b,0xc5,0x8b,0xcc,0x99,0x2b,0xc2,0xd1,0xf8,0x50,0xe8,0x96} },
 { "0x5ca747", MSG_WIDTH, 18, {0xb8,0x80,0x02,0x00,0x00,0x51,0x2b,0xc5,0x8b,0xcc,0x99,0x2b,0xc2,0xd1,0xf8,0x50,0xe8,0x34} },
 { "0x5caad6", MSG_WIDTH, 5, {0x3d,0x80,0x02,0x00,0x00} },
 { "0x5caadd", MSG_WIDTH, 5, {0xbe,0x80,0x02,0x00,0x00} },
 { "0x5cade3", MSG_WIDTH, 6, {0xb9,0x80,0x02,0x00,0x00,0x2b} },
 { "0x5cb088", MSG_WIDTH, 6, {0x68,0x80,0x02,0x00,0x00,0x66} },
 { "0x5cb0f0", MSG_WIDTH, 5, {0xba,0x80,0x02,0x00,0x00} },
 { "0x5ce4c6", MSG_WIDTH, 18, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc2,0x51,0x99,0x2b,0xc2,0x8b,0xcc,0xd1,0xf8,0x50,0xe8,0xb5} },
 { "0x5ce624", MSG_WIDTH, 18, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc2,0x51,0x99,0x2b,0xc2,0x8b,0xcc,0xd1,0xf8,0x50,0xe8,0x57} },
 { "0x5ce714", MSG_WIDTH, 18, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc2,0x51,0x99,0x2b,0xc2,0x8b,0xcc,0xd1,0xf8,0x50,0xe8,0x67} },
 { "0x5cea62", MSG_WIDTH, 7, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc1} },
 { "0x5cf4d1", MSG_WIDTH, 8, {0xb8,0x80,0x02,0x00,0x00,0x51,0x2b,0xc3} },
 { "0x5cf578", MSG_WIDTH, 6, {0xb8,0x80,0x02,0x00,0x00,0x83} },
 { "0x5cf5ce", MSG_WIDTH, 7, {0xb8,0x80,0x02,0x00,0x00,0x2b,0xc7} },

 /* bit 15 -- mission title ("TUTORIAL!").  A CENTRE point, not a width, which
    is why none of the 640 bits could ever have moved it:
        5cf426  push $0x140   x = 320   <- this
        5cf430  push $0x88    y = 136
    Measured off a screenshot the title spans virtual x 199..438, centre 319,
    at y 114..165 -- (320,136) exactly. */
 { "0x5cf426", MSG_CENTRE, 13,
   {0x68,0x40,0x01,0x00,0x00, 0xe8,0x60,0x62,0xe6,0xff, 0x68,0x88,0x00} },
};

#define MSG_IMM_AT 1        /* the imm32 sits at offset 1 in every signature */

static void ApplyMsgSites(unsigned int mask, unsigned int virtualHeight)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0, i;
    int            vw;

    if (!mask) return;
    if (virtualHeight < 120) virtualHeight = 120;

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    // The virtual width that maps onto the whole screen at the current scale.
    vw = (int)(((long long)GTA2_TARGET_W * virtualHeight) / GTA2_TARGET_H);

    for (i = 0; i < sizeof(g_msgSites) / sizeof(g_msgSites[0]); i++) {
        unsigned char *at;

        if (!(mask & (1u << i))) continue;

        int val;

        at = FindUnique(code, codeSize, g_msgSites[i].sig, g_msgSites[i].sigLen);
        if (!at) continue;

        val = (g_msgSites[i].kind == MSG_CENTRE) ? (vw / 2) : vw;
        WriteCode(at + MSG_IMM_AT, (const unsigned char *)&val, 4);
    }
}

// ---------------------------------------------------------------------------
// 6. DIAGNOSTIC -- TEMPORARY, remove once the caller map is complete
// ---------------------------------------------------------------------------
//
// Every element is identified by which call site feeds the transform, so what
// is needed is a complete caller -> x map.  Guessing at individual callers has
// now cost two hardware runs, so this produces the whole map in one.
//
// The earlier version of this failed for a mundane reason worth recording: it
// recorded every call, and the HUD emits ~1200 records per frame, so the buffer
// filled long before any transient element appeared.  Deduplicating by caller
// fixes that completely -- each distinct call site contributes a handful of
// records, so a full map fits easily and rare events are never crowded out.
//
// Hooked at `mov 0x66fce0,%ebx` in each transform's prologue: no stack side
// effects, so the stub runs the displaced instruction verbatim.  Both functions
// begin `push %ebx`, so the caller's return address is at a known slot.
//
#define DIAG_MAX_CALLERS 96
#define DIAG_PER_CALLER   3

struct DiagEnt {
    DWORD site, caller, x, y, hits;
};

static DiagEnt      g_diagEnt[DIAG_MAX_CALLERS];
static unsigned int g_diagEnts = 0;
//
// Key-triggered capture: INSERT starts, DELETE stops and writes.
//
// A frame countdown from launch was always the wrong control for this.  The
// elements worth catching are transient -- a mission title, a quit prompt --
// and the window had to cover loading, reaching the trigger, AND the element
// being on screen.  Two captures were lost that way and read as "this element
// does not use these paths", which was never what they showed.  Triggering by
// hand removes the guesswork entirely.
//
// g_diagArmed is set by diag=1; recording only runs between the keypresses, so
// the table holds exactly the frames of interest.  Pressing INSERT again starts
// a fresh capture, so several can be taken in one session.
//
// GetAsyncKeyState is polled once per frame from grBufferSwap.  It reads
// hardware state rather than the message queue, so it works even though the
// game owns input.  It lives in user32 and is ANSI-only -- no *W variant to
// worry about on Win9x.
//
static unsigned int g_diagFrames = 0, g_diagGoal = 0;
static BOOL         g_diagDone  = FALSE;
static BOOL         g_diagArmed = FALSE;   // diag=1 in the ini
static BOOL         g_diagRec   = FALSE;   // between INSERT and DELETE

static void DiagFlush(void);

//
// regs: [0]=eflags then pushad order in memory: edi,esi,ebp,esp,ebx,edx,ecx,eax
//
static void __cdecl DiagLog(unsigned int site, const DWORD *regs)
{
    const DWORD *stk;
    DWORD caller, x, y;
    unsigned int i;

    if (!g_diagRec) return;                /* only between INSERT and DELETE */

    stk = (const DWORD *)regs[4];          /* esp as the hooked site saw it */
    if (IsBadReadPtr(stk, 48)) return;

    //
    // Argument slots differ per copy of the transform: 0x5d0640 reads X from
    // 0x20(%esp) (arg3) while 0x4932c0 and 0x5d06f0 read it from 0x18/0x1c
    // (arg1).  Getting this wrong made site 2's first capture look like
    // nonsense (y = 5312), so it is spelled out rather than assumed.
    //
    caller = stk[2];                       /* past our retaddr + saved ebx  */
    if (site == 1) { x = stk[5]; y = stk[6]; }
    else           { x = stk[4]; y = stk[5]; }

    for (i = 0; i < g_diagEnts; i++) {
        if (g_diagEnt[i].site == site && g_diagEnt[i].caller == caller) {
            if (g_diagEnt[i].hits < DIAG_PER_CALLER) {
                g_diagEnt[i].x = x;        /* keep the most recent sample   */
                g_diagEnt[i].y = y;
            }
            g_diagEnt[i].hits++;
            return;                        /* already known -- do not flood */
        }
    }
    if (g_diagEnts >= DIAG_MAX_CALLERS) return;
    g_diagEnt[g_diagEnts].site   = site;
    g_diagEnt[g_diagEnts].caller = caller;
    g_diagEnt[g_diagEnts].x      = x;
    g_diagEnt[g_diagEnts].y      = y;
    g_diagEnt[g_diagEnts].hits   = 1;
    g_diagEnts++;
}

static void DiagFlush(void)
{
    char   path[MAX_PATH], line[256];
    HANDLE f;
    DWORD  done;
    unsigned int i;

    /* Written once per DELETE press; a repeat capture overwrites the file. */
    g_diagDone = TRUE;

    if (!PathBesideExe("wideDriver_diag.txt", path)) return;
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;

    i = (unsigned int)wsprintfA(line, "callers=%lu\r\nsite caller x y hits\r\n",
                                (unsigned long)g_diagEnts);
    WriteFile(f, line, i, &done, NULL);

    for (i = 0; i < g_diagEnts; i++) {
        int n = wsprintfA(line, "%lu %08lx %08lx %08lx %lu\r\n",
                          (unsigned long)g_diagEnt[i].site,
                          (unsigned long)g_diagEnt[i].caller,
                          (unsigned long)g_diagEnt[i].x,
                          (unsigned long)g_diagEnt[i].y,
                          (unsigned long)g_diagEnt[i].hits);
        WriteFile(f, line, n, &done, NULL);
    }
    CloseHandle(f);
}

static const unsigned char diag_sig0[] = {          /* 0x4932c1 */
    0x8b,0x1d,0xe0,0xfc,0x66,0x00, 0x55,0x56, 0x8b,0x83,0xa8,0x00,0x00,0x00,
    0x57,0x99, 0x8b,0xf0, 0x8b,0x44,0x24,0x1c, 0x8b,0xfa,0x99,0x57,0x56,0x52,
    0x50,0xe8,0xfd };
static const unsigned char diag_sig1[] = {          /* 0x5d0641 */
    0x8b,0x1d,0xe0,0xfc,0x66,0x00, 0x55,0x56, 0x8b,0x83,0xa8,0x00,0x00,0x00,
    0x57,0x99, 0x8b,0xf0, 0x8b,0x44,0x24,0x20 };
//
// The transform is duplicated FOUR times.  Only the first two were ever in the
// dedup diagnostic; these two were probed once, during ordinary gameplay with
// no mission title on screen, and read as silent.  That is not evidence -- it
// is the same "wrong moment" error that lost two earlier captures.
//
static const unsigned char diag_sig2[] = {          /* 0x5d06f1 */
    0x8b,0x1d,0xe0,0xfc,0x66,0x00, 0x55,0x56, 0x8b,0x83,0xa8,0x00,0x00,0x00,
    0x57,0x99, 0x8b,0xf0, 0x8b,0x44,0x24,0x1c, 0x8b,0xfa,0x99,0x57,0x56,0x52,
    0x50,0xe8,0xcd };
static const unsigned char diag_sig3[] = {          /* 0x5d0772 */
    0x8b,0x1d,0xe0,0xfc,0x66,0x00, 0xc7 };

static unsigned char diag_stub_tmpl[] = {
    0x60,                       /* pushad                       */
    0x9c,                       /* pushfd                       */
    0x54,                       /* push %esp                    */
    0x6a, 0x00,                 /* push $site      (imm8 at 4)  */
    0xe8, 0,0,0,0,              /* call DiagLog    (rel32 at 6) */
    0x83, 0xc4, 0x08,           /* add  $8,%esp                 */
    0x9d,                       /* popfd                        */
    0x61,                       /* popad                        */
    0x8b, 0x1d, 0xe0, 0xfc, 0x66, 0x00,   /* displaced, verbatim */
    0xc3                        /* ret                          */
};
#define DIAG_ID_AT  4
#define DIAG_REL_AT 6

void GameFix_DiagTick(void)
{
    static BOOL insPrev = FALSE, delPrev = FALSE;
    BOOL ins, del;

    if (!g_diagArmed) return;

    ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    del = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;

    /* Rising edges only, so holding a key does not retrigger. */
    if (ins && !insPrev) {
        g_diagEnts  = 0;               /* fresh capture */
        g_diagFrames = 0;
        g_diagDone  = FALSE;
        g_diagRec   = TRUE;
    } else if (del && !delPrev) {
        if (g_diagRec) {
            g_diagRec = FALSE;
            DiagFlush();
        }
    } else if (g_diagRec && g_diagGoal && ++g_diagFrames >= g_diagGoal) {
        //
        // Safety net.  If the keys turn out not to reach us -- the game owns
        // input and this is a 1999 title on Win9x -- a capture would otherwise
        // run forever and never produce a file, which is the failure mode that
        // has already cost two runs.  diag_frames=0 disables this.
        //
        g_diagRec = FALSE;
        DiagFlush();
    }

    insPrev = ins;
    delPrev = del;
}

static void InstallDiag(unsigned int on, unsigned int frames)
{
    HMODULE        mod;
    unsigned char *code = NULL;
    unsigned int   codeSize = 0, i;

    if (!on) return;
    g_diagArmed = TRUE;
    g_diagGoal  = frames;          /* 0 = no safety flush, keys only */

    mod = GetModuleHandleA(NULL);
    if (!mod) return;
    if (!GetCodeRange(mod, &code, &codeSize)) return;

    for (i = 0; i < 4; i++) {
        static const unsigned char *const sigs[4] =
            { diag_sig0, diag_sig1, diag_sig2, diag_sig3 };
        static const unsigned int         lens[4] =
            { sizeof(diag_sig0), sizeof(diag_sig1),
              sizeof(diag_sig2), sizeof(diag_sig3) };
        const unsigned char *sig = sigs[i];
        unsigned int         len = lens[i];
        unsigned char       *at, *stub, call[6];
        int                  rel;

        at = FindUnique(code, codeSize, sig, len);
        if (!at) continue;

        stub = (unsigned char *)VirtualAlloc(NULL, sizeof(diag_stub_tmpl),
                                             MEM_COMMIT | MEM_RESERVE,
                                             PAGE_EXECUTE_READWRITE);
        if (!stub) continue;

        memcpy(stub, diag_stub_tmpl, sizeof(diag_stub_tmpl));
        stub[DIAG_ID_AT] = (unsigned char)i;
        rel = (int)((unsigned char *)&DiagLog - (stub + DIAG_REL_AT + 4));
        memcpy(stub + DIAG_REL_AT, &rel, 4);

        rel = (int)(stub - (at + 5));
        call[0] = 0xe8;
        memcpy(call + 1, &rel, 4);
        call[5] = 0x90;

        if (!WriteCode(at, call, 6))
            VirtualFree(stub, 0, MEM_RELEASE);
    }
}

void GameFix_Apply(void)
{
    char         exePath[MAX_PATH];
    char         ini[MAX_PATH];
    DWORD        len;
    unsigned int i, vh = GTA2_VHEIGHT_DEFAULT;
    BOOL         haveIni;

    exePath[0] = '\0';
    len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;

    haveIni = PathBesideExe(GAMEFIX_INI, ini);
    if (haveIni)
        vh = ReadVirtualHeight(ini);

    for (i = 0; i < g_profileCount; i++) {
        if (PathEndsWith(exePath, g_profiles[i].exeName))
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
    // HUD horizontal placement.  Same reason it is not a BytePatch: the stub
    // holds a runtime-allocated caller table.
    //
    if (haveIni &&
        (PathEndsWith(exePath, "gta2.exe") || PathEndsWith(exePath, "gta2.icd")))
    {
        InstallHudHook((int)GetPrivateProfileIntA("GTA2", "hud_mode", 0, ini),
                       vh);

        // Message text: one bit per layout site, see section 5.
        ApplyMsgSites(
            (unsigned int)GetPrivateProfileIntA("GTA2", "msg_sites", 0, ini), vh);

        // TEMPORARY caller map -- see section 6.  Off unless diag=1.
        InstallDiag(
            (unsigned int)GetPrivateProfileIntA("GTA2", "diag",        0,   ini),
            (unsigned int)GetPrivateProfileIntA("GTA2", "diag_frames", 600, ini));
    }
}
