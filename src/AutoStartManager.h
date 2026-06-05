// AutoStartManager.h
// 开机自启动管理模块——通过注册表 Run 键管理程序的开机自动启动
// 注册表位置：HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run

#pragma once

class CAutoStartManager
{
public:
    // 检查程序是否已设置为开机自启动
    static BOOL IsAutoStartEnabled();

    // 启用开机自启动
    // strAppPath - 程序完整路径（为空时自动获取当前程序路径）
    static BOOL EnableAutoStart(const CString& strAppPath = _T(""));

    // 禁用开机自启动
    static BOOL DisableAutoStart();

    // 获取注册表中 Run 键的开机启动项值名（用于本程序）
    static CString GetRunKeyName();

private:
    // 注册表 Run 键完整路径
    static const TCHAR* REG_RUN_KEY;
};
