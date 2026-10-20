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
#include "portal_internal.h"
#include "portal_shareddefs.h"
#include "portal_player.h"
#include "te.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PORTAL_SHOOT_RANGE		( MAX_TRACE_LENGTH * 0.5f )
#define PORTAL_BEAM_NUDGE		0.25f

// Sprite used for the one-shot portal device beam. The material is only
// present in the mounted Portal/HL2 content; when missing the beam is fizzle-quit
// quietly on the client.
#define PORTAL_GUN_BEAM_MATERIAL	"sprites/laserbeam.vmt"

static int s_iPortalGunBeamModelIndex = 0;

LINK_ENTITY_TO_CLASS( weapon_portalgun, CWeaponPortalGun );
PRECACHE_WEAPON_REGISTER( weapon_portalgun );

//-----------------------------------------------------------------------------
// Purpose: Do the two portals' openings sit on the same part of the same wall?
//			Portals sharing a plane and overlapping in-plane cannot coexist.
//-----------------------------------------------------------------------------
static bool s_PortalRectanglesOverlap( CProp_Portal *pA, CProp_Portal *pB )
{
	if ( pA == NULL || pB == NULL )
		return false;

	// A different face is never overlapping
	if ( DotProduct( pA->GetPortalNormal(), pB->GetPortalNormal() ) < 0.9f )
		return false;

	// Different plane offsets are separated along the normal
	float flPlaneDelta = fabs( DotProduct( pA->GetPortalNormal(), pB->GetPortalOrigin() ) - pA->m_plane_Origin.dist );
	if ( flPlaneDelta > 8.0f )
		return false;

	// Axis-aligned overlap test on the world bounds of the corners
	Vector vecMinA, vecMaxA, vecMinB, vecMaxB;
	ClearBounds( vecMinA, vecMaxA );
	ClearBounds( vecMinB, vecMaxB );
	for ( int i = 0; i < 4; ++i )
	{
		AddPointToBounds( pA->m_vPortalCorners[i], vecMinA, vecMaxA );
		AddPointToBounds( pB->m_vPortalCorners[i], vecMinB, vecMaxB );
	}

	if ( vecMinA.x > vecMaxB.x || vecMinB.x > vecMaxA.x ||
		 vecMinA.y > vecMaxB.y || vecMinB.y > vecMaxA.y ||
		 vecMinA.z > vecMaxB.z || vecMinB.z > vecMaxA.z )
		return false;

	return true;
}

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

	s_iPortalGunBeamModelIndex = PrecacheModel( PORTAL_GUN_BEAM_MATERIAL );
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

//-----------------------------------------------------------------------------
// Purpose: Draw the portal device's firing beam. When the trace crosses an
//			open portal the beam is split into a segment in each room, with
//			the second segment drawn in the linked portal's space (the trace
//			contract already returns that space's coordinates in endpos).
//-----------------------------------------------------------------------------
static void s_PortalGunDrawBeam( CBasePlayer *pPlayer, const Vector &vecStart,
								const Ray_t &ray, const CGameTrace &tr, PortalType type )
{
	CBroadcastRecipientFilter filter;

	int r = 0, g = 160, b = 255;					// blue by default
	if ( type == PORTAL_TYPE_ORANGE )
	{
		r = 255; g = 160; b = 0;
	}

	// The beam continuation when the shot fell through an open portal
	Vector vecCrossPoint;
	float flPortalFraction = 1.0f;
	PORTAL_PTR_TYPE *pEntrance = UTIL_Portal_FirstAlongRay( ray, flPortalFraction );
	bool bCrossedPortal = ( pEntrance != NULL &&
							flPortalFraction < tr.fraction &&
							Portal_SegmentPassesThroughPortal( pEntrance, ray.m_Start, ray.m_Start + ray.m_Delta, flPortalFraction, &vecCrossPoint ) );

	te->BeamPoints( filter, 0.0f, &vecStart, &vecCrossPoint, s_iPortalGunBeamModelIndex,
					0, 0, 0, 0.2f, 2.0f, 1.0f, 0, 0.0f, r, g, b, 255, 0 );

	if ( !bCrossedPortal )
		return;

	// Second beam half: transform the crossing point into the linked room.
	const VMatrix &matThisToLinked = pEntrance->MatrixThisToLinked();
	Vector vecExitPoint;
	UTIL_Portal_PointTransform( matThisToLinked, vecCrossPoint, vecExitPoint );

	const cplane_t &planeExit = pEntrance->m_hLinkedPortal->m_plane_Origin;
	vecExitPoint += planeExit.normal * PORTAL_BEAM_NUDGE;

	te->BeamPoints( filter, 0.0f, &vecExitPoint, &tr.endpos, s_iPortalGunBeamModelIndex,
					0, 0, 0, 0.2f, 2.0f, 1.0f, 0, 0.0f, r, g, b, 255, 0 );
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

	// Draw the firing beam (world hit anywhere, valid surface or not) before
	// deciding on placement so failed shots are still visible.
	s_PortalGunDrawBeam( pPlayer, vecSrc, ray, tr, type );

	// Don't place portals on surfaces that can't hold them: no-draw faces,
	// sky faces, and explicit no-portal surfaces.
	if ( tr.surface.flags & ( SURF_NODRAW | SURF_SKY | SURF_SKY2D | SURF_NOPORTAL | SURF_NOLIGHT ) )
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
			// The other color portal cannot survive the new portal covering
			// the same spot: fizzle it and keep looking for a link target.
			if ( s_PortalRectanglesOverlap( pNewPortal, pPortal ) )
			{
				pPortal->Fizzle( PORTAL_FIZZLE_KILLED );
				continue;
			}

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
