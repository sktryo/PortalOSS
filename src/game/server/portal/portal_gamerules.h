//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_GAMERULES_H
#define PORTAL_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "hl2_gamerules.h"

class CPortalGameRules : public CHalfLife2
{
public:
	DECLARE_CLASS( CPortalGameRules, CHalfLife2 );

	CPortalGameRules( void );
	virtual ~CPortalGameRules( void );

	virtual const char	*GetGameDescription( void ) { return "Portal"; }

#ifdef GAME_DLL
	// This is the only way we can silence the radio sound in the first room
	// without touching the map (see sound.cpp hook).
	virtual bool		ShouldRemoveRadio( void ) { return true; }

	// A player starts with the portalgun
	virtual void		PlayerSpawn( CBasePlayer *pPlayer );
#endif
};

//-----------------------------------------------------------------------------
// Gets us at the Portal game rules
//-----------------------------------------------------------------------------
inline CPortalGameRules *PortalGameRules()
{
	// g_pGameRules is NULL before the world has spawned; callers such as
	// sound.cpp already check the result for NULL.
	if ( g_pGameRules == NULL )
		return NULL;

	return assert_cast< CPortalGameRules * >( g_pGameRules );
}

#endif // PORTAL_GAMERULES_H
