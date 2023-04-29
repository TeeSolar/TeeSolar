/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/color.h>
#include <base/tl/array.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/storage.h>
#include <engine/client.h>

#include "editor.h"

bool CEditor::PopupEvent(void *pContext, CUIRect View)
{
	CEditor *pEditor = (CEditor *)pContext;
	CUIRect Label, ButtonBar;

	// title
	View.HSplitTop(10.0f, 0, &View);
	View.HSplitTop(30.0f, &Label, &View);
	if(pEditor->m_PopupEventType == POPEVENT_EXIT)
		pEditor->UI()->DoLabel(&Label, "Exit the editor", 20.0f, TEXTALIGN_CENTER);

	View.HSplitBottom(10.0f, &View, 0);
	View.HSplitBottom(20.0f, &View, &ButtonBar);

	// notification text
	View.HSplitTop(30.0f, 0, &View);
	View.VMargin(40.0f, &View);
	View.HSplitTop(20.0f, &Label, &View);
	if(pEditor->m_PopupEventType == POPEVENT_EXIT)
		pEditor->UI()->DoLabel(&Label, "Are you sure to quit?", 10.0f, TEXTALIGN_LEFT, Label.w-10.0f);
	
	// button bar
	ButtonBar.VSplitLeft(30.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &Label, &ButtonBar);
	static int s_OkButton = 0;
	if(pEditor->DoButton_Editor(&s_OkButton, "OK", 0, &Label, 0, 0))
	{
		if(pEditor->m_PopupEventType == POPEVENT_EXIT)
			pEditor->Client()->Quit();
		pEditor->m_PopupEventWasActivated = false;
		return true;
	}
	ButtonBar.VSplitRight(30.0f, &ButtonBar, 0);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &Label);
	static int s_AbortButton = 0;
	if(pEditor->DoButton_Editor(&s_AbortButton, "Abort", 0, &Label, 0, 0))
	{
		pEditor->m_PopupEventWasActivated = false;
		return true;
	}

	return false;
}
