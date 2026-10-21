//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CPortalRenderable base class and CPortalRender implementation.
//			Stencil-based recursive portal view rendering.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "PortalRender.h"

#ifdef PORTAL

#include "prop_portal_shared.h"
#include "portal_util_shared.h"
#include "portal_render_targets.h"
#include "c_prop_portal.h"
#include "viewrender.h"
#include "view.h"
#include "view_scene.h"
#include "ivrenderview.h"
#include "cdll_client_int.h"
#include "materialsystem/imesh.h"
#include "tier2/tier2.h"
#include "c_baseplayer.h"
#include "KeyValues.h"
#include "engine/ivdebugoverlay.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// view ids for portal views are allocated past the shared view id space
#define PORTAL_VIEW_ID_START	( VIEW_ID_COUNT + 1 )
#define PORTAL_MAX_VIEW_DISTANCE	8000.0f

ConVar portal_render_targets_enabled( "portal_render_targets_enabled", "0", FCVAR_CHEAT,
	"Enable rendering of through-portal views to the _rt_Portal1/_rt_Portal2 render targets." );

// Selects how through-portal views are produced:
//  0 - stencil hole rendering (recursive, in-framebuffer)
//  1 - render the exit scene to a render target and composite it onto the
//      portal opening (the classic, movement-stable Portal technique)
ConVar portal_rt_mode( "portal_rt_mode", "1", FCVAR_CHEAT,
	"0 = stencil through-views, 1 = render-target through-views." );

// The exit view fills its render target with texture v = 0 at the image top
// on every driver we support; if a target ever samples upside-down, toggle this.
ConVar portal_rt_vflip( "portal_rt_vflip", "1", FCVAR_CHEAT,
	"1 = sample the exit RT with v flipped (normal), 0 = raw v." );

// Temporary visual aid: draws the opening's outline and normal with the debug
// overlay so the geometry can be verified even if the portal surfaces do not
// rasterise yet.
ConVar portal_debug_outline( "portal_debug_outline", "1", FCVAR_CHEAT,
	"Draw portal opening outlines with the debug overlay." );

// Diagnostic switch for the stencil path:
//  0 - normal rendering
//  1 - paint the stencil holes green and skip the deeper view (proves the mask)
//  2 - draw the opening bright magenta after the deeper view (proves the quad draw)
ConVar portal_debug_stencil_clear( "portal_debug_stencil_clear", "0", FCVAR_CHEAT,
	"0=off, 1=paint stencil holes green, 2=draw openings magenta." );

// Draws the opening material as a screen-space rectangle, proving the material
// itself rasterises independently of the world-space mesh path.
ConVar portal_debug_screen_quad( "portal_debug_screen_quad", "0", FCVAR_CHEAT,
	"Draw the portal opening material as a screen-space rectangle." );

// Selects how the opening quad is submitted to the material system:
//  0 - unbuffered dynamic mesh, End(true,true)
//  1 - buffered dynamic mesh, End(true,true)
//  2 - unbuffered dynamic mesh, End() + IMesh::Draw()
//  3 - buffered dynamic mesh, End() + IMesh::Draw()
//  4 - no auto-bind mesh, End() + IMesh::Draw() (verified beam/sprite path)
ConVar portal_debug_mesh_mode( "portal_debug_mesh_mode", "4", FCVAR_CHEAT,
	"Select the dynamic mesh submission path for the portal opening." );

// When 1, forces the opening material to ignore the depth test
// (MATERIAL_VAR_IGNOREZ) to rule out depth-occlusion of the opening quad.
ConVar portal_debug_ignorez( "portal_debug_ignorez", "0", FCVAR_CHEAT,
	"Force the portal opening quad to ignore the depth test." );

//-----------------------------------------------------------------------------
CPortalRender *g_pPortalRender = NULL;

static CPortalRender s_PortalRender;

CPortalRender::CPortalRender( void )
{
	g_pPortalRender = this;

	m_bIsRenderingPortal = false;
	m_iViewRecursionLevel = 0;
	m_bInWaterHandler = false;
	m_bPortalViewTargetsCreated = false;
	m_pExitPortal = NULL;
	m_vecExitPortalFogOrigin = vec3_origin;
	m_pPortalRTMaterial[0] = NULL;
	m_pPortalRTMaterial[1] = NULL;
	m_pPortalRTBaseTex[0] = NULL;
	m_pPortalRTBaseTex[1] = NULL;
}

//-----------------------------------------------------------------------------
// CPortalRenderable
//-----------------------------------------------------------------------------
CPortalRenderable::CPortalRenderable( C_Prop_Portal *pPortal )
{
	m_pPortal = pPortal;
}

CPortalRenderable::~CPortalRenderable( void )
{
}

void CPortalRenderable::DrawPortal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: portal views get view ids outside the standard range so they can
//			be identified without colliding with engine views
//-----------------------------------------------------------------------------
bool CPortalRender::IsPortalViewID( int id )
{
	return ( id >= PORTAL_VIEW_ID_START ) && ( id < PORTAL_VIEW_ID_START + ( MAX_PORTAL_RECURSIVE_VIEWS * 2 ) );
}

