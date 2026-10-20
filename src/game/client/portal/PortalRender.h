//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal rendering. CPortalRender drives stencil-based recursive
//			portal views, render targets, and the fog/water cooperation
//			hooks scattered through viewrender.cpp.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_RENDER_H
#define PORTAL_RENDER_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL

#include "cdll_int.h"
#include "utlvector.h"
#include "mathlib/vector.h"
#include "materialsystem/imaterialsystem.h"

class CViewRender;
struct CViewSetup;
class KeyValues;
class C_Prop_Portal;

//-----------------------------------------------------------------------------
// CPortalRenderable - per-portal rendering helper owned by the portal entity.
//-----------------------------------------------------------------------------
class CPortalRenderable
{
public:
	CPortalRenderable( C_Prop_Portal *pPortal );
	virtual ~CPortalRenderable( void );

	C_Prop_Portal *GetPortal( void ) const { return m_pPortal; }

	// Draw the visual surface of this portal in world space
	virtual void DrawPortal( void );

protected:
	C_Prop_Portal *m_pPortal;
};

//-----------------------------------------------------------------------------
// CPortalRender - drives all portal rendering. Access via g_pPortalRender.
//-----------------------------------------------------------------------------
class CPortalRender
{
public:
	CPortalRender( void );

	// ---- state queries used by viewrender / beam / sprite hooks ----
	bool				IsRenderingPortal( void ) const { return m_bIsRenderingPortal; }
	int					GetViewRecursionLevel( void ) const { return m_iViewRecursionLevel; }
	bool				ShouldObeyStencilForClears( void );
	Vector				GetExitPortalFogOrigin( void );
	bool				ShouldUseStencilsToRenderPortals( void );
	int					ShouldForceCheaperWaterLevel( void );
	bool				IsPortalViewID( int id );
	int					GetCurrentViewId( void );
	int					GetCurrentSkyboxViewId( void );
	SkyboxVisibility_t	IsSkyboxVisibleFromExitPortal( void );
	bool				DoesExitPortalViewIntersectWaterPlane( float waterZ, int leafWaterDataID );

	// ---- per-frame rendering entry points (called from viewrender.cpp) ----
	bool				DrawPortalsUsingStencils( CViewRender *pMainView );
	bool				DrawPortalsToTextures( CViewRender *pMainView, const CViewSetup &cameraView );
	void				UpdateDepthDoublerTexture( const CViewSetup &view );
	void				UpdatePortalPixelVisibility( void );
	void				ShiftFogForExitPortalView( void );

	// ---- water rendering cooperation ----
	void				WaterRenderingHandler_PreReflection( void );
	void				WaterRenderingHandler_PostReflection( void );
	void				WaterRenderingHandler_PreRefraction( void );
	void				WaterRenderingHandler_PostRefraction( void );

	// ---- debug / tools ----
	void				OverlayPortalRenderTargets( float w, float h );
	void				HandlePortalPlaybackMessage( KeyValues *pKeyValues );

private:
	void				BuildPortalView( C_Prop_Portal *pEnterPortal, CViewRender *pMainView, CViewSetup &portalView );
	void				DrawPortalStencilQuad( C_Prop_Portal *pPortal );
	void				DrawPortalQuadColored( C_Prop_Portal *pPortal, const color32 &color );
	void				DrawDormantPortalSurface( C_Prop_Portal *pPortal );

	bool				m_bIsRenderingPortal;
	int					m_iViewRecursionLevel;
	bool				m_bInWaterHandler;
	bool				m_bPortalViewTargetsCreated;
	C_Prop_Portal		*m_pExitPortal;
	Vector				m_vecExitPortalFogOrigin;
	CTextureReference	m_pPortalViewTextures[2];
};

extern CPortalRender *g_pPortalRender;

#endif // CLIENT_DLL

#endif // PORTAL_RENDER_H
