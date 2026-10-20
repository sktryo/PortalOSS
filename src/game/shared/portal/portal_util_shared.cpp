//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal trace and transform implementation (shared). Compiled into
//			both the server and client DLLs; implements the portal-aware
//			trace API the leftover #ifdef PORTAL hooks depend on.
//
// Design note: portal holes are simulated in software (no BSP holes are
// carved). A ray is traced normally; whenever its plane-crossing point lands
// inside an open, linked portal's opening, the remainder of the ray is
// rigidly transformed into the linked portal's space (using the pair's
// ThisToLinked matrix, which flips the right and normal axes so entering a
// portal exits out of the front of its linked partner) and traced again.
// Continuation fractions are mapped back into the original ray's parameter
// space so callers can still use trace.fraction directly.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_internal.h"
#include "tier1/utlvector.h"
#include <fstream>

#ifdef CLIENT_DLL
#include "c_prop_portal.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool g_bBulletPortalTrace = false;

//-----------------------------------------------------------------------------
// Purpose: Diagnostic log for portal placement / lifecycle. -condebug does not
//			work in this mod, so append to a plain file next to the launcher.
//-----------------------------------------------------------------------------
void Portal_DebugLog( const char *pszFormat, ... )
{
	char szBuffer[1024];
	va_list args;
	va_start( args, pszFormat );
	Q_vsnprintf( szBuffer, sizeof( szBuffer ), pszFormat, args );
	va_end( args );

	Msg( "%s\n", szBuffer );

	std::ofstream log( "portal_debug.log", std::ios::app );
	if ( log.is_open() )
	{
		log << szBuffer << "\n";
	}
}

static const int		PORTAL_TRACE_MAX_RECURSION = 6;
static const float		PORTAL_TRACE_EPSILON = 0.25f;

//-----------------------------------------------------------------------------
// Portal axes/status helpers
//-----------------------------------------------------------------------------
void Portal_GetAxes( const CProp_Portal_Shared *pPortal, Vector &vecRight, Vector &vecUp, Vector &vecNormal )
{
	vecRight = pPortal->m_vPortalCorners[1] - pPortal->m_vPortalCorners[0];
	vecUp = pPortal->m_vPortalCorners[3] - pPortal->m_vPortalCorners[0];
	vecRight.NormalizeInPlace();
	vecUp.NormalizeInPlace();
	vecNormal = pPortal->m_plane_Origin.normal;
}

void Portal_WorldToLocal( const CProp_Portal_Shared *pPortal, const Vector &vecWorld, Vector &vecLocal )
{
	Vector vecRight, vecUp, vecNormal;
	Portal_GetAxes( pPortal, vecRight, vecUp, vecNormal );
	Vector vecOffset = vecWorld - pPortal->GetAbsOrigin();
	vecLocal.x = DotProduct( vecOffset, vecRight );
	vecLocal.y = DotProduct( vecOffset, vecUp );
	vecLocal.z = DotProduct( vecOffset, vecNormal );
}

bool Portal_PointIsInPortalViewport( const CProp_Portal_Shared *pPortal, const Vector &vecLocal, float flExtraExtent )
{
	Vector vecRight, vecUp, vecNormal;
	Portal_GetAxes( pPortal, vecRight, vecUp, vecNormal );
	float flHalfWidth = ( pPortal->m_vPortalCorners[1] - pPortal->m_vPortalCorners[0] ).Length() * 0.5f;
	float flHalfHeight = ( pPortal->m_vPortalCorners[3] - pPortal->m_vPortalCorners[0] ).Length() * 0.5f;
	return ( vecLocal.x >= -flHalfWidth - flExtraExtent && vecLocal.x <= flHalfWidth + flExtraExtent &&
			 vecLocal.y >= -flHalfHeight - flExtraExtent && vecLocal.y <= flHalfHeight + flExtraExtent );
}

