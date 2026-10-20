//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal game movement. Player traversal through portals:
//			when the player pushes into an open, linked portal's opening,
//			origin, velocity and view angles are transformed through the
//			pair's ThisToLinked matrix before the regular movement runs, so
//			the rest of the tick carries the player out of the exit portal.
//
//			The exit point is projected back onto the linked portal's plane
//			(the transform maps points in front of the entrance to points
//			behind the exit wall) and pushed out along the exit normal far
//			enough that the player hull does not intersect the exit wall.
//
//			Traversal is skipped for a short time after a teleport so the
//			player can clear the exit plane before entry checks re-arm.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_gamemovement.h"
#include "portal_internal.h"
#include "portal_util_shared.h"
#include "portal_shareddefs.h"
#include "prop_portal_shared.h"

#ifdef CLIENT_DLL
#include "c_prop_portal.h"
#include "c_baseplayer.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// How close the player origin has to get to a portal on a wall before the
// traversal fires. The player hull (half width 16) stops just short of the
// wall face, so this has to reach past the skin thickness.
#define PORTAL_ENTRY_WALL_THRESHOLD			26.0f

// Portals on horizontal surfaces are entered simply by standing on the
// opening; the ground trace keeps the origin a few units above the plane.
#define PORTAL_ENTRY_FLOOR_THRESHOLD		6.0f

// How long after a traversal before the same portal pair becomes an entry
// candidate again. Without this the arrival position on/next to the exit
// plane would immediately re-trigger the pair.
#define PORTAL_PAIR_TRAVERSE_COOLDOWN		0.25f

// Minimum speed into a wall/ceiling portal for the traversal to fire (the
// player has to actually be moving into the surface).
#define PORTAL_ENTRY_MIN_SPEED				10.0f

// Minimum speed along the exit normal after arriving, so the player never
// hangs motionless halfway inside the exit wall.
#define PORTAL_EXIT_MIN_NORMAL_SPEED		50.0f

//-----------------------------------------------------------------------------
// Recent traversal bookkeeping: after a teleport, entry checks on the same
// portal pair (in both directions) are held off for a moment.
//-----------------------------------------------------------------------------
struct PortalTraverseRecord_t
{
	PORTAL_PTR_TYPE *pPortal;
	PORTAL_PTR_TYPE *pLinked;
	float			flTime;
};

static CUtlVector<PortalTraverseRecord_t> s_TraverseRecords;

static bool Portal_PairIsCoolingDown( PORTAL_PTR_TYPE *pEntry )
{
	if ( s_TraverseRecords.Count() == 0 )
		return false;

	float flNow = gpGlobals->curtime;
	for ( int i = s_TraverseRecords.Count() - 1; i >= 0; --i )
	{
		PortalTraverseRecord_t &record = s_TraverseRecords.Element( i );
		if ( flNow - record.flTime > PORTAL_PAIR_TRAVERSE_COOLDOWN )
		{
			s_TraverseRecords.FastRemove( i );
			continue;
		}

		if ( record.pPortal == pEntry || record.pLinked == pEntry )
			return true;
	}

	return false;
}

static void Portal_NoteTraversal( PORTAL_PTR_TYPE *pEntry )
{
	PortalTraverseRecord_t &record = s_TraverseRecords[ s_TraverseRecords.AddToTail() ];
	record.pPortal = pEntry;
	record.pLinked = pEntry->m_hLinkedPortal;
	record.flTime = gpGlobals->curtime;

	// Trim history so the vector cannot grow without bound
	while ( s_TraverseRecords.Count() > 16 )
		s_TraverseRecords.Remove( 0 );
}

//-----------------------------------------------------------------------------
// Portal axes/status helpers
//-----------------------------------------------------------------------------

