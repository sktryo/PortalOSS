//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CPortalSimulator - keeps vphysics state around portals and moves
//			props fully through them when they cross a portal plane.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTALSIMULATOR_H
#define PORTALSIMULATOR_H
#ifdef _WIN32
#pragma once
#endif

class CPortalSimulator
{
public:
	// Called at the start of each physics frame (physics.cpp hook)
	static void PrePhysFrame( void );

	// Called after the vphysics simulation; teleports anything that crossed a
	// portal plane inside its opening during this frame
	static void PostPhysFrame( void );
};

// Wraps the regular physics frame with the portal simulation steps. Declared
// here so physics.cpp and the other server code can share it.
void PortalPhysFrame( float deltaTime );

#endif // PORTALSIMULATOR_H
