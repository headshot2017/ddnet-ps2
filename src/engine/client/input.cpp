/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <unordered_map>

#include <base/system.h>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>

#include <libpad.h>

#include "input.h"

//print >>f, "int inp_key_code(const char *key_name) { int i; if (!strcmp(key_name, \"-?-\")) return -1; else for (i = 0; i < 512; i++) if (!strcmp(key_strings[i], key_name)) return i; return -1; }"

// this header is protected so you don't include it from anywere
#define KEYS_INCLUDE
#include "keynames.h"
#undef KEYS_INCLUDE

static char padBuf0[256] __attribute__((aligned(64)));

std::unordered_map<int, int> PS2keys = {
	{PAD_CIRCLE, KEY_RETURN},
	{PAD_UP, KEY_RSHIFT},
	{PAD_DOWN, KEY_LSHIFT},
	{PAD_LEFT, KEY_LEFT},
	{PAD_RIGHT, KEY_RIGHT},
	{PAD_L2, KEY_MOUSE_WHEEL_UP},
	{PAD_R2, KEY_MOUSE_WHEEL_DOWN},
	{PAD_START, KEY_ESCAPE},
	{PAD_SQUARE, KEY_SPACE},
	// left/right movement of left joystick: KEY_a, KEY_d
	// right joystick: mouse
	// L/R shoulders: KEY_MOUSE_2, KEY_MOUSE_1
	// CROSS: KEY_MOUSE_1
};

void CInput::AddEvent(int Unicode, int Key, int Flags)
{
	if(m_NumEvents != INPUT_BUFFER_SIZE)
	{
		m_aInputEvents[m_NumEvents].m_Unicode = Unicode;
		m_aInputEvents[m_NumEvents].m_Key = Key;
		m_aInputEvents[m_NumEvents].m_Flags = Flags;
		m_NumEvents++;
	}
}

bool CInput::LeftClick(unsigned short btns)
{
	return (btns & PAD_CROSS) || (btns & PAD_L1);
}

bool CInput::RightClick(unsigned short btns)
{
	return !!(btns & PAD_R1);
}

int CInput::LeftJoystickX(void* p)
{
	padButtonStatus* pad = (padButtonStatus*)p;
	return (pad->ljoy_h <= 127-24) ? -1 : (pad->ljoy_v >= 127+24) ? 1 : 0;
}

CInput::CInput()
{
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
	mem_zero(m_aInputState, sizeof(m_aInputState));

	m_InputCurrent = 0;
	m_InputGrabbed = 0;
	m_InputDispatched = false;

	m_LastRelease = 0;
	m_ReleaseDelta = -1;

	m_NumEvents = 0;

	m_RightJoyX = m_RightJoyY = 127;

	m_VideoRestartNeeded = 0;
}

void CInput::Init()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();

	padInit(0);
	padPortOpen(0, 0, padBuf0);
	while (padGetState(0, 0) != PAD_STATE_STABLE);
	padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
}

void CInput::MouseRelative(float *x, float *y)
{
	static float nx = 0, ny = 0;
	float Sens = ((g_Config.m_ClDyncam && g_Config.m_ClDyncamMousesens) ? g_Config.m_ClDyncamMousesens : g_Config.m_InpMousesens) / 5.f;

	float joyX = (m_RightJoyX-127) / 128.f;
	float joyY = (m_RightJoyY-127) / 128.f;

	nx = clamp(nx + (joyX*Sens), 0.f, (float)Graphics()->ScreenWidth());
	ny = clamp(ny + (joyY*Sens), 0.f, (float)Graphics()->ScreenHeight());

	*x = nx;
	*y = ny;
}

void CInput::MouseModeAbsolute()
{
	m_InputGrabbed = 0;
}

void CInput::MouseModeRelative()
{
	m_InputGrabbed = 1;
}

int CInput::MouseDoubleClick()
{
	if(m_ReleaseDelta >= 0 && m_ReleaseDelta < (time_freq() / 3))
	{
		m_LastRelease = 0;
		m_ReleaseDelta = -1;
		return 1;
	}
	return 0;
}

