//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal math helpers shared between the portal implementations.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_INTERNAL_H
#define PORTAL_INTERNAL_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "portal_util_shared.h"
#include "prop_portal_shared.h"

// Portal axis helper: fills the portal's right/up/normal axes derived from the
// corner set (BL, BR, TR, TL) plus the plane origin.
void Portal_GetAxes( const CProp_Portal_Shared *pPortal, Vector &vecRight, Vector &vecUp, Vector &vecNormal );

// Localize a world position in portal space (x = along right, y = along up,
// z = along the wall normal, positive in front of the wall)
void Portal_WorldToLocal( const CProp_Portal_Shared *pPortal, const Vector &vecWorld, Vector &vecLocal );

bool Portal_PointIsInPortalViewport( const CProp_Portal_Shared *pPortal, const Vector &vecLocal, float flExtraExtent = 0.0f );

// Does the segment start->end cross the portal's plane front->back through the
// portal opening? Returns the crossing fraction (0..1) when it does.
bool Portal_SegmentPassesThroughPortal( const CProp_Portal_Shared *pPortal, const Vector &start, const Vector &end, float &flFraction, Vector *pVecCrossPoint );

// Given a hit on the portal plane (usually a trace result), is this a pass-through location?
bool Portal_TraceHitIsInsidePortal( const CProp_Portal_Shared *pPortal, const CGameTrace &tr, const Vector &vecFrom, const Vector &vecTo, float flExtraExtent = 0.0f );

#endif // PORTAL_INTERNAL_H
