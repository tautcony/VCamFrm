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
#include <algorithm>
#include "Utils.h"


std::vector<unsigned char> bitmap_to_array(Gdiplus::Bitmap &bitmap, const int target_width, const int target_height)
{
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        printf("failed to load file\n");
        return std::vector<unsigned char>();
    }
    Gdiplus::Bitmap* ret;
    if (static_cast<int>(bitmap.GetWidth()) != target_width || static_cast<int>(bitmap.GetHeight()) != target_height)
    {
        // auto ratio = ((double)bitmap.GetWidth()) / ((double)bitmap.GetHeight());
        const auto resized_bitmap = new Gdiplus::Bitmap(target_width, target_height, bitmap.GetPixelFormat());
        const auto resized_width = target_width;
        const auto resized_height = target_height;
        Gdiplus::Graphics graphics(resized_bitmap);
        graphics.DrawImage(&bitmap, 0, 0, resized_width, resized_height);
        ret = resized_bitmap;
    }
    else
    {
        ret = &bitmap;
    }
    if (ret == nullptr)
    {
        return std::vector<unsigned char>();
    }

    const auto data = new Gdiplus::BitmapData;
    auto rect = Gdiplus::Rect(0, 0, target_width, target_height);
    const auto status = ret->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, data);
    if (status != Gdiplus::Ok)
    {
        printf("Failed to LockBits.\n");
    }
    const auto pixels = static_cast<unsigned char*>(data->Scan0);
    std::vector<unsigned char> rgb24array(pixels, pixels + target_width * target_height * 3);
    bitmap.UnlockBits(data);
    if (&bitmap != ret)
    {
        delete ret;
    }
    return rgb24array;
}

//https://en.wikipedia.org/wiki/YUV
void rgb24_yuy2(void* rgb, void* yuy2, const int width, const int height)
{
    const auto pRGBData = static_cast<unsigned char*>(rgb);
    const auto pYUVData = static_cast<unsigned char*>(yuy2);

    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width / 2; ++j)
        {
            const auto b1 = *(pRGBData + i * width * 3 + j * 6);
            const auto g1 = *(pRGBData + i * width * 3 + j * 6 + 1);
            const auto r1 = *(pRGBData + i * width * 3 + j * 6 + 2);
            const auto b2 = *(pRGBData + i * width * 3 + j * 6 + 3);
            const auto g2 = *(pRGBData + i * width * 3 + j * 6 + 4);
            const auto r2 = *(pRGBData + i * width * 3 + j * 6 + 5);

            const auto y1 = ((77 * r1 + 150 * g1 + 29 * b1 + 128) >> 8);
            const auto y2 = ((77 * r2 + 150 * g2 + 29 * b2 + 128) >> 8);

            const auto u1 = (((-43 * r1 - 84 * g1 + 127 * b1 + 128) >> 8) +
                      ((-43 * r2 - 84 * g2 + 127 * b2 + 128) >> 8)) / 2 + 128;
            const auto v1 = (((127 * r1 - 106 * g1 - 21 * b1 + 128) >> 8) +
                      ((127 * r2 - 106 * g2 - 21 * b2 + 128) >> 8)) / 2 + 128;

            *(pYUVData + i * width * 2 + j * 4 + 0) = static_cast<unsigned char>(max(min(y1, 255), 0));
            *(pYUVData + i * width * 2 + j * 4 + 1) = static_cast<unsigned char>(max(min(u1, 255), 0));
            *(pYUVData + i * width * 2 + j * 4 + 2) = static_cast<unsigned char>(max(min(y2, 255), 0));
            *(pYUVData + i * width * 2 + j * 4 + 3) = static_cast<unsigned char>(max(min(v1, 255), 0));
        }
    }
}


int frame_callback(frame_t* frame)
{
    auto p = static_cast<vcam_param*>(frame->param);
    if (p->updated)
    {
        EnterCriticalSection(&p->cs);
        const auto resized_img = resize(p->bitmap, frame->width, frame->height);
        auto ret = bitmap_to_array(*resized_img, frame->width, frame->height);
        delete resized_img;
        LeaveCriticalSection(&p->cs);
        if (ret.empty())
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
    delete p.bitmap;
    p.bitmap = bitmap;
    // if (p.yuv_buffer)
    //     delete[] p.yuv_buffer;
    // p.yuv_buffer = nullptr;
    p.updated = true;
}


Gdiplus::Bitmap* resize(Gdiplus::Bitmap *src, const int dst_width, const int dst_height)
{
    const auto ret = new Gdiplus::Bitmap(dst_width, dst_height, src->GetPixelFormat());
    Gdiplus::Graphics g(ret);
    const auto src_aspect = static_cast<double>(src->GetWidth()) / src->GetHeight();
    const auto dst_aspect = static_cast<double>(dst_width) / dst_height;
    auto new_width = dst_width;
    auto new_height = dst_height;
    if (src_aspect < dst_aspect)
    {
        new_width = static_cast<int>(floor(dst_height * src_aspect));
    }
    else
    {
        new_height = static_cast<int>(floor(dst_width / src_aspect));
    }
    const auto delta_width = (dst_width - new_width) / 2;
    const auto delta_height = (dst_height - new_height) / 2;
    g.DrawImage(src, delta_width, delta_height, new_width, new_height);
    return ret;
}
