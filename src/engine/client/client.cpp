/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <algorithm>

#include <stdarg.h>

#include <base/math.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/sound.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/protocol.h>

#include <game/version.h>

#include <mastersrv/mastersrv.h>
#include <versionsrv/versionsrv.h>

#include "client.h"

#if defined(CONF_FAMILY_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

#include "SDL.h"
#ifdef main
#undef main
#endif

void CGraph::Init(float Min, float Max)
{
	m_MinRange = m_Min = Min;
	m_MaxRange = m_Max = Max;
	m_Index = 0;
}

void CGraph::Scale()
{
	m_Min = m_MinRange;
	m_Max = m_MaxRange;
	for(int i = 0; i < MAX_VALUES; i++)
	{
		if(m_aValues[i] > m_Max)
			m_Max = m_aValues[i];
		else if(m_aValues[i] < m_Min)
			m_Min = m_aValues[i];
	}
}

void CGraph::Add(float v, float r, float g, float b)
{
	m_Index = (m_Index+1)%MAX_VALUES;
	m_aValues[m_Index] = v;
	m_aColors[m_Index][0] = r;
	m_aColors[m_Index][1] = g;
	m_aColors[m_Index][2] = b;
}

void CGraph::Render(IGraphics *pGraphics, IGraphics::CTextureHandle FontTexture, float x, float y, float w, float h, const char *pDescription)
{
	//m_pGraphics->BlendNormal();

	pGraphics->TextureClear();

	pGraphics->QuadsBegin();
	pGraphics->SetColor(0, 0, 0, 0.75f);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	pGraphics->QuadsDrawTL(&QuadItem, 1);
	pGraphics->QuadsEnd();

	pGraphics->LinesBegin();
	pGraphics->SetColor(0.95f, 0.95f, 0.95f, 1.00f);
	IGraphics::CLineItem LineItem(x, y+h/2, x+w, y+h/2);
	pGraphics->LinesDraw(&LineItem, 1);
	pGraphics->SetColor(0.5f, 0.5f, 0.5f, 0.75f);
	IGraphics::CLineItem aLineItems[2] = {
		IGraphics::CLineItem(x, y+(h*3)/4, x+w, y+(h*3)/4),
		IGraphics::CLineItem(x, y+h/4, x+w, y+h/4)};
	pGraphics->LinesDraw(aLineItems, 2);
	for(int i = 1; i < MAX_VALUES; i++)
	{
		float a0 = (i-1)/(float)MAX_VALUES;
		float a1 = i/(float)MAX_VALUES;
		int i0 = (m_Index+i-1)%MAX_VALUES;
		int i1 = (m_Index+i)%MAX_VALUES;

		float v0 = (m_aValues[i0]-m_Min) / (m_Max-m_Min);
		float v1 = (m_aValues[i1]-m_Min) / (m_Max-m_Min);

		IGraphics::CColorVertex aColorVertices[2] = {
			IGraphics::CColorVertex(0, m_aColors[i0][0], m_aColors[i0][1], m_aColors[i0][2], 0.75f),
			IGraphics::CColorVertex(1, m_aColors[i1][0], m_aColors[i1][1], m_aColors[i1][2], 0.75f)};
		pGraphics->SetColorVertex(aColorVertices, 2);
		IGraphics::CLineItem LineItem(x+a0*w, y+h-v0*h, x+a1*w, y+h-v1*h);
		pGraphics->LinesDraw(&LineItem, 1);

	}
	pGraphics->LinesEnd();

	pGraphics->TextureSet(FontTexture);
	pGraphics->QuadsBegin();
	pGraphics->QuadsText(x+2, y+h-16, 16, pDescription);

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%.2f", m_Max);
	pGraphics->QuadsText(x+w-8*str_length(aBuf)-8, y+2, 16, aBuf);

	str_format(aBuf, sizeof(aBuf), "%.2f", m_Min);
	pGraphics->QuadsText(x+w-8*str_length(aBuf)-8, y+h-16, 16, aBuf);
	pGraphics->QuadsEnd();
}

