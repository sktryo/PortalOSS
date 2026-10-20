//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of the shared portal class data
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_util_shared.h"
#include "prop_portal_shared.h"

#ifdef CLIENT_DLL
#include "c_prop_portal.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlVector<PORTAL_PTR_TYPE *> CProp_Portal_Shared::AllPortals;

// Inverse of an orthonormal portal basis (rotation + translation). The basis
// column layout is R, U, N with translation in the 4th column.
static VMatrix PortalOrthoBasisInverse( const VMatrix &m )
{
	// Inverse of a rigid VMatrix: R^T for the rotation part and -R^T * t for
	// the translation (row layout: rows are the rotated X/Y/Z basis vectors).
	VMatrix out;
	out.Identity();
	for ( int i = 0; i < 3; ++i )
		for ( int j = 0; j < 3; ++j )
			out.m[i][j] = m.m[j][i];

	for ( int i = 0; i < 3; ++i )
	{
		// t_inverse = -R^T * t (row i of R^T times the translation column)
		float flDot = out.m[i][0] * m.m[0][3] + out.m[i][1] * m.m[1][3] + out.m[i][2] * m.m[2][3];
		out.m[i][3] = -flDot;
	}
	return out;
}

CProp_Portal_Shared::CProp_Portal_Shared( void )
{
	m_bActivated = false;
	m_hLinkedPortal = NULL;

	m_plane_Origin.normal.Init();
	m_plane_Origin.dist = 0.0f;
	m_plane_Origin.type = PLANE_Z;
	m_plane_Origin.signbits = 0;

	// CNetworkArray default-constructs its elements, which for Vector does not
	// zero them in release builds. Explicitly collapse the corners so
	// HasValidCorners() is false until real data arrives.
	for ( int i = 0; i < 4; ++i )
		m_vPortalCorners.Set( i, vec3_origin );

	m_matThisToLinked.Identity();
	m_matLinkedToThis.Identity();
}

CProp_Portal_Shared::~CProp_Portal_Shared( void )
{
	RemoveFromAllPortals( static_cast<PORTAL_PTR_TYPE *>( this ) );
}

bool CProp_Portal_Shared::IsActivatedAndLinked( void ) const
{
	if ( !m_bActivated || m_hLinkedPortal == NULL || !m_hLinkedPortal->m_bActivated )
		return false;

	return HasValidCorners() && m_hLinkedPortal->HasValidCorners();
}

bool CProp_Portal_Shared::HasValidCorners( void ) const
{
	Vector vecRight = m_vPortalCorners[1] - m_vPortalCorners[0];
	Vector vecUp = m_vPortalCorners[3] - m_vPortalCorners[0];
	return ( vecRight.LengthSqr() > 0.0f ) && ( vecUp.LengthSqr() > 0.0f );
}

void CProp_Portal_Shared::UpdatePortalPlaneFromCorners( void )
{
	Vector vecRight = m_vPortalCorners[1] - m_vPortalCorners[0];
	Vector vecUp = m_vPortalCorners[3] - m_vPortalCorners[0];
	if ( vecRight.NormalizeInPlace() <= 0.0f || vecUp.NormalizeInPlace() <= 0.0f )
		return;

	// Right x Up points out of the wall, toward the room the portal opens into.
	CrossProduct( vecRight, vecUp, m_plane_Origin.normal );
	m_plane_Origin.normal.NormalizeInPlace();

	m_plane_Origin.dist = DotProduct( m_plane_Origin.normal, m_vPortalCorners[0] );
	m_plane_Origin.type = PLANE_Z;
	if ( fabs( m_plane_Origin.normal.x ) > 0.9999f )
		m_plane_Origin.type = PLANE_X;
	else if ( fabs( m_plane_Origin.normal.y ) > 0.9999f )
		m_plane_Origin.type = PLANE_Y;
	m_plane_Origin.signbits = SignbitsForPlane( &m_plane_Origin );
}

