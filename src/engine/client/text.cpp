/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <engine/graphics.h>
#include <engine/client.h>
#include <engine/textrender.h>

#ifdef CONF_FAMILY_WINDOWS
	#include <windows.h>
#endif


class CTextRender : public IEngineTextRender
{
	IGraphics *m_pGraphics;
	IClient *m_pClient;
	IGraphics *Graphics() { return m_pGraphics; }
	IClient *Client() { return m_pClient; }

	int WordLength(const char *pText)
	{
		int s = 1;
		while(1)
		{
			if(*pText == 0)
				return s-1;
			if(*pText == '\n' || *pText == '\t' || *pText == ' ')
				return s;
			pText++;
			s++;
		}
	}

	float m_TextR;
	float m_TextG;
	float m_TextB;
	float m_TextA;

	float m_TextOutlineR;
	float m_TextOutlineG;
	float m_TextOutlineB;
	float m_TextOutlineA;

public:
	CTextRender()
	{
		m_pGraphics = 0;
		m_pClient = 0;

		m_TextR = 1.0f;
		m_TextG = 1.0f;
		m_TextB = 1.0f;
		m_TextA = 1.0f;
		m_TextOutlineR = 0.0f;
		m_TextOutlineG = 0.0f;
		m_TextOutlineB = 0.0f;
		m_TextOutlineA = 0.3f;
	}

	virtual void Init()
	{
		m_pGraphics = Kernel()->RequestInterface<IGraphics>();
		m_pClient = Kernel()->RequestInterface<IClient>();
	}


	virtual CFont *LoadFont(const char *pFilename)
	{
		return 0;
	};

	virtual void DestroyFont(CFont *pFont)
	{
		
	}

