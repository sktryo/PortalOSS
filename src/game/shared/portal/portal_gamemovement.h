//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal game movement. Player traversal through portals:
//			when the player pushes into an open, linked portal's opening,
//			origin, velocity and view angles are transformed through the
//			pair's ThisToLinked matrix before the regular movement runs, so
//			the rest of the tick carries the player out of the exit portal.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_GAMEMOVEMENT_H
#define PORTAL_GAMEMOVEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "hl_gamemovement.h"

class CProp_Portal_Shared;

class CPortalGameMovement : public CHL2GameMovement
{
	typedef CHL2GameMovement BaseClass;

public:

	CPortalGameMovement();

	virtual void	ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );

private:
	// Finds the portal whose opening contains the player and whose surface
	// the player is moving into; fills in the exit transform when found.
	// angView is the move data's current (pre-transform) view angles.
	bool	PickPortalEntry( CBasePlayer *pPlayer, const Vector &vecOrigin, const Vector &vecVelocity,
							 const QAngle &angView, PORTAL_PTR_TYPE **ppEntry, Vector &vecExitOrigin,
							 Vector &vecExitVelocity, QAngle &angExitView );

	// Applies a traversal: writes the transformed origin/velocity/angles
	// back into the move data and player state.
	void	ApplyPortalTraversal( CBasePlayer *pPlayer, CMoveData *pMove,
								  const Vector &vecExitOrigin, const Vector &vecExitVelocity,
								  const QAngle &angExitView );
};

#endif // PORTAL_GAMEMOVEMENT_H

