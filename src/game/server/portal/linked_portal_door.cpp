//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: linked_portal_door - pairs of sliding doors that carry a portal on
//			their face. While a door is closed its portal cannot be used; when
//			it opens fully the portal appears on the door's resting plane and
//			links to the other color portal (the other door of the pair).
//			Approximates the Portal branch entity: the pairing of two doors
//			emerges from the normal blue/orange link rules.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "prop_portal_shared.h"
#include "portal_util_shared.h"
#include "portal_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Door travel: how far the panel slides down when it opens
#define PORTAL_DOOR_OPEN_DISTANCE		96.0f
#define PORTAL_DOOR_MOVE_TIME			1.0f

class CLinkedPortalDoor : public CBaseAnimating
{
public:
	DECLARE_CLASS( CLinkedPortalDoor, CBaseAnimating );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	Precache( void );
	void	Think( void );

	// Inputs
	void	InputOpen( inputdata_t &inputdata );
	void	InputClose( inputdata_t &inputdata );
	void	InputToggle( inputdata_t &inputdata );

private:
	void	SetState( bool bOpening );
	void	AnimateMove( void );
	void	FinishMove( void );

	void	PlacePortal( void );
	void	RemovePortal( void );

	// Movement
	Vector	m_vecRestOrigin;			// door position when closed
	bool	m_bOpening;
	bool	m_bOpen;
	float	m_flMoveEndTime;

	int		m_iPortalType;				// keyvalue portal_type: 0 blue, 1 orange
	EHANDLE	m_hSpawnedPortal;

	COutputEvent m_OnFullyOpen;
	COutputEvent m_OnFullyClosed;
};

LINK_ENTITY_TO_CLASS( linked_portal_door, CLinkedPortalDoor );

BEGIN_DATADESC( CLinkedPortalDoor )
	DEFINE_KEYFIELD( m_iPortalType, FIELD_INTEGER, "portal_type" ),
	DEFINE_FIELD( m_bOpening, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bOpen, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecRestOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_flMoveEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_hSpawnedPortal, FIELD_EHANDLE ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Open", InputOpen ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Close", InputClose ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_OUTPUT( m_OnFullyOpen, "OnFullyOpen" ),
	DEFINE_OUTPUT( m_OnFullyClosed, "OnFullyClosed" ),
	DEFINE_THINKFUNC( Think ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::Precache( void )
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/portal_door.mdl" ) );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::Spawn( void )
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/props/portal_door.mdl" ) );
	}

	Precache();
	SetModel( STRING( GetModelName() ) );

	SetSolid( SOLID_VPHYSICS );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	m_bOpening = false;
	m_bOpen = false;
	m_flMoveEndTime = 0.0f;

	if ( m_iPortalType != 1 )
		m_iPortalType = 0;

	m_vecRestOrigin = GetAbsOrigin();
}

//----------------------------------------------------------------------------- 
// Purpose: 
//----------------------------------------------------------------------------- 
void CLinkedPortalDoor::SetState( bool bOpening )
{
	if ( m_bOpening == bOpening || ( m_bOpen && bOpening ) )
		return;

	m_bOpening = bOpening;
	m_flMoveEndTime = gpGlobals->curtime + PORTAL_DOOR_MOVE_TIME;

	// Portal dies as soon as the door starts closing; it appears once the
	// door has fully moved out of the way.
	if ( !bOpening )
	{
		RemovePortal();
	}

	SetThink( &CLinkedPortalDoor::Think );
	SetNextThink( gpGlobals->curtime );
}

void CLinkedPortalDoor::InputOpen( inputdata_t &inputdata )
{
	SetState( true );
}

void CLinkedPortalDoor::InputClose( inputdata_t &inputdata )
{
	SetState( false );
}

void CLinkedPortalDoor::InputToggle( inputdata_t &inputdata )
{
	SetState( !m_bOpen );
}

//-----------------------------------------------------------------------------
// Purpose: Slide the panel down out of the way
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::AnimateMove( void )
{
	float flRemaining = m_flMoveEndTime - gpGlobals->curtime;
	if ( flRemaining <= 0.0f )
	{
		FinishMove();
		return;
	}

	// Interpolate toward the open or rest position from the current origin
	Vector vecTarget = m_bOpening ? ( m_vecRestOrigin - Vector( 0.0f, 0.0f, PORTAL_DOOR_OPEN_DISTANCE ) )
								  : m_vecRestOrigin;
	float flProgress = 1.0f - ( flRemaining / PORTAL_DOOR_MOVE_TIME );

	Vector vecCurrent = GetAbsOrigin();
	Vector vecNew = Lerp( 0.1f, vecCurrent, vecTarget );
	Teleport( &vecNew, NULL, NULL );

	SetNextThink( gpGlobals->curtime );
}

void CLinkedPortalDoor::FinishMove( void )
{
	Vector vecTarget = m_bOpening ? ( m_vecRestOrigin - Vector( 0.0f, 0.0f, PORTAL_DOOR_OPEN_DISTANCE ) )
								  : m_vecRestOrigin;
	Teleport( &vecTarget, NULL, NULL );

	m_bOpen = m_bOpening;
	if ( m_bOpen )
	{
		PlacePortal();
		m_OnFullyOpen.FireOutput( this, this );
	}
	else
	{
		m_OnFullyClosed.FireOutput( this, this );
	}
}

void CLinkedPortalDoor::Think( void )
{
	AnimateMove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLinkedPortalDoor::PlacePortal( void )
{
	// The portal lives on the plane the door occupied at rest: the door's
	// forward axis points into the wall it covers.
	Vector vecForward;
	AngleVectors( GetAbsAngles(), &vecForward );

	cplane_t plane;
	plane.normal = -vecForward;
	plane.normal.NormalizeInPlace();
	plane.dist = DotProduct( plane.normal, m_vecRestOrigin );
	plane.type = PLANE_Z;
	plane.signbits = SignbitsForPlane( &plane );

	Vector vecPortalCenter = m_vecRestOrigin + plane.normal * PORTAL_SKIN_LENGTH;

	m_hSpawnedPortal = Portal_PlacePortal( ( PortalType )m_iPortalType, plane, vecPortalCenter );
}

void CLinkedPortalDoor::RemovePortal( void )
{
	CProp_Portal *pPortal = assert_cast< CProp_Portal * >( m_hSpawnedPortal.Get() );
	if ( pPortal != NULL && pPortal->m_bActivated )
	{
		pPortal->Fizzle( PORTAL_FIZZLE_FIZZLER );
	}
	m_hSpawnedPortal = NULL;
}
