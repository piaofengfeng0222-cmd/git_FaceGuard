// targetver.h
// 定义程序所面向的 Windows 平台版本
// 使用 Windows 10 SDK 10.0.19041.0

#pragma once

// 包含 SDKDDKVer.h 将定义可用的最高版本的 Windows 平台
// 如果要为以前的 Windows 平台生成应用程序，请包含 WinSDKVer.h，并将
// WIN32_WINNT 宏设置为要支持的平台，然后再包含 SDKDDKVer.h

#include <SDKDDKVer.h>

// 定义目标 Windows 版本为 Windows 10
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // Windows 10
#endif

// 最小支持版本：Windows 7
#ifndef WINVER
#define WINVER 0x0601
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0A00
#endif
