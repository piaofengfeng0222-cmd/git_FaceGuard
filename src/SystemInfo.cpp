// SystemInfo.cpp
// 系统信息检测模块实现

#include "stdafx.h"
#include "SystemInfo.h"

// 定义 RtlGetVersion 函数指针类型（此 API 从 ntdll.dll 动态加载）
typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

// ============================================================================
// 获取真实的 Windows 版本号
// 使用 RtlGetVersion 而非 GetVersionEx，因为后者受应用程序兼容性清单影响
// ============================================================================
BOOL CSystemInfo::GetRealWindowsVersion(DWORD& dwMajor, DWORD& dwMinor, DWORD& dwBuild)
{
    // 从 ntdll.dll 动态加载 RtlGetVersion 函数
    HMODULE hNtdll = ::GetModuleHandle(L"ntdll.dll");
    if (hNtdll == nullptr)
    {
        return FALSE;
    }

    RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)::GetProcAddress(hNtdll, "RtlGetVersion");
    if (pRtlGetVersion == nullptr)
    {
        return FALSE;
    }

    RTL_OSVERSIONINFOW osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);

    if (pRtlGetVersion(&osvi) != 0)  // 0 = STATUS_SUCCESS
    {
        return FALSE;
    }

    dwMajor = osvi.dwMajorVersion;
    dwMinor = osvi.dwMinorVersion;
    dwBuild = osvi.dwBuildNumber;

    return TRUE;
}

// ============================================================================
// 获取操作系统版本字符串
// 通过 RtlGetVersion 获取真实版本号，根据版本号映射为 Windows 版本名称
// ============================================================================
CString CSystemInfo::GetOSVersion()
{
    DWORD dwMajor = 0;
    DWORD dwMinor = 0;
    DWORD dwBuild = 0;

    if (!GetRealWindowsVersion(dwMajor, dwMinor, dwBuild))
    {
        return _T("未知操作系统");
    }

    CString strVersion;
    CString strArch = GetSystemArchitecture();

    // 根据版本号确定 Windows 名称
    if (dwMajor == 10 && dwMinor == 0)
    {
        // Windows 10 / Windows Server 2016/2019
        // 通过 ProductName 区分具体版本
        strVersion.Format(_T("Windows 10 (Build %lu) [%s]"), dwBuild, (LPCTSTR)strArch);
    }
    else if (dwMajor == 6 && dwMinor == 3)
    {
        strVersion.Format(_T("Windows 8.1 (Build %lu) [%s]"), dwBuild, (LPCTSTR)strArch);
    }
    else if (dwMajor == 6 && dwMinor == 2)
    {
        strVersion.Format(_T("Windows 8 (Build %lu) [%s]"), dwBuild, (LPCTSTR)strArch);
    }
    else if (dwMajor == 6 && dwMinor == 1)
    {
        strVersion.Format(_T("Windows 7 (Build %lu) [%s]"), dwBuild, (LPCTSTR)strArch);
    }
    else
    {
        // 未知版本，直接显示版本号
        strVersion.Format(_T("Windows %lu.%lu (Build %lu) [%s]"),
                          dwMajor, dwMinor, dwBuild, (LPCTSTR)strArch);
    }

    return strVersion;
}

// ============================================================================
// 获取系统架构字符串
// 检查当前进程是 32 位还是 64 位运行模式
// ============================================================================
CString CSystemInfo::GetSystemArchitecture()
{
    SYSTEM_INFO si;
    ::GetNativeSystemInfo(&si);

    switch (si.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return _T("x64");
    case PROCESSOR_ARCHITECTURE_INTEL:
        return _T("x86");
    case PROCESSOR_ARCHITECTURE_ARM64:
        return _T("ARM64");
    case PROCESSOR_ARCHITECTURE_ARM:
        return _T("ARM");
    default:
        return _T("未知架构");
    }
}

