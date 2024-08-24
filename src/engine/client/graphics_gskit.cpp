/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/detect.h>
#include <base/math.h>

#include <gsKit.h>
#include <dmaKit.h>
#include <graph.h>

#include <base/system.h>
#include <engine/external/pnglite/pnglite.h>

#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/keys.h>
#include <engine/console.h>

#include <math.h> // cosf, sinf
#include <malloc.h>

#include "graphics_gskit.h"

#define GL_MAX_TEXTURE_SIZE 128
#define A_COLOR_SOURCE 0
#define A_COLOR_DEST 1
#define A_COLOR_NULL 2
#define A_ALPHA_SOURCE 0
#define A_ALPHA_DEST 1
#define A_ALPHA_FIX 2

static GSGLOBAL *gsGlobal;

void CGraphics_PS2_gsKit::Flush()
{
	if(m_NumVertices == 0)
		return;

	if(m_RenderEnable)
	{
		int vertexPrim = (g_Config.m_GfxQuadAsTriangle) ? 3 : 4;

		for (int i=0; i<m_NumVertices; i+=vertexPrim)
		{
			// adjust vertices based on ortho projection
			for (int j=0; j<vertexPrim; j++)
			{
				m_aVertices[i+j].m_Pos.x -= m_ScreenX0;
				m_aVertices[i+j].m_Pos.y -= m_ScreenY0;

				m_aVertices[i+j].m_Pos.x /= (m_ScreenX1-m_ScreenX0) / gsGlobal->Width;
				m_aVertices[i+j].m_Pos.y /= (m_ScreenY1-m_ScreenY0) / gsGlobal->Height;
			}

			if (m_CurrTexture == -1)
			{
				if (g_Config.m_GfxQuadAsTriangle)
					gsKit_prim_triangle_gouraud_3d(gsGlobal,
						m_aVertices[i+0].m_Pos.x, m_aVertices[i+0].m_Pos.y, 0,
						m_aVertices[i+1].m_Pos.x, m_aVertices[i+1].m_Pos.y, 0,
						m_aVertices[i+2].m_Pos.x, m_aVertices[i+2].m_Pos.y, 0,
						GS_SETREG_RGBAQ(m_aVertices[i+0].m_Color.r*255, m_aVertices[i+0].m_Color.g*255, m_aVertices[i+0].m_Color.b*255, 128-m_aVertices[i+0].m_Color.a*128, 0),
						GS_SETREG_RGBAQ(m_aVertices[i+1].m_Color.r*255, m_aVertices[i+1].m_Color.g*255, m_aVertices[i+1].m_Color.b*255, 128-m_aVertices[i+1].m_Color.a*128, 0),
						GS_SETREG_RGBAQ(m_aVertices[i+2].m_Color.r*255, m_aVertices[i+2].m_Color.g*255, m_aVertices[i+2].m_Color.b*255, 128-m_aVertices[i+2].m_Color.a*128, 0));
				else
					gsKit_prim_quad_gouraud_3d(gsGlobal,
						m_aVertices[i+0].m_Pos.x, m_aVertices[i+0].m_Pos.y, 0,
						m_aVertices[i+1].m_Pos.x, m_aVertices[i+1].m_Pos.y, 0,
						m_aVertices[i+2].m_Pos.x, m_aVertices[i+2].m_Pos.y, 0,
						m_aVertices[i+3].m_Pos.x, m_aVertices[i+3].m_Pos.y, 0,
						GS_SETREG_RGBAQ(m_aVertices[i+0].m_Color.r*255, m_aVertices[i+0].m_Color.g*255, m_aVertices[i+0].m_Color.b*255, 128-m_aVertices[i+0].m_Color.a*128, 0),
						GS_SETREG_RGBAQ(m_aVertices[i+1].m_Color.r*255, m_aVertices[i+1].m_Color.g*255, m_aVertices[i+1].m_Color.b*255, 128-m_aVertices[i+1].m_Color.a*128, 0),
						GS_SETREG_RGBAQ(m_aVertices[i+2].m_Color.r*255, m_aVertices[i+2].m_Color.g*255, m_aVertices[i+2].m_Color.b*255, 128-m_aVertices[i+2].m_Color.a*128, 0),
						GS_SETREG_RGBAQ(m_aVertices[i+3].m_Color.r*255, m_aVertices[i+3].m_Color.g*255, m_aVertices[i+3].m_Color.b*255, 128-m_aVertices[i+3].m_Color.a*128, 0));
			}
			else
			{
				GSTEXTURE* gsTex = (GSTEXTURE*)m_aTextures[m_CurrTexture].m_Tex;

				if (g_Config.m_GfxQuadAsTriangle)
					gsKit_prim_triangle_goraud_texture_3d(gsGlobal, gsTex,
						m_aVertices[i+0].m_Pos.x, m_aVertices[i+0].m_Pos.y, 0,    m_aVertices[i+0].m_Tex.u * gsTex->Width, m_aVertices[i+0].m_Tex.v * gsTex->Height,
						m_aVertices[i+1].m_Pos.x, m_aVertices[i+1].m_Pos.y, 0,    m_aVertices[i+1].m_Tex.u * gsTex->Width, m_aVertices[i+1].m_Tex.v * gsTex->Height,
						m_aVertices[i+2].m_Pos.x, m_aVertices[i+2].m_Pos.y, 0,    m_aVertices[i+2].m_Tex.u * gsTex->Width, m_aVertices[i+2].m_Tex.v * gsTex->Height,
						GS_SETREG_RGBAQ(m_aVertices[i+0].m_Color.r*128, m_aVertices[i+0].m_Color.g*128, m_aVertices[i+0].m_Color.b*128, (m_aVertices[i+0].m_Color.a*128), 0),
						GS_SETREG_RGBAQ(m_aVertices[i+1].m_Color.r*128, m_aVertices[i+1].m_Color.g*128, m_aVertices[i+1].m_Color.b*128, (m_aVertices[i+1].m_Color.a*128), 0),
						GS_SETREG_RGBAQ(m_aVertices[i+2].m_Color.r*128, m_aVertices[i+2].m_Color.g*128, m_aVertices[i+2].m_Color.b*128, (m_aVertices[i+2].m_Color.a*128), 0));
				else
					gsKit_prim_quad_goraud_texture_3d(gsGlobal, gsTex,
						m_aVertices[i+0].m_Pos.x, m_aVertices[i+0].m_Pos.y, 0,    m_aVertices[i+0].m_Tex.u * gsTex->Width, m_aVertices[i+0].m_Tex.v * gsTex->Height,
						m_aVertices[i+1].m_Pos.x, m_aVertices[i+1].m_Pos.y, 0,    m_aVertices[i+1].m_Tex.u * gsTex->Width, m_aVertices[i+1].m_Tex.v * gsTex->Height,
						m_aVertices[i+2].m_Pos.x, m_aVertices[i+2].m_Pos.y, 0,    m_aVertices[i+2].m_Tex.u * gsTex->Width, m_aVertices[i+2].m_Tex.v * gsTex->Height,
						m_aVertices[i+3].m_Pos.x, m_aVertices[i+3].m_Pos.y, 0,    m_aVertices[i+3].m_Tex.u * gsTex->Width, m_aVertices[i+3].m_Tex.v * gsTex->Height,
						GS_SETREG_RGBAQ(m_aVertices[i+0].m_Color.r*128, m_aVertices[i+0].m_Color.g*128, m_aVertices[i+0].m_Color.b*128, (m_aVertices[i+0].m_Color.a*128), 0),
						GS_SETREG_RGBAQ(m_aVertices[i+1].m_Color.r*128, m_aVertices[i+1].m_Color.g*128, m_aVertices[i+1].m_Color.b*128, (m_aVertices[i+1].m_Color.a*128), 0),
						GS_SETREG_RGBAQ(m_aVertices[i+2].m_Color.r*128, m_aVertices[i+2].m_Color.g*128, m_aVertices[i+2].m_Color.b*128, (m_aVertices[i+2].m_Color.a*128), 0),
						GS_SETREG_RGBAQ(m_aVertices[i+3].m_Color.r*128, m_aVertices[i+3].m_Color.g*128, m_aVertices[i+3].m_Color.b*128, (m_aVertices[i+3].m_Color.a*128), 0));
			}
		}
	}

	// Reset pointer
	m_NumVertices = 0;
}