CClient::CClient()
{
	m_pEditor = 0;
	m_pInput = 0;
	m_pGraphics = 0;
	m_pSound = 0;
	m_pMap = 0;
	m_pConfigManager = 0;
	m_pConfig = 0;
	m_pConsole = 0;

	m_RenderFrameTime = 0.0001f;
	m_RenderFrameTimeLow = 1.0f;
	m_RenderFrameTimeHigh = 0.0f;
	m_RenderFrames = 0;
	m_LastRenderTime = time_get();
	m_LastCpuTime = time_get();
	m_LastAvgCpuFrameTime = 0;

	m_GameTickSpeed = SERVER_TICK_SPEED;

	m_WindowMustRefocus = 0;
	m_SnapCrcErrors = 0;
	m_AutoScreenshotRecycle = false;
	m_AutoStatScreenshotRecycle = false;

	m_AckGameTick = -1;
	m_CurrentRecvTick = 0;
	m_RconAuthed = 0;

	// version-checking
	m_aVersionStr[0] = '0';
	m_aVersionStr[1] = 0;

	m_CurrentInput = 0;

	m_State = IClient::STATE_OFFLINE;
	m_aServerAddressStr[0] = 0;
	m_aServerPassword[0] = 0;
}

// ------ state handling -----
void CClient::SetState(int s)
{
	if(m_State == IClient::STATE_QUITING)
		return;

	if(Config()->m_Debug)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "state change. last=%d current=%d", m_State, s);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
	}
	m_State = s;
}

void CClient::Render()
{
	m_pEditor->UpdateAndRender();
	DebugRender();
}

const char *CClient::LatestVersion() const
{
	return m_aVersionStr;
}

// TODO: OPT: do this alot smarter!
const int *CClient::GetInput(int Tick) const
{
	int Best = -1;
	for(int i = 0; i < 200; i++)
	{
		if(m_aInputs[i].m_Tick <= Tick && (Best == -1 || m_aInputs[Best].m_Tick < m_aInputs[i].m_Tick))
			Best = i;
	}

	if(Best != -1)
		return (const int *)m_aInputs[Best].m_aData;
	return 0;
}