bool Portal_SegmentPassesThroughPortal( const CProp_Portal_Shared *pPortal, const Vector &start, const Vector &end, float &flFraction, Vector *pVecCrossPoint )
{
	if ( !pPortal || !pPortal->IsActivatedAndLinked() )
		return false;

	const cplane_t &plane = pPortal->m_plane_Origin;
	float flDotStart = DotProduct( start, plane.normal ) - plane.dist;
	float flDotEnd = DotProduct( end, plane.normal ) - plane.dist;
	if ( flDotStart * flDotEnd > 0.0f )
		return false;

	// Only pass through in the front -> back direction (into the wall)
	if ( flDotStart <= 0.0f )
		return false;

	float flSpan = flDotStart - flDotEnd;
	if ( flSpan <= 0.0f )
		return false;

	float flCross = flDotStart / flSpan;
	Vector vecCross;
	VectorLerp( start, end, flCross, vecCross );

	Vector vecLocal;
	Portal_WorldToLocal( pPortal, vecCross, vecLocal );
	if ( !Portal_PointIsInPortalViewport( pPortal, vecLocal, 2.0f ) )
		return false;

	flFraction = flCross;
	if ( pVecCrossPoint )
		*pVecCrossPoint = vecCross;
	return true;
}

bool Portal_TraceHitIsInsidePortal( const CProp_Portal_Shared *pPortal, const CGameTrace &tr, const Vector &vecFrom, const Vector &vecTo, float flExtraExtent )
{
	// The trace result lands on the wall behind the portal opening if the ray
	// entered the portal, or on the portal plane itself for a zero-thickness
	// hit. Portals are placed PORTAL_SKIN_LENGTH in front of their wall, so
	// accept hits up to that distance behind the plane. Swept entities also
	// stop with their center that much further back, hence flExtraExtent.
	float flFraction = 0.0f;
	Vector vecCross;
	if ( !Portal_SegmentPassesThroughPortal( pPortal, vecFrom, vecTo, flFraction, &vecCross ) )
		return false;

	const float flMaxDistance = PORTAL_SKIN_LENGTH + PORTAL_TRACE_EPSILON + flExtraExtent;

	return ( tr.fraction >= flFraction - 0.001f ) &&
		( tr.endpos.DistToSqr( vecCross ) < flMaxDistance * flMaxDistance );
}