VMatrix CProp_Portal_Shared::MatrixThisToLinked( void ) const
{
	VMatrix m;
	m.Identity();
	if ( !IsActivatedAndLinked() )
		return m;
	return m_matThisToLinked;
}

VMatrix CProp_Portal_Shared::MatrixLinkedToThis( void ) const
{
	VMatrix m;
	m.Identity();
	if ( !IsActivatedAndLinked() )
		return m;
	return m_matLinkedToThis;
}

const Vector &CProp_Portal_Shared::GetPortalOrigin( void ) const
{
	return GetAbsOrigin();
}

void CProp_Portal_Shared::RecalculatePortalTransforms( void )
{
	if ( !IsActivatedAndLinked() )
	{
		m_matThisToLinked.Identity();
		m_matLinkedToThis.Identity();
		return;
	}

	PORTAL_PTR_TYPE *pLinked = m_hLinkedPortal;
	CProp_Portal_Shared *pPortalA = this;
	CProp_Portal_Shared *pPortalB = pLinked;

	Vector vecRightA = pPortalA->m_vPortalCorners[1] - pPortalA->m_vPortalCorners[0];
	Vector vecUpA = pPortalA->m_vPortalCorners[3] - pPortalA->m_vPortalCorners[0];
	Vector vecRightB = pPortalB->m_vPortalCorners[1] - pPortalB->m_vPortalCorners[0];
	Vector vecUpB = pPortalB->m_vPortalCorners[3] - pPortalB->m_vPortalCorners[0];
	vecRightA.NormalizeInPlace();
	vecUpA.NormalizeInPlace();
	vecRightB.NormalizeInPlace();
	vecUpB.NormalizeInPlace();

	Vector vecNormalA = pPortalA->m_plane_Origin.normal;
	Vector vecNormalB = pPortalB->m_plane_Origin.normal;

	VMatrix matBasisA, matBasisB;
	matBasisA.Identity();
	matBasisB.Identity();
	MatrixSetColumn( matBasisA, 0, vecRightA );
	MatrixSetColumn( matBasisA, 1, vecUpA );
	MatrixSetColumn( matBasisA, 2, vecNormalA );
	matBasisA.SetTranslation( pPortalA->GetAbsOrigin() );

	MatrixSetColumn( matBasisB, 0, vecRightB );
	MatrixSetColumn( matBasisB, 1, vecUpB );
	MatrixSetColumn( matBasisB, 2, vecNormalB );
	matBasisB.SetTranslation( pPortalB->GetAbsOrigin() );

	// 180 degree rotation around the shared up axis: local (x,y,z) -> (-x,y,-z)
	VMatrix matFlip;
	matFlip.Identity();
	matFlip.m[0][0] = -1.0f;
	matFlip.m[2][2] = -1.0f;

	VMatrix matInvBasisA = PortalOrthoBasisInverse( matBasisA );
	VMatrix matInvBasisB = PortalOrthoBasisInverse( matBasisB );

	VMatrix matThisToLinked = matBasisB * matFlip * matInvBasisA;
	VMatrix matLinkedToThis = matBasisA * matFlip * matInvBasisB;

	pPortalA->m_matThisToLinked = matThisToLinked;
	pPortalB->m_matThisToLinked = matLinkedToThis;
	pPortalB->m_matLinkedToThis = matThisToLinked;
	pPortalA->m_matLinkedToThis = matLinkedToThis;
}

void CProp_Portal_Shared::AddToAllPortals( PORTAL_PTR_TYPE *pPortal )
{
	RemoveFromAllPortals( pPortal );
	AllPortals.AddToTail( pPortal );
}

void CProp_Portal_Shared::RemoveFromAllPortals( PORTAL_PTR_TYPE *pPortal )
{
	for ( int i = AllPortals.Count() - 1; i >= 0; --i )
	{
		if ( AllPortals[i] == pPortal )
			AllPortals.FastRemove( i );
	}
}

void CProp_Portal_Shared::ClearAllPortals( void )
{
	AllPortals.RemoveAll();
}
