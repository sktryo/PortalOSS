//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: prop_button - the pedestal push button. Pressed by a player with
//			the +USE key while close to and aiming at the button; fires
//			OnPressed while depressed and OnUnpressed when released.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "props.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PORTAL_BUTTON_USE_RANGE			72.0f
#define PORTAL_BUTTON_THINK_INTERVAL	0.1f

class CPropButton : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropButton, CDynamicProp );
	DECLARE_DATADESC();

	void	Precache( void );
	void	Spawn( void );
	void	Think( void );

private:
	bool	IsPlayerPressing( CBasePlayer *pPlayer );

	bool	m_bPressed;

	COutputEvent	m_OnPressed;
	COutputEvent	m_OnUnpressed;
};

LINK_ENTITY_TO_CLASS( prop_button, CPropButton );

BEGIN_DATADESC( CPropButton )
	DEFINE_FIELD( m_bPressed, FIELD_BOOLEAN ),
	DEFINE_OUTPUT( m_OnPressed, "OnPressed" ),
	DEFINE_OUTPUT( m_OnUnpressed, "OnUnpressed" ),
	DEFINE_THINKFUNC( Think ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropButton::Precache( void )
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/button_base.mdl" ) );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropButton::Spawn( void )
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/button_base.mdl" ) );
	}

	BaseClass::Spawn();

	m_bPressed = false;

	SetThink( &CPropButton::Think );
	SetNextThink( gpGlobals->curtime + PORTAL_BUTTON_THINK_INTERVAL );
}

//-----------------------------------------------------------------------------
// Purpose: A player pressing +use within range and looking at the button
//			counts as holding it down
//-----------------------------------------------------------------------------
bool CPropButton::IsPlayerPressing( CBasePlayer *pPlayer )
{
	if ( pPlayer == NULL )
		return false;

	Vector vecToButton = GetAbsOrigin() - pPlayer->EyePosition();
	float flDistance = vecToButton.Length();
	if ( flDistance > PORTAL_BUTTON_USE_RANGE )
		return false;

	if ( ( pPlayer->m_nButtons & IN_USE ) == 0 )
		return false;

	// Rough "is aiming at the button": angle between the view direction and
	// the button position
	Vector vecForward;
	AngleVectors( pPlayer->EyeAngles(), &vecForward );
	VectorNormalize( vecToButton );
	if ( DotProduct( vecForward, vecToButton ) < 0.8f )
		return false;

	return true;
}

//----------------------------------------------------------------------------- 
// Purpose: 
//----------------------------------------------------------------------------- 
void CPropButton::Think( void )
{
	bool bPressed = false;

	CBaseEntity *pEntityList[16];
	int iCount = UTIL_EntitiesInSphere( pEntityList, ARRAYSIZE( pEntityList ), GetAbsOrigin(), PORTAL_BUTTON_USE_RANGE, 0 );
	for ( int i = 0; i < iCount; ++i )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pEntityList[i] );
		if ( pPlayer != NULL && pPlayer->IsAlive() && IsPlayerPressing( pPlayer ) )
		{
			bPressed = true;
			break;
		}
	}

	if ( bPressed != m_bPressed )
	{
		m_bPressed = bPressed;
		if ( bPressed )
		{
			m_OnPressed.FireOutput( this, this );
		}
		else
		{
			m_OnUnpressed.FireOutput( this, this );
		}
	}

	SetNextThink( gpGlobals->curtime + PORTAL_BUTTON_THINK_INTERVAL );
}
