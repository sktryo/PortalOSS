//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal client/server game specific functions. Replaces hl2_client.cpp
//			in the Portal server project (which must not also compile it).
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_player.h"
#include "portal_gamerules.h"
#include "gamerules.h"
#include "entitylist.h"
#include "physics.h"
#include "game.h"
#include "player_resource.h"
#include "engine/IEngineSound.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Host_Say( edict_t *pEdict, bool teamonly );

extern CBaseEntity*	FindPickerEntityClass( CBasePlayer *pPlayer, char *classname );
extern bool			g_fGameOver;

// called each time a player is spawned into the game
void ClientPutInServer( edict_t *pEdict, const char *playername )
{
	// Allocate a CBasePlayer for pev, and call spawn
	CPortal_Player *pPlayer = ( CPortal_Player * )CPortal_Player::CreatePlayer( "player", pEdict );
	pPlayer->SetPlayerName( playername );
}

void ClientActive( edict_t *pEdict, bool bLoadGame )
{
	CPortal_Player *pPlayer = dynamic_cast< CPortal_Player * >( CBaseEntity::Instance( pEdict ) );
	Assert( pPlayer );

	if ( !pPlayer )
	{
		return;
	}

	pPlayer->InitialSpawn();

	if ( !bLoadGame )
	{
		pPlayer->Spawn();
	}
}

const char *GetGameDescription( void )
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Portal";
}

CBaseEntity* FindEntity( edict_t *pEdict, char *classname )
{
	// If no name was given set bits based on the picked
	if ( FStrEq( classname, "" ) )
	{
		return ( FindPickerEntityClass( static_cast< CBasePlayer * >( GetContainingEntity( pEdict ) ), classname ) );
	}
	return NULL;
}

// Purpose: Precache game-specific models & sounds
void ClientGamePrecache( void )
{
	CBaseEntity::PrecacheModel( "models/player.mdl" );
	CBaseEntity::PrecacheModel( "models/gibs/agibs.mdl" );
	CBaseEntity::PrecacheModel( "models/weapons/v_hands.mdl" );

	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowAmmo" );
	CBaseEntity::PrecacheScriptSound( "HUDQuickInfo.LowHealth" );
}

// called by ClientKill and DeadThink
void respawn( CBaseEntity *pEdict, bool fCopyCorpse )
{
	if ( gpGlobals->coop || gpGlobals->deathmatch )
	{
		// respawn player
		pEdict->Spawn();
	}
	else
	{
		// restart the entire server
		engine->ServerCommand("reload\n");
	}
}

void GameStartFrame( void )
{
	VPROF("GameStartFrame()");
	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = ( teamplay.GetInt() != 0 );
}

//_____________________________________________________________________________
// instantiate the proper game rules object
//_____________________________________________________________________________
void InstallGameRules( void )
{
	// Portal always uses its own rules
	CreateGameRulesObject( "CPortalGameRules" );
}
