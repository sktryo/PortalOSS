//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal-specific collisions. Creates special collision rules for
//			props that are moving through portals.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_PHYSICS_COLLISIONEVENT_H
#define PORTAL_PHYSICS_COLLISIONEVENT_H
#ifdef _WIN32
#pragma once
#endif

#include "physics_collisionevent.h"		// base class CCollisionEvent

class CPortal_CollisionEvent : public CCollisionEvent
{
public:
	CPortal_CollisionEvent( void );

	// Called by PortalPhysFrame before the buffered touch events are flushed.
	void PortalPostSimulationFrame( void );
};

#endif // PORTAL_PHYSICS_COLLISIONEVENT_H
