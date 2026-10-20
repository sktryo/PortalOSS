//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client-side implementation of the portal entity
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_prop_portal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( prop_portal, C_Prop_Portal );

BEGIN_RECV_TABLE( C_Prop_Portal, DT_Prop_Portal )
	RecvPropBool( RECVINFO( m_bActivated ) ),
	RecvPropInt( RECVINFO( m_iPortalType ) ),
	RecvPropEHandle( RECVINFO( m_hLinkedPortal ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_vPortalCorners ),
		RecvPropVector( RECVINFO( m_vPortalCorners[0] ) ) ),
END_RECV_TABLE()

C_Prop_Portal::C_Prop_Portal( void )
{
	m_iPortalType = PORTAL_TYPE_NONE;
	m_bDrawInPortalRender = true;

	// The server registers portals when they spawn; the client entity is
	// created from the network stream, so it has to register itself.
	CProp_Portal_Shared::AddToAllPortals( this );
}

C_Prop_Portal::~C_Prop_Portal( void )
{
}

void C_Prop_Portal::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Rebuild the surface plane from the freshly received corners and refresh
	// the pair transform. If the linked portal has not received its corners
	// yet, RecalculatePortalTransforms() leaves the matrices at identity and
	// the linked portal's own update will fill them in.
	UpdatePortalPlaneFromCorners();
	RecalculatePortalTransforms();
}

bool C_Prop_Portal::ShouldDraw( void )
{
	// Nothing to draw until the corner data has arrived from the server.
	return HasValidCorners() && BaseClass::ShouldDraw();
}

C_Prop_Portal *UTIL_Portal_TraceRay_Beam( const Ray_t &rayBeam, unsigned int mask, ITraceFilter *pTraceFilter, float *fEndFraction )
{
	trace_t tr;
	enginetrace->TraceRay( rayBeam, mask, pTraceFilter, &tr );

	float fPortalFraction = 1.0f;
	C_Prop_Portal *pPortal = UTIL_Portal_FirstAlongRay( rayBeam, fPortalFraction );

	// If the raw trace hits something before the portal plane, the beam is
	// blocked and must end at that hit instead of continuing through.
	if ( pPortal == NULL || tr.fraction < fPortalFraction - 0.001f )
	{
		if ( fEndFraction )
			(*fEndFraction) = tr.fraction;

		return NULL;
	}

	// The beam is clipped at the portal plane; the caller transforms the
	// remainder into the linked portal's space and draws a sub-beam.
	if ( fEndFraction )
		(*fEndFraction) = fPortalFraction;

	return pPortal;
}