int CPortalRender::GetCurrentViewId( void )
{
	return PORTAL_VIEW_ID_START + ( ( m_iViewRecursionLevel - 1 ) * 2 );
}

int CPortalRender::GetCurrentSkyboxViewId( void )
{
	return GetCurrentViewId() + 1;
}

bool CPortalRender::ShouldObeyStencilForClears( void )
{
	return m_bIsRenderingPortal;
}

bool CPortalRender::ShouldUseStencilsToRenderPortals( void )
{
	return true;
}

int CPortalRender::ShouldForceCheaperWaterLevel( void )
{
	//while rendering inside a portal view, don't reflect entities in expensive water passes.
	if ( m_iViewRecursionLevel != 0 )
		return 2;

	return -1;
}

Vector CPortalRender::GetExitPortalFogOrigin( void )
{
	return m_vecExitPortalFogOrigin;
}

SkyboxVisibility_t CPortalRender::IsSkyboxVisibleFromExitPortal( void )
{
	if ( m_pExitPortal != NULL )
		return engine->IsSkyboxVisibleFromPoint( m_vecExitPortalFogOrigin );

	return SKYBOX_NOT_VISIBLE;
}

bool CPortalRender::DoesExitPortalViewIntersectWaterPlane( float waterZ, int leafWaterDataID )
{
	if ( m_pExitPortal == NULL )
		return false;

	// Build the bounds from the actual opening, then expand a little along the
	// surface normal so the box straddles the portal plane.
	Vector vecMins = m_pExitPortal->m_vPortalCorners[0];
	Vector vecMaxs = vecMins;
	for ( int i = 1; i < 4; ++i )
	{
		Vector vecCorner = m_pExitPortal->m_vPortalCorners[i];
		for ( int axis = 0; axis < 3; ++axis )
		{
			vecMins[axis] = MIN( vecMins[axis], vecCorner[axis] );
			vecMaxs[axis] = MAX( vecMaxs[axis], vecCorner[axis] );
		}
	}

	Vector vecExpand( 8.0f, 8.0f, 8.0f );
	vecMins -= vecExpand;
	vecMaxs += vecExpand;

	// Cheap rejection: if the water plane is outside the expanded box, the
	// exit portal cannot touch that water volume.
	if ( waterZ < vecMins.z || waterZ > vecMaxs.z )
		return false;

	return render->DoesBoxIntersectWaterVolume( vecMins, vecMaxs, leafWaterDataID );
}

void CPortalRender::UpdatePortalPixelVisibility( void )
{
	//this is called once per frame by OnRenderStart(), use the cheap position
	//in the frame to reset the recursion machinery (stencil is off here anyway)
	m_iViewRecursionLevel = 0;
	m_bIsRenderingPortal = false;
	m_pExitPortal = NULL;
	m_vecExitPortalFogOrigin = vec3_origin;
}

void CPortalRender::UpdateDepthDoublerTexture( const CViewSetup &view )
{
	//a copy of the front buffer sans view model. unused in this implementation.
}

void CPortalRender::ShiftFogForExitPortalView( void )
{
	if ( m_pExitPortal == NULL )
		return;

	//fog distances measured from the real camera position don't work in portal views;
	//push the fog start/end out to the exit portal's plane so the fake space behind
	//the wall isn't fogged
	Vector vecToCamera = CurrentViewOrigin() - m_pExitPortal->GetPortalOrigin();
	float flShift = DotProduct( vecToCamera, m_pExitPortal->GetPortalNormal() );

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && ( flShift > 0.0f ) )
	{
		fogparams_t *pFog = pPlayer->GetFogParams();
		if ( pFog != NULL && pFog->enable )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->FogStart( pFog->start + flShift );
			pRenderContext->FogEnd( pFog->end + flShift );
		}
	}
}

void CPortalRender::WaterRenderingHandler_PreReflection( void )
{
	m_bInWaterHandler = true;
}

void CPortalRender::WaterRenderingHandler_PostReflection( void )
{
	m_bInWaterHandler = false;
}

void CPortalRender::WaterRenderingHandler_PreRefraction( void )
{
	m_bInWaterHandler = true;
}

void CPortalRender::WaterRenderingHandler_PostRefraction( void )
{
	m_bInWaterHandler = false;
}

void CPortalRender::OverlayPortalRenderTargets( float w, float h )
{
	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pMaterials[2];
	pMaterials[0] = materials->FindMaterial( "Engine/debug_portal_1", TEXTURE_GROUP_OTHER );
	pMaterials[1] = materials->FindMaterial( "Engine/debug_portal_2", TEXTURE_GROUP_OTHER );

	for ( int i = 0; i < 2; ++i )
	{
		if ( pMaterials[i] == NULL )
			continue;

		pRenderContext->DrawScreenSpaceRectangle( pMaterials[i], i * w, 0, w, h,
			0.0f, 0.0f, w - 1.0f, h - 1.0f, w, h );
	}
}

void CPortalRender::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{
	//tool playback of portal placement messages. nothing to do here.
}

