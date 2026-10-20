//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Portal player implementation (server side)
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_player.h"
#include "player.h"
#include "gamerules.h"
#include "portal_gamerules.h"
#include "prop_portal_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// In Portal builds, the player entity class is the portal player (the generic
// hl2 player's LINK_ENTITY_TO_CLASS is #ifdef'd out in hl2_player.cpp).
LINK_ENTITY_TO_CLASS( player, CPortal_Player );

IMPLEMENT_SERVERCLASS_ST( CPortal_Player, DT_Portal_Player )
	SendPropBool( SENDINFO( m_bSuppressCrosshair ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CPortal_Player )
	DEFINE_INPUTFUNC( FIELD_VOID, "SuppressCrosshair", InputSuppressCrosshair ),
END_DATADESC()

CPortal_Player::CPortal_Player( void )
{
	m_bSuppressCrosshair = false;
}

CPortal_Player::~CPortal_Player( void )
{
}

void CPortal_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/player.mdl" );
	PrecacheModel( "models/gibs/agibs.mdl" );
	PrecacheModel( "models/weapons/v_hands.mdl" );
}

void CPortal_Player::Spawn( void )
{
	SetModel( "models/player.mdl" );

	BaseClass::Spawn();

	m_bSuppressCrosshair = false;
}

void CPortal_Player::SuppressCrosshair( bool bSuppress )
{
	m_bSuppressCrosshair = bSuppress;
}

void CPortal_Player::InputSuppressCrosshair( inputdata_t &inputdata )
{
	SuppressCrosshair( true );
}

void CPortal_Player::UpdatePortalViewAreaBits( unsigned char *pvs, int pvssize )
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if ( iPortalCount == 0 )
		return;

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();

	// Everything that can be seen through an open, linked portal needs to be
	// in the player's PVS for networking
	for ( int i = 0; i != iPortalCount; ++i )
	{
		CProp_Portal *pLocalPortal = pPortals[i];
		if ( pLocalPortal == NULL || pLocalPortal->m_bActivated == false )
			continue;

		CProp_Portal *pRemotePortal = pLocalPortal->m_hLinkedPortal.Get();
		if ( pRemotePortal == NULL || pRemotePortal->m_bActivated == false )
			continue;

		// Add origins from both sides of the linked portal pair
		engine->AddOriginToPVS( pLocalPortal->GetAbsOrigin() );
		engine->AddOriginToPVS( pRemotePortal->GetAbsOrigin() );
	}
}

CPortal_Player *ToPortalPlayer( CBaseEntity *pEntity )
{
	if ( pEntity == NULL )
		return NULL;

	return dynamic_cast< CPortal_Player * >( pEntity );
}