	virtual void SetDefaultFont(CFont *pFont)
	{
		
	}


	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags)
	{
		mem_zero(pCursor, sizeof(*pCursor));
		pCursor->m_FontSize = FontSize*1.35f;
		pCursor->m_StartX = x;
		pCursor->m_StartY = y;
		pCursor->m_X = x;
		pCursor->m_Y = y;
		pCursor->m_LineCount = 1;
		pCursor->m_LineWidth = -1;
		pCursor->m_Flags = Flags;
		pCursor->m_CharCount = 0;
	}


	virtual void Text(void *pFontSetV, float x, float y, float Size, const char *pText, int MaxWidth)
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, x, y, Size, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = MaxWidth;
		TextEx(&Cursor, pText, -1);
	}

	virtual float TextWidth(void *pFontSetV, float Size, const char *pText, int Length)
	{
		float len = str_length(pText) * Size*1.4f*0.5f;
		return len;
	}

	virtual int TextLineCount(void *pFontSetV, float Size, const char *pText, float LineWidth)
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, 0, 0, Size, 0);
		Cursor.m_LineWidth = LineWidth;
		TextEx(&Cursor, pText, -1);
		return Cursor.m_LineCount;
	}

	virtual void TextColor(float r, float g, float b, float a)
	{
		m_TextR = r;
		m_TextG = g;
		m_TextB = b;
		m_TextA = a;
	}

	virtual void TextOutlineColor(float r, float g, float b, float a)
	{
		m_TextOutlineR = r;
		m_TextOutlineG = g;
		m_TextOutlineB = b;
		m_TextOutlineA = a;
	}

	virtual void TextEx(CTextCursor *pCursor, const char *pText, int Length)
	{
		CFont *pFont = pCursor->m_pFont;

		//dbg_msg("textrender", "rendering text '%s'", text);

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float FakeToScreenX, FakeToScreenY;
		int ActualX, ActualY;

		int ActualSize;
		int GotNewLine = 0;
		float DrawX = 0.0f, DrawY = 0.0f;
		int LineCount = 0;
		float CursorX, CursorY;

		float Size = pCursor->m_FontSize;

		// to correct coords, convert to screen coords, round, and convert back
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		FakeToScreenX = (Graphics()->ScreenWidth()/(ScreenX1-ScreenX0));
		FakeToScreenY = (Graphics()->ScreenHeight()/(ScreenY1-ScreenY0));
		ActualX = (int)(pCursor->m_X * FakeToScreenX);
		ActualY = (int)(pCursor->m_Y * FakeToScreenY);

		CursorX = ActualX / FakeToScreenX;
		CursorY = ActualY / FakeToScreenY;

		// same with size
		ActualSize = (int)(Size * FakeToScreenY);
		Size = ActualSize / FakeToScreenY;

		//pSizeData = GetSize(pFont, ActualSize);
		//RenderSetup(pFont, ActualSize);

		//float Scale = 1/pSizeData->m_FontSize;
		float Scale = 1;

		// set length
		if(Length < 0)
			Length = str_length(pText);

		//for(;i < 2; i++)
		{
			const char *pCurrent = (char *)pText;
			const char *pEnd = pCurrent+Length;
			DrawX = CursorX;
			DrawY = CursorY;
			LineCount = pCursor->m_LineCount;

			if(pCursor->m_Flags&TEXTFLAG_RENDER)
			{
				Graphics()->TextureSet(Client()->GetDebugFont());
				Graphics()->QuadsBegin();
				Graphics()->SetColor(m_TextR, m_TextG, m_TextB, m_TextA);
			}

			while(pCurrent < pEnd && (pCursor->m_MaxLines < 1 || LineCount <= pCursor->m_MaxLines))
			{
				int NewLine = 0;
				const char *pBatchEnd = pEnd;
				if(pCursor->m_LineWidth > 0 && !(pCursor->m_Flags&TEXTFLAG_STOP_AT_END))
				{
					int Wlen = min(WordLength((char *)pCurrent), (int)(pEnd-pCurrent));
					CTextCursor Compare = *pCursor;
					Compare.m_X = DrawX;
					Compare.m_Y = DrawY;
					Compare.m_Flags &= ~TEXTFLAG_RENDER;
					Compare.m_LineWidth = -1;
					TextEx(&Compare, pCurrent, Wlen);

					if(Compare.m_X-DrawX > pCursor->m_LineWidth)
					{
						// word can't be fitted in one line, cut it
						CTextCursor Cutter = *pCursor;
						Cutter.m_CharCount = 0;
						Cutter.m_X = DrawX;
						Cutter.m_Y = DrawY;
						Cutter.m_Flags &= ~TEXTFLAG_RENDER;
						Cutter.m_Flags |= TEXTFLAG_STOP_AT_END;

						TextEx(&Cutter, (const char *)pCurrent, Wlen);
						Wlen = Cutter.m_CharCount;
						NewLine = 1;

						if(Wlen <= 3) // if we can't place 3 chars of the word on this line, take the next
							Wlen = 0;
					}
					else if(Compare.m_X-pCursor->m_StartX > pCursor->m_LineWidth)
					{
						NewLine = 1;
						Wlen = 0;
					}

					pBatchEnd = pCurrent + Wlen;
				}

				const char *pTmp = pCurrent;
				int NextCharacter = str_utf8_decode(&pTmp);
				while(pCurrent < pBatchEnd)
				{
					uint8_t Character = (uint8_t)NextCharacter;
					pCurrent = pTmp;
					NextCharacter = str_utf8_decode(&pTmp);

					if(Character == '\n')
					{
						DrawX = pCursor->m_StartX;
						DrawY += Size;
						DrawX = (int)(DrawX * FakeToScreenX) / FakeToScreenX; // realign
						DrawY = (int)(DrawY * FakeToScreenY) / FakeToScreenY;
						++LineCount;
						if(pCursor->m_MaxLines > 0 && LineCount > pCursor->m_MaxLines)
							break;
						continue;
					}

					float Advance = Scale*0.5f;
					if(pCursor->m_Flags&TEXTFLAG_STOP_AT_END && DrawX+Advance*Size-pCursor->m_StartX > pCursor->m_LineWidth)
					{
						// we hit the end of the line, no more to render or count
						pCurrent = pEnd;
						break;
					}

					if(pCursor->m_Flags&TEXTFLAG_RENDER)
					{
						Graphics()->QuadsSetSubset(
							(Character%16)/16.0f,
							(Character/16)/16.0f,
							(Character%16)/16.0f+1.0f/16.0f,
							(Character/16)/16.0f+1.0f/16.0f);

						IGraphics::CQuadItem QuadItem(DrawX, DrawY, Size, Size);
						Graphics()->QuadsDrawTL(&QuadItem, 1);
					}

					DrawX += Advance*Size;
					pCursor->m_CharCount++;
				}

				if(NewLine)
				{
					DrawX = pCursor->m_StartX;
					DrawY += Size;
					GotNewLine = 1;
					DrawX = (int)(DrawX * FakeToScreenX) / FakeToScreenX; // realign
					DrawY = (int)(DrawY * FakeToScreenY) / FakeToScreenY;
					++LineCount;
				}
			}

			if(pCursor->m_Flags&TEXTFLAG_RENDER)
				Graphics()->QuadsEnd();
		}

		pCursor->m_X = DrawX;
		pCursor->m_LineCount = LineCount;

		if(GotNewLine)
			pCursor->m_Y = DrawY;
	}

};

IEngineTextRender *CreateEngineTextRender() { return new CTextRender; }