void CGraphics_PS2_gsKit::AddVertices(int Count)
{
	m_NumVertices += Count;
	if((m_NumVertices + Count) >= MAX_VERTICES)
		Flush();
}

void CGraphics_PS2_gsKit::Rotate(const CPoint &rCenter, CVertex *pPoints, int NumPoints)
{
	float c = cosf(m_Rotation);
	float s = sinf(m_Rotation);
	float x, y;
	int i;

	for(i = 0; i < NumPoints; i++)
	{
		x = pPoints[i].m_Pos.x - rCenter.x;
		y = pPoints[i].m_Pos.y - rCenter.y;
		pPoints[i].m_Pos.x = x * c - y * s + rCenter.x;
		pPoints[i].m_Pos.y = x * s + y * c + rCenter.y;
	}
}

unsigned char CGraphics_PS2_gsKit::Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp)
{
	int Value = 0;
	for(int x = 0; x < ScaleW; x++)
		for(int y = 0; y < ScaleH; y++)
			Value += pData[((v+y)*w+(u+x))*Bpp+Offset];
	return Value/(ScaleW*ScaleH);
}

unsigned char *CGraphics_PS2_gsKit::Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData)
{
	unsigned char *pTmpData;
	int ScaleW = Width/NewWidth;
	int ScaleH = Height/NewHeight;

	int Bpp = 3;
	if(Format == CImageInfo::FORMAT_RGBA)
		Bpp = 4;

	pTmpData = (unsigned char *)mem_alloc(NewWidth*NewHeight*Bpp, 1);

	int c = 0;
	for(int y = 0; y < NewHeight; y++)
		for(int x = 0; x < NewWidth; x++, c++)
		{
			pTmpData[c*Bpp] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 0, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+1] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 1, ScaleW, ScaleH, Bpp);
			pTmpData[c*Bpp+2] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 2, ScaleW, ScaleH, Bpp);
			if(Bpp == 4)
				pTmpData[c*Bpp+3] = Sample(Width, Height, pData, x*ScaleW, y*ScaleH, 3, ScaleW, ScaleH, Bpp);
		}

	return pTmpData;
}

