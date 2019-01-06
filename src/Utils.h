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
#pragma once
#include <vector>
#include <memory>
#include <Windows.h>
#include <Gdiplus.h>
#include "xusb_vcam/uvc_vcam.h"

struct vcam_param
{
    bool updated;
    std::shared_ptr<Gdiplus::Bitmap> bitmap;

    CRITICAL_SECTION cs{};
    vcam_param();
};

std::vector<unsigned char> bitmap_to_array(const std::shared_ptr<Gdiplus::Bitmap>& bitmap, int target_width, int target_height);
void rgb24_yuy2(void* rgb, void* yuy2, int width, int height);
int frame_callback(frame_t* frame);
void change_image(vcam_param& p, const std::wstring& image_path);
std::shared_ptr<Gdiplus::Bitmap> resize(const std::shared_ptr<Gdiplus::Bitmap>& src, int dst_width, int dst_height);