//-----------------------------------------------------------------------------
// Purpose: material used to fill portal openings. The engine/HL2 content
//			provides debug/debugvertexcolor, but this mod only mounts the SDK
//			platform + Portal content, so fall back to a material we ship.
//-----------------------------------------------------------------------------
// 0 = auto (the mod-shipped portal/portal_surface material)
// 1 = force portal/portal_surface
// 2 = force debug/debugvertexcolor without the NOCULL override
ConVar portal_debug_surface_mat( "portal_debug_surface_mat", "0", FCVAR_CHEAT,
	"Select the portal opening material for diagnostics." );

static IMaterial *GetPortalSurfaceMaterial( void )
{
	static IMaterial *s_pMaterial = NULL;
	static bool s_bResolved = false;
	static bool s_bLogged = false;

	if ( !s_bResolved )
	{
		s_bResolved = true;

		const char *pName = "portal/portal_surface";
		if ( portal_debug_surface_mat.GetInt() == 2 )
			pName = "debug/debugvertexcolor";

		IMaterial *pMat = materials->FindMaterial( pName, TEXTURE_GROUP_OTHER, false );
		if ( pMat == NULL || pMat->IsErrorMaterial() )
			pMat = materials->FindMaterial( "portal/portal_surface", TEXTURE_GROUP_OTHER, false );

		if ( pMat != NULL && !pMat->IsErrorMaterial() )
		{
			s_pMaterial = pMat;

			// The opening quad is drawn with whatever winding the corner order
			// produces; force two-sided so the stencil mask is always written.
			if ( portal_debug_surface_mat.GetInt() != 2 )
				s_pMaterial->SetMaterialVarFlag( MATERIAL_VAR_NOCULL, true );

			// Force the material to decide its vertex format up front. A lazy
			// GetVertexFormat()==0 makes GetDynamicMesh() hand back a zero-size
			// vertex buffer and the opening quad rasterises nothing.
			s_pMaterial->Refresh();
		}
	}

	if ( !s_bLogged )
	{
		s_bLogged = true;
		Portal_DebugLog( "[portal] surface material: '%s' stencil_buffer=%d stencil_bits=%d vertex_format=0x%x",
			( s_pMaterial != NULL ) ? s_pMaterial->GetName() : "(null)",
			g_pMaterialSystemHardwareConfig->HasStencilBuffer() ? 1 : 0,
			g_pMaterialSystemHardwareConfig->StencilBufferBits(),
			( s_pMaterial != NULL ) ? s_pMaterial->GetVertexFormat() : 0 );
	}

	return s_pMaterial;
}

void CPortalRender::DrawWorldQuad( const Vector *pQuad, const color32 &color )
{
	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pMaterial = GetPortalSurfaceMaterial();
	if ( pMaterial == NULL )
		return;

	// Bind first so the dynamic mesh adopts the material's vertex format. This
	// mirrors the proven world-space quad path in beamdraw.cpp / DrawSprite,
	// which calls GetDynamicMesh() with no arguments after binding.
	pRenderContext->Bind( pMaterial );

	// When requested, make the quad ignore the depth test so it draws over the
	// wall; rules out depth-z-fighting as the cause of an invisible opening.
	pMaterial->SetMaterialVarFlag( MATERIAL_VAR_IGNOREZ, portal_debug_ignorez.GetBool() );

	IMesh *pMesh = NULL;
	int iMode = portal_debug_mesh_mode.GetInt();
	if ( ( iMode & 4 ) != 0 )
		// engine'd world-space quad path (beam/sprite style): no auto-bind,
		// explicit End() + Draw()
		pMesh = pRenderContext->GetDynamicMesh( );
	else if ( iMode & 1 )
		pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );
	else
		pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, pMaterial );

	if ( pMesh == NULL )
		return;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	for ( int i = 0; i < 4; ++i )
	{
		meshBuilder.Position3fv( pQuad[i].Base() );
		meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();
	}

	if ( ( iMode & 4 ) != 0 || ( iMode & 2 ) != 0 )
	{
		meshBuilder.End();
		pMesh->Draw();
	}
	else
	{
		meshBuilder.End( true, true );
	}
}

void CPortalRender::DrawPortalQuadColored( C_Prop_Portal *pPortal, const color32 &color )
{
	//nudge the quad off the wall toward the camera to avoid z-fighting with the wall
	Vector vecNudge = pPortal->m_plane_Origin.normal;
	if ( DotProduct( CurrentViewOrigin() - pPortal->GetPortalOrigin(), vecNudge ) < 0.0f )
		vecNudge.Negate();
	vecNudge *= 0.5f;

	const Vector &vBL = pPortal->m_vPortalCorners[0];
	const Vector &vBR = pPortal->m_vPortalCorners[1];
	const Vector &vTR = pPortal->m_vPortalCorners[2];
	const Vector &vTL = pPortal->m_vPortalCorners[3];

	Vector vecQuad[4];
	vecQuad[0] = vBL + vecNudge;
	vecQuad[1] = vBR + vecNudge;
	vecQuad[2] = vTR + vecNudge;
	vecQuad[3] = vTL + vecNudge;

	DrawWorldQuad( vecQuad, color );
}