CGraphics_PS2_gsKit::CGraphics_PS2_gsKit()
{
	m_NumVertices = 0;

	m_ScreenX0 = 0;
	m_ScreenY0 = 0;
	m_ScreenX1 = 0;
	m_ScreenY1 = 0;

	m_ScreenWidth = -1;
	m_ScreenHeight = -1;

	m_Rotation = 0;
	m_Drawing = 0;
	m_InvalidTexture = 0;

	m_TextureMemoryUsage = 0;

	m_RenderEnable = true;
	m_DoScreenshot = false;
}

void CGraphics_PS2_gsKit::ClipEnable(int x, int y, int w, int h)
{
	gsKit_set_scissor(gsGlobal, GS_SETREG_SCISSOR(x, x+w, y, y+h));
}

void CGraphics_PS2_gsKit::ClipDisable()
{
	gsKit_set_scissor(gsGlobal, GS_SCISSOR_RESET);
}

void CGraphics_PS2_gsKit::BlendNone()
{
	gsKit_set_test(gsGlobal, GS_ATEST_OFF);
}

void CGraphics_PS2_gsKit::BlendNormal()
{
	gsKit_set_test(gsGlobal, GS_ATEST_ON);
	//gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(A_COLOR_SOURCE, A_COLOR_DEST, A_ALPHA_SOURCE, A_COLOR_DEST, 0), 0);
}

void CGraphics_PS2_gsKit::BlendAdditive()
{
	gsKit_set_test(gsGlobal, GS_ATEST_ON);
	//gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(A_COLOR_SOURCE, A_COLOR_NULL, A_ALPHA_FIX, A_COLOR_DEST, 0x80), 0);
}

void CGraphics_PS2_gsKit::WrapNormal()
{
	gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);
}

void CGraphics_PS2_gsKit::WrapClamp()
{
	gsKit_set_clamp(gsGlobal, GS_CMODE_CLAMP);
}

int CGraphics_PS2_gsKit::MemoryUsage() const
{
	return m_TextureMemoryUsage;
}

void CGraphics_PS2_gsKit::MapScreen(float TopLeftX, float TopLeftY, float BottomRightX, float BottomRightY)
{
	m_ScreenX0 = TopLeftX;
	m_ScreenY0 = TopLeftY;
	m_ScreenX1 = BottomRightX;
	m_ScreenY1 = BottomRightY;

	/*
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(m_ScreenX0, m_ScreenX1, m_ScreenY1, m_ScreenY0, -10.0f, 100.f);
	*/
}

void CGraphics_PS2_gsKit::GetScreen(float *pTopLeftX, float *pTopLeftY, float *pBottomRightX, float *pBottomRightY)
{
	*pTopLeftX = m_ScreenX0;
	*pTopLeftY = m_ScreenY0;
	*pBottomRightX = m_ScreenX1;
	*pBottomRightY = m_ScreenY1;
}

