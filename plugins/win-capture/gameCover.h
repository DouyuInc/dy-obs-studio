#pragma once
#ifdef __GAMECOVER_H


extern "C" {
#endif

#include<graphics/graphics.h>

    gs_texture_t *buildGamePixel(gs_texture_t *texture, uint32_t m_bmpWidth, uint32_t m_bmpHeight,
        const wchar_t *gameName, uint32_t gameNameSize, uint32_t gameNameColor,
        const wchar_t *tips, uint32_t tipsSize, uint32_t tipsColor,
        const wchar_t *des, uint32_t desSize, uint32_t desColor);

    gs_texture_t *buildGameImage(gs_texture_t *texture, uint32_t m_bmpWidth, uint32_t m_bmpHeight
        , const wchar_t *cover);

	bool detectTextureContains(gs_texture_t *texture);
	bool isTextureValid(gs_texture_t *OriTexture, bool isDetectColor);


#ifdef __GAMECOVER_H
}
#endif


