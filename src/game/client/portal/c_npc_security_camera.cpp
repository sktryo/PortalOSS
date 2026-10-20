//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side of npc_security_camera. The server class sends no
//			properties, so the client table is empty as well; the model and
//			the entity origin come from the base class.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_baseanimating.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_NPC_SecurityCamera : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_NPC_SecurityCamera, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
};

IMPLEMENT_CLIENTCLASS_DT( C_NPC_SecurityCamera, DT_NPC_SecurityCamera, CNPC_SecurityCamera )
END_RECV_TABLE()
