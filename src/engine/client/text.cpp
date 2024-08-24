/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <vector>
#include <string>
#include <unordered_map>
#include <base/system.h>
#include <base/math.h>
#include <engine/graphics.h>
#include <engine/client.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#ifdef CONF_FAMILY_WINDOWS
	#include <windows.h>
#endif

// https://github.com/vladimirgamalyan/fontbm

class BMFont
{
#pragma pack(push, 1)
	struct InfoBlock
	{
		int16_t fontSize;
		int8_t smooth:1;
		int8_t unicode:1;
		int8_t italic:1;
		int8_t bold:1;
		int8_t reserved:4;
		uint8_t charSet;
		uint16_t stretchH;
		int8_t aa;
		uint8_t paddingUp;
		uint8_t paddingRight;
		uint8_t paddingDown;
		uint8_t paddingLeft;
		uint8_t spacingHoriz;
		uint8_t spacingVert;
		uint8_t outline;
		char* fontName;
	} info;
	static_assert(sizeof(InfoBlock) == 18, "InfoBlock size is not 18");

	struct CommonBlock
	{
		uint16_t lineHeight;
		uint16_t base;
		uint16_t scaleW;
		uint16_t scaleH;
		uint16_t pages;
		uint8_t reserved:7;
		uint8_t packed:1;
		uint8_t alphaChnl;
		uint8_t redChnl;
		uint8_t greenChnl;
		uint8_t blueChnl;
	} common;
	static_assert(sizeof(CommonBlock) == 15, "CommonBlock size is not 15");

	struct CharBlock
	{
		uint32_t id;
		uint16_t x;
		uint16_t y;
		uint16_t width;
		uint16_t height;
		int16_t xoffset;
		int16_t yoffset;
		int16_t xadvance;
		uint8_t page;
		uint8_t channel;
	};
	static_assert(sizeof(CharBlock) == 20, "CharBlock size is not 20");

	struct KerningPairsBlock
	{
		uint32_t first;
		uint32_t second;
		int16_t amount;
	};
	static_assert(sizeof(KerningPairsBlock) == 10, "KerningPairsBlock size is not 10");
#pragma pack(pop)

	std::unordered_map<int, char*> pages;
	std::unordered_map<uint8_t, CharBlock> chars;
	std::vector<KerningPairsBlock> kernings;
	int m_TextureID;
	int m_Width;
	int m_Height;

	IGraphics *m_pGraphics;
	IStorage *m_pStorage;
	IGraphics *Graphics() { return m_pGraphics; }

	bool LoadPng(const std::string& png)
	{
		dbg_msg("bmfont", "Loading texture...");

		CImageInfo Img;
		if (!Graphics()->LoadPNG(&Img, png.c_str(), IStorage::TYPE_ALL))
		{
			dbg_msg("bmfont", "Failed to load PNG file '%s'", png.c_str());
			return false;
		}

		m_Width = Img.m_Width;
		m_Height = Img.m_Height;
		m_TextureID = Graphics()->LoadTextureRaw(Img.m_Width, Img.m_Height, Img.m_Format, Img.m_pData, Img.m_Format, 0);
		_mem_free(Img.m_pData);
		if(!m_TextureID)
		{
			dbg_msg("bmfont", "Failed to load texture '%s'", png.c_str());
			return false;
		}

		dbg_msg("bmfont", "BMFont loaded successfully");
		return true;
	}

public:
	std::unordered_map<int, char*>& Pages() {return pages;}
	std::unordered_map<uint8_t, CharBlock>& Chars() {return chars;}
	std::vector<KerningPairsBlock>& Kernings() {return kernings;}
	const int Texture() const {return m_TextureID;}

	BMFont(IGraphics* Graphics, IStorage* Storage) : m_pGraphics(Graphics), m_pStorage(Storage)
	{
		info.fontName = 0;
	}

	~BMFont()
	{
		if (info.fontName)
			_mem_free(info.fontName);
	}