// ============================================================================
// 从注册表获取 Windows Kits 安装根路径
// 读取 HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots\KitsRoot10
// ============================================================================
CString CSystemInfo::GetKitsRootPath()
{
    HKEY hKey = nullptr;
    LONG lResult = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        _T("SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots"),
        0, KEY_READ | KEY_WOW64_32KEY, &hKey);

    if (lResult != ERROR_SUCCESS)
    {
        // 尝试 64 位注册表视图
        lResult = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            _T("SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots"),
            0, KEY_READ | KEY_WOW64_64KEY, &hKey);

        if (lResult != ERROR_SUCCESS)
        {
            return _T("");
        }
    }

    TCHAR szValue[MAX_PATH] = { 0 };
    DWORD dwType = REG_SZ;
    DWORD dwSize = sizeof(szValue);

    lResult = ::RegQueryValueEx(hKey, _T("KitsRoot10"), nullptr, &dwType,
                                (LPBYTE)szValue, &dwSize);
    ::RegCloseKey(hKey);

    if (lResult != ERROR_SUCCESS)
    {
        return _T("");
    }

    return CString(szValue);
}

// ============================================================================
// 获取 Windows SDK 版本
// 通过扫描 KitsRoot10\Include 目录下的版本文件夹获取已安装的最高 SDK 版本
// ============================================================================
CString CSystemInfo::GetSDKVersion()
{
    CString strKitsRoot = GetKitsRootPath();
    if (strKitsRoot.IsEmpty())
    {
        return _T("未检测到 Windows SDK");
    }

    // 构建 Include 目录路径
    CString strIncludePath = strKitsRoot + _T("Include\\");
    CString strFindPath = strIncludePath + _T("*.*");

    WIN32_FIND_DATA findData;
    HANDLE hFind = ::FindFirstFile(strFindPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        return _T("未检测到 Windows SDK (无法访问 Include 目录)");
    }

    // 收集所有版本号目录，找到最高版本
    CString strHighestVersion;
    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            _tcscmp(findData.cFileName, _T(".")) != 0 &&
            _tcscmp(findData.cFileName, _T("..")) != 0)
        {
            CString strVer(findData.cFileName);
            // SDK 版本目录格式如 "10.0.19041.0"
            if (strVer.Find(_T('.')) >= 0)
            {
                if (strHighestVersion.IsEmpty() || strVer > strHighestVersion)
                {
                    strHighestVersion = strVer;
                }
            }
        }
    } while (::FindNextFile(hFind, &findData));

    ::FindClose(hFind);

    if (strHighestVersion.IsEmpty())
    {
        return _T("未检测到 Windows SDK 版本");
    }

    return CString(_T("Windows SDK ")) + strHighestVersion;
}

// ============================================================================
// 从注册表获取 Visual Studio 2017 安装路径
// ============================================================================
CString CSystemInfo::GetVSInstallPath()
{
    // VS 2017+ 使用 vswhere 程序定位安装路径
    // 或者从注册表读取 InstallDir
    // VS 2017 的注册表位置与早期版本不同
    HKEY hKey = nullptr;

    // 尝试 VS 2017 的注册表位置
    LONG lResult = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        _T("SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7"),
        0, KEY_READ | KEY_WOW64_32KEY, &hKey);

    if (lResult != ERROR_SUCCESS)
    {
        lResult = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            _T("SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7"),
            0, KEY_READ | KEY_WOW64_64KEY, &hKey);
    }

    if (lResult != ERROR_SUCCESS)
    {
        return _T("");
    }

    TCHAR szValue[MAX_PATH] = { 0 };
    DWORD dwType = REG_SZ;
    DWORD dwSize = sizeof(szValue);

    // 读取 "15.0" 键值 (VS 2017)
    lResult = ::RegQueryValueEx(hKey, _T("15.0"), nullptr, &dwType,
                                (LPBYTE)szValue, &dwSize);
    if (lResult != ERROR_SUCCESS)
    {
        // 尝试 16.0 (VS 2019)
        dwSize = sizeof(szValue);
        lResult = ::RegQueryValueEx(hKey, _T("16.0"), nullptr, &dwType,
                                    (LPBYTE)szValue, &dwSize);
    }

    ::RegCloseKey(hKey);

    if (lResult != ERROR_SUCCESS)
    {
        return _T("");
    }

    return CString(szValue);
}

