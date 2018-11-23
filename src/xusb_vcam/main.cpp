//////Fanxiushu 2016-10-06
#pragma comment(lib,"gdiplus.lib")

#include <stdio.h>
#include <string>
#include <codecvt>
#include <vector>
#include <iostream>
#include <algorithm>
#include <Windows.h>
#include <Gdiplus.h>

#include "uvc_vcam.h"


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

            Y1 = ((66 * R1 + 129 * G1 + 25 * B1 + 128) >> 8) + 16;
            U1 = (((-38 * R1 - 74 * G1 + 112 * B1 + 128) >> 8) + ((-38 * R2 - 74 * G2 + 112 * B2 + 128) >> 8)) / 2 + 128;
            Y2 = ((66 * R2 + 129 * G2 + 25 * B2 + 128) >> 8) + 16;
            V1 = (((112 * R1 - 94 * G1 - 18 * B1 + 128) >> 8) + ((112 * R2 - 94 * G2 - 18 * B2 + 128) >> 8)) / 2 + 128;

            *(pYUVData + i * width * 2 + j * 4) = max(min(Y1, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 1) = max(min(U1, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 2) = max(min(Y2, 255), 0);
            *(pYUVData + i * width * 2 + j * 4 + 3) = max(min(V1, 255), 0);
        }
    }
}

////////////////////
struct vcam_param
{
    std::wstring image_path;
    bool updated;
    BYTE* yuv_buffer;
    int buffer_size;
};

void init_vcam_param(vcam_param &p) {
    p.image_path = L"";
    p.updated = false;
    p.yuv_buffer = nullptr;
    p.buffer_size = 0;
}

void change_image(vcam_param &p, std::wstring filename)
{
    p.image_path = filename;
    p.updated = true;
    if(p.yuv_buffer)
        delete[] p.yuv_buffer;
    p.yuv_buffer = nullptr;
    p.buffer_size = 0;
}

std::vector<BYTE> load_image(std::wstring filename, UINT target_width, UINT target_height)
{
    Gdiplus::Bitmap* ret = nullptr;
    auto bitmap = Gdiplus::Bitmap::FromFile(filename.c_str());
    if (bitmap->GetLastStatus() != Gdiplus::Ok) {
        printf("failed to load file\n");
        return std::vector<BYTE>();
    }
    if (bitmap->GetWidth() != target_width || bitmap->GetHeight() != target_height)
    {
        // resize is required
        auto ratio = ((double)bitmap->GetWidth()) / ((double)bitmap->GetHeight());
        auto resized_bitmap = new Gdiplus::Bitmap(target_width, target_height, bitmap->GetPixelFormat());
        auto resized_width = target_width;
        auto resized_height = target_height;
        Gdiplus::Graphics graphics(resized_bitmap);
        graphics.DrawImage(bitmap, 0, 0, resized_width, resized_height);
        ret = resized_bitmap;
    }
    else
    {
        ret = bitmap;
    }

    auto data = new Gdiplus::BitmapData;
    auto rect = Gdiplus::Rect(0, 0, target_width, target_height);
    ret->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, data);
    BYTE* pixels = (BYTE*)data->Scan0;

    std::vector<BYTE> rgb24array(pixels, pixels + target_width * target_height * 3);
    bitmap->UnlockBits(data);
    return rgb24array;
}

int frame_callback(frame_t* frame)
{
    vcam_param* p = (vcam_param*)frame->param;
    if (p->updated)
    {
        auto ret = load_image(p->image_path, frame->width, frame->height);
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
    }

    //frame->delay_msec = 33; ///每帧的停留时间， 毫秒
    return 0;
}



int main(int argc, char** argv)
{
    std::wstring_convert< std::codecvt<wchar_t, char, std::mbstate_t> > converter;
    // use std::wstring_convert< codecvt_utf8<wchar_t> > if UTF-8 to wchar_t conversion is required

    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) args.push_back(converter.from_bytes(argv[i]));
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    uvc_vcam_t uvc1;
    uvc1.pid = 0xcc10; uvc1.vid = 0xbb10; //乱造， 不能与真实的重复
    uvc1.manu_fact = "Xx tech inc."; uvc1.product = "Virtual Xx Camera";
    uvc1.frame_callback = frame_callback;

    vcam_param p1;
    init_vcam_param(p1);
    std::wstring img_path = L".\\x64\\Debug\\flcl.png";
    if (argc > 1) {
        img_path = args[1];
    }
    change_image(p1, img_path);
    uvc1.param = &p1;

    void* vcam1 = vcam_create(&uvc1);

    std::wstring path;
    while (std::getline(std::wcin, path))
    {
        std::wcout << path << std::endl;
        p1.image_path = path;
        p1.updated = true;
    }
    vcam_destroy(vcam1);
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}