//-----------------------------------------------------------------------------
// Purpose: draw the opening polygon of a portal into the stencil buffer and
//			tint the opening so the portal is visible even if the through-view
//			cannot be rendered.
//-----------------------------------------------------------------------------
void CPortalRender::DrawPortalStencilQuad( C_Prop_Portal *pPortal )
{
	color32 color;
	if ( pPortal->m_iPortalType == PORTAL_TYPE_BLUE )
	{
		color.r = 0; color.g = 128; color.b = 255; color.a = 255;
	}
	else
	{
		color.r = 255; color.g = 128; color.b = 0; color.a = 255;
	}

	DrawPortalQuadColored( pPortal, color );
}

void CPortalRender::DrawDormantPortalSurface( C_Prop_Portal *pPortal )
{
	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pMaterial = GetPortalSurfaceMaterial();
	if ( pMaterial == NULL )
		return;

	pRenderContext->Bind( pMaterial );

	Vector vecNudge = pPortal->m_plane_Origin.normal;
	if ( DotProduct( CurrentViewOrigin() - pPortal->GetPortalOrigin(), vecNudge ) < 0.0f )
		vecNudge.Negate();
	vecNudge *= 0.5f;

	const Vector &vBL = pPortal->m_vPortalCorners[0];
	const Vector &vBR = pPortal->m_vPortalCorners[1];
	const Vector &vTR = pPortal->m_vPortalCorners[2];
	const Vector &vTL = pPortal->m_vPortalCorners[3];

	Vector vecQuad[4];
	vecQuad[0] = vBL + vecNudge;
	vecQuad[1] = vBR + vecNudge;
	vecQuad[2] = vTR + vecNudge;
	vecQuad[3] = vTL + vecNudge;

	color32 color;
	color.r = ( pPortal->m_iPortalType == PORTAL_TYPE_BLUE ) ? 40 : 84;
	color.g = ( pPortal->m_iPortalType == PORTAL_TYPE_BLUE ) ? 60 : 42;
	color.b = ( pPortal->m_iPortalType == PORTAL_TYPE_BLUE ) ? 68 : 8;
	color.a = 180;

	CMeshBuilder meshBuilder;
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, pMaterial );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	for ( int i = 0; i < 4; ++i )
	{
		meshBuilder.Position3fv( vecQuad[i].Base() );
		meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End( true, true );
}
void CPortalRender::BuildPortalView( C_Prop_Portal *pEnterPortal, CViewRender *pMainView, CViewSetup &portalView )
{
	portalView = *pMainView->GetPlayerViewSetup();
	portalView.origin = CurrentViewOrigin();
	portalView.angles = CurrentViewAngles();

	const VMatrix &matThisToLinked = pEnterPortal->MatrixThisToLinked();
	UTIL_Portal_PointTransform( matThisToLinked, CurrentViewOrigin(), portalView.origin );
	UTIL_Portal_AngleTransform( matThisToLinked, CurrentViewAngles(), portalView.angles );

	//nudge out of the exit plane a touch so the near plane doesn't shear geometry
	Vector vecForward;
	AngleVectors( portalView.angles, &vecForward );
	portalView.origin += vecForward * 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: called after opaques but in the middle of the translucent rendering
//			of the current view; draws every visible portal by masking the
//			screen with stencils and rendering the deeper scene in the holes
// Return : true - a deeper portal view was rendered (caller rebuilds lists)
//			false - nothing more to recurse into
//-----------------------------------------------------------------------------
bool CPortalRender::DrawPortalsUsingStencils( CViewRender *pMainView )
{
	VPROF_BUDGET( "CPortalRender::DrawPortalsUsingStencils", "MainViewPortals" );

	if ( !pMainView || m_bInWaterHandler )
		return false;

	//never put portal views inside water reflection/refraction or monitor views
	int iViewId = (int)CurrentViewID();
	if ( ( iViewId != VIEW_MAIN ) && !IsPortalViewID( iViewId ) )
		return false;

	//recursion depth exhausted, the current view renders normally
	if ( m_iViewRecursionLevel >= MAX_PORTAL_RECURSIVE_VIEWS )
		return false;

	if ( portal_debug_screen_quad.GetBool() && m_iViewRecursionLevel == 0 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		IMaterial *pMaterial = GetPortalSurfaceMaterial();
		if ( pMaterial != NULL )
			pRenderContext->DrawScreenSpaceRectangle( pMaterial, 100, 100, 200, 200,
				0.0f, 0.0f, 1.0f, 1.0f, 200, 200 );
	}

	//collect the portals that can be seen from here
	CUtlVector<C_Prop_Portal *> visiblePortals;
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();

	// Throttled diagnostics: portal_debug.log gets one block every 2 seconds
	// describing why portals are or are not drawn.
	static float s_flNextRenderLog = 0.0f;
	bool bLogRender = ( gpGlobals->curtime >= s_flNextRenderLog );
	if ( bLogRender )
	{
		s_flNextRenderLog = gpGlobals->curtime + 2.0f;
		Portal_DebugLog( "[portal] render: viewid=%d recursion=%d portals=%d", iViewId, m_iViewRecursionLevel, iPortalCount );
	}

	for ( int i = 0; i < iPortalCount; ++i )
	{
		C_Prop_Portal *pPortal = CProp_Portal_Shared::AllPortals[i];
		if ( pPortal == NULL )
			continue;

		Vector vecToPortal = pPortal->GetPortalOrigin() - CurrentViewOrigin();
		//positive when the camera is on the front (normal) side of the opening
		float flDot = DotProduct( pPortal->m_plane_Origin.normal, -vecToPortal );

		if ( bLogRender )
		{
			Portal_DebugLog( "[portal] render candidate %d: act=%d linked=%s corners=%d origin=(%.1f %.1f %.1f) normal=(%.2f %.2f %.2f) dist2=%.0f dot=%.2f draw=%d",
				i, pPortal->m_bActivated ? 1 : 0,
				( pPortal->m_hLinkedPortal.Get() != NULL ) ? "yes" : "no",
				pPortal->HasValidCorners() ? 1 : 0,
				pPortal->GetPortalOrigin().x, pPortal->GetPortalOrigin().y, pPortal->GetPortalOrigin().z,
				pPortal->m_plane_Origin.normal.x, pPortal->m_plane_Origin.normal.y, pPortal->m_plane_Origin.normal.z,
				vecToPortal.LengthSqr(), flDot, pPortal->m_bDrawInPortalRender ? 1 : 0 );
		}

		if ( !pPortal->IsActivatedAndLinked() )
			continue;

		if ( pPortal->m_bDrawInPortalRender == false )
			continue;

		if ( vecToPortal.LengthSqr() > ( PORTAL_MAX_VIEW_DISTANCE * PORTAL_MAX_VIEW_DISTANCE ) )
			continue;

		//must face the camera
		if ( flDot <= 0.0f )
			continue;

		visiblePortals.AddToTail( pPortal );
	}

	// Debug overlay: draw every portal opening's outline + normal so the
	// geometry can be checked independently of the surface rasterisation.
	if ( portal_debug_outline.GetBool() && m_iViewRecursionLevel == 0 )
	{
		for ( int i = 0; i < iPortalCount; ++i )
		{
			C_Prop_Portal *pPortal = CProp_Portal_Shared::AllPortals[i];
			if ( pPortal == NULL || !pPortal->HasValidCorners() )
				continue;

			int r = ( pPortal->m_iPortalType == PORTAL_TYPE_BLUE ) ? 0 : 255;
			int g = 128;
			int b = ( pPortal->m_iPortalType == PORTAL_TYPE_BLUE ) ? 255 : 0;

			for ( int c = 0; c < 4; ++c )
			{
				const Vector &vecA = pPortal->m_vPortalCorners[c];
				const Vector &vecB = pPortal->m_vPortalCorners[( c + 1 ) & 3];
				debugoverlay->AddLineOverlay( vecA, vecB, r, g, b, true, 0.0f );
			}

			Vector vecNormalEnd = pPortal->GetPortalOrigin() + pPortal->m_plane_Origin.normal * 48.0f;
			debugoverlay->AddLineOverlay( pPortal->GetPortalOrigin(), vecNormalEnd, 255, 255, 255, true, 0.0f );
		}
	}

	//even without linked views visible, dormant portals need their surfaces drawn
	if ( m_iViewRecursionLevel == 0 )
	{
		for ( int i = 0; i < iPortalCount; ++i )
		{
			C_Prop_Portal *pPortal = CProp_Portal_Shared::AllPortals[i];
			if ( pPortal == NULL || !pPortal->m_bActivated || pPortal->IsActivatedAndLinked() )
				continue;

			Vector vecToPortal = pPortal->GetPortalOrigin() - CurrentViewOrigin();
			if ( vecToPortal.LengthSqr() > ( PORTAL_MAX_VIEW_DISTANCE * PORTAL_MAX_VIEW_DISTANCE ) )
				continue;

			if ( DotProduct( pPortal->m_plane_Origin.normal, CurrentViewOrigin() - pPortal->GetPortalOrigin() ) <= 0.0f )
				continue;

			if ( bLogRender )
				Portal_DebugLog( "[portal] render: drawing dormant surface for portal %d", i );

			DrawDormantPortalSurface( pPortal );
		}
	}

	if ( visiblePortals.Count() == 0 )
	{
		if ( bLogRender )
			Portal_DebugLog( "[portal] render: nothing to draw (visible=%d)", visiblePortals.Count() );
		return false;
	}

	if ( bLogRender )
		Portal_DebugLog( "[portal] render: drawing %d portal view(s)", visiblePortals.Count() );

	// Render-target based through-views: render the exit scene into _rt_PortalN
	// and composite it onto the opening. This path has no recursive depth/stencil
	// coupling with the main scene, so it stays stable as the camera moves.
	if ( portal_rt_mode.GetInt() != 0 )
	{
		if ( bLogRender )
			Portal_DebugLog( "[portal] render: using render-target through-views" );
		return DrawPortalViewTargets( pMainView, visiblePortals );
	}

	CMatRenderContextPtr pRenderContext( materials );

	//start each frame's outermost view with a fresh stencil buffer
	if ( m_iViewRecursionLevel == 0 )
		pRenderContext->ClearBuffers( false, false, true );

	//the stencil value in a pixel counts the depth of portal holes enclosing it.
	//each level increments pixels in its hole (INCR), and only renders where the
	//count matches its own recursion depth.
	bool bDrewPortalView = false;
	int iOldRecursion = m_iViewRecursionLevel;
	C_Prop_Portal *pOldExitPortal = m_pExitPortal;
	Vector vecOldFogOrigin = m_vecExitPortalFogOrigin;

	pRenderContext->SetStencilEnable( true );
	pRenderContext->SetStencilWriteMask( 0xFF );
	pRenderContext->SetStencilTestMask( 0xFF );

	for ( int i = 0; i < visiblePortals.Count(); ++i )
	{
		C_Prop_Portal *pEnterPortal = visiblePortals[i];
		C_Prop_Portal *pExitPortal = pEnterPortal->m_hLinkedPortal;
		if ( pExitPortal == NULL )
			continue;

		if ( bLogRender )
			Portal_DebugLog( "[portal] render: stencil view through portal %d (type %d) -> exit type %d",
				i, pEnterPortal->m_iPortalType, pExitPortal->m_iPortalType );

		//build the virtual view through the portal
		CViewSetup portalView;
		BuildPortalView( pEnterPortal, pMainView, portalView );

		// ---------------------------------------------------------------------
		// mark the opening: pixels in the current enclosing area go one deeper
		// ---------------------------------------------------------------------
		pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
		pRenderContext->SetStencilReferenceValue( iOldRecursion );
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_INCR );
		pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
		pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );
		DrawPortalStencilQuad( pEnterPortal );

		const int iStencilTest = portal_debug_stencil_clear.GetInt();
		if ( iStencilTest == 1 )
		{
			//prove that the opening was marked: paint exactly those pixels
			pRenderContext->SetStencilReferenceValue( iOldRecursion + 1 );
			pRenderContext->ClearColor4ub( 0, 255, 0, 255 );
			pRenderContext->ClearBuffersObeyStencil( true, false );
			continue;
		}

		// ---------------------------------------------------------------------
		// render the deeper scene, confined to the pixels just marked
		// ---------------------------------------------------------------------
		pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
		pRenderContext->SetStencilReferenceValue( iOldRecursion + 1 );
		pRenderContext->SetStencilPassOperation( STENCILOPERATION_KEEP );
		pRenderContext->SetStencilFailOperation( STENCILOPERATION_KEEP );
		pRenderContext->SetStencilZFailOperation( STENCILOPERATION_KEEP );

		m_pExitPortal = pExitPortal;
		m_vecExitPortalFogOrigin = pExitPortal->GetPortalOrigin() + pExitPortal->GetPortalNormal() * 8.0f;

		m_iViewRecursionLevel = iOldRecursion + 1;
		m_bIsRenderingPortal = true;

		pMainView->ViewDrawScene_PortalStencil( portalView, NULL );

		m_iViewRecursionLevel = iOldRecursion;
		m_bIsRenderingPortal = ( iOldRecursion > 0 );
		m_pExitPortal = pOldExitPortal;
		m_vecExitPortalFogOrigin = vecOldFogOrigin;

		// ---------------------------------------------------------------------
		// restore the enclosing comparison for the rest of this view
		// ---------------------------------------------------------------------
		pRenderContext->SetStencilCompareFunction( STENCILCOMPARISONFUNCTION_EQUAL );
		pRenderContext->SetStencilReferenceValue( iOldRecursion );

		if ( iStencilTest == 2 )
		{
			color32 color;
			color.r = 255; color.g = 0; color.b = 255; color.a = 255;
			pRenderContext->SetStencilEnable( false );
			DrawPortalQuadColored( pEnterPortal, color );
			pRenderContext->SetStencilEnable( true );
		}

		bDrewPortalView = true;
	}

	//leave the stencil test enabled; the caller continues drawing this view's
	//translucents, which will skip the portal holes (pixels that don't match
	//the enclosing recursion value)
	pRenderContext->SetStencilEnable( true );

	return bDrewPortalView;
}