void CClient::DebugRender()
{
	if(!Config()->m_Debug)
		return;

	static NETSTATS Prev, Current;
	static int64 LastSnap = 0;
	static float FrameTimeAvg = 0;
	static IGraphics::CTextureHandle s_Font = Graphics()->LoadTexture("ui/debug_font.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_NORESAMPLE);
	char aBuffer[256];

	//m_pGraphics->BlendNormal();
	Graphics()->TextureSet(s_Font);
	Graphics()->MapScreen(0,0,Graphics()->ScreenWidth(),Graphics()->ScreenHeight());
	Graphics()->QuadsBegin();

	if(time_get()-LastSnap > time_freq())
	{
		LastSnap = time_get();
		Prev = Current;
		net_stats(&Current);
	}

	/*
		eth = 14
		ip = 20
		udp = 8
		total = 42
	*/
	FrameTimeAvg = FrameTimeAvg*0.9f + m_RenderFrameTime*0.1f;
	str_format(aBuffer, sizeof(aBuffer), "gfxmem: %dk fps: %3d",
		Graphics()->MemoryUsage()/1024,
		(int)(1.0f/FrameTimeAvg + 0.5f));
	Graphics()->QuadsText(2, 2, 16, aBuffer);


	{
		int SendPackets = (Current.sent_packets-Prev.sent_packets);
		int SendBytes = (Current.sent_bytes-Prev.sent_bytes);
		int SendTotal = SendBytes + SendPackets*42;
		int RecvPackets = (Current.recv_packets-Prev.recv_packets);
		int RecvBytes = (Current.recv_bytes-Prev.recv_bytes);
		int RecvTotal = RecvBytes + RecvPackets*42;

		if(!SendPackets) SendPackets++;
		if(!RecvPackets) RecvPackets++;
		str_format(aBuffer, sizeof(aBuffer), "send: %3d %5d+%4d=%5d (%3d kbps) avg: %5d\nrecv: %3d %5d+%4d=%5d (%3d kbps) avg: %5d",
			SendPackets, SendBytes, SendPackets*42, SendTotal, (SendTotal*8)/1024, SendBytes/SendPackets,
			RecvPackets, RecvBytes, RecvPackets*42, RecvTotal, (RecvTotal*8)/1024, RecvBytes/RecvPackets);
		Graphics()->QuadsText(2, 14, 16, aBuffer);
	}

	Graphics()->QuadsEnd();

	// render graphs
	if(Config()->m_DbgGraphs)
	{
		float w = Graphics()->ScreenWidth()/4.0f;
		float h = Graphics()->ScreenHeight()/6.0f;
		float sp = Graphics()->ScreenWidth()/100.0f;
		float x = Graphics()->ScreenWidth()-w-sp;

		m_FpsGraph.Scale();
		m_FpsGraph.Render(Graphics(), s_Font, x, sp*5, w, h, "FPS");
		m_InputtimeMarginGraph.Scale();
		m_InputtimeMarginGraph.Render(Graphics(), s_Font, x, sp*5+h+sp, w, h, "Prediction Margin");
		m_GametimeMarginGraph.Scale();
		m_GametimeMarginGraph.Render(Graphics(), s_Font, x, sp*5+h+sp+h+sp, w, h, "Gametime Margin");
	}
}

void CClient::Update()
{
}

void CClient::VersionUpdate()
{
}

void CClient::RegisterInterfaces()
{
}

void CClient::InitInterfaces()
{
	// fetch interfaces
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pEditor = Kernel()->RequestInterface<IEditor>();
	//m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pSound = Kernel()->RequestInterface<IEngineSound>();
	m_pTextRender = Kernel()->RequestInterface<IEngineTextRender>();
	m_pInput = Kernel()->RequestInterface<IEngineInput>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pConfig = m_pConfigManager->Values();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
}

bool CClient::LimitFps()
{
	if(Config()->m_GfxVsync || !Config()->m_GfxLimitFps) return false;

	/**
		If desired frame time is not reached:
			Skip rendering the frame
			Do another game loop

		If we don't have the time to do another game loop:
			Wait until desired frametime

		Returns true if frame should be skipped
	**/

#ifdef CONF_DEBUG
	static double DbgTimeWaited = 0.0;
	static int64 DbgFramesSkippedCount = 0;
	static int64 DbgLastSkippedDbgMsg = time_get();
#endif

	int64 Now = time_get();
	const double LastCpuFrameTime = (Now - m_LastCpuTime) / (double)time_freq();
	m_LastAvgCpuFrameTime = (m_LastAvgCpuFrameTime + LastCpuFrameTime * 4.0) / 5.0;
	m_LastCpuTime = Now;

	bool SkipFrame = true;
	double RenderDeltaTime = (Now - m_LastRenderTime) / (double)time_freq();
	const double DesiredTime = 1.0/Config()->m_GfxMaxFps;

	// we can't skip another frame, so wait instead
	if(SkipFrame && RenderDeltaTime < DesiredTime &&
	   m_LastAvgCpuFrameTime * 1.20 > (DesiredTime - RenderDeltaTime))
	{
#ifdef CONF_DEBUG
		DbgTimeWaited += DesiredTime - RenderDeltaTime;
#endif
		const double Freq = (double)time_freq();
		const int64 LastT = m_LastRenderTime;
		double d = DesiredTime - RenderDeltaTime;
		while(d > 0.00001)
		{
			Now = time_get();
			RenderDeltaTime = (Now - LastT) / Freq;
			d = DesiredTime - RenderDeltaTime;
			cpu_relax();
		}

		SkipFrame = false;
		m_LastCpuTime = Now;
	}

	// RenderDeltaTime exceeds DesiredTime, render
	if(SkipFrame && RenderDeltaTime > DesiredTime)
	{
		SkipFrame = false;
	}

#ifdef CONF_DEBUG
	DbgFramesSkippedCount += SkipFrame? 1:0;

	Now = time_get();
	if(Config()->m_GfxLimitFps &&
	   Config()->m_Debug &&
	   (Now - DbgLastSkippedDbgMsg) / (double)time_freq() > 5.0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "LimitFps: FramesSkippedCount=%lu TimeWaited=%.3f (per sec)",
				   (unsigned long)(DbgFramesSkippedCount/5),
				   DbgTimeWaited/5.0);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
		DbgFramesSkippedCount = 0;
		DbgTimeWaited = 0;
		DbgLastSkippedDbgMsg = Now;
	}
#endif

	return SkipFrame;
}

