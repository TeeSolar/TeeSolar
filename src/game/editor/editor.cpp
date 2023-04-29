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
#include <base/color.h>
#include <base/system.h>

#include <engine/shared/datafile.h>
#include <engine/shared/config.h>
#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/gui/ui_scrollregion.h>
#include <game/gui/ui_listbox.h>
#include <game/gui/ui_rect.h>

#include "editor.h"

const void* CEditor::ms_pUiGotContext;

enum
{
	BUTTON_CONTEXT=1,
};

CEditor::CEditor()
{
    m_pInput = 0;
    m_pClient = 0;
    m_pGraphics = 0;
    m_pTextRender = 0;

	m_pTooltip = 0;

	ms_pUiGotContext = 0;

	m_PopupEventActivated = false;
	m_PopupEventWasActivated = false;
}

CEditor::~CEditor()
{
}

void CEditor::Init()
{
    m_pInput = Kernel()->RequestInterface<IInput>();
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGraphics = Kernel()->RequestInterface<IGraphics>();
	m_pTextRender = Kernel()->RequestInterface<ITextRender>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_UI.Init(Kernel());
	m_RenderTools.Init(m_pConfig, m_pGraphics);

	m_CursorTexture = Graphics()->LoadTexture("editor/cursor.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	m_BackgroundTexture = Graphics()->LoadTexture("editor/background.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);

	m_pTextRender->LoadFonts(Storage(), Console());
	m_pTextRender->SetFontLanguageVariant(Config()->m_ClLanguagefile);

    m_ShowCursor = true;

	return;
}

vec4 CEditor::GetButtonColor(const void *pID, int Checked)
{
	if(Checked < 0)
		return vec4(0,0,0,0.5f);

	if(Checked > 0)
	{
		if(UI()->HotItem() == pID)
			return vec4(1,0,0,0.75f);
		return vec4(1,0,0,0.5f);
	}

	if(UI()->HotItem() == pID)
		return vec4(1,1,1,0.75f);
	return vec4(1,1,1,0.5f);
}

int CEditor::DoButton_Editor_Common(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	if(UI()->MouseInside(pRect))
	{
		if(Flags&BUTTON_CONTEXT)
			ms_pUiGotContext = pID;
		if(m_pTooltip)
			m_pTooltip = pToolTip;
	}

	if(UI()->HotItem() == pID && pToolTip)
		m_pTooltip = (const char *)pToolTip;

	if(UI()->DoButtonLogic(pID, pRect, 0))
		return 1;
	if(UI()->DoButtonLogic(pID, pRect, 1))
		return 2;
	return 0;
}

int CEditor::DoButton_Editor(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Flags, const char *pToolTip)
{
	pRect->Draw(GetButtonColor(pID, Checked), 3.0f);
	UI()->DoLabel(pRect, pText, 10.0f, TEXTALIGN_MC);
	return DoButton_Editor_Common(pID, pText, Checked, pRect, Flags, pToolTip);
}

void CEditor::UpdateAndRender()
{
    static float s_MouseX = 0.0f;
	static float s_MouseY = 0.0f;

	CUIElementBase::Init(UI()); // update static pointer because game and editor use separate UI
	UI()->StartCheck();

	for(int i = 0; i < Input()->NumEvents(); i++)
		UI()->OnInput(Input()->GetEvent(i));
	ms_pUiGotContext = 0;

	// handle cursor movement
	{
		float rx = 0.0f, ry = 0.0f;
		int CursorType = Input()->CursorRelative(&rx, &ry);
		UI()->ConvertCursorMove(&rx, &ry, CursorType);

		m_MouseDeltaX = rx;
		m_MouseDeltaY = ry;

		s_MouseX += rx;
		s_MouseY += ry;

		s_MouseX = clamp(s_MouseX, 0.0f, (float)Graphics()->ScreenWidth());
		s_MouseY = clamp(s_MouseY, 0.0f, (float)Graphics()->ScreenHeight());

		// update the ui
		float mx = (s_MouseX/(float)Graphics()->ScreenWidth())*UI()->Screen()->w;
		float my = (s_MouseY/(float)Graphics()->ScreenHeight())*UI()->Screen()->h;
		float Mdx = (m_MouseDeltaX/(float)Graphics()->ScreenWidth())*UI()->Screen()->w;
		float Mdy = (m_MouseDeltaY/(float)Graphics()->ScreenHeight())*UI()->Screen()->h;
		float Mwx = 0;
		float Mwy = 0;

		UI()->Update(mx, my, Mwx, Mwy);
	}

	if(Input()->KeyPress(KEY_F10))
	    m_ShowCursor = false;

	Render();

	if(Input()->KeyPress(KEY_F10))
	{
		Graphics()->TakeScreenshot(0);
		m_ShowCursor = true;
	}

	UI()->FinishCheck();
	UI()->ClearHotkeys();
	Input()->Clear();

	CLineInput::RenderCandidates();
}

void CEditor::Render()
{
	// basic start
	Graphics()->Clear(1.0f, 0.0f, 1.0f);
	CUIRect View = *UI()->Screen();
	Graphics()->MapScreen(View.x, View.y, View.w, View.h);

	float Width = View.w;
	float Height = View.h;

	// render background
	RenderBackground(View, m_BackgroundTexture, 128.0f, 0.25f);

	if(Input()->KeyPress(KEY_ESCAPE))
	{
		m_PopupEventType = POPEVENT_EXIT;
		m_PopupEventActivated = true;
	}

	UI()->MapScreen();

	if(m_PopupEventActivated)
	{
		UI()->DoPopupMenu(Width/2.0f-200.0f, Height/2.0f-100.0f, 400.0f, 200.0f, this, PopupEvent);
		m_PopupEventActivated = false;
		m_PopupEventWasActivated = true;
	}

	UI()->RenderPopupMenus();
	
    if(m_ShowCursor)
	{
		float mx = UI()->MouseX();
		float my = UI()->MouseY();

		{
			// render butt ugly mouse cursor
			Graphics()->TextureSet(m_CursorTexture);
			Graphics()->WrapClamp();
			Graphics()->QuadsBegin();
			if(ms_pUiGotContext == UI()->HotItem())
				Graphics()->SetColor(1,0,0,1);
			else Graphics()->SetColor(1,1,1,1);
			IGraphics::CQuadItem QuadItem(mx,my, 16.0f, 16.0f);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
	}
}

void CEditor::RenderBackground(CUIRect View, IGraphics::CTextureHandle Texture, float Size, float Brightness)
{
	Graphics()->TextureSet(Texture);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
	Graphics()->QuadsSetSubset(0,0, View.w/Size, View.h/Size);
	IGraphics::CQuadItem QuadItem(View.x, View.y, View.w, View.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

IEditor *CreateEditor() { return new CEditor; }