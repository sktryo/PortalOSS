//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: prop_floor_button - a floor mounted switch that fires OnPowered
//			outputs while a cube or player is resting on it
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "props.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPropFloorButton : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropFloorButton, CDynamicProp );
	DECLARE_DATADESC();

	void	Precache( void );
	void	Spawn( void );
	void	Think( void );

	bool	IsOn( void ) const { return m_bOn; }

private:
	bool	m_bOn;

	COutputEvent	m_OnPowered;
	COutputEvent	m_OnUnpowered;
};

LINK_ENTITY_TO_CLASS( prop_floor_button, CPropFloorButton );

BEGIN_DATADESC( CPropFloorButton )
	DEFINE_FIELD( m_bOn, FIELD_BOOLEAN ),
	DEFINE_OUTPUT( m_OnPowered, "OnPowered" ),
	DEFINE_OUTPUT( m_OnUnpowered, "OnUnpowered" ),
	DEFINE_THINKFUNC( Think ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropFloorButton::Precache( void )
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/button_base_reference.mdl" ) );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropFloorButton::Spawn( void )
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/button_base_reference.mdl" ) );
	}

	BaseClass::Spawn();

	m_bOn = false;

	SetThink( &CPropFloorButton::Think );
	SetNextThink( gpGlobals->curtime + 0.25f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropFloorButton::Think( void )
{
	bool bOn = false;

	//anything solid sitting on top of us that's a cube or the player presses the button
	CBaseEntity *pEntities[32];
	int iCount = UTIL_EntitiesInSphere( pEntities, ARRAYSIZE( pEntities ), GetAbsOrigin() + Vector( 0.0f, 0.0f, 8.0f ), 48.0f, 0 );
	for ( int i = 0; i < iCount; ++i )
	{
		CBaseEntity *pEntity = pEntities[i];
		if ( pEntity == NULL || pEntity == this || pEntity->GetMoveType() == MOVETYPE_NONE )
			continue;

		if ( !pEntity->IsPlayer() && !FClassnameIs( pEntity, "prop_weighted_cube" ) )
			continue;

		Vector vecOffset = pEntity->GetAbsOrigin() - GetAbsOrigin();
		if ( fabs( vecOffset.z ) > 72.0f )
			continue;

		bOn = true;
		break;
	}

	if ( bOn != m_bOn )
	{
		m_bOn = bOn;
		if ( bOn )
		{
			m_OnPowered.FireOutput( this, this );
		}
		else
		{
			m_OnUnpowered.FireOutput( this, this );
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}
