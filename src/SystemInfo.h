// SystemInfo.h
// 系统信息检测模块——自动获取当前电脑的操作系统版本、Windows SDK 版本和 IDE 版本
// 使用注册表查询和 Windows API 进行检测

#pragma once

class CSystemInfo
{
public:
    // 获取操作系统版本字符串，如 "Windows 10 Education 10.0.18363"
    static CString GetOSVersion();

    // 获取 Windows SDK 版本字符串，如 "10.0.19041.0"
    static CString GetSDKVersion();

    // 获取 IDE 版本字符串（Visual Studio 版本和 VS Code 版本）
    static CString GetIDEVersion();

    // 获取汇总的系统信息字符串（包含上述所有信息 + 架构信息）
    static CString GetAllInfo();

    // 获取系统架构 (x86-based / x64-based)
    static CString GetSystemArchitecture();

private:
    // 通过 RtlGetVersion 获取真实的 Windows 版本号（不受兼容性清单影响）
    static BOOL GetRealWindowsVersion(DWORD& dwMajor, DWORD& dwMinor, DWORD& dwBuild);

    // 从注册表查询 Windows Kits 安装根目录
    static CString GetKitsRootPath();

    // 从注册表查询 Visual Studio 安装路径
    static CString GetVSInstallPath();

    // 读取文件中的版本信息（用于 IDE 版本文件）
    static CString ReadVersionFile(const CString& filePath);
};