void CGraphics_PS2_gsKit::LinesBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->LinesBegin twice");
	m_Drawing = DRAWING_LINES;
	SetColor(1,1,1,1);
}

void CGraphics_PS2_gsKit::LinesEnd()
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesEnd without begin");
	Flush();
	m_Drawing = 0;
}

void CGraphics_PS2_gsKit::LinesDraw(const CLineItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_LINES, "called Graphics()->LinesDraw without begin");

	int vertexPrim = (g_Config.m_GfxQuadAsTriangle) ? 3 : 4;

	for(int i = 0; i < Num; ++i)
	{
		m_aVertices[m_NumVertices + 3*i].m_Pos.x = pArray[i].m_X0;
		m_aVertices[m_NumVertices + 3*i].m_Pos.y = pArray[i].m_Y0;
		m_aVertices[m_NumVertices + 3*i].m_Tex = m_aTexture[0];
		m_aVertices[m_NumVertices + 3*i].m_Color = m_aColor[0];

		m_aVertices[m_NumVertices + 3*i + 1].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 3*i + 1].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 3*i + 1].m_Tex = m_aTexture[1];
		m_aVertices[m_NumVertices + 3*i + 1].m_Color = m_aColor[1];

		m_aVertices[m_NumVertices + 3*i + 2].m_Pos.x = pArray[i].m_X1;
		m_aVertices[m_NumVertices + 3*i + 2].m_Pos.y = pArray[i].m_Y1;
		m_aVertices[m_NumVertices + 3*i + 2].m_Tex = m_aTexture[1];
		m_aVertices[m_NumVertices + 3*i + 2].m_Color = m_aColor[1];

		if (!g_Config.m_GfxQuadAsTriangle)
		{
			m_aVertices[m_NumVertices + 3*i + 3].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 3*i + 3].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 3*i + 3].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 3*i + 3].m_Color = m_aColor[0];
		}
	}

	AddVertices(vertexPrim*Num);
}

int CGraphics_PS2_gsKit::UnloadTexture(int Index)
{
	if(Index == m_InvalidTexture)
		return 0;

	if(Index < 0)
		return 0;

	GSTEXTURE* gsTex = (GSTEXTURE*)m_aTextures[Index].m_Tex;
	_mem_free(gsTex->Mem);
	_mem_free(gsTex);

	m_aTextures[Index].m_Next = m_FirstFreeTexture;
	m_TextureMemoryUsage -= m_aTextures[Index].m_MemSize;
	m_FirstFreeTexture = Index;
	return 0;
}

int CGraphics_PS2_gsKit::LoadTextureRawSub(int TextureID, int x, int y, int Width, int Height, int Format, const void *pData)
{
    return 0;
}

