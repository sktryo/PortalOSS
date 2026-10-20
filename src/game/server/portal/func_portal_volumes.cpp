//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal volume brush entities. See func_portal_volumes.h.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "entitylist.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "func_portal_volumes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_PORTAL_VOLUME_START_INACTIVE		1

//-----------------------------------------------------------------------------
// Local helpers
//-----------------------------------------------------------------------------
static bool Portal_PointInAABB( const Vector &vecPoint, const Vector &vecMins, const Vector &vecMaxs )
{
	return ( vecPoint.x >= vecMins.x && vecPoint.x <= vecMaxs.x &&
			 vecPoint.y >= vecMins.y && vecPoint.y <= vecMaxs.y &&
			 vecPoint.z >= vecMins.z && vecPoint.z <= vecMaxs.z );
}

static bool Portal_AABBsOverlap( const Vector &vecMinsA, const Vector &vecMaxsA,
								 const Vector &vecMinsB, const Vector &vecMaxsB )
{
	if ( vecMinsA.x > vecMaxsB.x || vecMinsB.x > vecMaxsA.x ||
		 vecMinsA.y > vecMaxsB.y || vecMinsB.y > vecMaxsA.y ||
		 vecMinsA.z > vecMaxsB.z || vecMinsB.z > vecMaxsA.z )
		return false;

	return true;
}

// Builds the world AABB of a proposed portal opening from its center and
// orthonormal frame + half extents.
static void Portal_GetPortalFootprintBounds( const Vector &vecCenter, const Vector &vecRight,
											 const Vector &vecUp, float flHalfWidth, float flHalfHeight,
											 Vector &vecMins, Vector &vecMaxs )
{
	Vector vCorners[4];
	vCorners[0] = vecCenter - vecRight * flHalfWidth - vecUp * flHalfHeight;
	vCorners[1] = vecCenter + vecRight * flHalfWidth - vecUp * flHalfHeight;
	vCorners[2] = vecCenter + vecRight * flHalfWidth + vecUp * flHalfHeight;
	vCorners[3] = vecCenter - vecRight * flHalfWidth + vecUp * flHalfHeight;

	ClearBounds( vecMins, vecMaxs );
	for ( int i = 0; i < 4; ++i )
		AddPointToBounds( vCorners[i], vecMins, vecMaxs );
}

//-----------------------------------------------------------------------------
// func_noportal_volume
// Surfaces inside the volume act as unportalable. Any new portal footprint
// intersecting the volume is either fizzled (openings on the covered surface)
// or nudged out of the volume.
//-----------------------------------------------------------------------------
class CFuncNoPortalVolume : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncNoPortalVolume, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void Spawn( void );
	bool IsActive( void ) const { return !m_bDisabled; }
	void SetActive( bool bActive ) { m_bDisabled = !bActive; }

	// Inputs
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

private:
	bool	m_bDisabled;
	bool	m_bStartDisabled;
};

LINK_ENTITY_TO_CLASS( func_noportal_volume, CFuncNoPortalVolume );
IMPLEMENT_SERVERCLASS_ST( CFuncNoPortalVolume, DT_FuncNoPortalVolume )
END_SEND_TABLE()

BEGIN_DATADESC( CFuncNoPortalVolume )
	DEFINE_KEYFIELD( m_bStartDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate", InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
END_DATADESC()

void CFuncNoPortalVolume::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );

	m_bDisabled = m_bStartDisabled || ( HasSpawnFlags( SF_PORTAL_VOLUME_START_INACTIVE ) != 0 );
}

void CFuncNoPortalVolume::InputActivate( inputdata_t &inputdata )
{
	SetActive( true );
}

void CFuncNoPortalVolume::InputDeactivate( inputdata_t &inputdata )
{
	SetActive( false );
}

void CFuncNoPortalVolume::InputToggle( inputdata_t &inputdata )
{
	SetActive( m_bDisabled );
}

//-----------------------------------------------------------------------------
// func_portal_bumper
// Portals whose footprint would touch the volume get nudged so they do not
// touch it (allows placing portals flush against unportalable surfaces).
//-----------------------------------------------------------------------------
class CFuncPortalBumper : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncPortalBumper, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void Spawn( void );
	bool IsActive( void ) const { return !m_bDisabled; }
	void SetActive( bool bActive ) { m_bDisabled = !bActive; }

	// Inputs
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

