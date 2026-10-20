//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The client-side Portal player class
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_PORTAL_PLAYER_H
#define C_PORTAL_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlplayer.h"

class C_Portal_Player;

C_Portal_Player *ToPortalPlayer( C_BasePlayer *pPlayer );
bool LocalPlayerIsCloseToPortal( void );

class C_Portal_Player : public C_BaseHLPlayer
{
public:
	DECLARE_CLASS( C_Portal_Player, C_BaseHLPlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_Portal_Player( void );
	virtual ~C_Portal_Player( void );

	// Should this object cast a shadow?
	virtual bool IsOverridingViewmodel( void ) { return false; }

	// suppress the crosshair (during portal placement etc)
	bool IsSuppressingCrosshair( void ) const { return m_bSuppressCrosshair; }

	virtual int DrawModel( int flags );

private:
	bool			m_bSuppressCrosshair;
};

#endif // C_PORTAL_PLAYER_H
