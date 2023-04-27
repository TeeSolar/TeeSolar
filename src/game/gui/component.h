/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_GUI_COMPONENT_H
#define GAME_GUI_COMPONENT_H

#include <engine/input.h>
#include <game/editor/editor.h>

class CComponent
{
protected:
	friend class CEditor;

	CEditor *m_pEditor;

	// perhaps propagte pointers for these as well
	class IKernel *Kernel() const { return m_pEditor->Kernel(); }
	class IGraphics *Graphics() const { return m_pEditor->Graphics(); }
	class ITextRender *TextRender() const { return m_pEditor->TextRender(); }
	class IClient *Client() const { return m_pEditor->Client(); }
	class IInput *Input() const { return m_pEditor->Input(); }
	class IStorage *Storage() const { return m_pEditor->Storage(); }
	class CUI *UI() const { return m_pEditor->UI(); }
	class CRenderTools *RenderTools() const { return m_pEditor->RenderTools(); }
	class CConfig *Config() const { return m_pEditor->Config(); }
	class IConsole *Console() const { return m_pEditor->Console(); }
public:
	virtual ~CComponent() {}

	virtual void OnStateChange(int NewState, int OldState) {}
	virtual void OnConsoleInit() {}
	virtual int GetInitAmount() const { return 0; } // Amount of progress reported by this component during OnInit
	virtual void OnInit() {}
	virtual void OnShutdown() {}
	virtual void OnReset() {}
	virtual void OnRender() {}
	virtual void OnRelease() {}
	virtual void OnMapLoad() {}
	virtual void OnMessage(int Msg, void *pRawMsg) {}
	virtual bool OnCursorMove(float x, float y, int CursorType) { return false; }
	virtual bool OnInput(IInput::CEvent e) { return false; }
};

#endif
