//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The server implementation for prop_portal. Creates and maintains
//			portal entities placed by the portalgun.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "prop_portal_shared.h"
#include "portal_util_shared.h"
#include "portal_shareddefs.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// How far out from the placement point the wall clearance traces run.
#define PORTAL_PLACEMENT_EXTENTS_TRACE_DIST		( PORTAL_HALF_WIDTH * 3.0f )

LINK_ENTITY_TO_CLASS( prop_portal, CProp_Portal );

BEGIN_DATADESC( CProp_Portal )
	DEFINE_FIELD( m_strLinkedPortalName, FIELD_STRING ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CProp_Portal, DT_Prop_Portal )
	SendPropBool( SENDINFO( m_bActivated ) ),
	SendPropInt( SENDINFO( m_iPortalType ), 3, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hLinkedPortal ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_vPortalCorners ),
		SendPropVector( SENDINFO_ARRAY( m_vPortalCorners ), -1, SPROP_COORD ) ),
END_SEND_TABLE()

CProp_Portal::CProp_Portal( void )
{
	SetPortalType( PORTAL_TYPE_NONE );
	m_strLinkedPortalName = NULL_STRING;
}

CProp_Portal::~CProp_Portal( void )
{
}

void CProp_Portal::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( PORTAL_FIZZLE_SOUND_STATE );
}

void CProp_Portal::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetBlocksLOS( false );

	m_bActivated = false;
	m_hLinkedPortal = NULL;

	CProp_Portal_Shared::AddToAllPortals( this );
}

void CProp_Portal::Activate( void )
{
	BaseClass::Activate();

	// Helper for maps that pre-place portals via .bsp
	if ( m_strLinkedPortalName != NULL_STRING )
	{
		CProp_Portal *pPortal = NULL;
		while ( ( pPortal = assert_cast< CProp_Portal * >( gEntList.FindEntityByName( pPortal, m_strLinkedPortalName ) ) ) != NULL )
		{
			if ( pPortal != this )
			{
				LinkPortal( pPortal );
				break;
			}
		}
	}

	CProp_Portal_Shared::AddToAllPortals( this );
}

