//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_gamerules.h"
#include "gamerules.h"

#ifdef GAME_DLL
#include "player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

REGISTER_GAMERULES_CLASS( CPortalGameRules );

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPortalGameRules::CPortalGameRules( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CPortalGameRules::~CPortalGameRules( void )
{
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: Called when a player spawns; give them the portal device
//-----------------------------------------------------------------------------
void CPortalGameRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	BaseClass::PlayerSpawn( pPlayer );

	if ( pPlayer == NULL )
		return;

	// Only give the portal gun once the player has spawned in
	if ( pPlayer->IsAlive() )
	{
		if ( pPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) == NULL )
		{
			pPlayer->GiveNamedItem( "weapon_portalgun" );
		}
	}
}
#endif