//-----------------------------------------------------------------------------
// Core recursive portal-crossing trace. Traces the ray, then recurses through
// any portal it enters. Results are merged back into the original ray's
// parameter space. pWentThroughPortal (optional) is set true when the ray
// entered an open portal during this call.
//-----------------------------------------------------------------------------
static void PortalTraceRayInternal( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter,
									CGameTrace *ptr, int nRecursion, bool *pWentThroughPortal )
{
	if ( pWentThroughPortal )
		*pWentThroughPortal = false;

	enginetrace->TraceRay( ray, fMask, pTraceFilter, ptr );

	if ( nRecursion >= PORTAL_TRACE_MAX_RECURSION )
		return;

	float fPortalFraction = 1.0f;
	PORTAL_PTR_TYPE *pEnterPortal = UTIL_Portal_FirstAlongRay( ray, fPortalFraction );
	if ( pEnterPortal == NULL || fPortalFraction >= 1.0f )
		return;

	// If the ordinary trace was stopped before reaching the portal plane
	// (ie. something else was hit first) and not on the world right at the
	// portal, keep that result. Only world hits landing on the portal plane
	// itself are replaced by the continuation.
	bool bBlockedBeforePortal = ( ptr->fraction < fPortalFraction - 0.001f );
	if ( bBlockedBeforePortal && ( ptr->DidHit() == false || ptr->DidHitNonWorldEntity() ) )
		return;

	// A world hit that is not on the portal opening blocks the ray normally.
	// This keeps bullets and movement from leaking through walls that happen
	// to sit in front of the portal plane.
	if ( ptr->DidHit() && ptr->DidHitWorld() &&
		 !Portal_TraceHitIsInsidePortal( pEnterPortal, *ptr, ray.m_Start, ray.m_Start + ray.m_Delta ) )
	{
		return;
	}

	if ( pWentThroughPortal )
		*pWentThroughPortal = true;

	const VMatrix &matThisToLinked = pEnterPortal->MatrixThisToLinked();

	// Continue from the point where the ray crosses the portal plane, not
	// from the transformed ray start. The transformed start is behind the
	// linked portal's wall and would make the continuation begin in solid.
	Vector vecCross = ray.m_Start + ray.m_Delta * fPortalFraction;
	Vector vTransformedCross, vTransformedEnd;
	UTIL_Portal_PointTransform( matThisToLinked, vecCross, vTransformedCross );
	UTIL_Portal_PointTransform( matThisToLinked, ray.m_Start + ray.m_Delta, vTransformedEnd );

	Vector vecEnterDir = vTransformedEnd - vTransformedCross;
	float flEnterDirLen = vecEnterDir.Length();
	if ( flEnterDirLen < 0.01f )
		return;
	vecEnterDir /= flEnterDirLen;

	// Nudge the continuation start out of the linked portal's wall.
	Vector vecContinueStart = vTransformedCross + vecEnterDir * PORTAL_TRACE_EPSILON;

	Ray_t rayContinue;
	rayContinue.Init( vecContinueStart, vTransformedEnd );

	CGameTrace trContinue;
	PortalTraceRayInternal( rayContinue, fMask, pTraceFilter, &trContinue, nRecursion + 1, NULL );

	// Convert the continuation trace's fraction into the ORIGINAL ray's
	// parameter space. Rigid transforms preserve length, and the continuation
	// covers whatever part of the original ray lies beyond the portal plane
	// (minus the epsilon offset applied to its start).
	float flOriginalLen = ray.m_Delta.Length();
	float flRemainder = flOriginalLen * ( 1.0f - fPortalFraction ) - PORTAL_TRACE_EPSILON;
	if ( flRemainder <= 0.0f )
		return;

	if ( trContinue.fraction >= 1.0f )
	{
		// The ray made it through the portal into open space. Keep the
		// original trace's endpoint; only the hit path is transformed.
		*ptr = trContinue;
		ptr->fraction = 1.0f;
		ptr->endpos = ray.m_Start + ray.m_Delta;
		return;
	}

	// Something was hit on the far side of the portal.
	*ptr = trContinue;
	ptr->fraction = fPortalFraction + trContinue.fraction * ( flRemainder / flOriginalLen );
}

void UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, PORTAL_ENTITY_TYPE *pEntIgnore, int collisionGroup, CGameTrace *ptr )
{
	CTraceFilterSimple traceFilter( pEntIgnore, collisionGroup );
	PortalTraceRayInternal( ray, fMask, &traceFilter, ptr, 0, NULL );
}

bool UTIL_Portal_TraceRay_Bullets( const PORTAL_PTR_TYPE *pEntrancePortal, const Ray_t &ray, unsigned int mask, const ITraceFilter *pTraceFilter, CGameTrace *ptr )
{
	// Returns true when the bullet actually traveled through pEntrancePortal.
	// tr is always filled with a valid trace: either the continuation of the
	// ray beyond the portal or a plain trace (if the bullet never made it).
	g_bBulletPortalTrace = true;

	bool bResult = false;

	float fPortalFraction = 1.0f;
	PORTAL_PTR_TYPE *pFirstPortal = UTIL_Portal_FirstAlongRay( ray, fPortalFraction );
	if ( pFirstPortal != NULL && pFirstPortal == pEntrancePortal && fPortalFraction < 1.0f )
	{
		bool bWentThrough = false;
		PortalTraceRayInternal( ray, mask, const_cast<ITraceFilter *>( pTraceFilter ), ptr, 0, &bWentThrough );
		bResult = bWentThrough;
	}
	else
	{
		enginetrace->TraceRay( ray, mask, const_cast<ITraceFilter *>( pTraceFilter ), ptr );
	}

	g_bBulletPortalTrace = false;
	return bResult;
}