void CClient::Run()
{
	m_LocalStartTime = time_get();
	m_SnapshotParts = 0;

	// init SDL
	{
		if(SDL_Init(0) < 0)
		{
			dbg_msg("client", "unable to init SDL base: %s", SDL_GetError());
			return;
		}

		atexit(SDL_Quit); // ignore_convention
	}

	// init graphics
	{
		m_pGraphics = CreateEngineGraphicsThreaded();

		bool RegisterFail = false;
		RegisterFail = RegisterFail || !Kernel()->RegisterInterface(static_cast<IEngineGraphics*>(m_pGraphics)); // register graphics as both
		RegisterFail = RegisterFail || !Kernel()->RegisterInterface(static_cast<IGraphics*>(m_pGraphics));

		if(RegisterFail || m_pGraphics->Init() != 0)
		{
			dbg_msg("client", "couldn't init graphics");
			return;
		}
	}

	// init sound, allowed to fail
	m_SoundInitFailed = Sound()->Init() != 0;
	Sound()->SetMaxDistance(1.5f*Graphics()->ScreenWidth()/2.0f);

	// init font rendering
	m_pTextRender->Init();

	// init the input
	Input()->Init();

	//
	m_FpsGraph.Init(0.0f, 120.0f);

	// process pending commands
	m_pConsole->StoreCommands(false);

	m_pEditor->Init();

	while (1)
	{
		//
		VersionUpdate();

		// update input
		if(Input()->Update())
			break;	// SDL_QUIT

		// update sound
		Sound()->Update();

		// release focus
		if(!m_pGraphics->WindowActive())
		{
			if(m_WindowMustRefocus == 0)
				Input()->MouseModeAbsolute();
			m_WindowMustRefocus = 1;
		}
		else if (Config()->m_DbgFocus && Input()->KeyPress(KEY_ESCAPE, true))
		{
			Input()->MouseModeAbsolute();
			m_WindowMustRefocus = 1;
		}

		// refocus
		if(m_WindowMustRefocus && m_pGraphics->WindowActive())
		{
			if(m_WindowMustRefocus < 3)
			{
				Input()->MouseModeAbsolute();
				m_WindowMustRefocus++;
			}

			if(m_WindowMustRefocus >= 3 || Input()->KeyPress(KEY_MOUSE_1, true))
			{
				Input()->MouseModeRelative();
				m_WindowMustRefocus = 0;

				// update screen in case it got moved
				int ActScreen = Graphics()->GetWindowScreen();
				if(ActScreen >= 0 && ActScreen != Config()->m_GfxScreen)
					Config()->m_GfxScreen = ActScreen;
			}
		}

		// panic quit button
		bool IsCtrlPressed = Input()->KeyIsPressed(KEY_LCTRL);
		bool IsLShiftPressed = Input()->KeyIsPressed(KEY_LSHIFT);
		if(IsCtrlPressed && IsLShiftPressed && Input()->KeyPress(KEY_Q, true))
		{
			break;
		}

		if(IsCtrlPressed && IsLShiftPressed && Input()->KeyPress(KEY_D, true))
			Config()->m_Debug ^= 1;

		if(IsCtrlPressed && IsLShiftPressed && Input()->KeyPress(KEY_G, true))
			Config()->m_DbgGraphs ^= 1;

		// render
		{
			Input()->MouseModeRelative();
			
			m_pTextRender->Update();

			Update();

			const bool SkipFrame = LimitFps();

			if(!SkipFrame && (!Config()->m_GfxAsyncRender || m_pGraphics->IsIdle()))
			{
				m_RenderFrames++;

				// update frametime
				int64 Now = time_get();
				m_RenderFrameTime = (Now - m_LastRenderTime) / (float)time_freq();

				if(m_RenderFrameTime < m_RenderFrameTimeLow)
					m_RenderFrameTimeLow = m_RenderFrameTime;
				if(m_RenderFrameTime > m_RenderFrameTimeHigh)
					m_RenderFrameTimeHigh = m_RenderFrameTime;
				m_FpsGraph.Add(1.0f/m_RenderFrameTime, 1,1,1);

				m_LastRenderTime = Now;

				// when we are stress testing only render every 10th frame
				if(!Config()->m_DbgStress || (m_RenderFrames%10) == 0 )
				{
					Render();
					m_pGraphics->Swap();
				}
			}
		}

		AutoScreenshot_Cleanup();

		// check conditions
		if(State() == IClient::STATE_QUITING)
			break;

		// beNice
		if(Config()->m_ClCpuThrottle)
			thread_sleep(Config()->m_ClCpuThrottle);
		else if(Config()->m_DbgStress || !m_pGraphics->WindowActive())
			thread_sleep(5);

		if(Config()->m_DbgHitch)
		{
			thread_sleep(Config()->m_DbgHitch);
			Config()->m_DbgHitch = 0;
		}

		/*
		if(ReportTime < time_get())
		{
			if(0 && Config()->m_Debug)
			{
				dbg_msg("client/report", "fps=%.02f (%.02f %.02f) netstate=%d",
					m_Frames/(float)(ReportInterval/time_freq()),
					1.0f/m_RenderFrameTimeHigh,
					1.0f/m_RenderFrameTimeLow,
					m_NetClient.State());
			}
			m_RenderFrameTimeLow = 1;
			m_RenderFrameTimeHigh = 0;
			m_RenderFrames = 0;
			ReportTime += ReportInterval;
		}*/

		// update local time
		m_LocalTime = (time_get()-m_LocalStartTime)/(float)time_freq();
	}

	m_pGraphics->Shutdown();
	m_pSound->Shutdown();
	m_pTextRender->Shutdown();

	// shutdown SDL
	SDL_Quit();
}

