//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Portal Gun. Primary fire places a blue portal, secondary fire
//			places an orange one. Shots trace through open portals.
//
// $NoKeywords: $
//===========================================================================//

#ifndef WEAPON_PORTALGUN_H
#define WEAPON_PORTALGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon_shared.h"
#include "portal_shareddefs.h"

class CWeaponPortalBase : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponPortalBase, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	void PrimaryAttack( void );
	void SecondaryAttack( void );
};

class CWeaponPortalGun : public CWeaponPortalBase
{
public:
	DECLARE_CLASS( CWeaponPortalGun, CWeaponPortalBase );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	void	Precache( void );

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );

	// No ammo/clip usage
	void	ItemPostFrame( void );

	virtual bool	Reload( void ) { return false; }
	virtual void	WeaponIdle( void );

private:
	bool	FirePortal( PortalType type );
};

#endif // WEAPON_PORTALGUN_H