private:
	bool	m_bDisabled;
	bool	m_bStartDisabled;
};

LINK_ENTITY_TO_CLASS( func_portal_bumper, CFuncPortalBumper );
IMPLEMENT_SERVERCLASS_ST( CFuncPortalBumper, DT_FuncPortalBumper )
END_SEND_TABLE()

BEGIN_DATADESC( CFuncPortalBumper )
	DEFINE_KEYFIELD( m_bStartDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate", InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
END_DATADESC()

void CFuncPortalBumper::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );

	m_bDisabled = m_bStartDisabled || ( HasSpawnFlags( SF_PORTAL_VOLUME_START_INACTIVE ) != 0 );
}

void CFuncPortalBumper::InputActivate( inputdata_t &inputdata )
{
	SetActive( true );
}

void CFuncPortalBumper::InputDeactivate( inputdata_t &inputdata )
{
	SetActive( false );
}

void CFuncPortalBumper::InputToggle( inputdata_t &inputdata )
{
	SetActive( m_bDisabled );
}

//-----------------------------------------------------------------------------
// func_portal_orientation
// Portals placed inside the volume have their bottom edge (bright end) aimed
// along the AnglesToFace direction.
//-----------------------------------------------------------------------------
class CFuncPortalOrientation : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncPortalOrientation, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void Spawn( void );
	bool IsActive( void ) const { return !m_bDisabled; }

	// Inputs
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

public:
	QAngle	m_angOrientation;

private:
	bool	m_bDisabled;
	bool	m_bStartDisabled;
};

LINK_ENTITY_TO_CLASS( func_portal_orientation, CFuncPortalOrientation );
IMPLEMENT_SERVERCLASS_ST( CFuncPortalOrientation, DT_FuncPortalOrientation )
END_SEND_TABLE()

BEGIN_DATADESC( CFuncPortalOrientation )
	DEFINE_KEYFIELD( m_angOrientation, FIELD_VECTOR, "AnglesToFace" ),
	DEFINE_KEYFIELD( m_bStartDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate", InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
END_DATADESC()

void CFuncPortalOrientation::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );

	m_bDisabled = m_bStartDisabled || ( HasSpawnFlags( SF_PORTAL_VOLUME_START_INACTIVE ) != 0 );
}

void CFuncPortalOrientation::InputActivate( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CFuncPortalOrientation::InputDeactivate( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

void CFuncPortalOrientation::InputToggle( inputdata_t &inputdata )
{
	m_bDisabled = !m_bDisabled;
}

//-----------------------------------------------------------------------------
// func_portal_detector
// Fires outputs as portals of each color (and linked portal pairs) appear
// inside the volume. Checked on a fast think rather than on placement events
// so portals spawned by map entities also register.
//-----------------------------------------------------------------------------
class CFuncPortalDetector : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncPortalDetector, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void Spawn( void );
	void Activate( void );
	bool IsActive( void ) const { return !m_bDisabled; }

	// Inputs
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

	// Think
	void DetectThink( void );
	void UpdateTouchState( void );

	// Outputs
	COutputEvent	m_OnStartTouchPortal1;
	COutputEvent	m_OnStartTouchPortal2;
	COutputEvent	m_OnStartTouchLinkedPortal;
	COutputEvent	m_OnStartTouchBothLinkedPortals;

private:
	// Which portals are currently inside (for rising-edge detection)
	bool	m_bPortal1Touching;
	bool	m_bPortal2Touching;
	bool	m_bLinkedPortalTouching;
	bool	m_bBothLinkedPortalsTouching;

	int		m_iLinkageGroupID;
	bool	m_bDisabled;
	bool	m_bStartDisabled;
};

LINK_ENTITY_TO_CLASS( func_portal_detector, CFuncPortalDetector );
IMPLEMENT_SERVERCLASS_ST( CFuncPortalDetector, DT_FuncPortalDetector )
END_SEND_TABLE()

BEGIN_DATADESC( CFuncPortalDetector )
	DEFINE_KEYFIELD( m_iLinkageGroupID, FIELD_INTEGER, "LinkageGroupID" ),
	DEFINE_KEYFIELD( m_bStartDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPortal1Touching, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPortal2Touching, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLinkedPortalTouching, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bBothLinkedPortalsTouching, FIELD_BOOLEAN ),
	DEFINE_OUTPUT( m_OnStartTouchPortal1, "OnStartTouchPortal1" ),
	DEFINE_OUTPUT( m_OnStartTouchPortal2, "OnStartTouchPortal2" ),
	DEFINE_OUTPUT( m_OnStartTouchLinkedPortal, "OnStartTouchLinkedPortal" ),
	DEFINE_OUTPUT( m_OnStartTouchBothLinkedPortals, "OnStartTouchBothLinkedPortals" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate", InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_THINKFUNC( DetectThink ),
END_DATADESC()

void CFuncPortalDetector::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );

	m_bDisabled = m_bStartDisabled || ( HasSpawnFlags( SF_PORTAL_VOLUME_START_INACTIVE ) != 0 );

	m_bPortal1Touching = false;
	m_bPortal2Touching = false;
	m_bLinkedPortalTouching = false;
	m_bBothLinkedPortalsTouching = false;
}

