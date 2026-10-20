//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CPortalRenderTargets implementation. One extra water reflection and
//			refraction render target per portal stencil recursion level.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_render_targets.h"
#include "prop_portal_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Static instance: viewrender.cpp dereferences portalrendertargets while a
// portal view is drawn, so it must never be NULL.
static CPortalRenderTargets s_PortalRenderTargets;
CPortalRenderTargets *portalrendertargets = &s_PortalRenderTargets;

CPortalRenderTargets::CPortalRenderTargets( void )
{
	m_bTargetsCreated = false;
}

void CPortalRenderTargets::CreateTargets( void )
{
	if ( m_bTargetsCreated )
		return;

	m_bTargetsCreated = true;

	for ( int i = 0; i < MAX_PORTAL_RECURSIVE_VIEWS; ++i )
	{
		char szName[64];

		// reflection target for this recursion depth
		Q_snprintf( szName, sizeof( szName ), "_rt_PortalWaterReflection%d", i + 1 );
		m_ReflectionTextures[i].Init( materials->CreateNamedRenderTargetTextureEx2(
			szName,
			256, 256, RT_SIZE_PICMIP,
			materials->GetBackBufferFormat(),
			MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
			CREATERENDERTARGETFLAGS_HDR ) );

		// refraction target for this recursion depth (needs alpha for fog factor)
		Q_snprintf( szName, sizeof( szName ), "_rt_PortalWaterRefraction%d", i + 1 );
		m_RefractionTextures[i].Init( materials->CreateNamedRenderTargetTextureEx2(
			szName,
			256, 256, RT_SIZE_PICMIP,
			IMAGE_FORMAT_RGBA8888,
			MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
			CREATERENDERTARGETFLAGS_HDR ) );
	}
}

void CPortalRenderTargets::Init( void )
{
	CreateTargets();
}

void CPortalRenderTargets::Shutdown( void )
{
	for ( int i = 0; i < MAX_PORTAL_RECURSIVE_VIEWS; ++i )
	{
		m_ReflectionTextures[i].Shutdown();
		m_RefractionTextures[i].Shutdown();
	}
	m_bTargetsCreated = false;
}

ITexture *CPortalRenderTargets::GetWaterReflectionTextureForStencilDepth( int iStencilDepth )
{
	CreateTargets();

	if ( iStencilDepth < 1 || iStencilDepth > MAX_PORTAL_RECURSIVE_VIEWS )
		return NULL;

	return m_ReflectionTextures[iStencilDepth - 1];
}

ITexture *CPortalRenderTargets::GetWaterRefractionTextureForStencilDepth( int iStencilDepth )
{
	CreateTargets();

	if ( iStencilDepth < 1 || iStencilDepth > MAX_PORTAL_RECURSIVE_VIEWS )
		return NULL;

	return m_RefractionTextures[iStencilDepth - 1];
}
