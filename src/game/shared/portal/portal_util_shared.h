//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal trace and transform helpers shared by server, client and the
//			game movement pipeline. This is the API surface the Portal branch
//			hooks left inside the 2013 SDK source depend on.
//
// NOTE: This header is included from util_shared.h when PORTAL is defined, so
//		 it must not include util_shared.h itself, and trace_t must be
//		 referenced via its underlying type (CGameTrace).
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_UTIL_SHARED_H
#define PORTAL_UTIL_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "cmodel.h"			// Ray_t, cplane_t
#include "gametrace.h"		// CGameTrace
#include "mathlib/vmatrix.h"

#ifdef CLIENT_DLL
class C_Prop_Portal;
class C_BaseEntity;
#define PORTAL_ENTITY_TYPE		C_BaseEntity
#define PORTAL_PTR_TYPE			C_Prop_Portal
#else
class CProp_Portal;
class CBaseEntity;
#define PORTAL_ENTITY_TYPE		CBaseEntity
#define PORTAL_PTR_TYPE			CProp_Portal
#endif

class CTraceFilter;
class ITraceFilter;

// Set while a bullet or similar high speed projectile is being traced
extern bool	g_bBulletPortalTrace;

// Main portal trace entry points ---------------------------------------------
// Trace a ray, following it through any open portals it encounters.
void UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, PORTAL_ENTITY_TYPE *pEntIgnore, int collisionGroup, CGameTrace *ptr );

bool UTIL_Portal_TraceRay_Bullets( const PORTAL_PTR_TYPE *pEntrancePortal, const Ray_t &ray, unsigned int mask, const ITraceFilter *pTraceFilter, CGameTrace *ptr );

void UTIL_Portal_TraceEntity( PORTAL_ENTITY_TYPE *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, const ITraceFilter *pTraceFilter, CGameTrace *ptr );

// (shared, both DLLs) Returns true if the beam's path crosses an open portal;
// the extra control point outputs are the transformed start/end points used to
// widen the beam's collision bounds. Mirrors the RelinkBeam hook call.
bool UTIL_Portal_Trace_Beam( PORTAL_ENTITY_TYPE *pEntity, const Vector &vecStart, const Vector &vecEnd,
							Vector &vecAbsExtra1, Vector &vecAbsExtra2, const ITraceFilter *pTraceFilter );

// Find the first portal along a ray; fraction is where the ray crosses it
PORTAL_PTR_TYPE *UTIL_Portal_FirstAlongRay( const Ray_t &ray, float &fFraction );

float UTIL_Portal_DistanceThroughPortalSqr( const PORTAL_PTR_TYPE *pThroughPortal, const Vector &vecSource, const Vector &vecDest );

bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const PORTAL_PTR_TYPE *pPortal );

// Builds an orthonormal frame on a surface from its outward normal. Used for
// portal placement so the opening's right/up axes are deterministic.
void UTIL_Portal_GetSurfaceBasis( const Vector &vecNormal, Vector &vecRight, Vector &vecUp );

// Diagnostic log shared by server and client (writes portal_debug.log).
void Portal_DebugLog( const char *pszFormat, ... );

// Transformation helpers
void UTIL_Portal_PointTransform( const VMatrix &matThisToLinked, const Vector &ptSource, Vector &ptDest );
void UTIL_Portal_VectorTransform( const VMatrix &matThisToLinked, const Vector &vecSource, Vector &vecDest );
void UTIL_Portal_AngleTransform( const VMatrix &matThisToLinked, const QAngle &qSource, QAngle &qDest );

#endif // PORTAL_UTIL_SHARED_H
