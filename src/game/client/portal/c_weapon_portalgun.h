//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side Portal weapon classes
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_WEAPON_PORTALGUN_H
#define C_WEAPON_PORTALGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlcombatweapon.h"

class C_WeaponPortalBase : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_WeaponPortalBase, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();

	C_WeaponPortalBase( void );
};

class C_WeaponPortalGun : public C_WeaponPortalBase
{
public:
	DECLARE_CLASS( C_WeaponPortalGun, C_WeaponPortalBase );
	DECLARE_CLIENTCLASS();

	C_WeaponPortalGun( void );
};

#endif // C_WEAPON_PORTALGUN_H
