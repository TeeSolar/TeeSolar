/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_H
#define ENGINE_CLIENT_H
#include "kernel.h"

#include "graphics.h"

class IClient : public IInterface
{
	MACRO_INTERFACE("client", 0)
protected:
	// quick access to state of the client
	int m_State;

	float m_LocalTime;
	float m_RenderFrameTime;

	int m_GameTickSpeed;
public:

	/* Constants: Client States
		STATE_OFFLINE - The client is offline.
		STATE_CONNECTING - The client is trying to connect to a server.
		STATE_LOADING - The client has connected to a server and is loading resources.
		STATE_ONLINE - The client is connected to a server and running the game.
		STATE_DEMOPLAYBACK - The client is playing a demo
		STATE_QUITING - The client is quitting.
	*/

	enum
	{
		STATE_OFFLINE=0,
		STATE_CONNECTING,
		STATE_LOADING,
		STATE_ONLINE,
		STATE_DEMOPLAYBACK,
		STATE_QUITING,
	};

	//
	inline int State() const { return m_State; }

	// other time access
	inline float RenderFrameTime() const { return m_RenderFrameTime; }
	inline float LocalTime() const { return m_LocalTime; }

	// actions
	virtual void Quit() = 0;
	virtual void AutoStatScreenshot_Start() = 0;
	virtual void AutoScreenshot_Start() = 0;

	// gfx
	virtual void SwitchWindowScreen(int Index) = 0;
	virtual bool ToggleFullscreen() = 0;
	virtual void ToggleWindowBordered() = 0;
	virtual void ToggleWindowVSync() = 0;

	// input
	virtual const int *GetInput(int Tick) const = 0;

	//
	virtual const char *LatestVersion() const = 0;
};

#endif
