//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal game movement.
//
// Player traversal through portals (teleporting the player's position and
// velocity through the linked portal) is not implemented. The stock HL2 game
// movement is used unchanged, so portal openings are treated like the wall
// they are placed on. A previous partial implementation only remapped traces
// and velocity; it did not move the player through the portal and could let
// the player clip through the wall behind it, so it was removed.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_gamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CPortalGameMovement::CPortalGameMovement( void )
{
}

// Expose our interface (the hl_gamemovement block is #ifdef'd out for PORTAL).
static CPortalGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPortalGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
