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
#include "cdll_client_int.h"
#include "materialsystem/imesh.h"
#include "c_baseplayer.h"
#include "KeyValues.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// view ids for portal views are allocated past the shared view id space
#define PORTAL_VIEW_ID_START	( VIEW_ID_COUNT + 1 )
#define PORTAL_MAX_VIEW_DISTANCE	8000.0f

ConVar portal_render_targets_enabled( "portal_render_targets_enabled", "0", FCVAR_CHEAT,
	"Enable rendering of through-portal views to the _rt_Portal1/_rt_Portal2 render targets." );

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
// Purpose: draw the opening polygon of a portal into the stencil buffer and
//			(optional, dormant portals) faintly on screen
//-----------------------------------------------------------------------------
void CPortalRender::DrawPortalStencilQuad( C_Prop_Portal *pPortal )
{
	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pMaterial = materials->FindMaterial( "debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true );
	pRenderContext->Bind( pMaterial );

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

	pRenderContext->OverrideColorWriteEnable( true, false );

	CMeshBuilder meshBuilder;
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, pMaterial );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	for ( int i = 0; i < 4; ++i )
	{
		meshBuilder.Position3fv( vecQuad[i].Base() );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End( true, true );

	pRenderContext->OverrideColorWriteEnable( false, false );
}

void CPortalRender::DrawDormantPortalSurface( C_Prop_Portal *pPortal )
{
	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pMaterial = materials->FindMaterial( "debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true );
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
		float flDot = DotProduct( pPortal->m_plane_Origin.normal, vecToPortal );

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

			if ( DotProduct( pPortal->m_plane_Origin.normal, vecToPortal ) <= 0.0f )
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

#endif // PORTAL
