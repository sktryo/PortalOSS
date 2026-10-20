//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: trigger_portal_cleanser - fizzes portals and dissolves entities
//			inside the brush
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "triggers.h"
#include "prop_portal_shared.h"
#include "portal_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CTriggerPortalCleanser : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerPortalCleanser, CBaseTrigger );
	DECLARE_DATADESC();

	void	Spawn( void );

	// fizzles portals overlapping the brush, and dissolves cubes that touch it
	void	CleanserThink( void );
	void	StartTouch( CBaseEntity *pOther );

private:
	bool	IsPointInsideCleanser( const Vector &vecPoint );
};

LINK_ENTITY_TO_CLASS( trigger_portal_cleanser, CTriggerPortalCleanser );

BEGIN_DATADESC( CTriggerPortalCleanser )
	DEFINE_THINKFUNC( CleanserThink ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerPortalCleanser::Spawn( void )
{
	BaseClass::Spawn();

	InitTrigger();

	SetThink( &CTriggerPortalCleanser::CleanserThink );
	SetNextThink( gpGlobals->curtime + 0.25f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerPortalCleanser::IsPointInsideCleanser( const Vector &vecPoint )
{
	Vector vecMins, vecMaxs;
	CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );

	for ( int i = 0; i < 3; ++i )
	{
		if ( vecPoint[i] < vecMins[i] || vecPoint[i] > vecMaxs[i] )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerPortalCleanser::CleanserThink( void )
{
	//fizzle any active portals intersecting the brush
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = 0; i < iPortalCount; ++i )
	{
		CProp_Portal *pPortal = CProp_Portal_Shared::AllPortals[i];
		if ( pPortal == NULL || pPortal->m_bActivated == false )
			continue;

		const Vector &vecPortalOrigin = pPortal->GetPortalOrigin();
		if ( IsPointInsideCleanser( vecPortalOrigin ) )
		{
			pPortal->Fizzle( PORTAL_FIZZLE_FIZZLER );
		}
	}

	SetNextThink( gpGlobals->curtime + 0.25f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerPortalCleanser::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	//cubes are destroyed on contact with the cleanser
	if ( FClassnameIs( pOther, "prop_weighted_cube" ) )
	{
		UTIL_Remove( pOther );
	}
}
