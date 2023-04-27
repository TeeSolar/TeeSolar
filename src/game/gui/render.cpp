/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <math.h>
#include <algorithm>

#include <base/math.h>

#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/textrender.h>
#include <generated/client_data.h>
#include <game/layers.h>
#include "render.h"

static float gs_SpriteWScale;
static float gs_SpriteHScale;

void CRenderTools::Init(CConfig *pConfig, IGraphics *pGraphics)
{
	m_pConfig = pConfig;
	m_pGraphics = pGraphics;
}

void CRenderTools::SelectSprite(const CDataSprite *pSpr, int Flags, int sx, int sy)
{
	int x = pSpr->m_X+sx;
	int y = pSpr->m_Y+sy;
	int w = pSpr->m_W;
	int h = pSpr->m_H;
	int cx = pSpr->m_pSet->m_Gridx;
	int cy = pSpr->m_pSet->m_Gridy;

	float f = sqrtf(h*h + w*w);
	gs_SpriteWScale = w/f;
	gs_SpriteHScale = h/f;

	float x1 = x/(float)cx + 0.5f/(float)(cx*32);
	float x2 = (x+w)/(float)cx - 0.5f/(float)(cx*32);
	float y1 = y/(float)cy + 0.5f/(float)(cy*32);
	float y2 = (y+h)/(float)cy - 0.5f/(float)(cy*32);

	if(Flags&SPRITE_FLAG_FLIP_Y)
		std::swap(y1, y2);

	if(Flags&SPRITE_FLAG_FLIP_X)
		std::swap(x1, x2);

	Graphics()->QuadsSetSubset(x1, y1, x2, y2);
}

void CRenderTools::SelectSprite(int Id, int Flags, int sx, int sy)
{
	if(Id < 0 || Id >= g_pData->m_NumSprites)
		return;
	SelectSprite(&g_pData->m_aSprites[Id], Flags, sx, sy);
}

void CRenderTools::DrawSprite(float x, float y, float Size)
{
	IGraphics::CQuadItem QuadItem(x, y, Size*gs_SpriteWScale, Size*gs_SpriteHScale);
	Graphics()->QuadsDraw(&QuadItem, 1);
}

void CRenderTools::RenderCursor(float CenterX, float CenterY, float Size)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->WrapClamp();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(CenterX, CenterY, Size, Size);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

static void CalcScreenParams(float Amount, float WMax, float HMax, float Aspect, float *w, float *h)
{
	float f = sqrtf(Amount) / sqrtf(Aspect);
	*w = f*Aspect;
	*h = f;

	// limit the view
	if(*w > WMax)
	{
		*w = WMax;
		*h = *w/Aspect;
	}

	if(*h > HMax)
	{
		*h = HMax;
		*w = *h*Aspect;
	}
}

void CRenderTools::MapScreenToWorld(float CenterX, float CenterY, float ParallaxX, float ParallaxY,
	float OffsetX, float OffsetY, float Aspect, float Zoom, float aPoints[4])
{
	float Width, Height;
	CalcScreenParams(1150*1000, 1500, 1050, Aspect, &Width, &Height);
	CenterX *= ParallaxX;
	CenterY *= ParallaxY;
	Width *= Zoom;
	Height *= Zoom;
	aPoints[0] = OffsetX+CenterX-Width/2;
	aPoints[1] = OffsetY+CenterY-Height/2;
	aPoints[2] = aPoints[0]+Width;
	aPoints[3] = aPoints[1]+Height;
}

void CRenderTools::MapScreenToGroup(float CenterX, float CenterY, const CMapItemGroup *pGroup, float Zoom)
{
	float aPoints[4];
	MapScreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX/100.0f, pGroup->m_ParallaxY/100.0f,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), Zoom, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}