void UTIL_Portal_TraceEntity( PORTAL_ENTITY_TYPE *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, unsigned int mask, const ITraceFilter *pTraceFilter, CGameTrace *ptr )
{
	ICollideable *pCollision = pEntity->GetCollideable();
	if ( pCollision == NULL )
	{
		Ray_t ray;
		ray.Init( vecAbsStart, vecAbsEnd );
		enginetrace->TraceRay( ray, mask, const_cast<ITraceFilter *>( pTraceFilter ), ptr );
		return;
	}

	// Sweep the collideable along the straight line first.
	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, pCollision->GetCollisionAngles(),
									mask, const_cast<ITraceFilter *>( pTraceFilter ), ptr );

	// Check for a portal crossing in the way.
	Ray_t rayPath;
	rayPath.Init( vecAbsStart, vecAbsEnd );
	float fPortalFraction = 1.0f;
	PORTAL_PTR_TYPE *pEnterPortal = UTIL_Portal_FirstAlongRay( rayPath, fPortalFraction );
	if ( pEnterPortal == NULL || fPortalFraction >= 1.0f )
		return;

	// Did the sweep stop before the portal? World hits exactly on the
	// portal's wall face are treated as entering the portal; any other world
	// hit blocks the movement. A swept box stops with its center further from
	// the plane than a ray, so include the box extent along the movement.
	float flExtraExtent = 0.0f;
	Vector vecMove = vecAbsEnd - vecAbsStart;
	if ( vecMove.LengthSqr() > 0.0001f )
	{
		vecMove.NormalizeInPlace();
		Vector vecHalfExtents = ( pCollision->OBBMaxs() - pCollision->OBBMins() ) * 0.5f;
		flExtraExtent = fabs( vecHalfExtents.x * vecMove.x ) +
						fabs( vecHalfExtents.y * vecMove.y ) +
						fabs( vecHalfExtents.z * vecMove.z );
	}

	if ( ptr->DidHit() )
	{
		if ( ptr->DidHitNonWorldEntity() )
		{
			if ( ptr->fraction < fPortalFraction - 0.001f )
				return;
		}
		else if ( !Portal_TraceHitIsInsidePortal( pEnterPortal, *ptr, vecAbsStart, vecAbsEnd, flExtraExtent ) )
		{
			return;
		}
	}

	const VMatrix &matThisToLinked = pEnterPortal->MatrixThisToLinked();

	Vector vTransformedStart, vTransformedEnd;
	UTIL_Portal_PointTransform( matThisToLinked, vecAbsStart, vTransformedStart );
	UTIL_Portal_PointTransform( matThisToLinked, vecAbsEnd, vTransformedEnd );

	Vector vecEnterDir = vTransformedEnd - vTransformedStart;
	float flEnterDirLen = vecEnterDir.Length();
	if ( flEnterDirLen < 0.01f )
		return;
	vecEnterDir /= flEnterDirLen;

	// Start the continuation at the transformed portal-plane crossing point,
	// nudged out of the linked portal's wall.
	Vector vContinueStart = vTransformedStart + vecEnterDir * ( flEnterDirLen * fPortalFraction + PORTAL_TRACE_EPSILON );
	Vector vContinueEnd = vTransformedEnd;

	// rotate the collideable's angles through the portal
	QAngle newAngles = pCollision->GetCollisionAngles();
	UTIL_Portal_AngleTransform( matThisToLinked, newAngles, newAngles );

	enginetrace->SweepCollideable( pCollision, vContinueStart, vContinueEnd, newAngles,
									mask, const_cast<ITraceFilter *>( pTraceFilter ), ptr );

	// map the continuation fraction back into the original parameter space
	float flOriginalLen = ( vecAbsEnd - vecAbsStart ).Length();
	float flRemainder = flOriginalLen * ( 1.0f - fPortalFraction ) - PORTAL_TRACE_EPSILON;
	if ( ptr->fraction >= 1.0f )
	{
		ptr->fraction = 1.0f;
		ptr->endpos = vecAbsEnd;
	}
	else if ( flRemainder > 0.0f )
	{
		ptr->fraction = fPortalFraction + ptr->fraction * ( flRemainder / flOriginalLen );
	}
	else
	{
		ptr->fraction = fPortalFraction;
	}
}