int64 CClient::TickStartTime(int Tick)
{
	return m_LocalStartTime + (time_freq()*Tick)/m_GameTickSpeed;
}

void CClient::Quit()
{
	SetState(IClient::STATE_QUITING);
}

void CClient::Con_Quit(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Quit();
}

void CClient::Con_Minimize(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Graphics()->Minimize();
}

void CClient::AutoScreenshot_Start()
{
	if(Config()->m_ClAutoScreenshot)
	{
		Graphics()->TakeScreenshot("auto/autoscreen");
		m_AutoScreenshotRecycle = true;
	}
}

void CClient::AutoStatScreenshot_Start()
{
	if(Config()->m_ClAutoStatScreenshot)
	{
		Graphics()->TakeScreenshot("auto/stat");
		m_AutoStatScreenshotRecycle = true;
	}
}

void CClient::AutoScreenshot_Cleanup()
{
	if(m_AutoScreenshotRecycle)
	{
		if(Config()->m_ClAutoScreenshotMax)
		{
			// clean up auto taken screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto", "autoscreen", ".png", Config()->m_ClAutoScreenshotMax);
		}
		m_AutoScreenshotRecycle = false;
	}
	if(m_AutoStatScreenshotRecycle)
	{
		if(Config()->m_ClAutoScreenshotMax)
		{
			// clean up auto taken stat screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto", "stat", ".png", Config()->m_ClAutoScreenshotMax);
		}
		m_AutoStatScreenshotRecycle = false;
	}
}

