/*
Copyright 2023 RemakePower

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef GAME_EDITOR_EDITOR_H
#define GAME_EDITOR_EDITOR_H

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/editor.h>
#include <engine/graphics.h>

#include <game/gui/render.h>
#include <game/gui/ui.h>

class CEditor : public IEditor
{
	class IInput *m_pInput;
	class IClient *m_pClient;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IGraphics *m_pGraphics;
	class ITextRender *m_pTextRender;
	class IStorage *m_pStorage;
	CRenderTools m_RenderTools;
	CUI m_UI;
public:
	class IInput *Input() { return m_pInput; }
	class IClient *Client() { return m_pClient; }
	class CConfig *Config() { return m_pConfig; }
	class IConsole *Console() { return m_pConsole; }
	class IGraphics *Graphics() { return m_pGraphics; }
	class ITextRender *TextRender() { return m_pTextRender; }
	class IStorage *Storage() { return m_pStorage; }
	CUI *UI() { return &m_UI; }
	CRenderTools *RenderTools() { return &m_RenderTools; }

    CEditor();
    ~CEditor();

    void Init() override;
    void UpdateAndRender() override;
	bool HasUnsavedData() const override { return false; }

    // gui
	void Render();
	void RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness);


private:
	IGraphics::CTextureHandle m_CursorTexture;
	IGraphics::CTextureHandle m_BackgroundTexture;
    bool m_ShowCursor;

	const char *m_pTooltip;

    // mouse
	float m_MouseDeltaX;
	float m_MouseDeltaY;
	float m_MouseDeltaWx;
	float m_MouseDeltaWy;

	static const void *ms_pUiGotContext;
	
	vec4 GetButtonColor(const void *pID, int Checked);
	int DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);
	int DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip);

	//popup
	enum
	{
		POPEVENT_EXIT=0,
	};

	int m_PopupEventType;
	int m_PopupEventActivated;
	int m_PopupEventWasActivated;

	static bool PopupEvent(void *pContext, CUIRect View);
};

#endif