void CInput::ClearKeyStates()
{
	mem_zero(m_aInputState, sizeof(m_aInputState));
	mem_zero(m_aInputCount, sizeof(m_aInputCount));
}

int CInput::KeyState(int Key)
{
	return m_aInputState[m_InputCurrent][Key];
}

int CInput::Update()
{
	if(m_InputGrabbed && !Graphics()->WindowActive())
		MouseModeAbsolute();

	/*if(!input_grabbed && Graphics()->WindowActive())
		Input()->MouseModeRelative();*/

	if(m_InputDispatched)
	{
		// clear and begin count on the other one
		m_InputCurrent^=1;
		mem_zero(&m_aInputCount[m_InputCurrent], sizeof(m_aInputCount[m_InputCurrent]));
		mem_zero(&m_aInputState[m_InputCurrent], sizeof(m_aInputState[m_InputCurrent]));
		m_InputDispatched = false;
	}

	padButtonStatus pad;
	int state = padGetState(0, 0);
	if (state != PAD_STATE_STABLE || padRead(0, 0, &pad) == 0) return 0;
	pad.btns ^= 0xffff; // flip bits

	static unsigned short lastBtns = 0;
	static bool lastLeftClick = false;
	static bool lastRightClick = false;
	static int lastJoyX = 0;

	unsigned short buttonsDown = pad.btns &~ lastBtns;
	unsigned short buttonsUp = lastBtns &~ pad.btns;
	unsigned short buttonsHeld = pad.btns;

	m_RightJoyX = pad.rjoy_h;
	m_RightJoyY = pad.rjoy_v;

	int Key;

	for(std::unordered_map<int, int>::iterator it = PS2keys.begin(); it != PS2keys.end(); ++it)
	{
		if (buttonsDown & it->first)
		{
			Key = it->second;
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			m_aInputState[m_InputCurrent][Key] = 1;
			AddEvent(0, Key, IInput::FLAG_PRESS);
		}

		if (buttonsUp & it->first)
		{
			Key = it->second;
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			AddEvent(0, Key, IInput::FLAG_RELEASE);
		}
	}

	bool click = LeftClick(buttonsHeld);
	m_aInputState[m_InputCurrent][KEY_MOUSE_1] = click;

	int action = (click) ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
	Key = KEY_MOUSE_1;
	if (lastLeftClick != click)
	{
		m_aInputCount[m_InputCurrent][Key].m_Presses++;
		AddEvent(0, Key, action);

		lastLeftClick = click;
		if (!click)
		{
			m_ReleaseDelta = time_get() - m_LastRelease;
			m_LastRelease = time_get();
		}
	}

	click = RightClick(buttonsHeld);
	action = (click) ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
	Key = KEY_MOUSE_2;
	if (lastRightClick != click)
	{
		m_aInputCount[m_InputCurrent][Key].m_Presses++;
		if (click) m_aInputState[m_InputCurrent][Key] = 1;
		AddEvent(0, Key, action);

		lastRightClick = click;
	}

	int joyX = LeftJoystickX(&pad);
	if (lastJoyX != joyX)
	{
		// release key
		Key = (lastJoyX == -1) ? KEY_a : KEY_d;
		if (lastJoyX)
		{
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			AddEvent(0, Key, IInput::FLAG_RELEASE);
		}

		// press key
		Key = (joyX == -1) ? KEY_a : KEY_d;
		if (joyX)
		{
			m_aInputCount[m_InputCurrent][Key].m_Presses++;
			m_aInputState[m_InputCurrent][Key] = 1;
			AddEvent(0, Key, IInput::FLAG_PRESS);
		}

		lastJoyX = joyX;
	}

	/*
	{
		int i;
		Uint8 *pState = SDL_GetKeyState(&i);
		if(i >= KEY_LAST)
			i = KEY_LAST-1;
		mem_copy(m_aInputState[m_InputCurrent], pState, i);
	}

	// these states must always be updated manually because they are not in the GetKeyState from SDL
	int i = SDL_GetMouseState(NULL, NULL);
	if(i&SDL_BUTTON(1)) m_aInputState[m_InputCurrent][KEY_MOUSE_1] = 1; // 1 is left
	if(i&SDL_BUTTON(3)) m_aInputState[m_InputCurrent][KEY_MOUSE_2] = 1; // 3 is right
	if(i&SDL_BUTTON(2)) m_aInputState[m_InputCurrent][KEY_MOUSE_3] = 1; // 2 is middle
	if(i&SDL_BUTTON(4)) m_aInputState[m_InputCurrent][KEY_MOUSE_4] = 1;
	if(i&SDL_BUTTON(5)) m_aInputState[m_InputCurrent][KEY_MOUSE_5] = 1;
	if(i&SDL_BUTTON(6)) m_aInputState[m_InputCurrent][KEY_MOUSE_6] = 1;
	if(i&SDL_BUTTON(7)) m_aInputState[m_InputCurrent][KEY_MOUSE_7] = 1;
	if(i&SDL_BUTTON(8)) m_aInputState[m_InputCurrent][KEY_MOUSE_8] = 1;
	if(i&SDL_BUTTON(9)) m_aInputState[m_InputCurrent][KEY_MOUSE_9] = 1;

	{
		SDL_Event Event;

		while(SDL_PollEvent(&Event))
		{
			int Key = -1;
			int Action = IInput::FLAG_PRESS;
			switch (Event.type)
			{
				// handle keys
				case SDL_KEYDOWN:
					// skip private use area of the BMP(contains the unicodes for keyboard function keys on MacOS)
					if(Event.key.keysym.unicode < 0xE000 || Event.key.keysym.unicode > 0xF8FF)	// ignore_convention
						AddEvent(Event.key.keysym.unicode, 0, 0); // ignore_convention
					Key = Event.key.keysym.sym; // ignore_convention
					break;
				case SDL_KEYUP:
					Action = IInput::FLAG_RELEASE;
					Key = Event.key.keysym.sym; // ignore_convention
					break;

				// handle mouse buttons
				case SDL_MOUSEBUTTONUP:
					Action = IInput::FLAG_RELEASE;

					if(Event.button.button == 1) // ignore_convention
					{
						m_ReleaseDelta = time_get() - m_LastRelease;
						m_LastRelease = time_get();
					}

					// fall through
				case SDL_MOUSEBUTTONDOWN:
					if(Event.button.button == SDL_BUTTON_LEFT) Key = KEY_MOUSE_1; // ignore_convention
					if(Event.button.button == SDL_BUTTON_RIGHT) Key = KEY_MOUSE_2; // ignore_convention
					if(Event.button.button == SDL_BUTTON_MIDDLE) Key = KEY_MOUSE_3; // ignore_convention
					if(Event.button.button == SDL_BUTTON_WHEELUP) Key = KEY_MOUSE_WHEEL_UP; // ignore_convention
					if(Event.button.button == SDL_BUTTON_WHEELDOWN) Key = KEY_MOUSE_WHEEL_DOWN; // ignore_convention
					if(Event.button.button == 6) Key = KEY_MOUSE_6; // ignore_convention
					if(Event.button.button == 7) Key = KEY_MOUSE_7; // ignore_convention
					if(Event.button.button == 8) Key = KEY_MOUSE_8; // ignore_convention
					if(Event.button.button == 9) Key = KEY_MOUSE_9; // ignore_convention
					break;

				// other messages
				case SDL_QUIT:
					return 1;

#if defined(__ANDROID__)
				case SDL_VIDEORESIZE:
					m_VideoRestartNeeded = 1;
					break;
#endif
			}

			//
			if(Key != -1)
			{
				m_aInputCount[m_InputCurrent][Key].m_Presses++;
				if(Action == IInput::FLAG_PRESS)
					m_aInputState[m_InputCurrent][Key] = 1;
				AddEvent(0, Key, Action);
			}

		}
	}
	*/

	return 0;
}

int CInput::VideoRestartNeeded()
{
	if( m_VideoRestartNeeded )
	{
		m_VideoRestartNeeded = 0;
		return 1;
	}
	return 0;
}

IEngineInput *CreateEngineInput() { return new CInput; }
