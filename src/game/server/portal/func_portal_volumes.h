//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal volume brush entities:
//
//			func_noportal_volume	- surfaces inside the volume cannot be
//									  portalized; portals near the edge are
//									  nudged out of the volume
//			func_portal_bumper		- portals that would touch the volume are
//									  nudged so they do not touch it
//			func_portal_orientation - portals placed inside are reoriented so
//									  their bottom edge points along a given
//									  direction
//			func_portal_detector	- outputs when portals are inside the volume
//
// All four operate on their world bounding box (like the original entities,
// which use the box rather than the brush faces), so concave regions should be
// split into multiple entities.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PORTAL_FUNC_VOLUMES_H
#define PORTAL_FUNC_VOLUMES_H
#pragma once

class CBaseEntity;
class CProp_Portal;

// True when the given point is inside any active func_noportal_volume.
bool Portal_PointBlockedByNoPortalVolume( const Vector &vecPoint );

// Returns the AnglesToFace angles of an active func_portal_orientation whose
// bounds contain the point. Returns false when no volume covers the point.
bool Portal_GetOrientationForPosition( const Vector &vecPoint, QAngle &angOrientation );

// Adjusts the placement center of a proposed portal so its footprint (the
// rectangle built from center/±halfWidth/right ±halfHeight/up) does not
// intersect any active func_noportal_volume or func_portal_bumper.
// Returns false when the placement must fail (surface inside a noportal
// volume, or the volumes cannot be avoided within the nudge range).
// bCenterMoved is set when the caller must re-measure placement extents.
bool Portal_AdjustPlacementForVolumes( Vector &vecCenter, const Vector &vecNormal,
									   const Vector &vecRight, const Vector &vecUp,
									   float flHalfWidth, float flHalfHeight,
									   bool &bCenterMoved );

#endif // PORTAL_FUNC_VOLUMES_H
