/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_CLIENT_H
#define ENGINE_CLIENT_CLIENT_H

#include <base/hash.h>

class CGraph
{
	enum
	{
		MAX_VALUES=128,
	};

	float m_Min, m_Max;
	float m_MinRange, m_MaxRange;
	float m_aValues[MAX_VALUES];
	float m_aColors[MAX_VALUES][3];
	int m_Index;

public:
	void Init(float Min, float Max);

	void Scale();
	void Add(float v, float r, float g, float b);
	void Render(IGraphics *pGraphics, IGraphics::CTextureHandle FontTexture, float x, float y, float w, float h, const char *pDescription);
};


class CClient : public IClient
{
	// needed interfaces
	IEngine *m_pEngine;
	IEditor *m_pEditor;
	IEngineInput *m_pInput;
	IEngineGraphics *m_pGraphics;
	IEngineSound *m_pSound;
	IEngineTextRender *m_pTextRender;
	IEngineMap *m_pMap;
	IConfigManager *m_pConfigManager;
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IStorage *m_pStorage;

	enum
	{
		NUM_SNAPSHOT_TYPES=2,
		PREDICTION_MARGIN=1000/50/2, // magic network prediction value
	};

	char m_aServerAddressStr[256];
	char m_aServerPassword[128];

	unsigned m_SnapshotParts;
	int64 m_LocalStartTime;

	int64 m_LastRenderTime;
	int64 m_LastCpuTime;
	float m_LastAvgCpuFrameTime;
	float m_RenderFrameTimeLow;
	float m_RenderFrameTimeHigh;
	int m_RenderFrames;

	NETADDR m_ServerAddress;
	int m_WindowMustRefocus;
	int m_SnapCrcErrors;
	bool m_AutoScreenshotRecycle;
	bool m_AutoStatScreenshotRecycle;
	bool m_SoundInitFailed;
	bool m_RecordGameMessage;

	int m_AckGameTick;
	int m_CurrentRecvTick;
	int m_RconAuthed;
	int m_UseTempRconCommands;

	// version-checking
	char m_aVersionStr[10];

	// input
	struct // TODO: handle input better
	{
		int m_aData[MAX_INPUT_SIZE]; // the input data
		int m_Tick; // the tick that the input is for
		int64 m_PredictedTime; // prediction latency when we sent this input
		int64 m_Time;
	} m_aInputs[200];

	int m_CurrentInput;

	// graphs
	CGraph m_InputtimeMarginGraph;
	CGraph m_GametimeMarginGraph;
	CGraph m_FpsGraph;

	int64 TickStartTime(int Tick);

public:
	IEngine *Engine() { return m_pEngine; }
	IEngineGraphics *Graphics() { return m_pGraphics; }
	IEngineInput *Input() { return m_pInput; }
	IEngineSound *Sound() { return m_pSound; }
	IEditor *Editor() { return m_pEditor; }
	IConfigManager *ConfigManager() { return m_pConfigManager; }
	CConfig *Config() { return m_pConfig; }
	IConsole *Console() { return m_pConsole; }
	IStorage *Storage() { return m_pStorage; }

	CClient();

	// TODO: OPT: do this alot smarter!
	virtual const int *GetInput(int Tick) const;

	const char *LatestVersion() const;
	void VersionUpdate();
	
	// ------ state handling -----
	void SetState(int s);

	void Render();
	void DebugRender();

	virtual void Quit();

	void Update();

	void RegisterInterfaces();
	void InitInterfaces();

	bool LimitFps();
	void Run();

	void DoVersionSpecificActions();

	static void Con_Quit(IConsole::IResult *pResult, void *pUserData);
	static void Con_Minimize(IConsole::IResult *pResult, void *pUserData);
	static void Con_Screenshot(IConsole::IResult *pResult, void *pUserData);
	static void ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowBordered(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowScreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowVSync(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void RegisterCommands();

	void AutoScreenshot_Start();
	void AutoStatScreenshot_Start();
	void AutoScreenshot_Cleanup();

	// gfx
	void SwitchWindowScreen(int Index);
	bool ToggleFullscreen();
	void ToggleWindowBordered();
	void ToggleWindowVSync();
};
#endif
