//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side portal entity (object hierarchy). This mirrors the
//			#CProp_Portal defined in prop_portal_shared.h for the server.
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_PROP_PORTAL_H
#define C_PROP_PORTAL_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "prop_portal_shared.h"

#ifdef PORTAL
//this file is reachable from shared code that needs g_pPortalRender (beams, sprites)
#include "PortalRender.h"
#endif

// Number of nested views the stencil-based portal renderer supports.
// Also used by CGlowOverlay (see glow_overlay.h) for the sky obstruction
// scale backup slots.
// MAX_PORTAL_RECURSIVE_VIEWS is defined in portal_shareddefs.h (shared with the
// server and the renderer); glow_overlay.h includes this file to get it.
#define MAX_PORTAL_RECURSIVE_VIEWS_HL2			1		// max recursive views in hl2/water refraction

class C_Prop_Portal : public CProp_Portal_Shared
{
public:
	DECLARE_CLASS( C_Prop_Portal, CProp_Portal_Shared );
	DECLARE_CLIENTCLASS();

	C_Prop_Portal( void );
	virtual ~C_Prop_Portal( void );

// Allow the renderer to read it's data
	friend class CPortalRenderable;
	friend class CPortalRenderer;

public:
	// Portal identity (networked)
	int						m_iPortalType;

	// During portal rendering, the portal does not need to be drawn
	bool m_bDrawInPortalRender;

	// Only render the portal if the local player can see it, "IsDormant" is used by the network
	bool ShouldDraw( void );

	// Rebuilds the portal plane and the pair transform after a network update.
	virtual void OnDataChanged( DataUpdateType_t updateType );
};

// Search for, and return, the first portal that the beam ray crosses.
// fEndFraction receives the fraction where the beam hits the portal (or 1.0).
C_Prop_Portal *UTIL_Portal_TraceRay_Beam( const Ray_t &rayBeam, unsigned int mask, ITraceFilter *pTraceFilter, float *fEndFraction );

#endif // C_PROP_PORTAL_H
