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
#include "func_portal_volumes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// How far out from the placement point the wall clearance traces run.
#define PORTAL_PLACEMENT_EXTENTS_TRACE_DIST		( PORTAL_HALF_WIDTH * 3.0f )

LINK_ENTITY_TO_CLASS( prop_portal, CProp_Portal );

BEGIN_DATADESC( CProp_Portal )
	DEFINE_KEYFIELD( m_strLinkedPortalName, FIELD_STRING, "LinkedPortal" ),
	DEFINE_KEYFIELD( m_iPortalType, FIELD_INTEGER, "PortalTwo" ),
	DEFINE_KEYFIELD( m_iPortalTypeAlias, FIELD_INTEGER, "PortalType" ),
	DEFINE_KEYFIELD( m_iLinkageGroupID, FIELD_INTEGER, "LinkageGroupID" ),
	DEFINE_KEYFIELD( m_bStartActivated, FIELD_BOOLEAN, "Activated" ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetActivatedState", InputSetActivatedState ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Fizzle", InputFizzle ),
	DEFINE_INPUTFUNC( FIELD_STRING, "NewLocation", InputNewLocation ),
	DEFINE_OUTPUT( m_OnPlacedSuccessfully, "OnPlacedSuccessfully" ),
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
	m_iPortalTypeAlias = PORTAL_TYPE_NONE;
	m_iLinkageGroupID = 0;
	m_bStartActivated = false;
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

	// Older Portal maps may carry the colour as "PortalType" instead of
	// "PortalTwo". Resolve it once when the primary key was absent.
	if ( GetPortalType() == PORTAL_TYPE_NONE &&
		 ( m_iPortalTypeAlias == PORTAL_TYPE_BLUE || m_iPortalTypeAlias == PORTAL_TYPE_ORANGE ) )
	{
		SetPortalType( ( PortalType )m_iPortalTypeAlias );
	}

	Portal_DebugLog( "[portal] prop_portal Activate: origin=(%.1f %.1f %.1f) angles=(%.1f %.1f %.1f) type=%d corners_valid=%d start_active=%d linkedname='%s'",
		GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z,
		GetAbsAngles().x, GetAbsAngles().y, GetAbsAngles().z,
		GetPortalType(), HasValidCorners() ? 1 : 0, m_bStartActivated ? 1 : 0,
		STRING( m_strLinkedPortalName ) );

	// The engine calls Activate() for map-placed portals, which (unlike the
	// gun/spawner placement path) have no corners yet. Build the opening from
	// the entity origin/angles so pre-placed portals have real geometry.
	if ( !HasValidCorners() )
	{
		BuildCornersFromOriginAngles();
	}

	// Map logic turns pre-placed portals on through SetActivatedState, but
	// "Activated" 1 in the VMF means the pair is live from the start.
	if ( m_bStartActivated )
	{
		SetPortalActive( true );
	}

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
// Purpose: Build the four corners of the opening from the entity's placement.
//			Used by portals placed directly in the map (the gun and the map
//			spawner entities go through Portal_PlacePortal instead).
//-----------------------------------------------------------------------------
void CProp_Portal::BuildCornersFromOriginAngles( void )
{
	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );
	vecForward.NormalizeInPlace();
	vecRight.NormalizeInPlace();
	vecUp.NormalizeInPlace();

	// Hammer shows the portal's facing direction as the entity's forward axis.
	// UpdatePortalPlaneFromCorners() derives the normal as Right x Up, so flip
	// the right axis to make the normal point along forward (out of the wall).
	vecRight.Negate();

	Vector vecCenter = GetAbsOrigin();
	Vector vBL = vecCenter - vecRight * PORTAL_HALF_WIDTH - vecUp * PORTAL_HALF_HEIGHT;
	Vector vBR = vecCenter + vecRight * PORTAL_HALF_WIDTH - vecUp * PORTAL_HALF_HEIGHT;
	Vector vTR = vecCenter + vecRight * PORTAL_HALF_WIDTH + vecUp * PORTAL_HALF_HEIGHT;
	Vector vTL = vecCenter - vecRight * PORTAL_HALF_WIDTH + vecUp * PORTAL_HALF_HEIGHT;
	SetupPortalCorners( vBL, vBR, vTR, vTL );

	Portal_DebugLog( "[portal] prop_portal: built corners from origin/angles, normal=(%.2f %.2f %.2f)",
		m_plane_Origin.normal.x, m_plane_Origin.normal.y, m_plane_Origin.normal.z );
}