void CClient::Con_Screenshot(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Graphics()->TakeScreenshot(0);
}

void CClient::SwitchWindowScreen(int Index)
{
	// Todo SDL: remove this when fixed (changing screen when in fullscreen is bugged)
	if(Config()->m_GfxFullscreen)
	{
		ToggleFullscreen();
		if(Graphics()->SetWindowScreen(Index))
			Config()->m_GfxScreen = Index;
		ToggleFullscreen();
	}
	else
	{
		if(Graphics()->SetWindowScreen(Index))
			Config()->m_GfxScreen = Index;
	}
}

void CClient::ConchainWindowScreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(pSelf->Config()->m_GfxScreen != pResult->GetInteger(0))
			pSelf->SwitchWindowScreen(pResult->GetInteger(0));
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

bool CClient::ToggleFullscreen()
{
#ifndef CONF_PLATFORM_MACOSX
	if(Graphics()->Fullscreen(Config()->m_GfxFullscreen^1))
		Config()->m_GfxFullscreen ^= 1;
	return true;
#else
	Config()->m_GfxFullscreen ^= 1;
	return false;
#endif
}

void CClient::ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(pSelf->Config()->m_GfxFullscreen != pResult->GetInteger(0))
			pSelf->ToggleFullscreen();
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ToggleWindowBordered()
{
	Config()->m_GfxBorderless ^= 1;
	Graphics()->SetWindowBordered(!Config()->m_GfxBorderless);
}

void CClient::ConchainWindowBordered(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(!pSelf->Config()->m_GfxFullscreen && (pSelf->Config()->m_GfxBorderless != pResult->GetInteger(0)))
			pSelf->ToggleWindowBordered();
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ToggleWindowVSync()
{
	if(Graphics()->SetVSync(Config()->m_GfxVsync^1))
		Config()->m_GfxVsync ^= 1;
}

void CClient::ConchainWindowVSync(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(pSelf->Config()->m_GfxVsync != pResult->GetInteger(0))
			pSelf->ToggleWindowVSync();
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	m_pConsole->Register("quit", "", CFGFLAG_CLIENT|CFGFLAG_STORE, Con_Quit, this, "Quit TeeSolar");
	m_pConsole->Register("exit", "", CFGFLAG_CLIENT|CFGFLAG_STORE, Con_Quit, this, "Quit TeeSolar");
	m_pConsole->Register("minimize", "", CFGFLAG_CLIENT|CFGFLAG_STORE, Con_Minimize, this, "Minimize TeeSolar");
	m_pConsole->Register("screenshot", "", CFGFLAG_CLIENT, Con_Screenshot, this, "Take a screenshot");

	m_pConsole->Chain("gfx_screen", ConchainWindowScreen, this);
	m_pConsole->Chain("gfx_fullscreen", ConchainFullscreen, this);
	m_pConsole->Chain("gfx_borderless", ConchainWindowBordered, this);
	m_pConsole->Chain("gfx_vsync", ConchainWindowVSync, this);
}

static CClient *CreateClient()
{
	CClient *pClient = static_cast<CClient *>(mem_alloc(sizeof(CClient)));
	mem_zero(pClient, sizeof(CClient));
	return new(pClient) CClient;
}

void CClient::DoVersionSpecificActions()
{
}

/*
	Server Time
	Client Mirror Time
	Client Predicted Time

	Snapshot Latency
		Downstream latency

	Prediction Latency
		Upstream latency
*/
#if defined(CONF_PLATFORM_MACOSX)
extern "C" int TWMain(int argc, const char **argv) // ignore_convention
#else
int main(int argc, const char **argv) // ignore_convention
#endif
{
	cmdline_fix(&argc, &argv);
#if defined(CONF_FAMILY_WINDOWS)
	bool QuickEditMode = false;
	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("--quickeditmode", argv[i]) == 0) // ignore_convention
		{
			QuickEditMode = true;
		}
	}
