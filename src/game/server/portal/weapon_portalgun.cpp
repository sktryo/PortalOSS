//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Portal Gun implementation (server side)
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "weapon_portalgun.h"
#include "in_buttons.h"
#include "prop_portal_shared.h"
#include "portal_util_shared.h"
#include "portal_shareddefs.h"
#include "portal_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PORTAL_SHOOT_RANGE		( MAX_TRACE_LENGTH * 0.5f )

LINK_ENTITY_TO_CLASS( weapon_portalgun, CWeaponPortalGun );
PRECACHE_WEAPON_REGISTER( weapon_portalgun );

IMPLEMENT_SERVERCLASS_ST( CWeaponPortalBase, DT_WeaponPortalBase )
END_SEND_TABLE()

BEGIN_DATADESC( CWeaponPortalBase )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CWeaponPortalGun, DT_WeaponPortalGun )
END_SEND_TABLE()

BEGIN_DATADESC( CWeaponPortalGun )
END_DATADESC()

void CWeaponPortalBase::PrimaryAttack( void )
{
}

void CWeaponPortalBase::SecondaryAttack( void )
{
}

void CWeaponPortalGun::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( "Weapon_Portalgun.fire_blue" );
	PrecacheScriptSound( "Weapon_Portalgun.fire_red" );
}

void CWeaponPortalGun::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	// The portalgun never runs dry, so we intercept attack buttons here
	if ( pPlayer->m_afButtonPressed & IN_ATTACK )
	{
		if ( gpGlobals->curtime >= m_flNextPrimaryAttack )
		{
			PrimaryAttack();
			WeaponSound( SINGLE );
			pPlayer->DoMuzzleFlash();
			SendWeaponAnim( GetPrimaryAttackActivity() );
			pPlayer->SetAnimation( PLAYER_ATTACK1 );
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.3f;
		}
	}
	else if ( pPlayer->m_afButtonPressed & IN_ATTACK2 )
	{
		if ( gpGlobals->curtime >= m_flNextSecondaryAttack )
		{
			SecondaryAttack();
			WeaponSound( SPECIAL2 );
			pPlayer->DoMuzzleFlash();
			SendWeaponAnim( GetPrimaryAttackActivity() );
			pPlayer->SetAnimation( PLAYER_ATTACK1 );
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.3f;
		}
	}

	WeaponIdle();
}

void CWeaponPortalGun::WeaponIdle( void )
{
	// The portalgun has no idle behavior; base class would zoom-check
}

void CWeaponPortalGun::PrimaryAttack( void )
{
	FirePortal( PORTAL_TYPE_BLUE );
}

void CWeaponPortalGun::SecondaryAttack( void )
{
	FirePortal( PORTAL_TYPE_ORANGE );
}

bool CWeaponPortalGun::FirePortal( PortalType type )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return false;

	// The portalgun fires a single shot beam that goes through open portals
	Vector vecSrc = pPlayer->EyePosition();
	Vector vecDir;
	AngleVectors( pPlayer->EyeAngles(), &vecDir );

	Ray_t ray;
	ray.Init( vecSrc, vecSrc + vecDir * PORTAL_SHOOT_RANGE );

	CGameTrace tr;
	UTIL_Portal_TraceRay( ray, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction >= 1.0f || !tr.DidHitWorld() )
		return false;

	// Don't place portals on the backside of surfaces
	if ( DotProduct( tr.plane.normal, vecDir ) > 0.2f )
		return false;

	// The placement origin sits just outside the wall face
	Vector vecPortalCenter = tr.endpos + tr.plane.normal * PORTAL_SKIN_LENGTH;

	CProp_Portal *pNewPortal = ( CProp_Portal * )CreateEntityByName( "prop_portal" );
	if ( pNewPortal == NULL )
		return false;

	pNewPortal->SetPortalType( type );

	float flNewWidth = 0.0f;
	float flNewHeight = 0.0f;
	if ( !pNewPortal->FindPlacementExtents( vecPortalCenter, tr.plane, flNewWidth, flNewHeight ) )
	{
		UTIL_Remove( pNewPortal );
		return false;
	}

	// Direction frame for the portal opening
	Vector vecRight, vecUp;
	UTIL_Portal_GetSurfaceBasis( tr.plane.normal, vecRight, vecUp );

	Vector vLeftBottom = vecPortalCenter - vecRight * flNewWidth - vecUp * flNewHeight;
	Vector vRightBottom = vecPortalCenter + vecRight * flNewWidth - vecUp * flNewHeight;
	Vector vRightTop = vecPortalCenter + vecRight * flNewWidth + vecUp * flNewHeight;
	Vector vLeftTop = vecPortalCenter - vecRight * flNewWidth + vecUp * flNewHeight;

	pNewPortal->Spawn();
	pNewPortal->SetupPortalCorners( vLeftBottom, vRightBottom, vRightTop, vLeftTop );

	// Find any previous portal of the same color (kill it) and the other color
	// portal for linking
	CProp_Portal *pOtherColorPortal = NULL;
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = iPortalCount - 1; i >= 0; --i )
	{
		CProp_Portal *pPortal = CProp_Portal_Shared::AllPortals.Element( i );
		if ( pPortal == NULL || pPortal == pNewPortal )
			continue;

		// Remove any previous portal of the same color, whether it is still
		// linked or not. Otherwise a stale, dormant portal would survive.
		if ( pPortal->GetPortalType() == type )
		{
			pPortal->Fizzle( PORTAL_FIZZLE_KILLED );
			continue;
		}

		// Remember the other color portal to link against. An unlinked
		// (dormant) portal is still a valid link target: this is what makes
		// the first pair of portals active.
		if ( ( ( type == PORTAL_TYPE_BLUE ) && ( pPortal->GetPortalType() == PORTAL_TYPE_ORANGE ) ) ||
			 ( ( type == PORTAL_TYPE_ORANGE ) && ( pPortal->GetPortalType() == PORTAL_TYPE_BLUE ) ) )
		{
			pOtherColorPortal = pPortal;
		}
	}

	if ( pOtherColorPortal != NULL )
	{
		pNewPortal->LinkPortal( pOtherColorPortal );
	}

	pNewPortal->Activate();
	return true;
}