//-----------------------------------------------------------------------------
// Portal-following traces for beams. The extra points are the beam endpoints
// transformed into the linked portal's space (used to widen collision bounds).
//-----------------------------------------------------------------------------
bool UTIL_Portal_Trace_Beam( PORTAL_ENTITY_TYPE *pEntity, const Vector &vecStart, const Vector &vecEnd,
							Vector &vecAbsExtra1, Vector &vecAbsExtra2, const ITraceFilter *pTraceFilter )
{
	( void )pEntity;

	Ray_t rayBeam;
	rayBeam.Init( vecStart, vecEnd );

	float fPortalFraction = 1.0f;
	PORTAL_PTR_TYPE *pPortal = UTIL_Portal_FirstAlongRay( rayBeam, fPortalFraction );
	if ( pPortal == NULL || fPortalFraction >= 1.0f )
		return false;

	// check the beam isn't interrupted before it gets to the portal
	trace_t tr;
	enginetrace->TraceRay( rayBeam, MASK_SHOT, const_cast<ITraceFilter *>( pTraceFilter ), &tr );
	if ( tr.DidHit() )
	{
		if ( tr.DidHitNonWorldEntity() )
		{
			if ( tr.fraction < fPortalFraction - 0.001f )
				return false;
		}
		else if ( !Portal_TraceHitIsInsidePortal( pPortal, tr, vecStart, vecEnd ) )
		{
			return false;
		}
	}

	// The beam goes through the portal: the extra control points are the beam
	// endpoints transformed into the linked portal's space.
	const VMatrix &matThisToLinked = pPortal->MatrixThisToLinked();
	UTIL_Portal_PointTransform( matThisToLinked, vecStart, vecAbsExtra1 );
	UTIL_Portal_PointTransform( matThisToLinked, vecEnd, vecAbsExtra2 );
	return true;
}

//-----------------------------------------------------------------------------
// Search for the first portal along a ray
//-----------------------------------------------------------------------------
PORTAL_PTR_TYPE *UTIL_Portal_FirstAlongRay( const Ray_t &ray, float &fFraction )
{
	fFraction = 1.0f + PORTAL_TRACE_EPSILON;
	PORTAL_PTR_TYPE *pBestPortal = NULL;

	int iCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = 0; i != iCount; ++i )
	{
		PORTAL_PTR_TYPE *pPortal = CProp_Portal_Shared::AllPortals[i];
		if ( pPortal == NULL || !pPortal->IsActivatedAndLinked() )
			continue;

		float flPortalFraction;
		Vector vecCross;
		if ( Portal_SegmentPassesThroughPortal( pPortal, ray.m_Start, ray.m_Start + ray.m_Delta, flPortalFraction, &vecCross ) == false )
			continue;

		if ( flPortalFraction < fFraction )
		{
			fFraction = flPortalFraction;
			pBestPortal = pPortal;
		}
	}

	if ( pBestPortal == NULL )
	{
		fFraction = 1.0f;
		return NULL;
	}

	return pBestPortal;
}