#endif

	bool UseDefaultConfig = false;
	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("-d", argv[i]) == 0 || str_comp("--default", argv[i]) == 0) // ignore_convention
		{
			UseDefaultConfig = true;
			break;
		}
	}

	bool RandInitFailed = secure_random_init() != 0;

	CClient *pClient = CreateClient();
	IKernel *pKernel = IKernel::Create();
	pKernel->RegisterInterface(pClient);
	pClient->RegisterInterfaces();

	// create the components
	int FlagMask = CFGFLAG_CLIENT;
	IEngine *pEngine = CreateEngine("TeeSolar");
	IConsole *pConsole = CreateConsole(FlagMask);
	IStorage *pStorage = CreateStorage("TeeSolar", IStorage::STORAGETYPE_CLIENT, argc, argv); // ignore_convention
	IConfigManager *pConfigManager = CreateConfigManager();
	IEngineSound *pEngineSound = CreateEngineSound();
	IEngineInput *pEngineInput = CreateEngineInput();
	IEngineTextRender *pEngineTextRender = CreateEngineTextRender();
	IEngineMap *pEngineMap = CreateEngineMap();

	if(RandInitFailed)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineSound*>(pEngineSound)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<ISound*>(pEngineSound));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineInput*>(pEngineInput)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IInput*>(pEngineInput));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineTextRender*>(pEngineTextRender)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<ITextRender*>(pEngineTextRender));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMap*>(pEngineMap)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap*>(pEngineMap));

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(CreateEditor());;
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);

		if(RegisterFail)
			return -1;
	}

	pEngine->Init();
	pConfigManager->Init(FlagMask);
	pConsole->Init();

	// register all console commands
	pClient->RegisterCommands();

	// init client's interfaces
	pClient->InitInterfaces();

	if(!UseDefaultConfig)
	{
		// execute config file
		if(!pConsole->ExecuteFile(SETTINGS_FILENAME ".cfg"))
			pConsole->ExecuteFile("settings.cfg"); // fallback to legacy naming scheme

		// execute autoexec file
		pConsole->ExecuteFile("autoexec.cfg");
	}
#if defined(CONF_FAMILY_WINDOWS)
	CConfig *pConfig = pConfigManager->Values();
	bool HideConsole = false;
	#ifdef CONF_RELEASE
	if(!(pConfig->m_ShowConsoleWindow&2))
	#else
	if(!(pConfig->m_ShowConsoleWindow&1))
	#endif
	{
		HideConsole = true;
		FreeConsole();
	}
	else if(!QuickEditMode)
		dbg_console_init();
#endif

	pClient->DoVersionSpecificActions();

	// restore empty config strings to their defaults
	pConfigManager->RestoreStrings();

	pClient->Engine()->InitLogfile();

	// run the client
	dbg_msg("client", "starting...");
	pClient->Run();

	// wait for background jobs to finish
	pEngine->ShutdownJobs();

	// write down the config and quit
	pConfigManager->Save();

#if defined(CONF_FAMILY_WINDOWS)
	if(!HideConsole && !QuickEditMode)
		dbg_console_cleanup();
#endif
	// free components
	pClient->~CClient();
	mem_free(pClient);
	delete pKernel;
	delete pEngine;
	delete pConsole;
	delete pStorage;
	delete pConfigManager;
	delete pEngineSound;
	delete pEngineInput;
	delete pEngineTextRender;
	delete pEngineMap;

	secure_random_uninit();
	cmdline_free(argc, argv);
	return 0;
}