void CFuncPortalDetector::Activate( void )
{
	BaseClass::Activate();

	if ( !m_bDisabled )
	{
		SetThink( &CFuncPortalDetector::DetectThink );
		SetNextThink( gpGlobals->curtime + 0.05f );
	}
}

void CFuncPortalDetector::InputActivate( inputdata_t &inputdata )
{
	m_bDisabled = false;
	SetThink( &CFuncPortalDetector::DetectThink );
	SetNextThink( gpGlobals->curtime + 0.05f );
}

void CFuncPortalDetector::InputDeactivate( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

void CFuncPortalDetector::InputToggle( inputdata_t &inputdata )
{
	m_bDisabled = !m_bDisabled;
	if ( !m_bDisabled )
	{
		SetThink( &CFuncPortalDetector::DetectThink );
		SetNextThink( gpGlobals->curtime + 0.05f );
	}
}

void CFuncPortalDetector::DetectThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.05f );
	UpdateTouchState();
}

void CFuncPortalDetector::UpdateTouchState( void )
{
	Vector vecVolumeMins, vecVolumeMaxs;
	CollisionProp()->WorldSpaceSurroundingBounds( &vecVolumeMins, &vecVolumeMaxs );

	Vector vecVolumeCenter = ( vecVolumeMins + vecVolumeMaxs ) * 0.5f;
	Vector vecVolumeExtents = ( vecVolumeMaxs - vecVolumeMins ) * 0.5f;

	bool bPortal1 = false;
	bool bPortal2 = false;
	bool bLinkedPortal1 = false;
	bool bLinkedPortal2 = false;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = 0; i < iPortalCount; ++i )
	{
		CProp_Portal *pPortal = CProp_Portal_Shared::AllPortals.Element( i );
		if ( pPortal == NULL || !pPortal->HasValidCorners() )
			continue;

		// Portal footprint AABB vs the volume box. Portals use the bounding
		// box of their corners so floor/ceiling/sideways doorways all test
		// correctly.
		if ( !pPortal->IsTouchingPortal( vecVolumeCenter, vecVolumeExtents ) )
			continue;

		if ( pPortal->GetPortalType() == PORTAL_TYPE_BLUE )
		{
			bPortal1 = true;
			if ( pPortal->IsActivatedAndLinked() )
				bLinkedPortal1 = true;
		}
		else
		{
			bPortal2 = true;
			if ( pPortal->IsActivatedAndLinked() )
				bLinkedPortal2 = true;
		}
	}

	// Careful about rising edges only: the outputs are "start touch" events.
	if ( bPortal1 && !m_bPortal1Touching )
		m_OnStartTouchPortal1.FireOutput( this, this );
	if ( bPortal2 && !m_bPortal2Touching )
		m_OnStartTouchPortal2.FireOutput( this, this );

	bool bLinkedTouching = ( bLinkedPortal1 || bLinkedPortal2 );
	if ( bLinkedTouching && !m_bLinkedPortalTouching )
		m_OnStartTouchLinkedPortal.FireOutput( this, this );

	bool bBothLinkedTouching = ( bLinkedPortal1 && bLinkedPortal2 );
	if ( bBothLinkedTouching && !m_bBothLinkedPortalsTouching )
		m_OnStartTouchBothLinkedPortals.FireOutput( this, this );

	m_bPortal1Touching = bPortal1;
	m_bPortal2Touching = bPortal2;
	m_bLinkedPortalTouching = bLinkedTouching;
	m_bBothLinkedPortalsTouching = bBothLinkedTouching;
}