int CGraphics_PS2_gsKit::LoadTextureRaw(int Width, int Height, int Format, const void *pData, int StoreFormat, int Flags)
{
	u8* pTexData = (u8*)pData;
	u8* pTmpData = 0;

	int Tex = 0;

	// don't waste memory on texture if we are stress testing
	if(g_Config.m_DbgStress)
		return 	m_InvalidTexture;

	// grab texture
	Tex = m_FirstFreeTexture;
	m_FirstFreeTexture = m_aTextures[Tex].m_Next;
	m_aTextures[Tex].m_Next = -1;

	if(!(Flags&TEXLOAD_NORESAMPLE) && (Format == CImageInfo::FORMAT_RGBA || Format == CImageInfo::FORMAT_RGB))
	{
		if(Width > GL_MAX_TEXTURE_SIZE || Height > GL_MAX_TEXTURE_SIZE)
		{
			int NewWidth = min(Width, GL_MAX_TEXTURE_SIZE);
			float div = NewWidth/(float)Width;
			int NewHeight = Height * div;
			pTmpData = Rescale(Width, Height, NewWidth, NewHeight, Format, pTexData);
			pTexData = pTmpData;
			Width = NewWidth;
			Height = NewHeight;
		}
		else if(Width > 16 && Height > 16 && g_Config.m_GfxTextureQuality == 0)
		{
			pTmpData = Rescale(Width, Height, Width/2, Height/2, Format, pTexData);
			pTexData = pTmpData;
			Width /= 2;
			Height /= 2;
		}
	}

	int PixelSize = 4;
	if(StoreFormat == CImageInfo::FORMAT_RGB)
		PixelSize = 3;

	GSTEXTURE* gsTex = (GSTEXTURE*)mem_alloc(sizeof(GSTEXTURE), 1);
	mem_zero(gsTex, sizeof(GSTEXTURE));
	gsTex->Width = Width;
	gsTex->Height = Height;
	gsTex->Filter = GS_FILTER_LINEAR;

	gsTex->PSM = GS_PSM_CT32;
	if(StoreFormat == CImageInfo::FORMAT_RGB)
		gsTex->PSM = GS_PSM_CT24;

	gsTex->Mem = (u32*)mem_alloc(gsKit_texture_size_ee(Width, Height, gsTex->PSM), 1);
	if (!gsTex->Mem)
	{
		if (pTmpData) _mem_free(pTmpData);
		_mem_free(gsTex);
		return m_InvalidTexture;
	}

	for (int i=0; i<Width*Height; i++)
	{
		u8 r = pTexData[i*4+0];
		u8 g = pTexData[i*4+1];
		u8 b = pTexData[i*4+2];
		u8 a = 128 - ((PixelSize == 4) ? pTexData[i*4+3]/255.f*128 : 128);

		gsTex->Mem[i] = (r<<0) | (g<<8) | (b<<16) | (a<<24);
	}

	if (pTmpData) _mem_free(pTmpData);

	m_aTextures[Tex].m_Tex = (void*)gsTex;

	// calculate memory usage
	{
		m_aTextures[Tex].m_MemSize = Width*Height*PixelSize;
	}

	m_TextureMemoryUsage += m_aTextures[Tex].m_MemSize;
	return Tex;
}

// simple uncompressed RGBA loaders
int CGraphics_PS2_gsKit::LoadTexture(const char *pFilename, int StorageType, int StoreFormat, int Flags)
{
	int l = str_length(pFilename);
	int ID;
	CImageInfo Img;

	if(l < 3)
		return m_InvalidTexture;
	if(LoadPNG(&Img, pFilename, StorageType))
	{
		if (StoreFormat == CImageInfo::FORMAT_AUTO)
			StoreFormat = Img.m_Format;

		ID = LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, StoreFormat, Flags);
		_mem_free(Img.m_pData);
		if(ID != m_InvalidTexture && g_Config.m_Debug)
			dbg_msg("graphics/texture", "loaded %s", pFilename);
		return ID;
	}

	return m_InvalidTexture;
}

int CGraphics_PS2_gsKit::LoadPNG(CImageInfo *pImg, const char *pFilename, int StorageType)
{
	char aCompleteFilename[512];
	unsigned char *pBuffer;
	png_t Png; // ignore_convention

	// open file for reading
	png_init(0,0); // ignore_convention

	IOHANDLE File = m_pStorage->OpenFile(pFilename, IOFLAG_READ, StorageType, aCompleteFilename, sizeof(aCompleteFilename));
	if(File)
		io_close(File);
	else
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", pFilename);
		return 0;
	}

	int Error = png_open_file(&Png, aCompleteFilename); // ignore_convention
	if(Error != PNG_NO_ERROR)
	{
		dbg_msg("game/png", "failed to open file. filename='%s'", aCompleteFilename);
		if(Error != PNG_FILE_ERROR)
			png_close_file(&Png); // ignore_convention
		return 0;
	}

	if(Png.depth != 8 || (Png.color_type != PNG_TRUECOLOR && Png.color_type != PNG_TRUECOLOR_ALPHA)) // ignore_convention
	{
		dbg_msg("game/png", "invalid format. filename='%s'", aCompleteFilename);
		png_close_file(&Png); // ignore_convention
		return 0;
	}

	pBuffer = (unsigned char *)mem_alloc(Png.width * Png.height * Png.bpp, 1); // ignore_convention
	png_get_data(&Png, pBuffer); // ignore_convention
	png_close_file(&Png); // ignore_convention

	pImg->m_Width = Png.width; // ignore_convention
	pImg->m_Height = Png.height; // ignore_convention
	if(Png.color_type == PNG_TRUECOLOR) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGB;
	else if(Png.color_type == PNG_TRUECOLOR_ALPHA) // ignore_convention
		pImg->m_Format = CImageInfo::FORMAT_RGBA;
	pImg->m_pData = pBuffer;
	return 1;
}

void CGraphics_PS2_gsKit::ScreenshotDirect(const char *pFilename)
{
	
}

