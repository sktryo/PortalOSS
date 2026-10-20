//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared definitions for the Portal game mode
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_SHAREDDEFS_H
#define PORTAL_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif

#define PORTAL_VERSION			"0.1"		// portal build version

// Portal subnetworking ids
#define PORTAL_SCREENSHOT		1

// The portal surfaces are tested against skin thickness
#define PORTAL_SKIN_LENGTH		2.0f			// how far the portal shell extends out of the wall

// Portal dimensions (world units)
#define PORTAL_HALF_WIDTH		24.0
#define PORTAL_HALF_HEIGHT		32.0
#define PORTAL_HALF_WIDTH_MIN	16.0			// minimum dimension to even consider a placement
#define PORTAL_HALF_HEIGHT_MIN	16.0

// Max number of nested portal views the stencil renderer supports
#define MAX_PORTAL_RECURSIVE_VIEWS		4

// Defaults
#define PORTAL_PLACEMENT_TRACE_DISTANCE		30.0f	// how far behind a surface trace can reach to find the wall face
#define PORTAL_PUSHLIMIT					115.0f	// max distance an object will push through a portal
#define MAX_PORTAL_EXTENSIONS_PER_UPDATE	5

// Valid portal group colors
#define PORTAL_CAN_USE_ENVIRONMENT 1
#define PORTAL_COLOR_ORANGE		"255 160 0"
#define PORTAL_COLOR_BLUE		"0 160 255"

enum PortalType
{
// NOTE: There are 2 levels of type setting before use 
		// this must be EXACTLY as it is used. BOTH must be set
		// EVERY TIME A PROP_PORTAL IS USED!
	PORTAL_TYPE_NONE		= -2,
	PORTAL_TYPE_FIZZLE		= -1,
	PORTAL_TYPE_BLUE		= 0,
	PORTAL_TYPE_ORANGE		= 1,
};

// Reasons a fizzle can occur
enum PortalFizzleType_t
{
	PORTAL_FIZZLE_INVALID	= -1,
	PORTAL_FIZZLE_FIZZLER	= 0,		// portal crossed a cleanser / fizzle field
	PORTAL_FIZZLE_KILLED	= 1,		// portal was killed by an entity
	PORTAL_FIZZLE_GPU_WALL	= 2,		// portal was killed by a change in the surface
	PORTAL_FIZZLE_LAUNCH	= 3,		// portal was killed from touching gib-able geometry
	PORTAL_FIZZLE_CONVERSIONS = 4,		// portal was drowned
};

#define PORTAL_FIZZLE_SOUND_STATE				"Portal.Fizzle"

// Per portal Data
// NOTE: DO NOT CHANGE THE ORDER OF THE STATE DATA || THE FIELDS.  SAVE/LOAD DEPENDS ON IT.
#define PORTAL_MAX_FIELDS		12

#endif // PORTAL_SHAREDDEFS_H