//-----------------------------------------------------------------------------
// Shared helpers used by portal placement
//-----------------------------------------------------------------------------

bool Portal_PointBlockedByNoPortalVolume( const Vector &vecPoint )
{
	for ( CBaseEntity *pEntity = gEntList.FindEntityByClassname( NULL, "func_noportal_volume" );
		 pEntity != NULL;
		 pEntity = gEntList.FindEntityByClassname( pEntity, "func_noportal_volume" ) )
	{
		CFuncNoPortalVolume *pVolume = assert_cast< CFuncNoPortalVolume * >( pEntity );
		if ( !pVolume->IsActive() )
			continue;

		Vector vecMins, vecMaxs;
		pVolume->CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );
		if ( Portal_PointInAABB( vecPoint, vecMins, vecMaxs ) )
			return true;
	}

	return false;
}

bool Portal_GetOrientationForPosition( const Vector &vecPoint, QAngle &angOrientation )
{
	for ( CBaseEntity *pEntity = gEntList.FindEntityByClassname( NULL, "func_portal_orientation" );
		 pEntity != NULL;
		 pEntity = gEntList.FindEntityByClassname( pEntity, "func_portal_orientation" ) )
	{
		CFuncPortalOrientation *pVolume = assert_cast< CFuncPortalOrientation * >( pEntity );
		if ( !pVolume->IsActive() )
			continue;

		Vector vecMins, vecMaxs;
		pVolume->CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );
		if ( Portal_PointInAABB( vecPoint, vecMins, vecMaxs ) )
		{
			angOrientation = pVolume->m_angOrientation;
			return true;
		}
	}

	return false;
}

bool Portal_AdjustPlacementForVolumes( Vector &vecCenter, const Vector &vecNormal,
									   const Vector &vecRight, const Vector &vecUp,
									   float flHalfWidth, float flHalfHeight,
									   bool &bCenterMoved )
{
	const float flNudgeStep = 2.0f;
	const int nMaxNudges = 64;

	bCenterMoved = false;

	for ( int iNudge = 0; iNudge < nMaxNudges; ++iNudge )
	{
		Vector vecFootprintMins, vecFootprintMaxs;
		Portal_GetPortalFootprintBounds( vecCenter, vecRight, vecUp, flHalfWidth, flHalfHeight,
										 vecFootprintMins, vecFootprintMaxs );

		CBaseEntity *pOverlappingVolume = NULL;
		bool bNoPortalVolumeOverlap = false;

		// func_noportal_volume takes precedence: the portal fizzles whenever
		// the shot point itself is inside one of them.
		const char *pClassNames[2] = { "func_noportal_volume", "func_portal_bumper" };
		for ( int iClass = 0; iClass < 2 && pOverlappingVolume == NULL; ++iClass )
		{
			for ( CBaseEntity *pEntity = gEntList.FindEntityByClassname( NULL, pClassNames[iClass] );
				 pEntity != NULL && pOverlappingVolume == NULL;
				 pEntity = gEntList.FindEntityByClassname( pEntity, pClassNames[iClass] ) )
			{
				Vector vecMins, vecMaxs;
				pEntity->CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );

				if ( !Portal_AABBsOverlap( vecFootprintMins, vecFootprintMaxs, vecMins, vecMaxs ) )
					continue;

				// A shot point inside a func_noportal_volume means that
				// surface is unportalable outright.
				if ( iClass == 0 && Portal_PointInAABB( vecCenter, vecMins, vecMaxs ) )
					return false;

				pOverlappingVolume = pEntity;
				bNoPortalVolumeOverlap = ( iClass == 0 );
			}
		}

		// Nothing overlaps any longer
		if ( pOverlappingVolume == NULL )
			return true;

		// Push the placement point away from the volume, along the surface
		Vector vecMins, vecMaxs;
		pOverlappingVolume->CollisionProp()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );

		Vector vecVolumeCenter = ( vecMins + vecMaxs ) * 0.5f;
		Vector vecPushDir = vecCenter - vecVolumeCenter;
		vecPushDir -= vecNormal * DotProduct( vecPushDir, vecNormal );

		if ( vecPushDir.NormalizeInPlace() < 0.1f )
			vecPushDir = vecUp;

		vecCenter += vecPushDir * flNudgeStep;
		bCenterMoved = true;
	}

	// The portal could not be nudged clear of the volumes
	return false;
}
