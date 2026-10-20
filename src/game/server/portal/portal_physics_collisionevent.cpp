//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal-specific collisions (implementation)
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_physics_collisionevent.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CPortal_CollisionEvent::CPortal_CollisionEvent( void )
{
}

void CPortal_CollisionEvent::PortalPostSimulationFrame( void )
{
	// Process damage events and queued physics operations before the caller
	// flushes touch events with FrameUpdate().
	PostSimulationFrame();
}