void CProp_Portal::UpdateOnRemove( void )
{
	CProp_Portal_Shared::RemoveFromAllPortals( this );
	UnLinkPortal();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Which portal type this portal instance is
//-----------------------------------------------------------------------------
int CProp_Portal::GetPortalType( void )
{
	return m_iPortalType;
}

void CProp_Portal::SetPortalType( PortalType iPortalType )
{
	m_iPortalType = iPortalType;
}

//-----------------------------------------------------------------------------
// Purpose: Portal took damage; fizzles if hit by a dissolving attack.
//-----------------------------------------------------------------------------
int CProp_Portal::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & DMG_DISSOLVE )
	{
		Fizzle( PORTAL_FIZZLE_KILLED );
		return 1;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Set up both portals of the link
//-----------------------------------------------------------------------------
void CProp_Portal::LinkPortal( CProp_Portal *pLinkedPortal )
{
	if ( pLinkedPortal == NULL )
	{
		UnLinkPortal();
		return;
	}

	// Both portals link each other
	m_hLinkedPortal = pLinkedPortal;
	pLinkedPortal->m_hLinkedPortal = this;

	m_bActivated = true;
	pLinkedPortal->m_bActivated = true;

	// Recompute both transforms
	RecalculatePortalTransforms();
	pLinkedPortal->RecalculatePortalTransforms();
}

void CProp_Portal::UnLinkPortal( void )
{
	if ( m_hLinkedPortal != NULL )
	{
		CProp_Portal *pLinkedPortal = m_hLinkedPortal;
		pLinkedPortal->m_hLinkedPortal = NULL;
		pLinkedPortal->m_bActivated = false;
		pLinkedPortal->RecalculatePortalTransforms();
	}

	m_hLinkedPortal = NULL;
	m_bActivated = false;
	RecalculatePortalTransforms();
}

//-----------------------------------------------------------------------------
// Purpose: Set the portal to align exactly through the four given corners
//			(order: BL, BR, TR, TL). The plane origin/frustum is derived.
//-----------------------------------------------------------------------------
void CProp_Portal::SetupPortalCorners( const Vector &vBottomLeft, const Vector &vBottomRight,
									 const Vector &vTopRight, const Vector &vTopLeft )
{
	m_vPortalCorners.Set( 0, vBottomLeft );
	m_vPortalCorners.Set( 1, vBottomRight );
	m_vPortalCorners.Set( 2, vTopRight );
	m_vPortalCorners.Set( 3, vTopLeft );

	// The portal entity origin is the center of the corners
	Vector vecCenter = ( vBottomLeft + vBottomRight + vTopRight + vTopLeft ) * 0.25f;
	SetAbsOrigin( vecCenter );

	UpdatePortalPlaneFromCorners();
	RecalculatePortalTransforms();
}

//-----------------------------------------------------------------------------
// Purpose: How much space is there for a portal at this location? Traces out
//			in the plane around the portal center and clips the requested half
//			extents to whatever walls are found.
//-----------------------------------------------------------------------------
bool CProp_Portal::FindPlacementExtents( const Vector &vPlacementOrigin, const cplane_t &tracePlane,
									   float &fAvailableHalfWidth, float &fAvailableHalfHeight )
{
	Vector vecRight, vecUp;
	UTIL_Portal_GetSurfaceBasis( tracePlane.normal, vecRight, vecUp );

	fAvailableHalfWidth = PORTAL_HALF_WIDTH;
	fAvailableHalfHeight = PORTAL_HALF_HEIGHT;

	trace_t tr;
	float flDist;

	Vector vecEnd;
	Ray_t ray;

	// Trace out in four directions to clamp the extents to whatever walls exist
	vecEnd = vPlacementOrigin + vecRight * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	ray.Init( vPlacementOrigin, vecEnd );
	CTraceFilterSimple traceFilter( this, COLLISION_GROUP_NONE );
	enginetrace->TraceRay( ray, MASK_SOLID, &traceFilter, &tr );
	flDist = tr.fraction * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	if ( flDist < PORTAL_HALF_WIDTH_MIN )
		return false;
	if ( flDist < fAvailableHalfWidth )
		fAvailableHalfWidth = flDist;

	vecEnd = vPlacementOrigin - vecRight * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	ray.Init( vPlacementOrigin, vecEnd );
	enginetrace->TraceRay( ray, MASK_SOLID, &traceFilter, &tr );
	flDist = tr.fraction * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	if ( flDist < PORTAL_HALF_WIDTH_MIN )
		return false;
	if ( flDist < fAvailableHalfWidth )
		fAvailableHalfWidth = flDist;

	vecEnd = vPlacementOrigin + vecUp * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	ray.Init( vPlacementOrigin, vecEnd );
	enginetrace->TraceRay( ray, MASK_SOLID, &traceFilter, &tr );
	flDist = tr.fraction * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	if ( flDist < PORTAL_HALF_HEIGHT_MIN )
		return false;
	if ( flDist < fAvailableHalfHeight )
		fAvailableHalfHeight = flDist;

	vecEnd = vPlacementOrigin - vecUp * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	ray.Init( vPlacementOrigin, vecEnd );
	enginetrace->TraceRay( ray, MASK_SOLID, &traceFilter, &tr );
	flDist = tr.fraction * PORTAL_PLACEMENT_EXTENTS_TRACE_DIST;
	if ( flDist < PORTAL_HALF_HEIGHT_MIN )
		return false;
	if ( flDist < fAvailableHalfHeight )
		fAvailableHalfHeight = flDist;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Fire the portal fizzle effects
//-----------------------------------------------------------------------------
void CProp_Portal::DoFizzleEffect( PortalFizzleType_t fizzleType, bool bFailedToOpenPortal )
{
	if ( !bFailedToOpenPortal )
	{
		EmitSound( PORTAL_FIZZLE_SOUND_STATE );
	}

	CEffectData data;
	data.m_vOrigin = GetPortalOrigin();
	data.m_vNormal = GetPortalNormal();
	data.m_nDamageType = fizzleType;
	data.m_fFlags = bFailedToOpenPortal ? 1 : 0;
	DispatchEffect( "PortalFizzle", data );
}

void CProp_Portal::Fizzle( PortalFizzleType_t fizzleType )
{
	DoFizzleEffect( fizzleType, false );
	UnLinkPortal();

	// Fizzling always destroys the portal. UTIL_Remove defers the actual
	// deletion to the end of the frame, so it is safe from touch/damage code.
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Is anything overlapping this portal's opening?
//-----------------------------------------------------------------------------
bool CProp_Portal::IsTouchingPortal( void )
{
	// Use the placed opening rather than a fixed axis-aligned size so portals
	// on floors, ceilings, and rotated walls all get the right bounds.
	Vector vMin = m_vPortalCorners[0];
	Vector vMax = m_vPortalCorners[0];
	for ( int i = 1; i < 4; ++i )
	{
		for ( int axis = 0; axis < 3; ++axis )
		{
			vMin[axis] = MIN( vMin[axis], m_vPortalCorners[i][axis] );
			vMax[axis] = MAX( vMax[axis], m_vPortalCorners[i][axis] );
		}
	}

	Vector vCenter = ( vMin + vMax ) * 0.5f;
	Vector vExtents = ( vMax - vMin ) * 0.5f;

	return IsTouchingPortal( vCenter, vExtents );
}

bool CProp_Portal::IsTouchingPortal( const Vector &vecCenter, const Vector &vecExtents )
{
	if ( !m_bActivated )
		return false;

	return UTIL_IsBoxIntersectingPortal( vecCenter, vecExtents, this );
}
