// TrayManager.cpp
// 系统托盘管理模块实现

#include "stdafx.h"
#include "TrayManager.h"
#include "resource.h"

// ============================================================================
// 单例实现
// ============================================================================
CTrayManager& CTrayManager::GetInstance()
{
    static CTrayManager instance;
    return instance;
}

CTrayManager::CTrayManager()
    : m_bCreated(FALSE)
    , m_pParentWnd(nullptr)
    , m_uCallbackMsg(0)
{
    // 初始化 NOTIFYICONDATA 结构
    ::ZeroMemory(&m_nid, sizeof(NOTIFYICONDATA));
    m_nid.cbSize = sizeof(NOTIFYICONDATA);
}

CTrayManager::~CTrayManager()
{
    Destroy();
}

// ============================================================================
// 创建托盘图标
// 1. 加载程序图标
// 2. 填充 NOTIFYICONDATA 结构
// 3. 调用 Shell_NotifyIcon 添加托盘图标
// ============================================================================
BOOL CTrayManager::Create(CWnd* pParentWnd, UINT uCallbackMessage)
{
    if (m_bCreated)
    {
        return TRUE;  // 已创建
    }

    m_pParentWnd = pParentWnd;
    m_uCallbackMsg = uCallbackMessage;

    // 加载图标资源
    HICON hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    if (hIcon == nullptr)
    {
        // 如果资源加载失败，使用默认应用程序图标
        // 注意：不使用 IDI_APPLICATION 宏，因为 resource.h 将其重定义为纯整数 32512，
        //       破坏了 MAKEINTRESOURCE() 包装，导致 Unicode 下类型不匹配。
        hIcon = AfxGetApp()->LoadStandardIcon(MAKEINTRESOURCE(32512));
    }

    // 填充托盘数据结构
    m_nid.cbSize = sizeof(NOTIFYICONDATA);
    m_nid.hWnd = pParentWnd->GetSafeHwnd();
    m_nid.uID = 1;  // 托盘图标 ID
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = uCallbackMessage;
    m_nid.hIcon = hIcon;

    // 设置默认提示文字
    _tcscpy_s(m_nid.szTip, _countof(m_nid.szTip), FACEGUARD_APP_TITLE);

    // 添加到系统托盘
    if (!::Shell_NotifyIcon(NIM_ADD, &m_nid))
    {
        return FALSE;
    }

    // 设置版本（Windows 2000+ 支持）
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    ::Shell_NotifyIcon(NIM_SETVERSION, &m_nid);

    m_bCreated = TRUE;
    return TRUE;
}

// ============================================================================
// 销毁托盘图标
// ============================================================================
void CTrayManager::Destroy()
{
    if (m_bCreated)
    {
        ::Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_bCreated = FALSE;
    }
}

// ============================================================================
// 显示气泡提示通知
// strTitle - 提示标题
// strText  - 提示正文内容
// dwIcon   - 图标类型：
//   NIIF_INFO    (信息图标)
//   NIIF_WARNING (警告图标)
//   NIIF_ERROR   (错误图标)
// ============================================================================
void CTrayManager::ShowBalloonTip(const CString& strTitle, const CString& strText,
                                   DWORD dwIcon)
{
    if (!m_bCreated)
    {
        return;
    }

    m_nid.uFlags = NIF_INFO;
    m_nid.dwInfoFlags = dwIcon;

    // 复制标题和正文（注意截断过长的文本）
    _tcsncpy_s(m_nid.szInfoTitle, _countof(m_nid.szInfoTitle),
               strTitle, _TRUNCATE);
    _tcsncpy_s(m_nid.szInfo, _countof(m_nid.szInfo),
               strText, _TRUNCATE);

    ::Shell_NotifyIcon(NIM_MODIFY, &m_nid);
}

// ============================================================================
// 更新鼠标悬停提示文字
// ============================================================================
void CTrayManager::UpdateTooltip(const CString& strText)
{
    if (!m_bCreated)
    {
        return;
    }

    m_nid.uFlags = NIF_TIP;
    _tcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), strText, _TRUNCATE);

    ::Shell_NotifyIcon(NIM_MODIFY, &m_nid);
}

// ============================================================================
// 设置托盘图标的可见性
// bVisible - TRUE 显示托盘图标，FALSE 隐藏
// ============================================================================
void CTrayManager::SetVisible(BOOL bVisible)
{
    if (!m_bCreated)
    {
        return;
    }

    m_nid.uFlags = NIF_STATE;
    m_nid.dwState = bVisible ? 0 : NIS_HIDDEN;
    m_nid.dwStateMask = NIS_HIDDEN;

    ::Shell_NotifyIcon(NIM_MODIFY, &m_nid);
}

// ============================================================================
// 显示托盘右键菜单
// 菜单项：显示主窗口 | 立即检查 | 查看非法记录 | 关于 | 退出
// ============================================================================
void CTrayManager::ShowContextMenu()
{
    if (m_pParentWnd == nullptr)
    {
        return;
    }

    // 创建弹出菜单
    CMenu menu;
    menu.CreatePopupMenu();

    // 添加菜单项
    menu.AppendMenu(MF_STRING, IDM_TRAY_SHOW, _T("显示主窗口"));
    menu.AppendMenu(MF_STRING, IDM_TRAY_CHECK, _T("立即检查"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_TRAY_VIEW_LOG, _T("查看非法记录"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_TRAY_ABOUT, _T("关于 FaceGuard"));
    menu.AppendMenu(MF_STRING, IDM_TRAY_EXIT, _T("退出程序"));

    // 设置默认菜单项为粗体
    ::SetMenuDefaultItem(menu.GetSafeHmenu(), IDM_TRAY_SHOW, FALSE);

    // 获取鼠标当前位置
    CPoint ptCursor;
    ::GetCursorPos(&ptCursor);

    // 确保菜单正确显示在最前面
    ::SetForegroundWindow(m_pParentWnd->GetSafeHwnd());

    // 显示弹出菜单
    menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                        ptCursor.x, ptCursor.y, m_pParentWnd, nullptr);

    // 恢复前台窗口
    ::PostMessage(m_pParentWnd->GetSafeHwnd(), WM_NULL, 0, 0);
}
