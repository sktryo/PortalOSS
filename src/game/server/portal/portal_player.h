//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Portal player entity (server side).
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_PLAYER_H
#define PORTAL_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "hl2_player.h"

class CPortal_Player : public CHL2_Player
{
public:
	DECLARE_CLASS( CPortal_Player, CHL2_Player );

	CPortal_Player( void );
	virtual ~CPortal_Player( void );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void	Spawn( void );
	virtual void	Precache( void );

	// Portal-style hand pickup: +USE grabs the physics prop under the
	// crosshair (Crosshair pickup isn't in stock HL2, which needs the
	// weapon_physcannon for the same effect).
	virtual void	PlayerUse( void );

	// Crosshair suppression (used by logic_playerproxy inputs in cutscene maps)
	void			SuppressCrosshair( bool bSuppress );
	bool			IsSuppressingCrosshair( void ) const { return m_bSuppressCrosshair; }
	void			InputSuppressCrosshair( inputdata_t &inputdata );

	// After the player's PVS has been set up, extend it with everything visible
	// through open portals
	void			UpdatePortalViewAreaBits( unsigned char *pvs, int pvssize );
private:
	CNetworkVar( bool, m_bSuppressCrosshair );
};

CPortal_Player *ToPortalPlayer( CBaseEntity *pEntity );

#endif // PORTAL_PLAYER_H
