#include <stdio.h>
#include <string>
#include <codecvt>
#include <iostream>
#include <algorithm>
#include "Utils.h"


std::vector<BYTE> bitmapToArray(Gdiplus::Bitmap &bitmap, UINT target_width, UINT target_height)
{
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        printf("failed to load file\n");
        return std::vector<BYTE>();
    }
    Gdiplus::Bitmap* ret = nullptr;
    if (bitmap.GetWidth() != target_width || bitmap.GetHeight() != target_height)
    {
        // resize is required
        auto ratio = ((double)bitmap.GetWidth()) / ((double)bitmap.GetHeight());
        auto resized_bitmap = new Gdiplus::Bitmap(target_width, target_height, bitmap.GetPixelFormat());
        auto resized_width = target_width;
        auto resized_height = target_height;
        Gdiplus::Graphics graphics(resized_bitmap);
        graphics.DrawImage(&bitmap, 0, 0, resized_width, resized_height);
        ret = resized_bitmap;
        //todo: delete bitmap
    }
    else
    {
        ret = &bitmap;
    }

    auto data = new Gdiplus::BitmapData;
    auto rect = Gdiplus::Rect(0, 0, target_width, target_height);
    Gdiplus::Status status = ret->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, data);
    BYTE* pixels = (BYTE*)data->Scan0;
    std::vector<BYTE> rgb24array(pixels, pixels + target_width * target_height * 3);
    bitmap.UnlockBits(data);
    return rgb24array;
}

//https://en.wikipedia.org/wiki/YUV
void rgb24_yuy2(void* rgb, void* yuy2, int width, int height)
{
    unsigned char R1, G1, B1, R2, G2, B2, Y1, U1, Y2, V1;
    unsigned char* pRGBData = (unsigned char *)rgb;
    unsigned char* pYUVData = (unsigned char *)yuy2;

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width / 2; ++j)
        {
            B1 = *(pRGBData + i * width * 3 + j * 6);
            G1 = *(pRGBData + i * width * 3 + j * 6 + 1);
            R1 = *(pRGBData + i * width * 3 + j * 6 + 2);
            B2 = *(pRGBData + i * width * 3 + j * 6 + 3);
            G2 = *(pRGBData + i * width * 3 + j * 6 + 4);
            R2 = *(pRGBData + i * width * 3 + j * 6 + 5);

            Y1 = ((77 * R1 + 150 * G1 + 29 * B1 + 128) >> 8);
            Y2 = ((77 * R2 + 150 * G2 + 29 * B2 + 128) >> 8);

            U1 = (((-43 * R1 - 84 * G1 + 127 * B1 + 128) >> 8) +
                  ((-43 * R2 - 84 * G2 + 127 * B2 + 128) >> 8)) / 2 + 128;
            V1 = (((127 * R1 - 106 * G1 - 21 * B1 + 128) >> 8) +
                  ((127 * R2 - 106 * G2 - 21 * B2 + 128) >> 8)) / 2 + 128;

            *(pYUVData + i * width * 2 + j * 4) = max(min(Y1, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 1) = max(min(U1, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 2) = max(min(Y2, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 3) = max(min(V1, 255), 0);
        }
    }
}


int frame_callback(frame_t* frame)
{
    vcam_param* p = (vcam_param*)frame->param;
    if (p->updated)
    {
        // WaitForSingleObject(p->ghMutex, INFINITE);
        EnterCriticalSection(&p->cs);
        auto ret = bitmapToArray(*p->bitmap, frame->width, frame->height);
        LeaveCriticalSection(&p->cs);
        // ReleaseMutex(p->ghMutex);
        if (ret.size() == 0)
        {
            p->updated = false;
            return 1;
        }
        printf("frame_len=%d,w=%d,h=%d\n", frame->length, frame->width, frame->height);
        p->updated = false;
        //1920*1080 yuv buffer size: 4147200
        p->buffer_size = ret.size() * 2 / 3;
        p->yuv_buffer = new BYTE[p->buffer_size];
        rgb24_yuy2(ret.data(), p->yuv_buffer, frame->width, frame->height);
        memcpy(frame->buffer, p->yuv_buffer, p->buffer_size);
        //if (p->hWnd)
        //    RedrawWindow(p->hWnd, NULL, NULL, RDW_INVALIDATE);
    }

    //frame->delay_msec = 33; ///每帧的停留时间， 毫秒
    return 0;
}

void init_vcam_param(vcam_param &p) {
    p.updated = false;
    p.bitmap = nullptr;
    p.yuv_buffer = nullptr;
    p.buffer_size = 0;
    InitializeCriticalSection(&p.cs);
}

void change_image(vcam_param &p, Gdiplus::Bitmap* bitmap)
{
    if (p.bitmap)
        delete p.bitmap;
    p.bitmap = bitmap;
    if (p.yuv_buffer)
        delete[] p.yuv_buffer;
    p.yuv_buffer = nullptr;
    p.buffer_size = 0;
    p.updated = true;
}