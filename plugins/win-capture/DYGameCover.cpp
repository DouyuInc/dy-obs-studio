#include <stdlib.h>

#include <graphics/math-defs.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <obs-module.h>
#include <sys/stat.h>
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <memory>
#include <locale>
#include "../Deps/ImageFilter/imagefilter.h"

#pragma comment(lib, "gdiplus.lib")

#include "gameCover.h"

extern "C" {
    gs_texture_t *drawTexture(gs_texture_t *texture, Gdiplus::Bitmap& bitmap) {
        int bmpWidth = bitmap.GetWidth();
        int bmpHeight = bitmap.GetHeight();

        Gdiplus::BitmapData bitmapData;
        bitmapData.Stride = bmpWidth * 4;
        bitmapData.Scan0 = new BYTE[bmpWidth * bmpHeight * 4];
        Gdiplus::Rect _rect(0, 0, bmpWidth, bmpHeight);

        bitmap.LockBits(&_rect,
            Gdiplus::ImageLockModeRead |
            Gdiplus::ImageLockModeWrite |
            Gdiplus::ImageLockModeUserInputBuf,
            PixelFormat32bppARGB, &bitmapData);

        const uint8_t *data = (BYTE *)bitmapData.Scan0;

        if (NULL == data) {
            return texture;
        }
        obs_enter_graphics();
        if (texture && bmpWidth == gs_texture_get_width(texture) &&
            bmpHeight == gs_texture_get_height(texture)) {
            gs_texture_set_image(texture, data, bmpWidth * 4, false);

        }
        else {
            if (texture) {
                gs_texture_destroy(texture);
            }
            texture = gs_texture_create(bmpWidth, bmpHeight, GS_BGRA, 1,
                &data, GS_DYNAMIC);
        }
        obs_leave_graphics();

        bitmap.UnlockBits(&bitmapData);
        delete[] bitmapData.Scan0;

        return texture;
    }

	gs_texture_t *buildGamePixel(gs_texture_t *texture, uint32_t m_bmpWidth, uint32_t m_bmpHeight,
		const wchar_t *gameName, uint32_t gameNameSize, uint32_t gameNameColor,
		const wchar_t *tips, uint32_t tipsSize, uint32_t tipsColor,
		const wchar_t *des, uint32_t desSize, uint32_t desColor)
	{
		if (m_bmpWidth <= 0 || m_bmpHeight <= 0)
		{
			return texture;
		}
		if (m_bmpWidth*m_bmpHeight <= 0 || m_bmpWidth * m_bmpHeight>= 4096*2160) {
			return texture;
		}
		int maxLen = m_bmpWidth / (gameNameSize * 2);
        if (maxLen <= 0)
        {
            return texture;
        }
        std::wstring testName(gameName);
        int len = wcslen(gameName);
        if (len > maxLen)
        {
            testName[maxLen - 1] = testName[len - 1];
            for (int i = maxLen - 2; i >= 0 && i >= maxLen - 4; i--)
            {
                testName[i] = L'.';
            }
            for (size_t i = maxLen; i < testName.length(); i++)
            {
                testName[i] = L'\0';
            }
        }

        //字体设置
        //const int gameNameSize = (int)gameNameData->font;
		const int tipsHOffset = 20;
		const int gameNameHOffset = -gameNameSize- tipsHOffset;
        //const int tipsSize = (int)tipsData->font;
        
		const int _tipsHOffset = 2 * tipsSize + tipsHOffset;

        Gdiplus::Bitmap bitmap(m_bmpWidth, m_bmpHeight);

        Gdiplus::Graphics g(&bitmap);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::Font font(L"微软雅黑", (Gdiplus::REAL)gameNameSize,
            Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 238, 238, 238));

        Gdiplus::RectF rect((Gdiplus::REAL)0,
            (Gdiplus::REAL)bitmap.GetHeight() / 2 -
			font.GetHeight(&g),
            (Gdiplus::REAL)bitmap.GetWidth(),
            (Gdiplus::REAL)bitmap.GetHeight());
        g.DrawString(testName.c_str(), -1, &font, rect, &sf, &brush);


        Gdiplus::Font font2(L"微软雅黑", (Gdiplus::REAL)tipsSize,
            Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        brush.SetColor(Gdiplus::Color(255, 255, 119, 0));
        rect = Gdiplus::RectF((Gdiplus::REAL)0,
            (Gdiplus::REAL)bitmap.GetHeight() / 2 +
            tipsHOffset,
            (Gdiplus::REAL)bitmap.GetWidth(),
            (Gdiplus::REAL)bitmap.GetHeight());
        g.DrawString(tips, -1, &font2, rect, &sf, &brush);

        brush.SetColor(Gdiplus::Color(255, 238, 238, 238));
        rect = Gdiplus::RectF((Gdiplus::REAL)0,
            (Gdiplus::REAL)bitmap.GetHeight() / 2 +
			2*tipsHOffset + 2*font2.GetHeight(&g),
            (Gdiplus::REAL)bitmap.GetWidth(),
            (Gdiplus::REAL)bitmap.GetHeight());
        g.DrawString(des, -1, &font2,
            rect, &sf, &brush);

        return drawTexture(texture, bitmap);
    }

    gs_texture_t *buildGameImage(gs_texture_t *texture, uint32_t m_bmpWidth, uint32_t m_bmpHeight
        , const wchar_t *cover) {

        // 画布指定的大小异常
        if (m_bmpWidth <= 0 || m_bmpHeight <= 0
            || m_bmpWidth * m_bmpHeight >= 4096 * 2160)
        {
            return texture;
        }

        // 路径不对
        if (wcslen(cover) <= 0)
            return texture;

        Gdiplus::Bitmap bitmap(cover);

        return drawTexture(texture, bitmap);
    }

	bool detectTextureContains(gs_texture_t *texture) {
		obs_enter_graphics();
		if (!texture || gs_texture_get_width(texture) <= 0 || gs_texture_get_height(texture) <= 0) {
			obs_leave_graphics();
			return false;
		}
		int widthSee = gs_texture_get_width(texture);
		int heightSee = gs_texture_get_height(texture);

		bool zero = true;
		/*uint8_t *data;
		uint32_t linesieze;
		bool bMapRet=gs_texture_map(texture, &data, &linesieze);
		void *hdc = gs_texture_get_dc(texture);

		uint64_t *color = (uint64_t*)(data + linesieze);
		gs_texture_unmap(texture);*/
		uint32_t width = gs_texture_get_width(texture);
		uint32_t height = gs_texture_get_height(texture);

		VideoFilterResult input;
		input.width = width;
		input.height = height;

		uint32_t sharedHandler = gs_texture_get_shared_handle(texture);
		void *hdc = gs_texture_get_dc(texture);
		if (hdc != NULL)
		{
			input.type = BT_HDC;
			input.buffer.hdc = hdc;
		}
		else if (sharedHandler == GS_INVALID_HANDLE)
		{
			//void *hdc = gs_texture_get_dc(texture);
			input.type = BT_SHARDHANDLE;
			input.buffer.sharedTexture.sharedHandle = (void*)sharedHandler;
		}
		else
		{
			return false;
		}

		
		void* filter = ImageFilterInit(NULL, DT_D3D11);
		
		//void *hdc = gs_texture_get_dc(texture);
		
		//input.buffer.sharedTexture.sharedHandle = (void*)sharedHandler;
		VideoFilterResult output;
		output.type = BT_RAWPIXELS;
		output.width = width;
		output.height = height;
		bool bRet = ImageFilterTransformBufferType(filter, &input, &output);
		if (!bRet)
		{
			ImageFilterUninit(filter);
			obs_leave_graphics();
			return false;
		}
		int num = 100;
		//bool zero = true;
		uint8_t* data = output.buffer.pixelBuffer.buffer;
		int dataSize = output.buffer.pixelBuffer.size;
		int randomMax = dataSize * 8 / 64;
		randomMax = randomMax > 0 ? randomMax : 1;

		for (int i = 0; i < num; i++)
		{
			uint64_t *color;
			color = (uint64_t*)(data + (std::rand() % randomMax));
			if (*color != 0)
			{
				zero = false;
				break;
			}
		}
		ImageFilterFree(filter, &output);
		ImageFilterUninit(filter);
		obs_leave_graphics();
		return !zero;
	}

	bool isTextureValid(gs_texture_t *OriTexture, bool isDetectColor)
	{
		if (!OriTexture)
		{
			return false;
		}
		obs_enter_graphics();
		uint32_t width = gs_texture_get_width(OriTexture);
		uint32_t height = gs_texture_get_height(OriTexture);
		if (width <= 0 || height <= 0)
		{
			obs_leave_graphics();
			return false;
		}
		gs_color_format format = gs_texture_get_color_format(OriTexture);
		if (!format)
		{
			obs_leave_graphics();
			return false;
		}
		if (!isDetectColor)
		{
			obs_leave_graphics();
			return true;
		}
		uint8_t *mapData;
		uint32_t lineSize;
		gs_stagesurf_t* stagesurf = gs_stagesurface_create(width, height, format);
		if (!stagesurf)
		{
			obs_leave_graphics();
			return false;
		}
		gs_stage_texture(stagesurf, OriTexture);
		bool mapSuccess=gs_stagesurface_map(stagesurf, &mapData, &lineSize);
		if (!mapSuccess)
		{
			gs_stagesurface_destroy(stagesurf);
			obs_leave_graphics();
			return false;
		}
		uint32_t dataSize = lineSize * height;
		/*uint8_t* data = (uint8_t*)malloc(dataSize);
		memcpy(data, mapData, dataSize);*/

		uint32_t cxOffset = lineSize / 8;
		uint32_t cxNum = lineSize * 8 / 64;
		uint32_t cyNum = height;
		bool beAllzero = true;
		uint32_t pixelOffsetx = 16;
		uint32_t pixelOffsety = 16;
		for (uint32_t i=0;i< cxNum;i+= pixelOffsetx)
		{
			for (uint32_t j=0;j<cyNum;j+= pixelOffsety)
			{
				uint64_t *color = ((uint64_t*)mapData) + i+j* cxOffset;
				if (*color != 0)
				{
					beAllzero = false;
					break;
				}
			}
		}

		gs_stagesurface_unmap(stagesurf);
		gs_stagesurface_destroy(stagesurf);
		obs_leave_graphics();
		return !beAllzero;
	}
}