//-----------------------------------------------------------------------------
// Purpose: render through-portal views into the _rt_Portal1/_rt_Portal2 render
//			targets. Off by default - stencil rendering covers gameplay.
//-----------------------------------------------------------------------------
bool CPortalRender::DrawPortalsToTextures( CViewRender *pMainView, const CViewSetup &cameraView )
{
	VPROF_BUDGET( "CPortalRender::DrawPortalsToTextures", "MainViewPortals" );

	if ( !pMainView || !portal_render_targets_enabled.GetBool() )
		return false;

	C_Prop_Portal *pBluePortal = NULL;
	C_Prop_Portal *pOrangePortal = NULL;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	for ( int i = 0; i < iPortalCount; ++i )
	{
		C_Prop_Portal *pPortal = CProp_Portal_Shared::AllPortals[i];
		if ( pPortal == NULL || !pPortal->IsActivatedAndLinked() )
			continue;

		if ( pPortal->m_iPortalType == PORTAL_TYPE_BLUE )
		{
			if ( pBluePortal == NULL )
				pBluePortal = pPortal;
		}
		else
		{
			if ( pOrangePortal == NULL )
				pOrangePortal = pPortal;
		}
	}

	if ( pBluePortal == NULL && pOrangePortal == NULL )
		return false;

	//create/refresh the portal view render targets
	if ( m_bPortalViewTargetsCreated == false )
	{
		m_bPortalViewTargetsCreated = true;
		m_pPortalViewTextures[0].Init( materials->CreateNamedRenderTargetTextureEx2(
			"_rt_Portal1", 256, 256, RT_SIZE_PICMIP,
			materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 0 ) );
		m_pPortalViewTextures[1].Init( materials->CreateNamedRenderTargetTextureEx2(
			"_rt_Portal2", 256, 256, RT_SIZE_PICMIP,
			materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 0 ) );
	}

	CViewSetup portalView( cameraView );
	portalView.x = 0;
	portalView.y = 0;
	portalView.width = 256;
	portalView.height = 256;

	C_Prop_Portal *pPortalViews[2] = { pBluePortal, pOrangePortal };

	for ( int i = 0; i < 2; ++i )
	{
		C_Prop_Portal *pPortal = pPortalViews[i];
		if ( pPortal == NULL || m_pPortalViewTextures[i] == NULL )
			continue;

		const VMatrix &matThisToLinked = pPortal->MatrixThisToLinked();
		UTIL_Portal_PointTransform( matThisToLinked, cameraView.origin, portalView.origin );
		UTIL_Portal_AngleTransform( matThisToLinked, cameraView.angles, portalView.angles );

		Vector vecForward;
		AngleVectors( portalView.angles, &vecForward );
		portalView.origin += vecForward * 2.0f;

		//push the 3d view with the render target and draw the world into it
		render->Push3DView( portalView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, m_pPortalViewTextures[i], pMainView->GetFrustum() );
		pMainView->ViewDrawScene( false, SKYBOX_NOT_VISIBLE, portalView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, VIEW_MONITOR );
		render->PopView( pMainView->GetFrustum() );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: render-target based through-portal rendering. For every visible
//			portal, render the exit scene (from the transform of the local
//			camera through the portal, same fov/aspect) into a render target,
//			then composite that RT onto the opening as a textured world-space
//			quad. Because the exit view is this view's exact teleport, the
//			opening's screen position and the RT uv at the same coordinates
//			show matching rays - so the quad samples the RT by its own screen
//			position, the same frames of reference the stencil path relies on.
//			This path has no per-frame depth/stencil coupling with the main
//			scene, so it stays stable while the camera moves.
// Return : true if any portal's through-view was composited
//-----------------------------------------------------------------------------
bool CPortalRender::DrawPortalViewTargets( CViewRender *pMainView, CUtlVector<C_Prop_Portal *> &visiblePortals )
{
	VPROF_BUDGET( "CPortalRender::DrawPortalViewTargets", "MainViewPortals" );

	if ( !pMainView )
		return false;

	//create/refresh the portal view render targets if needed. The exit view must
	//use exactly the same fov and aspect as this view, so size the RT to this
	//view's aspect: then a hole pixel and the RT pixel at the same texture uv
	//show matching rays, and the opening can be filled with the RT sampled by
	//the opening screen position itself (the same frames of reference the
	//stencil path relies on, without a stencil buffer).
	if ( m_bPortalViewTargetsCreated == false )
	{
		m_bPortalViewTargetsCreated = true;

		int iWidth = 256;
		int iHeight = 256;
		const CViewSetup *pPlayerView = pMainView->GetPlayerViewSetup();
		if ( pPlayerView != NULL && pPlayerView->width > 0 && pPlayerView->height > 0 )
		{
			float flAspect = (float)pPlayerView->width / (float)pPlayerView->height;
			iHeight = (int)( (float)iWidth / flAspect + 0.5f );
			if ( iHeight < 64 )
				iHeight = 64;
		}

		m_pPortalViewTextures[0].Init( materials->CreateNamedRenderTargetTextureEx2(
			"_rt_Portal1", iWidth, iHeight, RT_SIZE_PICMIP,
			materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 0 ) );
		m_pPortalViewTextures[1].Init( materials->CreateNamedRenderTargetTextureEx2(
			"_rt_Portal2", iWidth, iHeight, RT_SIZE_PICMIP,
			materials->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SHARED,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 0 ) );
	}

	bool bDrewAny = false;

	for ( int i = 0; i < visiblePortals.Count() && i < 2; ++i )
	{
		C_Prop_Portal *pEnterPortal = visiblePortals[i];
		C_Prop_Portal *pExitPortal = pEnterPortal->m_hLinkedPortal;
		if ( pExitPortal == NULL )
			continue;

		int iSlot = ( pEnterPortal->m_iPortalType == PORTAL_TYPE_BLUE ) ? 0 : 1;
		if ( m_pPortalViewTextures[iSlot] == NULL )
			continue;

		//build the virtual view through the enter portal. the view setup keeps
		//this view's fov and aspect, and is dimensioned to the RT's actual size
		//(picmip may have downscaled the texture) so the viewport fills the RT
		CViewSetup portalView;
		BuildPortalView( pEnterPortal, pMainView, portalView );
		portalView.x = 0;
		portalView.y = 0;
		portalView.width = m_pPortalViewTextures[iSlot]->GetActualWidth();
		portalView.height = m_pPortalViewTextures[iSlot]->GetActualHeight();

		//render the exit scene into the per-slot render target; the local
		//frustum scratch matches the DrawMonitors pattern (Push3DView fills it
		//from the view setup - the main view's frustum must not be reused)
		Frustum frustum;
		render->Push3DView( portalView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, m_pPortalViewTextures[iSlot], (VPlane *)frustum );
		pMainView->ViewDrawScene( false, SKYBOX_NOT_VISIBLE, portalView, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, VIEW_MONITOR );
		render->PopView( (VPlane *)frustum );

		DrawPortalRTThroughQuad( pEnterPortal );
		bDrewAny = true;
	}

	return bDrewAny;
}

//-----------------------------------------------------------------------------
// Purpose: draw a world-space quad at the enter portal's opening, textured with
//			the exit render target. The exit view is this view teleported
//			through the portal with the same fov/aspect, so a hole pixel and the
//			RT pixel at the same texture uv show the matching ray. Each
//			corner's UV is therefore its own projection in this view.
//-----------------------------------------------------------------------------
void CPortalRender::DrawPortalRTThroughQuad( C_Prop_Portal *pPortal )
{
	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pMaterial = GetPortalRTMaterial( pPortal->m_iPortalType );
	if ( pMaterial == NULL )
		return;

	pRenderContext->Bind( pMaterial );

	//nudge the opening off the wall toward the camera to avoid z-fighting
	Vector vecNudge = pPortal->m_plane_Origin.normal;
	if ( DotProduct( CurrentViewOrigin() - pPortal->GetPortalOrigin(), vecNudge ) < 0.0f )
		vecNudge.Negate();
	vecNudge *= 0.5f;

	const Vector &vBL = pPortal->m_vPortalCorners[0];
	const Vector &vBR = pPortal->m_vPortalCorners[1];
	const Vector &vTR = pPortal->m_vPortalCorners[2];
	const Vector &vTL = pPortal->m_vPortalCorners[3];

	Vector vecQuad[4];
	vecQuad[0] = vBL + vecNudge;
	vecQuad[1] = vBR + vecNudge;
	vecQuad[2] = vTR + vecNudge;
	vecQuad[3] = vTL + vecNudge;

	//uv for each corner: its own screen position in this view
	Vector2D vecUV[4];
	for ( int c = 0; c < 4; ++c )
	{
		Vector screen;
		if ( ScreenTransform( vecQuad[c], screen ) != 0 )
		{
			//corner behind the camera; clamp so the quad still rasterises
			screen.x = ( screen.x >= 0.0f ) ? 4.0f : -4.0f;
			screen.y = ( screen.y >= 0.0f ) ? 4.0f : -4.0f;
		}
		vecUV[c].x = ( screen.x + 1.0f ) * 0.5f;
		vecUV[c].y = portal_rt_vflip.GetBool() ? ( 1.0f - screen.y ) * 0.5f : ( screen.y + 1.0f ) * 0.5f;
	}

	//texture winding may be flipped; keep two-sided so the quad always draws
	pMaterial->SetMaterialVarFlag( MATERIAL_VAR_NOCULL, true );

	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	if ( pMesh == NULL )
		return;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	for ( int i = 0; i < 4; ++i )
	{
		meshBuilder.Position3fv( vecQuad[i].Base() );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.TexCoord2f( 0, vecUV[i].x, vecUV[i].y );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: return (and lazily create) the material used to composite a portal
//			render target onto an opening. Kept per-colour so both portals can
//			show through simultaneously.
//-----------------------------------------------------------------------------
IMaterial *CPortalRender::GetPortalRTMaterial( int iPortalType )
{
	int iSlot = ( iPortalType == PORTAL_TYPE_BLUE ) ? 0 : 1;

	if ( m_pPortalRTMaterial[iSlot] == NULL )
	{
		KeyValues *pKV = new KeyValues( "UnlitGeneric" );
		pKV->SetInt( "$vertexcolor", 1 );
		pKV->SetInt( "$nocull", 1 );

		char szName[] = "portal/portal_rt_0";
		szName[ sizeof( szName ) - 2 ] = ( iSlot == 0 ) ? '1' : '2';
		m_pPortalRTMaterial[iSlot] = materials->CreateMaterial( szName, pKV );

		if ( m_pPortalRTMaterial[iSlot] != NULL )
		{
			m_pPortalRTBaseTex[iSlot] = m_pPortalRTMaterial[iSlot]->FindVar( "$basetexture", NULL );
		}
	}

	if ( m_pPortalRTMaterial[iSlot] != NULL &&
		 m_pPortalRTBaseTex[iSlot] != NULL &&
		 m_pPortalViewTextures[iSlot] != NULL )
	{
		m_pPortalRTBaseTex[iSlot]->SetTextureValue( m_pPortalViewTextures[iSlot] );
	}

	return m_pPortalRTMaterial[iSlot];
}

#endif // PORTAL
