//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Extra water reflection/refraction render targets, one pair per
//			portal stencil recursion level, so nested portal views don't
//			trample the primary water render targets.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_RENDER_TARGETS_H
#define PORTAL_RENDER_TARGETS_H
#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterialsystem.h"
#include "portal_shareddefs.h"

class CPortalRenderTargets
{
public:
	CPortalRenderTargets( void );

	// Lazily creates the render targets on first use
	void Init( void );
	void Shutdown( void );

	ITexture *GetWaterReflectionTextureForStencilDepth( int iStencilDepth );
	ITexture *GetWaterRefractionTextureForStencilDepth( int iStencilDepth );

private:
	void CreateTargets( void );

	CTextureReference m_ReflectionTextures[MAX_PORTAL_RECURSIVE_VIEWS];
	CTextureReference m_RefractionTextures[MAX_PORTAL_RECURSIVE_VIEWS];
	bool m_bTargetsCreated;
};

extern CPortalRenderTargets *portalrendertargets;

#endif // PORTAL_RENDER_TARGETS_H
