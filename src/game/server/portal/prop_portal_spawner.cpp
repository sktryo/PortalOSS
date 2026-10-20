//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: prop_portal_spawner - the wall mounted portal emitter. Fires the
//			SpawnPortal input to place a portal (blue by default) on the
//			surface the emitter points at. Approximates the Portal branch
//			entity: renders as a static prop, creates and removes portals
//			through the shared placement pipeline.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "props.h"
#include "prop_portal_shared.h"
#include "portal_util_shared.h"
#include "portal_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The emitter sits right in front of the wall it opens; the wall is found by
// tracing that distance in both directions along the local forward axis.
#define PORTAL_SPAWNER_TRACE_DIST		64.0f

class CPropPortalSpawner : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropPortalSpawner, CDynamicProp );
	DECLARE_DATADESC();

	void	Spawn( void );

	// Inputs
	void	InputSpawnPortal( inputdata_t &inputdata );
	void	InputSpawnPortalWithoutDoor( inputdata_t &inputdata );
	void	InputSetPortalType( inputdata_t &inputdata );
	void	InputClosePortal( inputdata_t &inputdata );

private:
	void	SpawnPortalInternal( void );

	bool	m_bSpawnBlue;				// maps usually key blue = 1
	EHANDLE	m_hSpawnedPortal;

	COutputEvent	m_OnPreSpawnedPortal;
	COutputEvent	m_OnPostSpawnedPortal;
};

LINK_ENTITY_TO_CLASS( prop_portal_spawner, CPropPortalSpawner );

BEGIN_DATADESC( CPropPortalSpawner )
	DEFINE_KEYFIELD( m_bSpawnBlue, FIELD_BOOLEAN, "spawn_blue" ),
	DEFINE_FIELD( m_hSpawnedPortal, FIELD_EHANDLE ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SpawnPortal", InputSpawnPortal ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SpawnPortalWithoutDoor", InputSpawnPortalWithoutDoor ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetPortalType", InputSetPortalType ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ClosePortal", InputClosePortal ),
	DEFINE_OUTPUT( m_OnPreSpawnedPortal, "OnPreSpawnedPortal" ),
	DEFINE_OUTPUT( m_OnPostSpawnedPortal, "OnPostSpawnedPortal" ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalSpawner::Spawn( void )
{
	// Emitter model when the map did not key one. Portal's content provides it;
	// a missing model only costs a console warning, the logic is unaffected.
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/portal_emitter.mdl" ) );
	}

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalSpawner::SpawnPortalInternal( void )
{
	// Trace both ways: the emitters are mounted facing their wall, but so the
	// entity works on either orientation the shorter hit is the host wall.
	QAngle angDirection = GetAbsAngles();
	Vector vecForward;
	AngleVectors( angDirection, &vecForward );

	trace_t trForward, trBackward;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecForward * PORTAL_SPAWNER_TRACE_DIST,
					MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trForward );
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - vecForward * PORTAL_SPAWNER_TRACE_DIST,
					MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &trBackward );

	const trace_t *pWallTrace = NULL;
	if ( trForward.DidHitWorld() && trBackward.DidHitWorld() )
	{
		pWallTrace = ( trForward.fraction <= trBackward.fraction ) ? &trForward : &trBackward;
	}
	else if ( trForward.DidHitWorld() )
	{
		pWallTrace = &trForward;
	}
	else if ( trBackward.DidHitWorld() )
	{
		pWallTrace = &trBackward;
	}

	if ( pWallTrace == NULL )
	{
		Portal_DebugLog( "[portal] prop_portal_spawner '%s' SpawnPortal: no wall trace hit (fwd %.3f bwd %.3f)",
			STRING( GetEntityName() ), trForward.fraction, trBackward.fraction );
		return;
	}

	if ( pWallTrace->surface.flags & ( SURF_NODRAW | SURF_SKY | SURF_SKY2D | SURF_NOPORTAL | SURF_NOLIGHT ) )
	{
		Portal_DebugLog( "[portal] prop_portal_spawner '%s' SpawnPortal: surface rejected (flags 0x%x)",
			STRING( GetEntityName() ), pWallTrace->surface.flags );
		return;
	}

	Portal_DebugLog( "[portal] prop_portal_spawner '%s' SpawnPortal: wall hit at (%.1f %.1f %.1f) normal=(%.2f %.2f %.2f)",
		STRING( GetEntityName() ), pWallTrace->endpos.x, pWallTrace->endpos.y, pWallTrace->endpos.z,
		pWallTrace->plane.normal.x, pWallTrace->plane.normal.y, pWallTrace->plane.normal.z );

	m_OnPreSpawnedPortal.FireOutput( this, this );

	Vector vecPortalCenter = pWallTrace->endpos + pWallTrace->plane.normal * PORTAL_SKIN_LENGTH;

	PortalType type = m_bSpawnBlue ? PORTAL_TYPE_BLUE : PORTAL_TYPE_ORANGE;
	m_hSpawnedPortal = Portal_PlacePortal( type, pWallTrace->plane, vecPortalCenter );

	if ( m_hSpawnedPortal != NULL )
	{
		m_OnPostSpawnedPortal.FireOutput( this, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropPortalSpawner::InputSpawnPortal( inputdata_t &inputdata )
{
	SpawnPortalInternal();
}

void CPropPortalSpawner::InputSpawnPortalWithoutDoor( inputdata_t &inputdata )
{
	SpawnPortalInternal();
}

//----------------------------------------------------------------------------- 
// Purpose: 
//----------------------------------------------------------------------------- 
void CPropPortalSpawner::InputSetPortalType( inputdata_t &inputdata )
{
	m_bSpawnBlue = ( inputdata.value.Int() != 1 );
}

//----------------------------------------------------------------------------- 
// Purpose: 
//----------------------------------------------------------------------------- 
void CPropPortalSpawner::InputClosePortal( inputdata_t &inputdata )
{
	CProp_Portal *pPortal = assert_cast< CProp_Portal * >( m_hSpawnedPortal.Get() );
	if ( pPortal != NULL && pPortal->m_bActivated )
	{
		pPortal->Fizzle( PORTAL_FIZZLE_FIZZLER );
	}
	m_hSpawnedPortal = NULL;
}
