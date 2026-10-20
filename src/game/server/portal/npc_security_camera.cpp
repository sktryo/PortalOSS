//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: npc_security_camera - the wall mounted cameras seen in Portal test
//			chambers. This is a visual-only implementation: the camera spawns
//			as a static model and does not track the player yet.
//
//			Maps that contain npc_security_camera need the class registered,
//			otherwise the engine leaves an uninitialised entity slot behind
//			("Attempted to create unknown entity type npc_security_camera!")
//			which then faults in the first snapshot pack.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "baseanimating.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SECURITY_CAMERA_DEFAULT_MODEL	"models/props/security_camera.mdl"

class CNPC_SecurityCamera : public CBaseAnimating
{
public:
	DECLARE_CLASS( CNPC_SecurityCamera, CBaseAnimating );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	void	Precache( void );
	void	Spawn( void );
};

LINK_ENTITY_TO_CLASS( npc_security_camera, CNPC_SecurityCamera );

IMPLEMENT_SERVERCLASS_ST( CNPC_SecurityCamera, DT_NPC_SecurityCamera )
END_SEND_TABLE()

BEGIN_DATADESC( CNPC_SecurityCamera )
END_DATADESC()

void CNPC_SecurityCamera::Precache( void )
{
	BaseClass::Precache();

	if ( GetModelName() != NULL_STRING )
	{
		PrecacheModel( STRING( GetModelName() ) );
	}
}

void CNPC_SecurityCamera::Spawn( void )
{
	// Prefer the model the map asked for; fall back to the stock camera prop.
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( SECURITY_CAMERA_DEFAULT_MODEL ) );
	}

	Precache();
	SetModel( STRING( GetModelName() ) );

	// Wall mounted decoration: no collision, no movement, no AI.
	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NOSHADOW );

	BaseClass::Spawn();
}
