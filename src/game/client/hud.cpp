/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud.cpp
//
// implementation of CHud class
//

#include <cstring>
#include <cstdio>
#include <vgui/IScheme.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Controls.h>

#include <appversion.h>
#include <bhl_urls.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui/client_viewport.h"

#include "demo.h"
#include "demo_api.h"
#include "cl_voice_status.h"
#include "bhlcfg.h"

// HUD Elements
#include "hud/ammo.h"
#include "hud/chat.h"
#include "hud/crosshair.h"
#include "hud/health.h"
#include "hud/spectator.h"
#include "hud/geiger.h"
#include "hud/train.h"
#include "hud/battery.h"
#include "hud/flashlight.h"
#include "hud/message.h"
#include "hud/statusbar.h"
#include "hud/death_notice.h"
#include "hud/ammo_secondary.h"
#include "hud/text_message.h"
#include "hud/status_icons.h"
#include "hud/menu.h"
#include "hud/voice_status.h"
#include "hud/voice_status_self.h"

extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

extern cvar_t *sensitivity;
cvar_t *cl_lw = NULL;

ConVar cl_bhopcap("cl_bhopcap", "2", FCVAR_BHL_ARCHIVE, "Enables/disables bhop speed cap, '2' - detect automatically");
ConVar hud_color("hud_color", "255 160 0", FCVAR_BHL_ARCHIVE, "Main color of HUD elements");
ConVar hud_color1("hud_color1", "0 255 0", FCVAR_BHL_ARCHIVE, "HUD color when >= 90%");
ConVar hud_color2("hud_color2", "255 160 0", FCVAR_BHL_ARCHIVE, "HUD color when [50%; 90%)");
ConVar hud_color3("hud_color3", "255 96 0", FCVAR_BHL_ARCHIVE, "HUD color when (25%; 50%)");
ConVar hud_dim("hud_dim", "1", FCVAR_BHL_ARCHIVE, "Dim inactive HUD elements");

static Color s_DefaultColorCodeColors[10] = {
	Color(0xFF, 0xAA, 0x00, 0xFF), // ^0 orange/reset
	Color(0xFF, 0x00, 0x00, 0xFF), // ^1 red
	Color(0x00, 0xFF, 0x00, 0xFF), // ^2 green
	Color(0xFF, 0xFF, 0x00, 0xFF), // ^3 yellow
	Color(0x00, 0x00, 0xFF, 0xFF), // ^4 blue
	Color(0x00, 0xFF, 0xFF, 0xFF), // ^5 cyan
	Color(0xFF, 0x00, 0xFF, 0xFF), // ^6 magenta
	Color(0x88, 0x88, 0x88, 0xFF), // ^7 grey
	Color(0xFF, 0xFF, 0xFF, 0xFF), // ^8 white
	Color(0xFF, 0xAA, 0x00, 0xFF), // ^9 orange/reset
};

const Color NoTeamColor::Orange = Color(255, 178, 0, 255);
const Color NoTeamColor::White = Color(216, 216, 216, 255);

template <void (CClientViewport::*FUNC)(const char *, int, void *)>
void HookViewportMessage(const char *name)
{
	gEngfuncs.pfnHookUserMsg((char *)name, [](const char *pszName, int iSize, void *pbuf) -> int {
		if (g_pViewport)
			(g_pViewport->*FUNC)(pszName, iSize, pbuf);
		return 1;
	});
}

template <int (CHud::*FUNC)(const char *, int, void *)>
void HookHudMessage(const char *name)
{
	gEngfuncs.pfnHookUserMsg((char *)name, [](const char *pszName, int iSize, void *pbuf) -> int {
		return (gHUD.*FUNC)(pszName, iSize, pbuf);
	});
}

static void AboutCommand(void)
{
	ConsolePrint("BugfixedHL-NG\n");
	ConsolePrint("Bugfixed and improved Half-Life Client\n");
	ConsolePrint("Version: " APP_VERSION "\n");
	ConsolePrint("\n");
	ConsolePrint("Github: " BHL_GITHUB_URL "\n");
	ConsolePrint("Discussion forum: " BHL_FORUM_URL "\n");
}

