﻿# VCamFrm

本程序为基于[该repo](https://github.com/fanxiushu/xusb_vcam)开发的虚拟摄像头应用，提供简单的界面，实现了载入指定图像作为虚拟摄像头内容的功能。

原代码的主要问题为其选用的rgb24 -> yuv2的转换矩阵为limited range即Y ∈ [16, 235] and U/V ∈ [16, 240]，这样一来显示的颜色与预期的就有所差别，故更换了其相关的系数。

需要根据driver文件下的相关说明安装驱动后才可正常使用。