CPortalGameMovement::CPortalGameMovement( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Find the portal the player is entering this tick. Returns false
//			when no qualifying opening is found.
//-----------------------------------------------------------------------------
bool CPortalGameMovement::PickPortalEntry( CBasePlayer *pPlayer, const Vector &vecOrigin, const Vector &vecVelocity,
										   PORTAL_PTR_TYPE **ppEntry, Vector &vecExitOrigin,
										   Vector &vecExitVelocity, QAngle &angExitView )
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = 0; i != iPortalCount; ++i )
	{
		PORTAL_PTR_TYPE *pPortal = CProp_Portal_Shared::AllPortals[i];
		if ( pPortal == NULL || !pPortal->IsActivatedAndLinked() )
			continue;

		if ( Portal_PairIsCoolingDown( pPortal ) )
			continue;

		Vector vecLocal;
		Portal_WorldToLocal( pPortal, vecOrigin, vecLocal );

		// The player origin has to be within the opening (plus the skin
		// slack) horizontally, otherwise the hull is entering solid wall
		// beside the portal and the normal collision keeps it out.
		if ( Portal_PointIsInPortalViewport( pPortal, vecLocal, PORTAL_SKIN_LENGTH ) == false )
			continue;

		const Vector &vecNormal = pPortal->m_plane_Origin.normal;
		float flOriginDistance = vecLocal.z;	// distance in front of the plane

		if ( fabs( vecNormal.z ) > 0.7f )
		{
			// Portal on a floor or ceiling. On a floor, resting, walking, or
			// falling onto the opening all drop the player through; on a
			// ceiling the player has to move into it.
			bool bFloorPortal = ( vecNormal.z > 0.0f );

			if ( flOriginDistance < -2.0f || ( bFloorPortal ? flOriginDistance > PORTAL_ENTRY_FLOOR_THRESHOLD
															: flOriginDistance > PORTAL_ENTRY_WALL_THRESHOLD ) )
				continue;

			float flNormalVelocity = DotProduct( vecVelocity, vecNormal );
			if ( flNormalVelocity > PORTAL_ENTRY_MIN_SPEED &&
				 pPlayer->GetGroundEntity() == NULL )
			{
				// Actively moving out of the opening (rising through a floor
				// portal area); the normal collision owns the player here.
				continue;
			}
		}
		else
		{
			// Portal on a wall: entered by pressing into the opening.
			if ( flOriginDistance < 0.0f || flOriginDistance > PORTAL_ENTRY_WALL_THRESHOLD )
				continue;

			float flNormalVelocity = DotProduct( vecVelocity, vecNormal );
			if ( flNormalVelocity >= -PORTAL_ENTRY_MIN_SPEED )
				continue;
		}

		// This portal qualifies; compute the exit transform
		PORTAL_PTR_TYPE *pLinked = pPortal->m_hLinkedPortal;
		const VMatrix &matThisToLinked = pPortal->MatrixThisToLinked();

		// Map the player origin through the link. Everything in front of the
		// entrance plane maps to behind the exit plane (its wall), so project
		// back onto the plane first.
		Vector vecMapped;
		UTIL_Portal_PointTransform( matThisToLinked, vecOrigin, vecMapped );

		const cplane_t &planeExit = pLinked->m_plane_Origin;
		float flBehind = DotProduct( vecMapped, planeExit.normal ) - planeExit.dist;
		vecExitOrigin = vecMapped - planeExit.normal * flBehind;

		// Push the arrival point out along the exit normal so the player hull
		// starts clear of the exit wall.
		float flExitOffset = 26.0f;
		if ( planeExit.normal.z <= -0.7f )
			flExitOffset = 78.0f;					// exiting downward through a ceiling portal
		else if ( planeExit.normal.z >= 0.7f )
			flExitOffset = 6.0f;					// exiting upward through a floor portal

		vecExitOrigin += planeExit.normal * flExitOffset;

		// Speed out of the exit plane; the transformed velocity already has
		// this guaranteed except for the "just standing" floor cases, where
		// a nudge in the exit direction keeps gravity from re-dropping the
		// player back onto the opening while it is still cooling down.
		UTIL_Portal_VectorTransform( matThisToLinked, vecVelocity, vecExitVelocity );
		float flExitNormalVelocity = DotProduct( vecExitVelocity, planeExit.normal );
		if ( flExitNormalVelocity < PORTAL_EXIT_MIN_NORMAL_SPEED )
		{
			vecExitVelocity += planeExit.normal * ( PORTAL_EXIT_MIN_NORMAL_SPEED - flExitNormalVelocity );
		}

		UTIL_Portal_AngleTransform( matThisToLinked, mv->m_vecAngles, angExitView );

		if ( ppEntry != NULL )
			*ppEntry = pPortal;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Write the traversal results into the move data and player state.
//-----------------------------------------------------------------------------
void CPortalGameMovement::ApplyPortalTraversal( CBasePlayer *pPlayer, CMoveData *pMove,
												const Vector &vecExitOrigin, const Vector &vecExitVelocity,
												const QAngle &angExitView )
{
	pMove->SetAbsOrigin( vecExitOrigin );
	pMove->m_vecVelocity = vecExitVelocity;
	pMove->m_vecAngles = angExitView;
	pMove->m_vecViewAngles = angExitView;
	pMove->m_vecAbsViewAngles = angExitView;

	pPlayer->pl.v_angle = angExitView;
	pPlayer->SetGroundEntity( NULL );

#ifdef CLIENT_DLL
	// The client camera is driven by the engine's persistent view angles, so
	// rotate them once per command so mouse input keeps its new frame of
	// reference. Prediction re-runs the same command with the same angles, so
	// guarding on the command number keeps re-prediction from applying the
	// rotation repeatedly.
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( pLocal == pPlayer )
	{
		int nCommandNumber = pPlayer->CurrentCommandNumber();
		static int s_nLastRotatedCommand = -1;
		if ( nCommandNumber != s_nLastRotatedCommand )
		{
			s_nLastRotatedCommand = nCommandNumber;
			QAngle angCamera = angExitView;
			engine->SetViewAngles( angCamera );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Top level movement entry; performs portal traversal before the
//			regular move, then lets the remainder of the tick eject the
//			player from the exit portal.
//-----------------------------------------------------------------------------
void CPortalGameMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	if ( pPlayer == NULL || pMove == NULL )
	{
		BaseClass::ProcessMovement( pPlayer, pMove );
		return;
	}

	// Only moving players traverse wall/ceiling portals; but a player
	// standing still on top of a floor portal must also fall through, so
	// grounded players always run the entry check (wall entries still
	// require meaningful inward velocity).
	bool bMotivatedToMove = ( pMove->m_vecVelocity.LengthSqr() > 25.0f ||
							  pPlayer->GetGroundEntity() != NULL );
	if ( bMotivatedToMove &&
		 pPlayer->GetMoveType() == MOVETYPE_WALK &&
		 pPlayer->IsAlive() && !pPlayer->IsObserver() )
	{
		PORTAL_PTR_TYPE *pEntry = NULL;
		Vector vecEntryOrigin( pMove->GetAbsOrigin() );
		Vector vecEntryVelocity( pMove->m_vecVelocity );
		Vector vecExitOrigin, vecExitVelocity;
		QAngle angExitView;

		if ( PickPortalEntry( pPlayer, vecEntryOrigin, vecEntryVelocity, &pEntry, vecExitOrigin, vecExitVelocity, angExitView ) )
		{
			// Teleport first at the start of the tick; the regular move that
			// follows then carries the player the rest of the way out of the
			// exit portal.
			Portal_NoteTraversal( pEntry );
			ApplyPortalTraversal( pPlayer, pMove, vecExitOrigin, vecExitVelocity, angExitView );
		}
	}

	BaseClass::ProcessMovement( pPlayer, pMove );
}

// Expose our interface (the hl_gamemovement block is #ifdef'd out for PORTAL).
static CPortalGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPortalGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