float UTIL_Portal_DistanceThroughPortalSqr( const PORTAL_PTR_TYPE *pThroughPortal, const Vector &vecSource, const Vector &vecDest )
{
	if ( pThroughPortal == NULL )
		return 1.0e6f;

	// Measure both points through the portal's link space
	const VMatrix &matThisToLinked = pThroughPortal->MatrixThisToLinked();
	Vector vecTranslatedSource, vecTranslatedDest;
	UTIL_Portal_PointTransform( matThisToLinked, vecSource, vecTranslatedSource );
	UTIL_Portal_PointTransform( matThisToLinked, vecDest, vecTranslatedDest );
	return vecTranslatedDest.DistToSqr( vecTranslatedSource );
}

bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const PORTAL_PTR_TYPE *pPortal )
{
	if ( pPortal == NULL || !pPortal->IsActivatedAndLinked() )
		return false;

	float flHalfWidth = ( pPortal->m_vPortalCorners[1] - pPortal->m_vPortalCorners[0] ).Length() * 0.5f;
	float flHalfHeight = ( pPortal->m_vPortalCorners[3] - pPortal->m_vPortalCorners[0] ).Length() * 0.5f;
	float flFluff = vecBoxExtents.Length() * 0.5f;

	Vector vecLocal;
	Portal_WorldToLocal( pPortal, vecBoxCenter, vecLocal );

	if ( fabs( vecLocal.x ) > flHalfWidth + flFluff ||
		 fabs( vecLocal.y ) > flHalfHeight + flFluff ||
		 fabs( vecLocal.z ) > PORTAL_SKIN_LENGTH + 16.0f + flFluff )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Builds an orthonormal frame for a surface with the given outward normal.
//-----------------------------------------------------------------------------
void UTIL_Portal_GetSurfaceBasis( const Vector &vecNormal, Vector &vecRight, Vector &vecUp )
{
	if ( fabs( vecNormal.z ) > 0.9f )
	{
		vecRight.Init( 1.0f, 0.0f, 0.0f );
	}
	else
	{
		vecRight = CrossProduct( Vector( 0.0f, 0.0f, 1.0f ), vecNormal );
		vecRight.NormalizeInPlace();
	}

	vecUp = CrossProduct( vecNormal, vecRight );
	vecUp.NormalizeInPlace();
}

//-----------------------------------------------------------------------------
// Transformation helpers
//-----------------------------------------------------------------------------
void UTIL_Portal_PointTransform( const VMatrix &matThisToLinked, const Vector &ptSource, Vector &ptDest )
{
	Vector3DMultiplyPosition( matThisToLinked, ptSource, ptDest );
}

void UTIL_Portal_VectorTransform( const VMatrix &matThisToLinked, const Vector &vecSource, Vector &vecDest )
{
	Vector3DMultiply( matThisToLinked, vecSource, vecDest );
}

void UTIL_Portal_AngleTransform( const VMatrix &matThisToLinked, const QAngle &qSource, QAngle &qDest )
{
	// out = portal rotation * source rotation
	matrix3x4_t matPortal;
	matThisToLinked.Set3x4( matPortal );

	// VMatrix stores its basis vectors in columns. Guard against scale drift
	// by renormalizing them before building the quaternion rotation.
	Vector xAxis( matPortal[0][0], matPortal[1][0], matPortal[2][0] );
	Vector yAxis( matPortal[0][1], matPortal[1][1], matPortal[2][1] );
	Vector zAxis( matPortal[0][2], matPortal[1][2], matPortal[2][2] );
	xAxis.NormalizeInPlace();
	yAxis.NormalizeInPlace();
	zAxis.NormalizeInPlace();

	matrix3x4_t matPortalRot;
	MatrixInitialize( matPortalRot, vec3_origin, xAxis, yAxis, zAxis );

	Quaternion qPortal;
	MatrixQuaternion( matPortalRot, qPortal );

	Quaternion qSourceQuat;
	AngleQuaternion( qSource, qSourceQuat );

	Quaternion qOut;
	QuaternionMult( qPortal, qSourceQuat, qOut );

	QuaternionAngles( qOut, qDest );
}
