//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2state.h: State Handling
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

// Various bits of state we need to know ourselves
struct G2State {

	// The Context
	GrContext_t context;

	// Set true when doing wBuffering, false when doing zBuffering. Default false
	FxBool	wBuffer;

	// The settings for the stw hints. Default GR_STWHINT_ST_DIFF_TMU0
	FxU32	stwHints;

	// This is used to specify which of the STW coords are enabled (uses GR_STWHINT masks)
	FxU32	stwEnabled;

	// The TMU usage mask. Default (GR_TMUMASK_TMU0)
	FxU32	tmuMask;

	// Pointcasted palette handing if the POINTCAST extension isn't supported
	struct {
		GrNCCTable_t	nccSelected;		// Which ncc table has been selected for this TMU
		GuNccTable		nccTable[2];		// the nccTables loaded in this TMU
		GuTexPalette	texPalette;			// The palette for this TMU
	} tmuTables[GLIDE_NUM_TMU];

	// The TMU that the tables belong to. -1 if none or unknown
	int nccTableActive[2];
	int texPaletteActive;

	// Glide Utility Texturing Info
	GrMipMapId_t current_mm[GLIDE_NUM_TMU];

	// Clip Window Dims
	float clipwindowf_xmin;
	float clipwindowf_xmax;
	float clipwindowf_ymin;
	float clipwindowf_ymax;

	inline void Reset() {
		context = 0;
		wBuffer = FXFALSE;
		stwHints = GR_STWHINT_ST_DIFF_TMU0;
		stwEnabled = GR_STWHINT_ST_DIFF_TMU0;
		tmuMask = GR_TMUMASK_TMU0;
		nccTableActive[0] = -1;
		nccTableActive[1] = -1;
		texPaletteActive = -1;
	}
};

extern G2State theState;

struct StatePointer;
struct StateBuffer;

// Our structure to hold all the state details for the state hack
struct StateBuffer {
	StatePointer		*pointer;	// Pointer of the Glide2 state variable
	StateBuffer			*next;		// Pointer to the next in the linked list
	void				*state;		// The Glide3 state
	G2State				state2;		// Our own internal state
};

// This is what we actually give to the app
struct StatePointer {
	FxU32				magic0;		// 'TS2G'
	FxU32				magic1;		// '\0ETA'
	StateBuffer			*buffer;	// Pointer to the buffer that actually contains the state

	inline FxBool		Verify() {
	#define FN_NAME "StatePointer::Verify"

		return magic0 == 'TS2G' && magic1 == '\0ETA';

	#undef FN_NAME
	}

	inline void			Init(StateBuffer *b) {
	#define FN_NAME "StatePointer::Init"

		magic0 = 'TS2G';
		magic1 = '\0ETA';
		buffer = b;

	#undef FN_NAME
	}
};

// Functions
void g2FreeStateBuffers();
void g2SetupStateHack();
StateBuffer *g2CreateStateBuffer(void *pointer);
const StateBuffer *g2GetStateBuffer(const void *pointer);

//
// The following is info that isn't to be touched by restored grSstGetState and grSstSetState
// 
#ifdef MAX_MIPMAPS_PER_SST
#undef MAX_MIPMAPS_PER_SST
#define MAX_MIPMAPS_PER_SST    1024
#endif

struct NonStateInfo {

	// Envionment Settings
	struct Environment_s {
		FxI32	no_multitexture;	// GXP_GLIDE2_NO_MULTITEXTURE
		FxI32	faster_palettes;	// GXP_GLIDE2_FASTER_PALETTES

		void Init();
	} environment;

	// Currently selected SST
	int	current_sst;

	// The number of tmu's that belong to this sst
	FxU32 num_tmus;

	// Glide Utils Texture Managmange
	GrMipMapInfo data[MAX_MIPMAPS_PER_SST];
	GrMipMapId_t free_mmid;

	// Non pointcasted tables
	#if !(ALLOW_POINTCASTS)
		FxU32             next_ncc_table;
		GrMipMapId_t      ncc_mmids[2];
		const GuNccTable *ncc_table[2];
	#endif
	
	// Per TMU Glide Utils Texture Management info
	struct {
		FxU32             freemem_base;
		FxU32             total_mem;

		// Pointcasted tables
		#if ALLOW_POINTCASTS
			FxU32             next_ncc_table;
			GrMipMapId_t      ncc_mmids[2];
			const GuNccTable *ncc_table[2];
		#endif
	} tmu_state[GLIDE_NUM_TMU];

	// Real display height, cached at grSstWinOpen.  Only grSstVRetraceOn uses
	// it, and it is cached rather than queried because the caller is a spin
	// loop -- see the comment there.  Zero means "not known yet".
	FxU32 screen_height;

	// Does the Glide3 underneath us answer GR_VIDEO_POSITION?  Probed once at
	// grSstWinOpen, because it is optional: a stock glide3x declines it and
	// grSstVRetraceOn must then fall back rather than wait for a beam nobody
	// is reporting.
	FxBool have_video_position;

	// Reset the nsinfo. Can only be called AFTER grGlideInit
	inline void Reset() {
	#define FN_NAME "NonStateInfo::Reset"

		// Reset basic things
		current_sst = 0;
		num_tmus = 1;
		screen_height = 0;
		have_video_position = FXFALSE;

		// This will reset all of the Glide Utils Texture Management stuff
		guTexMemReset();
	#undef FN_NAME
	}

};

extern NonStateInfo nsInfo;

