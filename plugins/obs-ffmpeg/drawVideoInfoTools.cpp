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

#pragma comment(lib, "gdiplus.lib")

#include "drawVideoInfoTools.h"

std::wstring String2WString(const std::string &s)
{
    int len = MultiByteToWideChar(CP_UTF8, NULL, s.c_str(), -1, NULL, 0);

    wchar_t *dst = new wchar_t[len + 1];
    memset(dst, 0, sizeof(wchar_t) * (len + 1));
    MultiByteToWideChar(CP_UTF8, NULL, s.c_str(), -1, dst, len);

    std::wstring wstr = dst;
    delete[] dst;
    return wstr;
}

std::wstring getTwoBitNum(int a)
{
    std::wstring b;
    if (a >= 10)
    {
        b.append(std::to_wstring(a));
    }
    else if (a <= 0)
    {
        b.append(L"00");
    }
    else
    {
        b.append(L"0");
        b.append(std::to_wstring(a));
    }
    return b;
}

std::wstring transIntToTimeString(int duration)
{
    std::wstring tortalTime;
    int s = duration % 60;
    duration /= 60;
    int m = duration % 60;
    int h = duration / 60;
    tortalTime.append(getTwoBitNum(h));
    tortalTime.append(L":");
    tortalTime.append(getTwoBitNum(m));
    tortalTime.append(L":");
    tortalTime.append(getTwoBitNum(s));
    return tortalTime;
}

std::wstring getTimeRateString(int tempTime, int tortalTime)
{
    tempTime /= 1000;
    tortalTime /= 1000;
    std::wstring ttTime = transIntToTimeString(tortalTime);
    std::wstring tmpTime = transIntToTimeString(tempTime);
    std::wstring time;
    time.append(tmpTime);
    time.append(L"/");
    time.append(ttTime);
    return time;
}