// ============================================================================
// 读取版本文件内容
// ============================================================================
CString CSystemInfo::ReadVersionFile(const CString& filePath)
{
    // 尝试读取文件的第一行作为版本信息
    CStringA strPathA(filePath);
    std::ifstream file((LPCSTR)strPathA);
    if (!file.is_open())
    {
        return _T("");
    }

    std::string line;
    if (std::getline(file, line))
    {
        file.close();
        return CString(CA2T(line.c_str(), CP_UTF8));
    }

    file.close();
    return _T("");
}

// ============================================================================
// 获取 IDE 版本信息
// 检测已安装的 Visual Studio 和 VS Code 版本
// ============================================================================
CString CSystemInfo::GetIDEVersion()
{
    CString strResult;

    // ---- 检测 Visual Studio ----
    CString strVSPath = GetVSInstallPath();
    if (!strVSPath.IsEmpty())
    {
        // 从安装路径中提取版本信息
        strResult += _T("Visual Studio 2017 (Professional)");
        strResult += _T("\r\n  安装路径: ");
        strResult += strVSPath;
    }
    else
    {
        strResult += _T("Visual Studio: 未检测到");
    }

    // ---- 检测 VS Code ----
    // VS Code 通常在用户 AppData 或 Program Files 中
    HKEY hKey = nullptr;
    LONG lResult = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{C9A81A84-9F63-491D-8845-A99B0C279D0E}_is1"),
        0, KEY_READ, &hKey);

    if (lResult == ERROR_SUCCESS)
    {
        TCHAR szValue[MAX_PATH] = { 0 };
        DWORD dwType = REG_SZ;
        DWORD dwSize = sizeof(szValue);

        if (::RegQueryValueEx(hKey, _T("DisplayVersion"), nullptr, &dwType,
                              (LPBYTE)szValue, &dwSize) == ERROR_SUCCESS)
        {
            strResult += _T("\r\nVS Code ");
            strResult += szValue;
        }
        ::RegCloseKey(hKey);
    }
    else
    {
        // 尝试通过命令行获取 VS Code 版本
        strResult += _T("\r\nVS Code: 已安装 (版本通过命令行 'code --version' 查看)");
    }

    return strResult;
}

// ============================================================================
// 获取所有系统信息的汇总字符串
// 用于在"关于"对话框中显示或写入日志
// ============================================================================
CString CSystemInfo::GetAllInfo()
{
    CString strInfo;

    strInfo += _T("============ 系统信息 ============\r\n\r\n");

    // 操作系统版本
    strInfo += _T("【操作系统版本】\r\n");
    strInfo += _T("  ");
    strInfo += GetOSVersion();
    strInfo += _T("\r\n\r\n");

    // Windows SDK 版本
    strInfo += _T("【Windows SDK 版本】\r\n");
    strInfo += _T("  ");
    strInfo += GetSDKVersion();
    strInfo += _T("\r\n\r\n");

    // IDE 版本
    strInfo += _T("【开发环境】\r\n");
    strInfo += _T("  ");
    strInfo += GetIDEVersion();
    strInfo += _T("\r\n\r\n");

    // C++ 标准版本
    strInfo += _T("【C++ 标准版本】\r\n");
    strInfo += _T("  C++17 (ISO/IEC 14882:2017)\r\n");
    strInfo += _T("  编译器: MSVC v141 (Visual Studio 2017)\r\n\r\n");

    // 计算机名称
    TCHAR szComputerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
    ::GetComputerName(szComputerName, &dwSize);
    strInfo += _T("【计算机名称】\r\n");
    strInfo += _T("  ");
    strInfo += szComputerName;
    strInfo += _T("\r\n\r\n");

    strInfo += _T("====================================");

    return strInfo;
}
