//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal game movement. Player traversal through portals is not
//			implemented yet, so this class currently uses the stock HL2
//			movement unchanged.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_GAMEMOVEMENT_H
#define PORTAL_GAMEMOVEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "hl_gamemovement.h"

class CPortalGameMovement : public CHL2GameMovement
{
	typedef CHL2GameMovement BaseClass;

public:

	CPortalGameMovement();
};

#endif // PORTAL_GAMEMOVEMENT_H
