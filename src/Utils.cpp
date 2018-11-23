/* VCamFrm
Copyright (C) 2018  TautCony

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <cstdio>
#include <string>
#include <iostream>
#include <algorithm>
#include "Utils.h"


std::vector<unsigned char> bitmapToArray(Gdiplus::Bitmap &bitmap, int target_width, int target_height)
{
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        printf("failed to load file\n");
        return std::vector<unsigned char>();
    }
    Gdiplus::Bitmap* ret = nullptr;
    if (bitmap.GetWidth() != target_width || bitmap.GetHeight() != target_height)
    {
        // auto ratio = ((double)bitmap.GetWidth()) / ((double)bitmap.GetHeight());
        auto resized_bitmap = new Gdiplus::Bitmap(target_width, target_height, bitmap.GetPixelFormat());
        auto resized_width = target_width;
        auto resized_height = target_height;
        Gdiplus::Graphics graphics(resized_bitmap);
        graphics.DrawImage(&bitmap, 0, 0, resized_width, resized_height);
        ret = resized_bitmap;
    }
    else
    {
        ret = &bitmap;
    }

    auto data = new Gdiplus::BitmapData;
    auto rect = Gdiplus::Rect(0, 0, target_width, target_height);
    auto status = ret->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, data);
    if (status != Gdiplus::Ok)
    {
        printf("Failed to LockBits.\n");
    }
    auto pixels = (unsigned char*)data->Scan0;
    std::vector<unsigned char> rgb24array(pixels, pixels + target_width * target_height * 3);
    bitmap.UnlockBits(data);
    if (&bitmap != ret)
    {
        delete ret;
    }
    return rgb24array;
}

//https://en.wikipedia.org/wiki/YUV
void rgb24_yuy2(void* rgb, void* yuy2, int width, int height)
{
    unsigned char* pRGBData = (unsigned char*)rgb;
    unsigned char* pYUVData = (unsigned char*)yuy2;

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width / 2; ++j)
        {
            auto B1 = *(pRGBData + i * width * 3 + j * 6);
            auto G1 = *(pRGBData + i * width * 3 + j * 6 + 1);
            auto R1 = *(pRGBData + i * width * 3 + j * 6 + 2);
            auto B2 = *(pRGBData + i * width * 3 + j * 6 + 3);
            auto G2 = *(pRGBData + i * width * 3 + j * 6 + 4);
            auto R2 = *(pRGBData + i * width * 3 + j * 6 + 5);

            auto Y1 = ((77 * R1 + 150 * G1 + 29 * B1 + 128) >> 8);
            auto Y2 = ((77 * R2 + 150 * G2 + 29 * B2 + 128) >> 8);
             
            auto U1 = (((-43 * R1 - 84 * G1 + 127 * B1 + 128) >> 8) +
                      ((-43 * R2 - 84 * G2 + 127 * B2 + 128) >> 8)) / 2 + 128;
            auto V1 = (((127 * R1 - 106 * G1 - 21 * B1 + 128) >> 8) +
                      ((127 * R2 - 106 * G2 - 21 * B2 + 128) >> 8)) / 2 + 128;

            *(pYUVData + i * width * 2 + j * 4 + 0) = (unsigned char)max(min(Y1, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 1) = (unsigned char)max(min(U1, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 2) = (unsigned char)max(min(Y2, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 3) = (unsigned char)max(min(V1, 255), 0);
        }
    }
}


int frame_callback(frame_t* frame)
{
    vcam_param* p = (vcam_param*)frame->param;
    if (p->updated)
    {
        EnterCriticalSection(&p->cs);
        auto ret = bitmapToArray(*p->bitmap, frame->width, frame->height);
        LeaveCriticalSection(&p->cs);
        if (ret.size() == 0)
        {
            p->updated = false;
            return 1;
        }
        printf("frame_len=%d,w=%d,h=%d\n", frame->length, frame->width, frame->height);
        p->updated = false;
        // 1920*1080 yuv buffer size: 4147200
        // auto buffer_size = ret.size() * 2 / 3;
        rgb24_yuy2(ret.data(), frame->buffer, frame->width, frame->height);
    }
    //frame->delay_msec = 33; ///每帧的停留时间， 毫秒
    return 0;
}

void init_vcam_param(vcam_param &p) {
    p.updated = false;
    p.bitmap = nullptr;
    // p.yuv_buffer = nullptr;
    InitializeCriticalSection(&p.cs);
}

void change_image(vcam_param &p, Gdiplus::Bitmap* bitmap)
{
    if (p.bitmap)
        delete p.bitmap;
    p.bitmap = bitmap;
    // if (p.yuv_buffer)
    //     delete[] p.yuv_buffer;
    // p.yuv_buffer = nullptr;
    p.updated = true;
}
