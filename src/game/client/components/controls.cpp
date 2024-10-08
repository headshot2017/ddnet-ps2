/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/tl/sorted_array.h>

#include <base/math.h>

#include <libpad.h>

#include <engine/shared/config.h>
#include <engine/serverbrowser.h>

#include <game/collision.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>
#include <game/client/components/chat.h>
#include <game/client/components/menus.h>
#include <game/client/components/scoreboard.h>

#include "controls.h"

enum { LEFT_JOYSTICK_X = 0, LEFT_JOYSTICK_Y = 1,
	RIGHT_JOYSTICK_X = 2, RIGHT_JOYSTICK_Y = 3,
	SECOND_RIGHT_JOYSTICK_X = 20, SECOND_RIGHT_JOYSTICK_Y = 21,
	NUM_JOYSTICK_AXES = 22 };

CControls::CControls()
{
	mem_zero(&m_LastData, sizeof(m_LastData));
	m_LastDummy = 0;
	m_OtherFire = 0;

	m_UsingGamepad = false;
}

void CControls::OnReset()
{
	ResetInput(0);
	ResetInput(1);

	m_JoystickFirePressed = false;
	m_JoystickRunPressed = false;
	m_JoystickTapTime = 0;
	for( int i = 0; i < NUM_WEAPONS; i++ )
		m_AmmoCount[i] = 0;
	m_OldMouseX = m_OldMouseY = 0.0f;
}

void CControls::ResetInput(int dummy)
{
	m_LastData[dummy].m_Direction = 0;
	//m_LastData.m_Hook = 0;
	// simulate releasing the fire button
	if((m_LastData[dummy].m_Fire&1) != 0)
		m_LastData[dummy].m_Fire++;
	m_LastData[dummy].m_Fire &= INPUT_STATE_MASK;
	m_LastData[dummy].m_Jump = 0;
	m_InputData[dummy] = m_LastData[dummy];

	m_InputDirectionLeft[dummy] = 0;
	m_InputDirectionRight[dummy] = 0;
}

void CControls::OnRelease()
{
	//OnReset();
}

void CControls::OnPlayerDeath()
{
	if (g_Config.m_ClResetWantedWeaponOnDeath)
		m_LastData[g_Config.m_ClDummy].m_WantedWeapon = m_InputData[g_Config.m_ClDummy].m_WantedWeapon = 0;
	for( int i = 0; i < NUM_WEAPONS; i++ )
		m_AmmoCount[i] = 0;
	m_JoystickTapTime = 0; // Do not launch hook on first tap
}

struct CInputState
{
	CControls *m_pControls;
	int *m_pVariable1;
	int *m_pVariable2;
};

static void ConKeyInputState(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;

	CServerInfo Info;
	pState->m_pControls->GameClient()->Client()->GetServerInfo(&Info);

	if ((IsRace(&Info) || IsDDRace(&Info)) && pState->m_pControls->GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	if (g_Config.m_ClDummy)
		*pState->m_pVariable2 = pResult->GetInteger(0);
	else
		*pState->m_pVariable1 = pResult->GetInteger(0);
}

static void ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;

	CServerInfo Info;
	pState->m_pControls->GameClient()->Client()->GetServerInfo(&Info);

	if ((IsRace(&Info) || IsDDRace(&Info)) && pState->m_pControls->GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	int *v;
	if (g_Config.m_ClDummy)
		v = pState->m_pVariable2;
	else
		v = pState->m_pVariable1;

	if(((*v)&1) != pResult->GetInteger(0))
		(*v)++;
	*v &= INPUT_STATE_MASK;
}

struct CInputSet
{
	CControls *m_pControls;
	int *m_pVariable1;
	int *m_pVariable2;
	int m_Value;
};

static void ConKeyInputSet(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	if(pResult->GetInteger(0))
	{
		if (g_Config.m_ClDummy)
			*pSet->m_pVariable2 = pSet->m_Value;
		else
			*pSet->m_pVariable1 = pSet->m_Value;
	}
}

static void ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	ConKeyInputCounter(pResult, pSet);
	pSet->m_pControls->m_InputData[g_Config.m_ClDummy].m_WantedWeapon = 0;
}

