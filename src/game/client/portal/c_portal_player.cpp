//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The client-side Portal player class
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_portal_player.h"
#ifdef PORTAL
	#include "c_prop_portal.h"
	#include "prop_portal_shared.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_Portal_Player, DT_Portal_Player, CPortal_Player )
	RecvPropBool( RECVINFO( m_bSuppressCrosshair ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_Portal_Player )
	// No additional predicted fields; inherit the base player prediction data.
END_PREDICTION_DATA()

C_Portal_Player::C_Portal_Player( void )
{
	m_bSuppressCrosshair = false;
}

C_Portal_Player::~C_Portal_Player( void )
{
}

int C_Portal_Player::DrawModel( int flags )
{
	// In Portal this check is performed here rather than in C_BasePlayer::DrawModel
	if ( !ShouldDrawThisPlayer() )
	{
		return 0;
	}

	return BaseClass::DrawModel( flags );
}

C_Portal_Player *ToPortalPlayer( C_BasePlayer *pPlayer )
{
	if ( pPlayer == NULL )
		return NULL;

	return dynamic_cast<C_Portal_Player *>( pPlayer );
}

bool LocalPlayerIsCloseToPortal( void )
{
#ifdef PORTAL
	C_Portal_Player *pLocalPlayer = ToPortalPlayer( C_BasePlayer::GetLocalPlayer() );
	if ( pLocalPlayer == NULL )
		return false;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = 0; i < iPortalCount; ++i )
	{
		C_Prop_Portal *pPortal = CProp_Portal_Shared::AllPortals[i];
		if ( pPortal == NULL || pPortal->m_bActivated == false )
			continue;

		Vector vecToPortal = pLocalPlayer->GetAbsOrigin() - pPortal->GetPortalOrigin();
		if ( fabs( DotProduct( vecToPortal, pPortal->GetPortalNormal() ) ) < PORTAL_HALF_HEIGHT + 44.0f )
		{
			return true;
		}
	}
#endif // PORTAL

	return false;
}
