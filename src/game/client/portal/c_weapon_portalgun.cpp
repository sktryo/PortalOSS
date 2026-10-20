//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side Portal weapon classes
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_weapon_portalgun.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_WeaponPortalBase, DT_WeaponPortalBase, CWeaponPortalBase )
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_WeaponPortalGun, DT_WeaponPortalGun, CWeaponPortalGun )
END_RECV_TABLE()

C_WeaponPortalBase::C_WeaponPortalBase( void )
{
}

C_WeaponPortalGun::C_WeaponPortalGun( void )
{
}