// This is called every time the DLL is loaded
void CHud::Init(void)
{
	// Check that elem list is empty
	Assert(m_HudList.empty());

	// Fill color code colors with default ones
	memcpy(m_ColorCodeColors, s_DefaultColorCodeColors, sizeof(s_DefaultColorCodeColors));

	// Set player info IDs
	for (int i = 1; i < MAX_PLAYERS; i++)
		CPlayerInfo::m_sPlayerInfo[i].m_iIndex = i;

	HookHudMessage<&CHud::MsgFunc_Logo>("Logo");
	HookHudMessage<&CHud::MsgFunc_ResetHUD>("ResetHUD");
	HookHudMessage<&CHud::MsgFunc_GameMode>("GameMode");
	HookHudMessage<&CHud::MsgFunc_InitHUD>("InitHUD");
	HookHudMessage<&CHud::MsgFunc_ViewMode>("ViewMode");
	HookHudMessage<&CHud::MsgFunc_SetFOV>("SetFOV");
	HookHudMessage<&CHud::MsgFunc_Concuss>("Concuss");
	HookHudMessage<&CHud::MsgFunc_Logo>("Logo");

	// TFFree CommandMenu
	HookCommand("+commandmenu", [] {
		// FIXME:
		//if (g_pViewport)
		//	g_pViewport->ShowCommandMenu(g_pViewport->m_StandardMenu);
	});

	HookCommand("-commandmenu", [] {
		if (g_pViewport)
			g_pViewport->InputSignalHideCommandMenu();
	});

	HookCommand("ForceCloseCommandMenu", [] {
		if (g_pViewport)
			g_pViewport->HideCommandMenu();
	});

	HookCommand("special", [] {
		if (g_pViewport)
			g_pViewport->InputPlayerSpecial();
	});

	HookCommand("about", AboutCommand);

	HookViewportMessage<&CClientViewport::MsgFunc_ValClass>("ValClass");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamNames>("TeamNames");
	HookViewportMessage<&CClientViewport::MsgFunc_Feign>("Feign");
	HookViewportMessage<&CClientViewport::MsgFunc_Detpack>("Detpack");
	HookViewportMessage<&CClientViewport::MsgFunc_MOTD>("MOTD");
	HookViewportMessage<&CClientViewport::MsgFunc_BuildSt>("BuildSt");
	HookViewportMessage<&CClientViewport::MsgFunc_RandomPC>("RandomPC");
	HookViewportMessage<&CClientViewport::MsgFunc_ServerName>("ServerName");
	HookViewportMessage<&CClientViewport::MsgFunc_ScoreInfo>("ScoreInfo");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamScore>("TeamScore");
	HookViewportMessage<&CClientViewport::MsgFunc_TeamInfo>("TeamInfo");

	HookViewportMessage<&CClientViewport::MsgFunc_Spectator>("Spectator");
	HookViewportMessage<&CClientViewport::MsgFunc_AllowSpec>("AllowSpec");

	// VGUI Menus
	HookViewportMessage<&CClientViewport::MsgFunc_VGUIMenu>("VGUIMenu");

	CVAR_CREATE("hud_classautokill", "1", FCVAR_ARCHIVE | FCVAR_USERINFO); // controls whether or not to suicide immediately on TF class switch
	CVAR_CREATE("hud_takesshots", "0", FCVAR_ARCHIVE); // controls whether or not to automatically take screenshots at the end of a round

	m_iLogo = 0;
	m_iFOV = 0;

	CVAR_CREATE("zoom_sensitivity_ratio", "1.2", 0);
	default_fov = CVAR_CREATE("default_fov", "90", FCVAR_ARCHIVE);
	m_pCvarStealMouse = CVAR_CREATE("hud_capturemouse", "1", FCVAR_ARCHIVE);
	m_pCvarDraw = CVAR_CREATE("hud_draw", "1", FCVAR_ARCHIVE);
	cl_lw = gEngfuncs.pfnGetCvarPointer("cl_lw");

	m_pSpriteList = NULL;

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	// Load default HUD colors into m_HudColor*.
	UpdateHudColors();

	// Create HUD elements
	RegisterHudElem<CHudAmmo>();
	RegisterHudElem<CHudHealth>();
	RegisterHudElem<CHudChat>();
	RegisterHudElem<CHudCrosshair>();
	RegisterHudElem<CHudSpectator>();
	RegisterHudElem<CHudGeiger>();
	RegisterHudElem<CHudTrain>();
	RegisterHudElem<CHudBattery>();
	RegisterHudElem<CHudFlashlight>();
	RegisterHudElem<CHudMessage>();
	RegisterHudElem<CHudStatusBar>();
	RegisterHudElem<CHudDeathNotice>();
	RegisterHudElem<CHudAmmoSecondary>();
	RegisterHudElem<CHudTextMessage>();
	RegisterHudElem<CHudStatusIcons>();
	RegisterHudElem<CHudMenu>();
	RegisterHudElem<CHudVoiceStatus>();
	RegisterHudElem<CHudVoiceStatusSelf>();

	ClientVoiceMgr_Init();

	// Init HUD elements
	for (CHudElem *i : m_HudList)
		i->Init();

	m_HudList.shrink_to_fit();
	MsgFunc_ResetHUD(0, 0, NULL);

	g_pViewport->ReloadLayout();

	bhlcfg::Init();
}

