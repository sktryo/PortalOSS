//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared class definitions for prop_portal and CProp_Portal_Shared.
//
// This file is compiled into both the client and server DLLs. The shared base
// class keeps the networked portal data, and the server (CProp_Portal) and
// client (C_Prop_Portal) subclasses add their own behaviors. This mirrors the
// original Portal branch layout so the leftover #ifdef PORTAL hooks in the
// 2013 SDK source compile and link.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PROP_PORTAL_SHARED_H
#define PROP_PORTAL_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "mathlib/vmatrix.h"
#include "portal_shareddefs.h"

#ifdef CLIENT_DLL
class C_Prop_Portal;
#define PORTAL_CLASSNAME C_Prop_Portal
#else
class CProp_Portal;
#define PORTAL_CLASSNAME CProp_Portal
#endif

//-----------------------------------------------------------------------------
// Shared portal data
//-----------------------------------------------------------------------------
class CProp_Portal_Shared : public CBaseEntity
{
public:
	DECLARE_CLASS( CProp_Portal_Shared, CBaseEntity );

	CProp_Portal_Shared( void );
	virtual ~CProp_Portal_Shared( void );

	// NOTE: This is the plane of the surface the portal is attached to.
	// The normal points OUT of the wall (toward the room the portal opens into).
	cplane_t				m_plane_Origin;

	// Corners of the portal rectangle, order: BL, BR, TR, TL.
	// Networked so the client can rebuild the plane and the pair transform.
	CNetworkArray( Vector, m_vPortalCorners, 4 );

	// Link state
	CNetworkVar( bool, m_bActivated );
	CNetworkHandle( PORTAL_CLASSNAME, m_hLinkedPortal );

	// Transform snapshots (not networked; server sets the shared computed
	// matrix on both portals of a pair)
	VMatrix				m_matThisToLinked;
	VMatrix				m_matLinkedToThis;

public:
	bool					IsActivatedAndLinked( void ) const;

	// True once all four corners describe a non-degenerate rectangle. Client
	// portals briefly have no corner data before their first network update.
	bool					HasValidCorners( void ) const;

	// Fills in m_matThisToLinked / m_matLinkedToThis from the current link
	// state. The server calls this when a link is set; the client calls it
	// whenever network updates arrive. Recalculating either portal updates
	// both directions for the pair.
	void					RecalculatePortalTransforms( void );

	void					UpdatePortalPlaneFromCorners( void );

	VMatrix					MatrixThisToLinked( void ) const;
	VMatrix					MatrixLinkedToThis( void ) const;

	const Vector			&GetPortalOrigin( void ) const;
	const Vector			&GetPortalNormal( void ) const { return m_plane_Origin.normal; }

	// Active portal list
	static CUtlVector<PORTAL_CLASSNAME *>	AllPortals;
	static void				AddToAllPortals( PORTAL_CLASSNAME *pPortal );
	static void				RemoveFromAllPortals( PORTAL_CLASSNAME *pPortal );
	static void				ClearAllPortals( void );
};

//-----------------------------------------------------------------------------
// Server-side portal entity (classname prop_portal)
//-----------------------------------------------------------------------------
#if !defined( CLIENT_DLL )

class CProp_Portal : public CProp_Portal_Shared
{
public:
	DECLARE_CLASS( CProp_Portal, CProp_Portal_Shared );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CProp_Portal( void );
	virtual ~CProp_Portal( void );

	// Portal identity
	int						GetPortalType( void );
	void					SetPortalType( PortalType iPortalType );

	// Lifetime
	void					Precache( void );
	void					Spawn( void );
	virtual void			Activate( void );
	virtual void			UpdateOnRemove( void );
	virtual int				OnTakeDamage( const CTakeDamageInfo &info );

	// Linking
	void					LinkPortal( CProp_Portal *pLinkedPortal );
	void					UnLinkPortal( void );

	// Map-logic control (prop_portal inputs: SetActivatedState / Fizzle /
	// NewLocation). SetPortalActive(false) drops the link but keeps the entity.
	void					SetPortalActive( bool bActive );
	void					InputSetActivatedState( inputdata_t &inputdata );
	void					InputFizzle( inputdata_t &inputdata );
	void					InputNewLocation( inputdata_t &inputdata );

	// Derive the opening's corners from the entity origin/angles (map placed).
	void					BuildCornersFromOriginAngles( void );

	// Fires OnPlacedSuccessfully for map logic that watches gun placements.
	void					FirePlacedOutput( void ) { m_OnPlacedSuccessfully.FireOutput( this, this ); }

	// Corners are passed in the same order they are stored: BL, BR, TR, TL.
	void					SetupPortalCorners( const Vector &vBottomLeft, const Vector &vBottomRight,
												const Vector &vTopRight, const Vector &vTopLeft );

	// Portal placement lifecycle
	bool					FindPlacementExtents( const Vector &vPlacementOrigin, const cplane_t &tracePlane,
												  float &fAvailableHalfWidth, float &fAvailableHalfHeight );

	// Fizzle (the type is forwarded to the effect; the default is a manual kill)
	void					Fizzle( PortalFizzleType_t fizzleType = PORTAL_FIZZLE_INVALID );
	void					DoFizzleEffect( PortalFizzleType_t fizzleType, bool bFailedToOpenPortal );

	// Extent tests
	bool					IsTouchingPortal( void );
	bool					IsTouchingPortal( const Vector &vecCenter, const Vector &vecExtents );

private:
	CNetworkVar( int, m_iPortalType );
	string_t				m_strLinkedPortalName;
	int						m_iLinkageGroupID;		// "LinkageGroupID" (single-player always 0)
	bool					m_bStartActivated;		// "Activated" keyvalue
	COutputEvent			m_OnPlacedSuccessfully;
};

//-----------------------------------------------------------------------------
// Server portal placement helpers
//-----------------------------------------------------------------------------
// Shared placement path used by the portal gun and map spawner entities:
// creates a prop_portal on the given surface point, fizzles the previous
// portal of the same color and overlapping other-color portals, and links it
// to the other color whenever that portal exists. Returns the new portal or
// NULL when the placement failed.
CProp_Portal *Portal_PlacePortal( PortalType type, const cplane_t &tracePlane, const Vector &vecPortalCenter );

// Do the two portals share their wall face and overlap in-plane?
bool Portal_RectanglesOverlap( CProp_Portal *pA, CProp_Portal *pB );

#endif // !defined( CLIENT_DLL )

#endif // PROP_PORTAL_SHARED_H