void CControls::OnConsoleInit()
{
	// game commands
	{ static CInputState s_State = {this, &m_InputDirectionLeft[0], &m_InputDirectionLeft[1]}; Console()->Register("+left", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Move left"); }
	{ static CInputState s_State = {this, &m_InputDirectionRight[0], &m_InputDirectionRight[1]}; Console()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Move right"); }
	{ static CInputState s_State = {this, &m_InputData[0].m_Jump, &m_InputData[1].m_Jump}; Console()->Register("+jump", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Jump"); }
	{ static CInputState s_State = {this, &m_InputData[0].m_Hook, &m_InputData[1].m_Hook}; Console()->Register("+hook", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Hook"); }
	{ static CInputState s_State = {this, &m_InputData[0].m_Fire, &m_InputData[1].m_Fire}; Console()->Register("+fire", "", CFGFLAG_CLIENT, ConKeyInputCounter, (void *)&s_State, "Fire"); }
	{ static CInputState s_State = {this, &m_ShowHookColl[0], &m_ShowHookColl[1]}; Console()->Register("+showhookcoll", "", CFGFLAG_CLIENT, ConKeyInputState, (void *)&s_State, "Show Hook Collision"); }

	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 1}; Console()->Register("+weapon1", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to hammer"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 2}; Console()->Register("+weapon2", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to gun"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 3}; Console()->Register("+weapon3", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to shotgun"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 4}; Console()->Register("+weapon4", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to grenade"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_WantedWeapon, &m_InputData[1].m_WantedWeapon, 5}; Console()->Register("+weapon5", "", CFGFLAG_CLIENT, ConKeyInputSet, (void *)&s_Set, "Switch to rifle"); }

	{ static CInputSet s_Set = {this, &m_InputData[0].m_NextWeapon, &m_InputData[1].m_NextWeapon, 0}; Console()->Register("+nextweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *)&s_Set, "Switch to next weapon"); }
	{ static CInputSet s_Set = {this, &m_InputData[0].m_PrevWeapon, &m_InputData[1].m_PrevWeapon, 0}; Console()->Register("+prevweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, (void *)&s_Set, "Switch to previous weapon"); }
}

void CControls::OnMessage(int Msg, void *pRawMsg)
{
	if(Msg == NETMSGTYPE_SV_WEAPONPICKUP)
	{
		CNetMsg_Sv_WeaponPickup *pMsg = (CNetMsg_Sv_WeaponPickup *)pRawMsg;
		if(g_Config.m_ClAutoswitchWeapons)
			m_InputData[g_Config.m_ClDummy].m_WantedWeapon = pMsg->m_Weapon+1;
		// We don't really know ammo count, until we'll switch to that weapon, but any non-zero count will suffice here
		m_AmmoCount[pMsg->m_Weapon%NUM_WEAPONS] = 10;
	}
}

int CControls::SnapInput(int *pData)
{
	static int64 LastSendTime = 0;
	bool Send = false;

	// update player state
	if(m_pClient->m_pChat->IsActive())
		m_InputData[g_Config.m_ClDummy].m_PlayerFlags = PLAYERFLAG_CHATTING;
	else if(m_pClient->m_pMenus->IsActive())
		m_InputData[g_Config.m_ClDummy].m_PlayerFlags = PLAYERFLAG_IN_MENU;
	else
		m_InputData[g_Config.m_ClDummy].m_PlayerFlags = PLAYERFLAG_PLAYING;

	if(m_pClient->m_pScoreboard->Active())
		m_InputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_SCOREBOARD;

	if(m_InputData[g_Config.m_ClDummy].m_PlayerFlags != PLAYERFLAG_PLAYING)
		m_JoystickTapTime = 0; // Do not launch hook on first tap

	if (m_pClient->m_pControls->m_ShowHookColl[g_Config.m_ClDummy])
		m_InputData[g_Config.m_ClDummy].m_PlayerFlags |= PLAYERFLAG_AIM;

	if(m_LastData[g_Config.m_ClDummy].m_PlayerFlags != m_InputData[g_Config.m_ClDummy].m_PlayerFlags)
		Send = true;

	m_LastData[g_Config.m_ClDummy].m_PlayerFlags = m_InputData[g_Config.m_ClDummy].m_PlayerFlags;

	// we freeze the input if chat or menu is activated
	if(!(m_InputData[g_Config.m_ClDummy].m_PlayerFlags&PLAYERFLAG_PLAYING))
	{
		ResetInput(g_Config.m_ClDummy);

		mem_copy(pData, &m_InputData[g_Config.m_ClDummy], sizeof(m_InputData[0]));

		// send once a second just to be sure
		if(time_get() > LastSendTime + time_freq())
			Send = true;
	}
	else
	{
		m_InputData[g_Config.m_ClDummy].m_TargetX = (int)m_MousePos[g_Config.m_ClDummy].x;
		m_InputData[g_Config.m_ClDummy].m_TargetY = (int)m_MousePos[g_Config.m_ClDummy].y;
		if(!m_InputData[g_Config.m_ClDummy].m_TargetX && !m_InputData[g_Config.m_ClDummy].m_TargetY)
		{
			m_InputData[g_Config.m_ClDummy].m_TargetX = 1;
			m_MousePos[g_Config.m_ClDummy].x = 1;
		}

		// set direction
		m_InputData[g_Config.m_ClDummy].m_Direction = 0;
		if(m_InputDirectionLeft[g_Config.m_ClDummy] && !m_InputDirectionRight[g_Config.m_ClDummy])
			m_InputData[g_Config.m_ClDummy].m_Direction = -1;
		if(!m_InputDirectionLeft[g_Config.m_ClDummy] && m_InputDirectionRight[g_Config.m_ClDummy])
			m_InputData[g_Config.m_ClDummy].m_Direction = 1;

		// dummy copy moves
		if(g_Config.m_ClDummyCopyMoves)
		{
			CNetObj_PlayerInput *DummyInput = &Client()->m_DummyInput;
			DummyInput->m_Direction = m_InputData[g_Config.m_ClDummy].m_Direction;
			DummyInput->m_Hook = m_InputData[g_Config.m_ClDummy].m_Hook;
			DummyInput->m_Jump = m_InputData[g_Config.m_ClDummy].m_Jump;
			DummyInput->m_PlayerFlags = m_InputData[g_Config.m_ClDummy].m_PlayerFlags;
			DummyInput->m_TargetX = m_InputData[g_Config.m_ClDummy].m_TargetX;
			DummyInput->m_TargetY = m_InputData[g_Config.m_ClDummy].m_TargetY;
			DummyInput->m_WantedWeapon = m_InputData[g_Config.m_ClDummy].m_WantedWeapon;



			DummyInput->m_Fire += m_InputData[g_Config.m_ClDummy].m_Fire - m_LastData[g_Config.m_ClDummy].m_Fire;
			DummyInput->m_NextWeapon += m_InputData[g_Config.m_ClDummy].m_NextWeapon - m_LastData[g_Config.m_ClDummy].m_NextWeapon;
			DummyInput->m_PrevWeapon += m_InputData[g_Config.m_ClDummy].m_PrevWeapon - m_LastData[g_Config.m_ClDummy].m_PrevWeapon;

			m_InputData[!g_Config.m_ClDummy] = *DummyInput;
		}

		// stress testing
		if(g_Config.m_DbgStress)
		{
			float t = Client()->LocalTime();
			mem_zero(&m_InputData[g_Config.m_ClDummy], sizeof(m_InputData[0]));

			m_InputData[g_Config.m_ClDummy].m_Direction = ((int)t/2)&1;
			m_InputData[g_Config.m_ClDummy].m_Jump = ((int)t);
			m_InputData[g_Config.m_ClDummy].m_Fire = ((int)(t*10));
			m_InputData[g_Config.m_ClDummy].m_Hook = ((int)(t*2))&1;
			m_InputData[g_Config.m_ClDummy].m_WantedWeapon = ((int)t)%NUM_WEAPONS;
			m_InputData[g_Config.m_ClDummy].m_TargetX = (int)(sinf(t*3)*100.0f);
			m_InputData[g_Config.m_ClDummy].m_TargetY = (int)(cosf(t*3)*100.0f);
		}

		// check if we need to send input
		if(m_InputData[g_Config.m_ClDummy].m_Direction != m_LastData[g_Config.m_ClDummy].m_Direction) Send = true;
		else if(m_InputData[g_Config.m_ClDummy].m_Jump != m_LastData[g_Config.m_ClDummy].m_Jump) Send = true;
		else if(m_InputData[g_Config.m_ClDummy].m_Fire != m_LastData[g_Config.m_ClDummy].m_Fire) Send = true;
		else if(m_InputData[g_Config.m_ClDummy].m_Hook != m_LastData[g_Config.m_ClDummy].m_Hook) Send = true;
		else if(m_InputData[g_Config.m_ClDummy].m_WantedWeapon != m_LastData[g_Config.m_ClDummy].m_WantedWeapon) Send = true;
		else if(m_InputData[g_Config.m_ClDummy].m_NextWeapon != m_LastData[g_Config.m_ClDummy].m_NextWeapon) Send = true;
		else if(m_InputData[g_Config.m_ClDummy].m_PrevWeapon != m_LastData[g_Config.m_ClDummy].m_PrevWeapon) Send = true;

		// send at at least 10hz
		if(time_get() > LastSendTime + time_freq()/25)
			Send = true;

		if(m_pClient->m_Snap.m_pLocalCharacter && m_pClient->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_NINJA
			&& (m_InputData[g_Config.m_ClDummy].m_Direction || m_InputData[g_Config.m_ClDummy].m_Jump || m_InputData[g_Config.m_ClDummy].m_Hook))
			Send = true;
	}

	// copy and return size
	m_LastData[g_Config.m_ClDummy] = m_InputData[g_Config.m_ClDummy];

	if(!Send)
		return 0;

	LastSendTime = time_get();
	mem_copy(pData, &m_InputData[g_Config.m_ClDummy], sizeof(m_InputData[0]));
	return sizeof(m_InputData[0]);
}

void CControls::OnRender()
{
	enum {
		JOYSTICK_RUN_DISTANCE = 65536 / 8,
		GAMEPAD_DEAD_ZONE = 65536 / 8,
	};

	int64 CurTime = time_get();
	bool FireWasPressed = false;

	CServerInfo Info;
	GameClient()->Client()->GetServerInfo(&Info);

	if( g_Config.m_ClAutoswitchWeaponsOutOfAmmo && !IsRace(&Info) && !IsDDRace(&Info) && m_pClient->m_Snap.m_pLocalCharacter )
	{
		// Keep track of ammo count, we know weapon ammo only when we switch to that weapon, this is tracked on server and protocol does not track that
		m_AmmoCount[m_pClient->m_Snap.m_pLocalCharacter->m_Weapon%NUM_WEAPONS] = m_pClient->m_Snap.m_pLocalCharacter->m_AmmoCount;
		// Autoswitch weapon if we're out of ammo
		if( (m_InputData[g_Config.m_ClDummy].m_Fire % 2 != 0 || FireWasPressed) &&
			m_pClient->m_Snap.m_pLocalCharacter->m_AmmoCount == 0 &&
			m_pClient->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_HAMMER &&
			m_pClient->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_NINJA )
		{
			int w;
			for( w = WEAPON_RIFLE; w > WEAPON_GUN; w-- )
			{
				if( w == m_pClient->m_Snap.m_pLocalCharacter->m_Weapon )
					continue;
				if( m_AmmoCount[w] > 0 )
					break;
			}
			if( w != m_pClient->m_Snap.m_pLocalCharacter->m_Weapon )
				m_InputData[g_Config.m_ClDummy].m_WantedWeapon = w+1;
		}
	}

	// update target pos
	if(m_pClient->m_Snap.m_pGameInfoObj && !m_pClient->m_Snap.m_SpecInfo.m_Active)
		m_TargetPos[g_Config.m_ClDummy] = m_pClient->m_LocalCharacterPos + m_MousePos[g_Config.m_ClDummy];
	else if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
		m_TargetPos[g_Config.m_ClDummy] = m_pClient->m_Snap.m_SpecInfo.m_Position + m_MousePos[g_Config.m_ClDummy];
	else
		m_TargetPos[g_Config.m_ClDummy] = m_MousePos[g_Config.m_ClDummy];
}

bool CControls::OnMouseMove(float x, float y)
{
	if((m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED) ||
		(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_pChat->IsActive()))
		return false;

	if( m_OldMouseX != x || m_OldMouseY != y )
	{
		m_OldMouseX = -g_Config.m_GfxScreenWidth/2 + x * 2.f;
		m_OldMouseY = -g_Config.m_GfxScreenHeight/2 + y * 2.f;
		m_MousePos[g_Config.m_ClDummy] = vec2((m_OldMouseX - g_Config.m_GfxScreenWidth/2), (m_OldMouseY - g_Config.m_GfxScreenHeight/2));
		ClampMousePos();
	}

	return true;
}

void CControls::ClampMousePos()
{
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		m_MousePos[g_Config.m_ClDummy].x = clamp(m_MousePos[g_Config.m_ClDummy].x, 200.0f, Collision()->GetWidth()*32-200.0f);
		m_MousePos[g_Config.m_ClDummy].y = clamp(m_MousePos[g_Config.m_ClDummy].y, 200.0f, Collision()->GetHeight()*32-200.0f);
	}
	else
	{
		float CameraMaxDistance = 200.0f;
		float FollowFactor = (g_Config.m_ClDyncam ? g_Config.m_ClDyncamFollowFactor : g_Config.m_ClMouseFollowfactor) / 100.0f;
		float DeadZone = g_Config.m_ClDyncam ? g_Config.m_ClDyncamDeadzone : g_Config.m_ClMouseDeadzone;
		float MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
		float MouseMax = min(CameraMaxDistance/FollowFactor + DeadZone, MaxDistance);

		if(length(m_MousePos[g_Config.m_ClDummy]) > MouseMax)
			m_MousePos[g_Config.m_ClDummy] = normalize(m_MousePos[g_Config.m_ClDummy])*MouseMax;
	}
}
