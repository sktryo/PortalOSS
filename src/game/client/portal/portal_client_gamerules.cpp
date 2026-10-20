//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side Portal game rules.
//
// The server creates CPortalGameRules and announces the class name through the
// "GameRulesCreation" string table (gamerules_register.cpp); the client looks
// the name up in its own CGameRulesRegister list, so the client must register
// a class with the exact same name. The client-side object derives the shared
// CHalfLife2 rules; everything Portal-specific so far (portalgun loadout,
// radio silencing) is server-side only.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "hl2_gamerules.h"
#include "gamerules_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPortalGameRules : public CHalfLife2
{
public:
	DECLARE_CLASS( CPortalGameRules, CHalfLife2 );

	CPortalGameRules( void );
	virtual ~CPortalGameRules( void );

	virtual const char	*GetGameDescription( void ) { return "Portal"; }
};

REGISTER_GAMERULES_CLASS( CPortalGameRules );

CPortalGameRules::CPortalGameRules( void )
{
}

CPortalGameRules::~CPortalGameRules( void )
{
}
