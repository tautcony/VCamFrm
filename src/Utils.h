#pragma once
#include <vector>
#include <Windows.h>
#include <Gdiplus.h>
#include "xusb_vcam/uvc_vcam.h"

struct vcam_param
{
    bool updated;
    Gdiplus::Bitmap *bitmap;
    
    BYTE* yuv_buffer;
    size_t buffer_size;
    CRITICAL_SECTION cs;
};

std::vector<BYTE> bitmapToArray(Gdiplus::Bitmap &bitmap, UINT target_width, UINT target_height);
void rgb24_yuy2(void* rgb, void* yuy2, int width, int height);
int frame_callback(frame_t* frame);
void init_vcam_param(vcam_param &p);
void change_image(vcam_param &p, Gdiplus::Bitmap* bitmap);