void CGraphics_PS2_gsKit::TextureSet(int TextureID)
{
	dbg_assert(m_Drawing == 0, "called Graphics()->TextureSet within begin");

	m_CurrTexture = TextureID;
	if(TextureID != -1)
	{
		GSTEXTURE* gsTex = (GSTEXTURE*)m_aTextures[TextureID].m_Tex;
		gsKit_TexManager_bind(gsGlobal, gsTex);
	}
}

void CGraphics_PS2_gsKit::Clear(float r, float g, float b)
{
	gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(r*255, g*255, b*255, 0, 0));
}

void CGraphics_PS2_gsKit::QuadsBegin()
{
	dbg_assert(m_Drawing == 0, "called Graphics()->QuadsBegin twice");
	m_Drawing = DRAWING_QUADS;

	QuadsSetSubset(0,0,1,1);
	QuadsSetRotation(0);
	SetColor(1,1,1,1);
}

void CGraphics_PS2_gsKit::QuadsEnd()
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsEnd without begin");
	Flush();
	m_Drawing = 0;
}

void CGraphics_PS2_gsKit::QuadsSetRotation(float Angle)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetRotation without begin");
	m_Rotation = Angle;
}

void CGraphics_PS2_gsKit::SetColorVertex(const CColorVertex *pArray, int Num)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColorVertex without begin");

	for(int i = 0; i < Num; ++i)
	{
		m_aColor[pArray[i].m_Index].r = pArray[i].m_R;
		m_aColor[pArray[i].m_Index].g = pArray[i].m_G;
		m_aColor[pArray[i].m_Index].b = pArray[i].m_B;
		m_aColor[pArray[i].m_Index].a = pArray[i].m_A;
	}
}

void CGraphics_PS2_gsKit::SetColor(float r, float g, float b, float a)
{
	dbg_assert(m_Drawing != 0, "called Graphics()->SetColor without begin");
	CColorVertex Array[4] = {
		CColorVertex(0, r, g, b, a),
		CColorVertex(1, r, g, b, a),
		CColorVertex(2, r, g, b, a),
		CColorVertex(3, r, g, b, a)};
	SetColorVertex(Array, 4);
}

void CGraphics_PS2_gsKit::QuadsSetSubset(float TlU, float TlV, float BrU, float BrV)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsSetSubset without begin");

	m_aTexture[0].u = TlU;	m_aTexture[1].u = BrU;
	m_aTexture[0].v = TlV;	m_aTexture[1].v = TlV;

	m_aTexture[3].u = TlU;	m_aTexture[2].u = BrU;
	m_aTexture[3].v = BrV;	m_aTexture[2].v = BrV;
}

void CGraphics_PS2_gsKit::QuadsSetSubsetFree(
	float x0, float y0, float x1, float y1,
	float x2, float y2, float x3, float y3)
{
	m_aTexture[0].u = x0; m_aTexture[0].v = y0;
	m_aTexture[1].u = x1; m_aTexture[1].v = y1;
	m_aTexture[2].u = x2; m_aTexture[2].v = y2;
	m_aTexture[3].u = x3; m_aTexture[3].v = y3;
}

void CGraphics_PS2_gsKit::QuadsDraw(CQuadItem *pArray, int Num)
{
	for(int i = 0; i < Num; ++i)
	{
		pArray[i].m_X -= pArray[i].m_Width/2;
		pArray[i].m_Y -= pArray[i].m_Height/2;
	}

	QuadsDrawTL(pArray, Num);
}

void CGraphics_PS2_gsKit::QuadsDrawTL(const CQuadItem *pArray, int Num)
{
	CPoint Center;
	Center.z = 0;

	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawTL without begin");

	if(g_Config.m_GfxQuadAsTriangle)
	{
		for(int i = 0; i < Num; ++i)
		{
			// first triangle
			m_aVertices[m_NumVertices + 6*i].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 6*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 2].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 6*i + 2].m_Color = m_aColor[2];

			// second triangle
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 6*i + 3].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i + 3].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 4].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 6*i + 4].m_Color = m_aColor[2];

			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 6*i + 5].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 6*i + 5].m_Color = m_aColor[3];

			if(m_Rotation != 0)
			{
				Center.x = pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, &m_aVertices[m_NumVertices + 6*i], 6);
			}
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 4*i].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 4*i].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 4*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 4*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.y = pArray[i].m_Y;
			m_aVertices[m_NumVertices + 4*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 4*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.x = pArray[i].m_X + pArray[i].m_Width;
			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 4*i + 2].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 4*i + 2].m_Color = m_aColor[2];

			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.x = pArray[i].m_X;
			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.y = pArray[i].m_Y + pArray[i].m_Height;
			m_aVertices[m_NumVertices + 4*i + 3].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 4*i + 3].m_Color = m_aColor[3];

			if(m_Rotation != 0)
			{
				Center.x = pArray[i].m_X + pArray[i].m_Width/2;
				Center.y = pArray[i].m_Y + pArray[i].m_Height/2;

				Rotate(Center, &m_aVertices[m_NumVertices + 4*i], 4);
			}
		}

		AddVertices(4*Num);
	}
}