extern "C" {
    gs_texture_t *drawProgress(gs_texture_t *texture, int cx, int cy, int ballRadius, int pts, int duration)
    {
        if (cx <= 0 || cy <= 0)
        {
            return texture;
        }
        Gdiplus::Bitmap bmp(cx, cy);
        Gdiplus::Graphics bkg(&bmp);
        //设置图像渲染的模式
        // 	bkg.SetSmoothingMode(SmoothingModeAntiAlias);//指定消除锯齿的形式
        //设置文本渲染的模式
        bkg.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        //设置像素偏移模式
        bkg.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        //背景线
        Gdiplus::Pen pbot(Gdiplus::Color(48, 48, 48), cy);
        bkg.DrawLine(&pbot, Gdiplus::Point(0, cy / 2), Gdiplus::Point(cx, cy / 2));
        //总进度线
        int lx = 2 * ballRadius;
        int y = cy / 2;
        int rx = cx - 2 * ballRadius;
        Gdiplus::Pen pp(Gdiplus::Color(24, 24, 24), cy / 10);
        bkg.DrawLine(&pp, Gdiplus::Point(lx, y), Gdiplus::Point(rx, y));
        //当前进度线
        float rate = (float)pts / (float)duration;
        int trx = (int)(rate * rx + (1 - rate) * lx);
        Gdiplus::Pen pp1(Gdiplus::Color(13, 120, 270), cy / 5);
        bkg.DrawLine(&pp1, Gdiplus::Point(lx, y), Gdiplus::Point(trx, y));
        //当前标识球
        bkg.FillEllipse(&Gdiplus::SolidBrush(Gdiplus::Color(130, 220, 225)), trx - ballRadius, y - ballRadius, 2 * ballRadius, 2 * ballRadius);
        //数字时间
        std::wstring time = getTimeRateString(pts, duration);
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentNear);
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::Font font(L"微软雅黑", 14 * cy / 20, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        bkg.DrawString(time.c_str(), -1, &font,
            Gdiplus::RectF(0, 0, bmp.GetWidth(), bmp.GetHeight()),
            &sf,
            &Gdiplus::SolidBrush(Gdiplus::Color(120, 210, 210, 210))); //时间

        Gdiplus::BitmapData bitmapData;
        bitmapData.Stride = cx * 4;
        bitmapData.Scan0 = new BYTE[cx * cy * 4];
        Gdiplus::Rect _rect(0, 0, cx, cy);

        bmp.LockBits(&_rect,
            Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite | Gdiplus::ImageLockModeUserInputBuf,
            PixelFormat32bppARGB,
            &bitmapData);
        const uint8_t *data = (BYTE *)bitmapData.Scan0;

        if (NULL == data)
        {
            return texture;
        }
        obs_enter_graphics();
        if (texture && cx == gs_texture_get_width(texture) && cy == gs_texture_get_height(texture))
        {
            gs_texture_set_image(texture, data, cx * 4, false);

        }
        else
        {
            if (texture)
            {
                gs_texture_destroy(texture);
            }
            texture = gs_texture_create(cx, cy, GS_BGRA, 1, &data, GS_DYNAMIC);
        }
        obs_leave_graphics();
        bmp.UnlockBits(&bitmapData);
        delete[] bitmapData.Scan0;
        bitmapData.Scan0 = NULL;
        return texture;
    }

    gs_texture_t *DrawVideoInfo(gs_texture_t *texture, char* filePath, int sourceCx, int sourceCy, int tempTime, int tortalTime, uint32_t color, uint32_t fontSize)
    {
        //获取文件名称
        std::string filePathStr = std::string(filePath);
        size_t begin = filePathStr.find_last_of('/', -1) + 1;
        size_t end = filePathStr.find_last_of('.', -1);
        std::string fileNameStr = filePathStr.substr(begin, end - begin);

        std::wstring wFileName = String2WString(fileNameStr);
        //计算显示时间
        std::wstring showTime = getTimeRateString(tempTime, tortalTime);
        tortalTime = tortalTime == 0 ? 0x7fffffff : tortalTime;
        int ratePer = tempTime * 100 / tortalTime;
        showTime.append(L"(");
        showTime.append(std::to_wstring(ratePer));
        showTime.append(L"%");
        showTime.append(L")");
        showTime.append(L"\n");
        showTime.append(wFileName);
        //bitmap
        int cx = sourceCx;
        int cy = sourceCy;
        Gdiplus::Bitmap bmp(cx, cy);
        Gdiplus::Graphics g(&bmp);

        //设置图像渲染的模式
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);//指定消除锯齿的形式
        //设置文本渲染的模式
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        //设置像素偏移模式
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentNear);
        //画
        Gdiplus::Color mColor(color);
        Gdiplus::Font font(L"微软雅黑", fontSize, 0, Gdiplus::UnitPixel);

        g.DrawString(showTime.c_str(), -1, &font, Gdiplus::PointF(2, 0), &sf, &Gdiplus::SolidBrush(color));
        Gdiplus::BitmapData bitmapData;
        bitmapData.Stride = cx * 4;
        bitmapData.Scan0 = new BYTE[cx * cy * 4];
        Gdiplus::Rect _rect(0, 0, cx, cy);

        bmp.LockBits(&_rect,
            Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite | Gdiplus::ImageLockModeUserInputBuf,
            PixelFormat32bppARGB,
            &bitmapData);
        const uint8_t *data = (BYTE *)bitmapData.Scan0;

        if (NULL == data)
        {
            return texture;
        }
        obs_enter_graphics();
        if (texture && cx == gs_texture_get_width(texture) && cy == gs_texture_get_height(texture))
        {
            gs_texture_set_image(texture, data, cx * 4, false);

        }
        else
        {
            if (texture)
            {
                gs_texture_destroy(texture);
            }
            texture = gs_texture_create(cx, cy, GS_BGRA, 1, &data, GS_DYNAMIC);
        }
        obs_leave_graphics();
        bmp.UnlockBits(&bitmapData);
        delete[] bitmapData.Scan0;
        bitmapData.Scan0 = NULL;
        return texture;
    }

    gs_texture_t *getTextTexture(gs_texture_t *texture, int cx,int cy,char* text)
    {
        if(cx<=0 || cy <= 0 || !text ||!*text){
            return texture;
        }
        std::wstring textWstr = String2WString(std::string(text));

        int clrText = 0xFFFFFFFF;
        int clrBack = 0x00000000;

        int fontSize = cy / 18;
        int width = cx;
        int height = cy;
        int bmpSize = 4 * width * height;

        Gdiplus::Bitmap bitmap(width, height);
        Gdiplus::Graphics g(&bitmap);
        g.Clear(Gdiplus::Color(clrBack));
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::Font font(L"微软雅黑", fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush* brush = new Gdiplus::SolidBrush(Gdiplus::Color(clrText));

        Gdiplus::RectF rect(0, (bitmap.GetHeight() - fontSize) / 2, bitmap.GetWidth(), bitmap.GetHeight());
        g.DrawString(textWstr.c_str(), -1, &font, rect, &sf, brush);
        delete brush;
        Gdiplus::BitmapData bitmapData;
        bitmapData.Stride = cx * 4;
        bitmapData.Scan0 = new BYTE[cx * cy * 4];
        Gdiplus::Rect _rect(0, 0, cx, cy);

        bitmap.LockBits(&_rect,
            Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite | Gdiplus::ImageLockModeUserInputBuf,
            PixelFormat32bppARGB,
            &bitmapData);
        const uint8_t *data = (BYTE *)bitmapData.Scan0;

        if (NULL == data)
        {
            return texture;
        }
        obs_enter_graphics();
        if (texture && cx == gs_texture_get_width(texture) && cy == gs_texture_get_height(texture))
        {
            gs_texture_set_image(texture, data, cx * 4, false);

        }
        else
        {
            if (texture)
            {
                gs_texture_destroy(texture);
            }
            texture = gs_texture_create(cx, cy, GS_BGRA, 1, &data, GS_DYNAMIC);
        }
        obs_leave_graphics();
        bitmap.UnlockBits(&bitmapData);
        delete[] bitmapData.Scan0;
        bitmapData.Scan0 = NULL;
        return texture;
    }

}
