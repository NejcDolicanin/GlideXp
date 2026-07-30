//
// Glide 2 to Glide 3 Translator for GlideXP
//
// g2glide.cpp : Glide 2 Initializtion Functions
//
// Copyright (c) 2002 Ryan Nunn
//
// Just a note that this is written in C++ so I can use namespaces.
// No other C++ features are used.
//

#include "g2pch.h"

// The linked list
static StateBuffer *allStates = NULL;
static FxI32 glideStateSize = 0;

//
// g2GetStateBuffer
//
// Gets the StateBuffer for a pointer
//
const StateBuffer *g2GetStateBuffer(const void *pointer)
{
#define FN_NAME "g2GetStateBuffer"
	GDBG_INFO(80, "%s:  0x%x\n", FN_NAME, pointer);

	if (pointer == NULL) return NULL;
	StatePointer *state_p = (StatePointer *) pointer;

	// Verify the buffer, and return it if it's ok
	if (state_p->Verify()) return state_p->buffer;

	return NULL;
#undef FN_NAME
}

//
// g2CreateStateBuffer
//
// Creates a new/returns a StateBuffer for the pointer
//
StateBuffer *g2CreateStateBuffer(void *pointer)
{
#define FN_NAME "g2CreateStateBuffer"
	GDBG_INFO(80, "%s:  0x%x\n", FN_NAME, pointer);
	
	if (pointer == NULL) return NULL;
	StatePointer *state_p = (StatePointer *) pointer;
	StateBuffer *state = NULL;
	
	// Verify the buffer
	if (state_p->Verify()) {
		// Ok, we managed to verify it.
		state = state_p->buffer;

	} else {

		// Couldn't verify, create a new one
		state = new StateBuffer;
		if (!state) return NULL;
		
		// Just push onto the start
		state->pointer = state_p;
		state->next = allStates;
		state->state = new FxU8[glideStateSize];
		allStates = state;

		// Init the pointer
		state_p->Init(state);
	}
	
	GDBG_INFO(110, FN_NAME":  State pointer %x\n", state);
	
	// Return the state
	return state;
	
#undef FN_NAME
}

//
// g2FreeStateBuffers
//
// This frees the Linked List
//
void g2FreeStateBuffers()
{
#define FN_NAME "g2FreeStateBuffers"
    GDBG_INFO(80, "%s\n", FN_NAME);
	
	StateBuffer *state, *next;
	
	for (state = allStates; state != NULL; state = next)
	{
		next = state->next;
		delete [] state->state;
		delete state;
	}
	
	allStates = NULL;
	
#undef FN_NAME
}

//
// g2SetupStateHack
//
// This frees the Linked List
//
void g2SetupStateHack()
{
#define FN_NAME "g2SetupStateHack"
    GDBG_INFO(80, "%s\n", FN_NAME);
	
	Glide3::grGet(GR_GLIDE_STATE_SIZE, 4, &glideStateSize);
	
    GDBG_INFO(80, "%s: Glide3 State size: %i bytes\n", FN_NAME, glideStateSize);
	
#undef FN_NAME
} /* g2FreeStatePointers */


//
// NonStateInfo::Environment_s::Init()
//
// Sets up the environment settings
//
void NonStateInfo::Environment_s::Init()
{
#define FN_NAME "NonStateInfo::Environment_s::init"
    GDBG_INFO(80, "%s\n", FN_NAME);

	// GXP_GLIDE2_NO_MULTITEXTURE (0 = Multitexture Enabled, 1 = Multitexture Disabled)
	char *str = Glide3::grGetRegistryOrEnvironmentStringExt("GXP_GLIDE2_NO_MULTITEXTURE");
	if (str) no_multitexture = atoi(str);
	else no_multitexture = 0;
	
	if (no_multitexture) {
		GDBG_INFO(80, "%s: Glide2 Multitexture disabled by registry\n", FN_NAME);
	}
	
	// GXP_GLIDE2_FASTER_PALETTES (0 = Compatible Mode, 1 = Faster Mode)
	str = Glide3::grGetRegistryOrEnvironmentStringExt("GXP_GLIDE2_FASTER_PALETTES");
	if (str) faster_palettes = atoi(str);
	else faster_palettes = 0;
	
	if (faster_palettes) {
		GDBG_INFO(80, "%s: Enabling Faster Glide 2 Palette handling\n", FN_NAME);
	}
	
#undef FN_NAME
}