void CGraphics_PS2_gsKit::QuadsDrawFreeform(const CFreeformItem *pArray, int Num)
{
	dbg_assert(m_Drawing == DRAWING_QUADS, "called Graphics()->QuadsDrawFreeform without begin");

	if(g_Config.m_GfxQuadAsTriangle)
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 6*i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6*i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 6*i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 6*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 6*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6*i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6*i + 2].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 6*i + 2].m_Color = m_aColor[3];

			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 6*i + 3].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 6*i + 3].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 6*i + 3].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 6*i + 4].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 6*i + 4].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 6*i + 4].m_Color = m_aColor[3];

			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 6*i + 5].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 6*i + 5].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 6*i + 5].m_Color = m_aColor[2];
		}

		AddVertices(3*2*Num);
	}
	else
	{
		for(int i = 0; i < Num; ++i)
		{
			m_aVertices[m_NumVertices + 4*i].m_Pos.x = pArray[i].m_X0;
			m_aVertices[m_NumVertices + 4*i].m_Pos.y = pArray[i].m_Y0;
			m_aVertices[m_NumVertices + 4*i].m_Tex = m_aTexture[0];
			m_aVertices[m_NumVertices + 4*i].m_Color = m_aColor[0];

			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.x = pArray[i].m_X1;
			m_aVertices[m_NumVertices + 4*i + 1].m_Pos.y = pArray[i].m_Y1;
			m_aVertices[m_NumVertices + 4*i + 1].m_Tex = m_aTexture[1];
			m_aVertices[m_NumVertices + 4*i + 1].m_Color = m_aColor[1];

			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.x = pArray[i].m_X3;
			m_aVertices[m_NumVertices + 4*i + 2].m_Pos.y = pArray[i].m_Y3;
			m_aVertices[m_NumVertices + 4*i + 2].m_Tex = m_aTexture[3];
			m_aVertices[m_NumVertices + 4*i + 2].m_Color = m_aColor[3];

			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.x = pArray[i].m_X2;
			m_aVertices[m_NumVertices + 4*i + 3].m_Pos.y = pArray[i].m_Y2;
			m_aVertices[m_NumVertices + 4*i + 3].m_Tex = m_aTexture[2];
			m_aVertices[m_NumVertices + 4*i + 3].m_Color = m_aColor[2];
		}

		AddVertices(4*Num);
	}
}

void CGraphics_PS2_gsKit::QuadsText(float x, float y, float Size, const char *pText)
{
	float StartX = x;

	while(*pText)
	{
		char c = *pText;
		pText++;

		if(c == '\n')
		{
			x = StartX;
			y += Size;
		}
		else
		{
			QuadsSetSubset(
				(c%16)/16.0f,
				(c/16)/16.0f,
				(c%16)/16.0f+1.0f/16.0f,
				(c/16)/16.0f+1.0f/16.0f);

			CQuadItem QuadItem(x, y, Size, Size);
			QuadsDrawTL(&QuadItem, 1);
			x += Size/2;
		}
	}
}

