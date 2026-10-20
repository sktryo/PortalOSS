//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CPortalSimulator implementation - keeps vphysics state around
//			portals and teleports props fully through them.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "PortalSimulator.h"
#include "portal_internal.h"
#include "portal_util_shared.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct PropTrackRecord_t
{
	CBaseEntity		*pEntity;
	Vector			vecPrevOrigin;
};

static CUtlVector<PropTrackRecord_t> s_PropTrackRecords;

void CPortalSimulator::PrePhysFrame( void )
{
	s_PropTrackRecords.RemoveAll();

	// NOTE: nothing to track when no portals are activated (fast path)
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	bool bAnyActive = false;
	for ( int i = 0; i != iPortalCount; ++i )
	{
		PORTAL_PTR_TYPE *pPortal = CProp_Portal_Shared::AllPortals[i];
		if ( pPortal && pPortal->IsActivatedAndLinked() )
		{
			bAnyActive = true;
			break;
		}
	}
	if ( !bAnyActive )
		return;

	// Check the prev positions of every vphysics prop
	for ( CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity; pEntity = gEntList.NextEnt( pEntity ) )
	{
		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if ( pPhysics == NULL || pPhysics->IsStatic() || pPhysics->IsAsleep() )
			continue;

		PropTrackRecord_t &record = s_PropTrackRecords[ s_PropTrackRecords.AddToTail() ];
		record.pEntity = pEntity;

		Vector vecOrigin = pEntity->GetAbsOrigin();
		record.vecPrevOrigin = vecOrigin;
	}
}

void CPortalSimulator::PostPhysFrame( void )
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if ( iPortalCount == 0 )
	{
		s_PropTrackRecords.RemoveAll();
		return;
	}

	for ( int i = 0; i != s_PropTrackRecords.Count(); ++i )
	{
		PropTrackRecord_t &record = s_PropTrackRecords[i];
		CBaseEntity *pEntity = record.pEntity;
		if ( pEntity == NULL || ( pEntity->IsEFlagSet( EFL_KILLME ) ) )
			continue;

		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if ( pPhysics == NULL )
			continue;

		Vector vecCurrOrigin = pEntity->GetAbsOrigin();
		Vector vecPrevOrigin = record.vecPrevOrigin;

		// Skip if it didn't move this frame
		if ( vecCurrOrigin.DistToSqr( vecPrevOrigin ) < 0.01f )
			continue;

		// Find the portal this object crossed front->back this frame
		for ( int j = 0; j != iPortalCount; ++j )
		{
			PORTAL_PTR_TYPE *pPortal = CProp_Portal_Shared::AllPortals[j];
			if ( pPortal == NULL || !pPortal->IsActivatedAndLinked() )
				continue;

			float flFraction;
			Vector vecCross;
			if ( !Portal_SegmentPassesThroughPortal( pPortal, vecPrevOrigin, vecCurrOrigin, flFraction, &vecCross ) )
				continue;

			// Teleport the object through the linked portal
			const VMatrix &matThisToLinked = pPortal->MatrixThisToLinked();
			Vector vecTransformed;
			UTIL_Portal_PointTransform( matThisToLinked, vecCurrOrigin, vecTransformed );

			// The transform lands the object behind the exit wall; push it out
			// to just in front of the exit plane so vphysics never sees it
			// inside the solid.
			const cplane_t &planeExit = pPortal->m_hLinkedPortal->m_plane_Origin;
			float flBehind = DotProduct( vecTransformed, planeExit.normal ) - planeExit.dist;
			vecTransformed += planeExit.normal * ( PORTAL_SKIN_LENGTH + 2.0f - flBehind );

			Vector vecVelocity;
			AngularImpulse angImpulse;
			pPhysics->GetVelocity( &vecVelocity, &angImpulse );

			Vector vecNewVelocity;
			AngularImpulse angNewImpulse;
			UTIL_Portal_VectorTransform( matThisToLinked, vecVelocity, vecNewVelocity );
			UTIL_Portal_VectorTransform( matThisToLinked, angImpulse, angNewImpulse );

			pEntity->Teleport( &vecTransformed, NULL, NULL );
			pPhysics->SetVelocity( &vecNewVelocity, &angNewImpulse );

			// A held prop that goes through without its carrier tears out of the
			// grab controller's constraint and gets yanked back through the
			// plane (oscillating); drop the carry instead, which is also how
			// pushing an object through a portal by hand works.
			CBasePlayer *pHeldBy = pEntity->HasPhysicsAttacker( 0.5f );
			if ( pHeldBy != NULL && ( pPhysics->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
			{
				pHeldBy->ForceDropOfCarriedPhysObjects( pEntity );
			}
			break;
		}
	}

	s_PropTrackRecords.RemoveAll();
}