	bool Init(std::string filename)
	{
		dbg_msg("bmfont", "Loading BMFont '%s'", filename.c_str());

		std::string fnt = filename+".fnt";
		std::string png = filename+".png";

		char aCompleteFilename[512];
		IOHANDLE f = m_pStorage->OpenFile(fnt.c_str(), IOFLAG_READ, IStorage::TYPE_ALL, aCompleteFilename, sizeof(aCompleteFilename));

		char header[3]; uint8_t version;
		io_read(f, header, 3);
		io_read(f, &version, 1);
		if (header[0] != 'B' || header[1] != 'M' || header[2] != 'F')
		{
			dbg_msg("bmfont", "Buffer is not binary BMFont file");
			io_close(f);
			return false;
		}

		uint8_t currBlock; int blockSize;

		// Block 1: Info
		io_read(f, &currBlock, 1);
		io_read(f, &blockSize, 4);

		if (currBlock != 1)
		{
			dbg_msg("bmfont", "Failed loading font (currBlock: expected 1, got %d)", currBlock);
			io_close(f);
			return false;
		}
		if (blockSize <= 0)
		{
			dbg_msg("bmfont", "Failed loading font on block %d (block size is %d)", currBlock, blockSize);
			io_close(f);
			return false;
		}

		io_read(f, &info, sizeof(BMFont::InfoBlock) - sizeof(char*));

		int fontNameSize = blockSize - (sizeof(BMFont::InfoBlock) - sizeof(char*));
		info.fontName = (char*)mem_alloc(fontNameSize, 1);
		io_read(f, info.fontName, fontNameSize);
		dbg_msg("bmfont", "Font name is '%s'", info.fontName);

		// Block 2: Common
		io_read(f, &currBlock, 1);
		io_read(f, &blockSize, 4);

		if (currBlock != 2)
		{
			dbg_msg("bmfont", "Failed loading font (currBlock: expected 2, got %d) (%d)", currBlock, blockSize);
			io_close(f);
			return false;
		}
		if (blockSize <= 0)
		{
			dbg_msg("bmfont", "Failed loading font on block %d (block size is %d)", currBlock, blockSize);
			io_close(f);
			return false;
		}

		io_read(f, &common, sizeof(BMFont::CommonBlock));

		// Block 3: Pages
		io_read(f, &currBlock, 1);
		io_read(f, &blockSize, 4);

		if (currBlock != 3)
		{
			dbg_msg("bmfont", "Failed loading font (currBlock: expected 2, got %d) (%d)", currBlock, blockSize);
			io_close(f);
			return false;
		}
		if (blockSize <= 0)
		{
			dbg_msg("bmfont", "Failed loading font on block %d (block size is %d)", currBlock, blockSize);
			io_close(f);
			return false;
		}

		int i = 0;
		while (blockSize > 0)
		{
			char currChar;

			uint32_t startPos = io_tell(f);
			do {
				io_read(f, &currChar, 1);
			} while (currChar != 0);
			uint32_t endPos = io_tell(f); // to include null terminator
			uint32_t pageSize = endPos - startPos;

			char* pageName = new char[pageSize];
			io_seek(f, startPos, IOSEEK_START);
			io_read(f, pageName, pageSize);
			
			pages[i++] = pageName;
			blockSize -= pageSize;
		}

		// Block 4: Chars
		io_read(f, &currBlock, 1);
		io_read(f, &blockSize, 4);

		if (currBlock != 4)
		{
			dbg_msg("bmfont", "Failed loading font (currBlock: expected 4, got %d)", currBlock);
			io_close(f);
			return false;
		}
		if (blockSize <= 0)
		{
			dbg_msg("bmfont", "Failed loading font on block %d (block size is %d)", currBlock, blockSize);
			io_close(f);
			return false;
		}

		int charCount = blockSize / sizeof(BMFont::CharBlock);
		for (int i=0; i<charCount; i++)
		{
			BMFont::CharBlock block;
			io_read(f, &block, sizeof(BMFont::CharBlock));
			chars[block.id] = block;
		}

		// Block 5 (optional): Kernings
		if (io_read(f, &currBlock, 1) < 1)
		{
			// No kernings. We're done
			io_close(f);
			return LoadPng(png);
		}
		io_read(f, &blockSize, 4);

		if (currBlock != 5)
		{
			dbg_msg("bmfont", "BMFont: Failed loading font (currBlock: expected 5, got %d)", currBlock);
			io_close(f);
			return false;
		}
		if (blockSize <= 0)
		{
			dbg_msg("bmfont", "BMFont: Failed loading font on block %d (block size is %d)", currBlock, blockSize);
			io_close(f);
			return false;
		}

		int kernCount = blockSize / sizeof(BMFont::KerningPairsBlock);
		for (int i=0; i<kernCount; i++)
		{
			BMFont::KerningPairsBlock block;
			io_read(f, &block, sizeof(BMFont::KerningPairsBlock));
			kernings.push_back(block);
		}

		io_close(f);
		return LoadPng(png);
	}

