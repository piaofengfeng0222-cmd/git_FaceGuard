// AutoStartManager.cpp
// 开机自启动管理模块实现
// 通过注册表 HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run 键
// 写入程序路径来实现开机自动启动

#include "stdafx.h"
#include "AutoStartManager.h"

// 注册表 Run 键路径（当前用户）
const TCHAR* CAutoStartManager::REG_RUN_KEY =
    _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");

// ============================================================================
// 获取注册表中本程序启动项的值名
// 使用程序名称作为值名，如 "FaceGuard"
// ============================================================================
CString CAutoStartManager::GetRunKeyName()
{
    return CString(FACEGUARD_APP_TITLE);
}

// ============================================================================
// 检查程序是否已设置为开机自启动
// 在 Run 键中查找本程序的值名
// ============================================================================
BOOL CAutoStartManager::IsAutoStartEnabled()
{
    HKEY hKey = nullptr;
    LONG lResult = ::RegOpenKeyEx(HKEY_CURRENT_USER, REG_RUN_KEY,
                                  0, KEY_READ, &hKey);

    if (lResult != ERROR_SUCCESS)
    {
        return FALSE;
    }

    // 查询本程序的值名是否存在
    CString strKeyName = GetRunKeyName();
    DWORD dwType = REG_SZ;
    TCHAR szValue[MAX_PATH] = { 0 };
    DWORD dwSize = sizeof(szValue);

    lResult = ::RegQueryValueEx(hKey, strKeyName, nullptr, &dwType,
                                (LPBYTE)szValue, &dwSize);
    ::RegCloseKey(hKey);

    return (lResult == ERROR_SUCCESS);
}

// ============================================================================
// 启用开机自启动
// 在 Run 键下写入程序完整路径
// strAppPath - 程序完整路径，为空时自动获取当前运行程序的路径
// ============================================================================
BOOL CAutoStartManager::EnableAutoStart(const CString& strAppPath)
{
    CString strPath = strAppPath;

    // 如果未指定路径，使用当前程序路径
    if (strPath.IsEmpty())
    {
        TCHAR szModulePath[MAX_PATH] = { 0 };
        ::GetModuleFileName(nullptr, szModulePath, MAX_PATH);
        strPath = szModulePath;
    }

    // 打开或创建 Run 键
    HKEY hKey = nullptr;
    LONG lResult = ::RegCreateKeyEx(HKEY_CURRENT_USER, REG_RUN_KEY,
                                    0, nullptr, REG_OPTION_NON_VOLATILE,
                                    KEY_WRITE, nullptr, &hKey, nullptr);

    if (lResult != ERROR_SUCCESS)
    {
        return FALSE;
    }

    // 写入程序路径
    CString strKeyName = GetRunKeyName();
    lResult = ::RegSetValueEx(hKey, strKeyName, 0, REG_SZ,
                              (const BYTE*)(LPCTSTR)strPath,
                              (strPath.GetLength() + 1) * sizeof(TCHAR));

    ::RegCloseKey(hKey);

    return (lResult == ERROR_SUCCESS);
}

// ============================================================================
// 禁用开机自启动
// 从 Run 键中删除本程序的启动项
// ============================================================================
BOOL CAutoStartManager::DisableAutoStart()
{
    HKEY hKey = nullptr;
    LONG lResult = ::RegOpenKeyEx(HKEY_CURRENT_USER, REG_RUN_KEY,
                                  0, KEY_SET_VALUE, &hKey);

    if (lResult != ERROR_SUCCESS)
    {
        return FALSE;  // 注册表项不存在，视为已禁用
    }

    CString strKeyName = GetRunKeyName();
    lResult = ::RegDeleteValue(hKey, strKeyName);
    ::RegCloseKey(hKey);

    return (lResult == ERROR_SUCCESS || lResult == ERROR_FILE_NOT_FOUND);
}