int CGraphics_PS2_gsKit::Init()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	// default initialization
	gsGlobal = gsKit_init_global();
	gsGlobal->PSM = GS_PSM_CT32;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->ZBuffering = GS_SETTING_OFF;

	// default dmaKit initialization
	dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	gsKit_init_screen(gsGlobal);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

	gsKit_TexManager_init(gsGlobal);

	gsKit_set_test(gsGlobal, GS_ZTEST_OFF);

	m_ScreenWidth = g_Config.m_GfxScreenWidth = gsGlobal->Width;
	m_ScreenHeight = g_Config.m_GfxScreenHeight = gsGlobal->Height;

	m_CurrTexture = -1;

	// Set all z to -5.0f
	for(int i = 0; i < MAX_VERTICES; i++)
		m_aVertices[i].m_Pos.z = -5.0f;

	// init textures
	m_FirstFreeTexture = 0;
	for(int i = 0; i < MAX_TEXTURES; i++)
		m_aTextures[i].m_Next = i+1;
	m_aTextures[MAX_TEXTURES-1].m_Next = -1;

	/*
	glInit();
	glViewport(0, 0, 255, 191);

	g_Config.m_GfxScreenWidth = 256;
	g_Config.m_GfxScreenHeight = 192;

	m_ScreenWidth = g_Config.m_GfxScreenWidth;
	m_ScreenHeight = g_Config.m_GfxScreenHeight;

	// Set all z to -5.0f
	for(int i = 0; i < MAX_VERTICES; i++)
		m_aVertices[i].m_Pos.z = -5.0f;

	// init textures
	m_FirstFreeTexture = 0;
	for(int i = 0; i < MAX_TEXTURES; i++)
		m_aTextures[i].m_Next = i+1;
	m_aTextures[MAX_TEXTURES-1].m_Next = -1;

	// set some default settings
	glEnable(GL_BLEND);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef32(inttof32(SCALE_VERTICES), inttof32(SCALE_VERTICES), inttof32(SCALE_VERTICES));

	glAlphaFunc(7);
	glEnable(GL_ALPHA_TEST);
	//glEnable(GL_ANTIALIAS);
	//glClearPolyID(63);

	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);

	vramSetBankA(VRAM_A_TEXTURE);
	vramSetBankB(VRAM_B_TEXTURE);
	vramSetBankC(VRAM_C_TEXTURE);
	vramSetBankD(VRAM_D_TEXTURE);
	*/

	// create null texture, will get id=0
	static const unsigned char aNullTextureData[] = {
		0xff,0x00,0x00,0xff, 0xff,0x00,0x00,0xff, 0x00,0xff,0x00,0xff, 0x00,0xff,0x00,0xff,
		0xff,0x00,0x00,0xff, 0xff,0x00,0x00,0xff, 0x00,0xff,0x00,0xff, 0x00,0xff,0x00,0xff,
		0x00,0x00,0xff,0xff, 0x00,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0x00,0xff,
		0x00,0x00,0xff,0xff, 0x00,0x00,0xff,0xff, 0xff,0xff,0x00,0xff, 0xff,0xff,0x00,0xff,
	};

	m_InvalidTexture = LoadTextureRaw(4,4,CImageInfo::FORMAT_RGBA,aNullTextureData,CImageInfo::FORMAT_RGBA,TEXLOAD_NORESAMPLE);

	return 0;
}

void CGraphics_PS2_gsKit::Shutdown()
{
	gsKit_deinit_global(gsGlobal);
}

void CGraphics_PS2_gsKit::Minimize()
{
	
}

void CGraphics_PS2_gsKit::Maximize()
{
	
}

int CGraphics_PS2_gsKit::WindowActive()
{
	return 1;
}

int CGraphics_PS2_gsKit::WindowOpen()
{
	return 1;
}

void CGraphics_PS2_gsKit::NotifyWindow()
{
	
}

void CGraphics_PS2_gsKit::TakeScreenshot(const char *pFilename)
{
	
}

void CGraphics_PS2_gsKit::TakeCustomScreenshot(const char *pFilename)
{
	
}


void CGraphics_PS2_gsKit::Swap()
{
	gsKit_queue_exec(gsGlobal);
	gsKit_sync_flip(gsGlobal);
	gsKit_TexManager_nextFrame(gsGlobal);
}


int CGraphics_PS2_gsKit::GetVideoModes(CVideoMode *pModes, int MaxModes)
{
	pModes[0].m_Width = gsGlobal->Width;
	pModes[0].m_Height = gsGlobal->Height;
	pModes[0].m_Red = 8;
	pModes[0].m_Green = 8;
	pModes[0].m_Blue = 8;
	return 1;
}

// syncronization
void CGraphics_PS2_gsKit::InsertSignal(semaphore *pSemaphore)
{
	
}

bool CGraphics_PS2_gsKit::IsIdle()
{
	return true;
}

void CGraphics_PS2_gsKit::WaitForIdle()
{
}

extern IEngineGraphics *CreateEngineGraphics() { return new CGraphics_PS2_gsKit(); }