//-----------------------------------------------------------------------------
// Purpose: Turn a portal on/off from map logic without destroying the entity.
//			An activated portal links itself to the opposite colour portal when
//			that portal already has geometry (map logic activates each portal
//			of a pair independently; the one activated last completes the pair).
//-----------------------------------------------------------------------------
void CProp_Portal::SetPortalActive( bool bActive )
{
	if ( !bActive )
	{
		CProp_Portal *pLinkedPortal = m_hLinkedPortal;
		m_hLinkedPortal = NULL;
		m_bActivated = false;
		if ( pLinkedPortal != NULL )
		{
			// The partner keeps its own activated state; only the link dies.
			pLinkedPortal->m_hLinkedPortal = NULL;
		}
		RecalculatePortalTransforms();
		Portal_DebugLog( "[portal] prop_portal deactivated (origin=(%.1f %.1f %.1f))",
			GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		return;
	}

	if ( !HasValidCorners() )
	{
		BuildCornersFromOriginAngles();
	}

	m_bActivated = true;

	if ( m_hLinkedPortal == NULL )
	{
		for ( int i = 0; i < CProp_Portal_Shared::AllPortals.Count(); ++i )
		{
			CProp_Portal *pOther = CProp_Portal_Shared::AllPortals[i];
			if ( pOther == this || pOther == NULL || !pOther->HasValidCorners() )
				continue;

			if ( pOther->GetPortalType() == GetPortalType() )
				continue;

			// LinkPortal() activates both sides of the pair.
			LinkPortal( pOther );
			Portal_DebugLog( "[portal] prop_portal activated and linked to portal type %d",
				pOther->GetPortalType() );
			return;
		}
	}

	RecalculatePortalTransforms();
	Portal_DebugLog( "[portal] prop_portal activated, no link partner yet (origin=(%.1f %.1f %.1f))",
		GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
}

//-----------------------------------------------------------------------------
// Purpose: Input handlers matching Portal's prop_portal entity.
//-----------------------------------------------------------------------------
void CProp_Portal::InputSetActivatedState( inputdata_t &inputdata )
{
	SetPortalActive( inputdata.value.Bool() );
}

void CProp_Portal::InputFizzle( inputdata_t &inputdata )
{
	Fizzle( PORTAL_FIZZLE_FIZZLER );
}

void CProp_Portal::InputNewLocation( inputdata_t &inputdata )
{
	// NewLocation expects "X Y Z pitch yaw roll".
	float flValues[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	UTIL_StringToFloatArray( flValues, 6, STRING( inputdata.value.StringID() ) );

	Vector vecOrigin( flValues[0], flValues[1], flValues[2] );
	QAngle angAngles( flValues[3], flValues[4], flValues[5] );

	Teleport( &vecOrigin, &angAngles, NULL );
	BuildCornersFromOriginAngles();

	Portal_DebugLog( "[portal] prop_portal NewLocation: value='%s' origin=(%.1f %.1f %.1f) angles=(%.1f %.1f %.1f)",
		STRING( inputdata.value.StringID() ),
		vecOrigin.x, vecOrigin.y, vecOrigin.z, angAngles.x, angAngles.y, angAngles.z );
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

// Near, into-wall trace used when an emitted portal needs its host wall
#define PORTAL_SPAWNER_TRACE_DIST			96.0f

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
	data.m_nColor = ( GetPortalType() == PORTAL_TYPE_ORANGE ) ? 1 : 0;
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

//-----------------------------------------------------------------------------
// Purpose: Do the two portals' openings sit on the same part of the same wall?
//			Portals sharing a plane and overlapping in-plane cannot coexist.
//-----------------------------------------------------------------------------
bool Portal_RectanglesOverlap( CProp_Portal *pA, CProp_Portal *pB )
{
	if ( pA == NULL || pB == NULL )
		return false;

	// A different face is never overlapping
	if ( DotProduct( pA->GetPortalNormal(), pB->GetPortalNormal() ) < 0.9f )
		return false;

	// Different plane offsets are separated along the normal
	float flPlaneDelta = fabs( DotProduct( pA->GetPortalNormal(), pB->GetPortalOrigin() ) - pA->m_plane_Origin.dist );
	if ( flPlaneDelta > 8.0f )
		return false;

	// Axis-aligned overlap test on the world bounds of the corners
	Vector vecMinA, vecMaxA, vecMinB, vecMaxB;
	ClearBounds( vecMinA, vecMaxA );
	ClearBounds( vecMinB, vecMaxB );
	for ( int i = 0; i < 4; ++i )
	{
		AddPointToBounds( pA->m_vPortalCorners[i], vecMinA, vecMaxA );
		AddPointToBounds( pB->m_vPortalCorners[i], vecMinB, vecMaxB );
	}

	if ( vecMinA.x > vecMaxB.x || vecMinB.x > vecMaxA.x ||
		 vecMinA.y > vecMaxB.y || vecMinB.y > vecMaxA.y ||
		 vecMinA.z > vecMaxB.z || vecMinB.z > vecMaxA.z )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Shared server-side portal placement: creates a prop_portal of the
//			given type centered on the given surface point, fizzles the
//			previous portal of the same color and overlapping other-color
//			portals, and links the new portal to the other color when
//			present. Used by both the portal gun and map spawner entities.
//-----------------------------------------------------------------------------
CProp_Portal *Portal_PlacePortal( PortalType type, const cplane_t &tracePlane, const Vector &vecPortalCenter )
{
	Portal_DebugLog( "[portal] PlacePortal enter: type=%d center=(%.1f %.1f %.1f) normal=(%.2f %.2f %.2f)",
		(int)type, vecPortalCenter.x, vecPortalCenter.y, vecPortalCenter.z,
		tracePlane.normal.x, tracePlane.normal.y, tracePlane.normal.z );

	CProp_Portal *pNewPortal = ( CProp_Portal * )CreateEntityByName( "prop_portal" );
	if ( pNewPortal == NULL )
	{
		Portal_DebugLog( "[portal] PlacePortal FAILED: CreateEntityByName( prop_portal ) returned NULL" );
		return NULL;
	}

	pNewPortal->SetPortalType( type );

	float flNewWidth = 0.0f;
	float flNewHeight = 0.0f;
	if ( !pNewPortal->FindPlacementExtents( vecPortalCenter, tracePlane, flNewWidth, flNewHeight ) )
	{
		Portal_DebugLog( "[portal] PlacePortal FAILED: FindPlacementExtents (no room on surface)" );
		UTIL_Remove( pNewPortal );
		return NULL;
	}

	// Direction frame for the portal opening
	Vector vecRight, vecUp;
	UTIL_Portal_GetSurfaceBasis( tracePlane.normal, vecRight, vecUp );

	// func_portal_orientation: portals placed inside an active orientation
	// volume get their opening rolled so the bottom edge (bright end) points
	// along the volume's AnglesToFace direction.
	QAngle angOrientation;
	if ( Portal_GetOrientationForPosition( vecPortalCenter, angOrientation ) )
	{
		Vector vecFaceDir;
		AngleVectors( angOrientation, &vecFaceDir );
		vecFaceDir -= tracePlane.normal * DotProduct( vecFaceDir, tracePlane.normal );
		if ( vecFaceDir.NormalizeInPlace() > 0.7f )
		{
			vecUp = -vecFaceDir;
			vecRight = CrossProduct( vecUp, tracePlane.normal );
			vecRight.NormalizeInPlace();
		}
	}

	// func_noportal_volume / func_portal_bumper: the portal footprint must
	// not intersect these volumes. A shot on a surface inside a noportal
	// volume fizzles; anything else gets nudged out of the way.
	Vector vecPlacementCenter = vecPortalCenter;
	bool bCenterMoved = false;
	if ( !Portal_AdjustPlacementForVolumes( vecPlacementCenter, tracePlane.normal,
											vecRight, vecUp, flNewWidth, flNewHeight,
											bCenterMoved ) )
	{
		Portal_DebugLog( "[portal] PlacePortal FAILED: Portal_AdjustPlacementForVolumes (blocked by func_noportal_volume)" );
		UTIL_Remove( pNewPortal );
		return NULL;
	}

	if ( bCenterMoved )
	{
		// The opening sits in different space now; measure the room again so
		// the portal still fits on the surface.
		if ( !pNewPortal->FindPlacementExtents( vecPlacementCenter, tracePlane, flNewWidth, flNewHeight ) )
		{
			UTIL_Remove( pNewPortal );
			return NULL;
		}
	}

	Vector vLeftBottom = vecPlacementCenter - vecRight * flNewWidth - vecUp * flNewHeight;
	Vector vRightBottom = vecPlacementCenter + vecRight * flNewWidth - vecUp * flNewHeight;
	Vector vRightTop = vecPlacementCenter + vecRight * flNewWidth + vecUp * flNewHeight;
	Vector vLeftTop = vecPlacementCenter - vecRight * flNewWidth + vecUp * flNewHeight;

	pNewPortal->Spawn();
	pNewPortal->SetupPortalCorners( vLeftBottom, vRightBottom, vRightTop, vLeftTop );

	// Find any previous portal of the same color (kill it) and the other color
	// portal for linking
	CProp_Portal *pOtherColorPortal = NULL;
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = iPortalCount - 1; i >= 0; --i )
	{
		CProp_Portal *pPortal = CProp_Portal_Shared::AllPortals.Element( i );
		if ( pPortal == NULL || pPortal == pNewPortal )
			continue;

		// Remove any previous portal of the same color, whether it is still
		// linked or not. Otherwise a stale, dormant portal would survive.
		if ( pPortal->GetPortalType() == type )
		{
			pPortal->Fizzle( PORTAL_FIZZLE_KILLED );
			continue;
		}

		// Remember the other color portal to link against. An unlinked
		// (dormant) portal is still a valid link target: this is what makes
		// the first pair of portals active.
		if ( ( ( type == PORTAL_TYPE_BLUE ) && ( pPortal->GetPortalType() == PORTAL_TYPE_ORANGE ) ) ||
			 ( ( type == PORTAL_TYPE_ORANGE ) && ( pPortal->GetPortalType() == PORTAL_TYPE_BLUE ) ) )
		{
			// The other color portal cannot survive the new portal covering
			// the same spot: fizzle it and keep looking for a link target.
			if ( Portal_RectanglesOverlap( pNewPortal, pPortal ) )
			{
				pPortal->Fizzle( PORTAL_FIZZLE_KILLED );
				continue;
			}

			pOtherColorPortal = pPortal;
		}
	}

	if ( pOtherColorPortal != NULL )
	{
		pNewPortal->LinkPortal( pOtherColorPortal );
	}
	else
	{
		// A lone portal is still visible (its opening is drawn as a dormant
		// surface); only inactive map portals stay hidden.
		pNewPortal->m_bActivated = true;
	}

	pNewPortal->Activate();
	pNewPortal->FirePlacedOutput();

	Portal_DebugLog( "[portal] PlacePortal OK: type=%d center=(%.1f %.1f %.1f) size=%.1fx%.1f linked=%s",
		(int)type, vecPlacementCenter.x, vecPlacementCenter.y, vecPlacementCenter.z,
		flNewWidth, flNewHeight, ( pOtherColorPortal != NULL ) ? "yes" : "no" );

	return pNewPortal;
}
