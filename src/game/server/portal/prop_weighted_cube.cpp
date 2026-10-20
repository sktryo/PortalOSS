//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: prop_weighted_cube - the weighted companion cube
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "props.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPropWeightedCube : public CPhysicsProp
{
public:
	DECLARE_CLASS( CPropWeightedCube, CPhysicsProp );
	DECLARE_DATADESC();

	void	Precache( void );
	void	Spawn( void );

private:
};

LINK_ENTITY_TO_CLASS( prop_weighted_cube, CPropWeightedCube );

BEGIN_DATADESC( CPropWeightedCube )
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::Precache( void )
{
	// Use the default cube model when the map doesn't specify one
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/metal_box.mdl" ) );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::Spawn( void )
{
	// Use the default cube model when the map doesn't specify one
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/metal_box.mdl" ) );
	}

	BaseClass::Spawn();
}