CHud::CHud()
{
}

CHud::~CHud()
{
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud::GetSpriteIndex(const char *SpriteName)
{
	// look through the loaded sprite name list for SpriteName
	for (int i = 0; i < m_iSpriteCount; i++)
	{
		if (Q_stricmp(SpriteName, m_rgszSpriteNames[i].name) == 0)
			return i;
	}

	return -1; // invalid sprite
}

void CHud::AddSprite(client_sprite_t *p)
{
	// Search for existing sprite
	int i = 0;
	for (i = 0; i < m_iSpriteCount; i++)
	{
		if (!Q_stricmp(m_rgszSpriteNames[i].name, p->szName))
			return;
	}

	char sz[256];
	snprintf(sz, sizeof(sz), "sprites/%s.spr", p->szSprite);

	m_rghSprites.push_back(SPR_Load(sz));
	m_rgrcRects.push_back(p->rc);

	// Copy sprite name
	m_rgszSpriteNames.push_back({});
	Q_strncpy(m_rgszSpriteNames[m_iSpriteCount].name, p->szName, MAX_SPRITE_NAME_LENGTH);

	m_iSpriteCount++;

	Assert(m_rghSprites.size() == m_iSpriteCount);
	Assert(m_rgrcRects.size() == m_iSpriteCount);
	Assert(m_rgszSpriteNames.size() == m_iSpriteCount);
}

void CHud::VidInit(void)
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// ----------
	// Load Sprites
	// ---------
	//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;

	if (ScreenWidth < 640)
		m_iRes = 320;
	else
		m_iRes = 640;

	// Only load this once
	if (!m_pSpriteList)
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			int j;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
			m_rghSprites.resize(m_iSpriteCount);
			m_rgrcRects.resize(m_iSpriteCount);
			m_rgszSpriteNames.resize(m_iSpriteCount);

			p = m_pSpriteList;
			int index = 0;
			for (j = 0; j < m_iSpriteCountAllRes; j++)
			{
				if (p->iRes == m_iRes)
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					Q_strncpy(m_rgszSpriteNames[index].name, p->szName, MAX_SPRITE_NAME_LENGTH);

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for (int j = 0; j < m_iSpriteCountAllRes; j++)
		{
			if (p->iRes == m_iRes)
			{
				char sz[256];
				sprintf(sz, "sprites/%s.spr", p->szSprite);
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex("number_0");

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	for (CHudElem *i : m_HudList)
		i->VidInit();
}

void CHud::Frame(double time)
{
	vgui2::GetAnimationController()->UpdateAnimations(gEngfuncs.GetClientTime());

	CHudVoiceStatus::Get()->RunFrame(time);
	g_pViewport->GetAllPlayersInfo();

	while (m_NextFrameQueue.size())
	{
		auto &i = m_NextFrameQueue.front();
		i();
		m_NextFrameQueue.pop();
	}
}

void CHud::Shutdown()
{
	bhlcfg::Shutdown();
	ClientVoiceMgr_Shutdown();

	for (CHudElem *i : m_HudList)
	{
		// VGUI2 is shut down before HUD_Shutdown is called.
		// vgui2::~Panel calls VGUI2 interfaces which are not available.
		if (!dynamic_cast<vgui2::Panel *>(i))
			delete i;
	}
}

void CHud::ApplyViewportSchemeSettings(vgui2::IScheme *pScheme)
{
	char buf[64];
	for (int i = 0; i <= 9; i++)
	{
		snprintf(buf, sizeof(buf), "ColorCode%d", i);
		m_ColorCodeColors[i] = pScheme->GetColor(buf, s_DefaultColorCodeColors[i]);
	}
}

int CHud::MsgFunc_Logo(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	// update Train data
	m_iLogo = READ_BYTE();

	return 1;
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase(const char *in, char *out)
{
	int len, start, end;

	len = strlen(in);

	// scan backward for '.'
	end = len - 1;
	while (end && in[end] != '.' && in[end] != '/' && in[end] != '\\')
		end--;

	if (in[end] != '.') // no '.', copy to end
		end = len - 1;
	else
		end--; // Found ',', copy to left of '.'

	// Scan backward for '/'
	start = len - 1;
	while (start >= 0 && in[start] != '/' && in[start] != '\\')
		start--;

	if (in[start] != '/' && in[start] != '\\')
		start = 0;
	else
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy(out, &in[start], len);
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame(const char *game)
{
	const char *gamedir;
	char gd[1024];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if (gamedir && gamedir[0])
	{
		COM_FileBase(gamedir, gd);
		if (!_stricmp(gd, game))
			return 1;
	}
	return 0;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV(void)
{
	if (gEngfuncs.pDemoAPI->IsRecording())
	{
		// Write it
		int i = 0;
		unsigned char buf[100];

		// Active
		*(float *)&buf[i] = g_lastFOV;
		i += sizeof(float);

		Demo_WriteBuffer(TYPE_ZOOM, i, buf);
	}

	if (gEngfuncs.pDemoAPI->IsPlayingback())
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

int CHud::MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT("default_fov");

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	if (cl_lw && cl_lw->value)
		return 1;

	g_lastFOV = newfov;

	if (newfov == 0)
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if (m_iFOV == def_fov)
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)def_fov) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return 1;
}

float CHud::GetSensitivity(void)
{
	return m_flMouseSensitivity;
}

BHopCap CHud::GetBHopCapState()
{
	return (BHopCap)clamp(cl_bhopcap.GetInt(), (int)BHopCap::Disabled, (int)BHopCap::Auto);
}

void CHud::CallOnNextFrame(std::function<void()> f)
{
	Assert(f);
	m_NextFrameQueue.push(f);
}

Color CHud::GetHudColor(HudPart hudPart, int value)
{
	if (hudPart == HudPart::Common)
		return m_HudColor;

	if (value >= 90)
		return m_HudColor1;

	if (value >= 50)
		return m_HudColor2;

	if (value > 25 || hudPart == HudPart::Armor)
		return m_HudColor3;

	return Color(255, 0, 0, 255);
}

void CHud::GetHudColor(HudPart hudPart, int value, int &r, int &g, int &b)
{
	Color c = GetHudColor(hudPart, value);
	r = c.r();
	g = c.g();
	b = c.b();
}

void CHud::GetHudAmmoColor(int value, int maxvalue, int &r, int &g, int &b)
{
	Color c;
	if (maxvalue == -1 || maxvalue == 0)
	{
		// Custom weapons will use default colors
		c = m_HudColor;
	}
	else if ((value * 100) / maxvalue > 90)
	{
		c = m_HudColor1;
	}
	else if ((value * 100) / maxvalue > 50)
	{
		c = m_HudColor2;
	}
	else if ((value * 100) / maxvalue > 20)
	{
		c = m_HudColor3;
	}
	else
	{
		r = 255;
		g = 0;
		b = 0;
		return;
	}

	r = c.r();
	g = c.g();
	b = c.b();
}

float CHud::GetHudTransparency()
{
	return clamp(m_pCvarDraw->value, 0.f, 1.f);
}

inline Color CHud::GetClientColor(int idx, Color noTeamColor)
{
	int team = GetPlayerInfo(idx)->Update()->GetTeamNumber();

	if (team == 0)
		return noTeamColor;
	else
		return g_pViewport->GetTeamColor(team);
}

void CHud::GetClientColorAsFloat(int idx, float out[3], Color noTeamColor)
{
	Color c = GetClientColor(idx, noTeamColor);
	out[0] = c[0] / 255.f;
	out[1] = c[1] / 255.f;
	out[2] = c[2] / 255.f;
}

void CHud::UpdateHudColors()
{
	ParseColor(hud_color.GetString(), m_HudColor);
	ParseColor(hud_color1.GetString(), m_HudColor1);
	ParseColor(hud_color2.GetString(), m_HudColor2);
	ParseColor(hud_color3.GetString(), m_HudColor3);
}