	void RenderChar(uint8_t c, float x, float y, float scale, float size, bool outline)
	{
		if (!chars.count(c))
			return;

		Graphics()->QuadsSetSubset(
			chars[c].x / (float)m_Width,
			chars[c].y / (float)m_Height,
			(chars[c].x + chars[c].width) / (float)m_Width,
			(chars[c].y + chars[c].height) / (float)m_Height);

		IGraphics::CQuadItem QuadItem(
			x + (chars[c].xoffset*size*scale/10),
			y + (chars[c].yoffset*size*scale/10),
			chars[c].width*size*scale/10,
			chars[c].height*size*scale/10);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	}
};


class CTextRender : public IEngineTextRender
{
	IGraphics *m_pGraphics;
	IStorage *m_pStorage;
	IGraphics *Graphics() { return m_pGraphics; }

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

	BMFont *m_pDefaultFont;

public:
	CTextRender()
	{
		m_pGraphics = 0;

		m_TextR = 1.0f;
		m_TextG = 1.0f;
		m_TextB = 1.0f;
		m_TextA = 1.0f;
		m_TextOutlineR = 0.0f;
		m_TextOutlineG = 0.0f;
		m_TextOutlineB = 0.0f;
		m_TextOutlineA = 0.3f;

		m_pDefaultFont = 0;
	}

	virtual void Init()
	{
		m_pGraphics = Kernel()->RequestInterface<IGraphics>();
		m_pStorage = Kernel()->RequestInterface<IStorage>();
	}


	virtual BMFont *LoadFont(const char *pFilename)
	{
		BMFont* pFont = new BMFont(m_pGraphics, m_pStorage);
		if (!pFont->Init(pFilename))
		{
			delete pFont;
			return 0;
		}
		dbg_msg("textrender", "loaded pFont from '%s'", pFilename);
		return pFont;
	};

	virtual void DestroyFont(BMFont *pFont)
	{
		delete pFont;
	}

	virtual void SetDefaultFont(BMFont *pFont)
	{
		m_pDefaultFont = pFont;
	}


	virtual void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags)
	{
		mem_zero(pCursor, sizeof(*pCursor));
		pCursor->m_FontSize = FontSize;
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
		CTextCursor Cursor;
		SetCursor(&Cursor, 0, 0, Size, 0);
		TextEx(&Cursor, pText, Length);
		return Cursor.m_X;
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
		//BMFont *pFont = pCursor->m_pFont;
		BMFont *pFont = m_pDefaultFont;

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
		float DefaultScale = 0.75f;

		// set length
		if(Length < 0)
			Length = str_length(pText);

		// if we don't want to render, we can just skip the first outline pass
		int i = 1;
		if(pCursor->m_Flags&TEXTFLAG_RENDER)
			i = 0;

		auto& chars = pFont->Chars();

		for(;i < 2; i++)
		{
			const char *pCurrent = (char *)pText;
			const char *pEnd = pCurrent+Length;
			DrawX = CursorX;
			DrawY = CursorY;
			float Scale = DefaultScale;
			LineCount = pCursor->m_LineCount;

			if (i == 0)
				Scale *= 1.3f;

			if(pCursor->m_Flags&TEXTFLAG_RENDER)
			{
				Graphics()->TextureSet(pFont->Texture());
				Graphics()->QuadsBegin();
				if (i == 0)
					Graphics()->SetColor(m_TextOutlineR, m_TextOutlineG, m_TextOutlineB, m_TextOutlineA*m_TextA);
				else
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

					if (chars.count(Character))
					{
						float Advance = chars[Character].xadvance * DefaultScale / 10.f;
						if(pCursor->m_Flags&TEXTFLAG_STOP_AT_END && DrawX+Advance*Size-pCursor->m_StartX > pCursor->m_LineWidth)
						{
							// we hit the end of the line, no more to render or count
							pCurrent = pEnd;
							break;
						}

						if(pCursor->m_Flags&TEXTFLAG_RENDER)
						{
							float offsetX = (i==0) ? DefaultScale*Size/4.f : 0;
							float offsetY = (i==0) ? DefaultScale*Size/4.f : 0;
							DrawX -= offsetX;
							DrawY -= offsetY;
							pFont->RenderChar(Character, DrawX, DrawY, Scale, Size, i==0);
							DrawX += offsetX;
							DrawY += offsetY;
						}

						DrawX += Advance*Size;
						pCursor->m_CharCount++;
					}
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
