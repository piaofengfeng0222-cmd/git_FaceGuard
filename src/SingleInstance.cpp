// SingleInstance.cpp
// 单实例检测模块实现

#include "stdafx.h"
#include "SingleInstance.h"

// ============================================================================
// 构造函数
// 创建命名互斥体来检测程序是否已经在运行
// strMutexName - 互斥体名称，应对于程序全局唯一
// ============================================================================
CSingleInstance::CSingleInstance(const CString& strMutexName)
    : m_hMutex(nullptr)
    , m_bAlreadyRunning(FALSE)
    , m_strMutexName(strMutexName)
{
    // 创建互斥体（使用 Global\ 前缀确保跨会话可见）
    CString strFullName;
    strFullName.Format(_T("%s"), (LPCTSTR)strMutexName);

    m_hMutex = ::CreateMutex(nullptr, FALSE, strFullName);

    // 如果互斥体已存在，并且 CreateMutex 成功，则 GetLastError 返回 ERROR_ALREADY_EXISTS
    if (::GetLastError() == ERROR_ALREADY_EXISTS || m_hMutex == nullptr)
    {
        m_bAlreadyRunning = TRUE;

        // 如果互斥体创建失败但句柄有效，释放它
        if (m_hMutex != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS)
        {
            // 保持句柄直到析构，以维持互斥体引用计数
        }
    }
}

// ============================================================================
// 析构函数：释放互斥体句柄
// ============================================================================
CSingleInstance::~CSingleInstance()
{
    if (m_hMutex != nullptr)
    {
        ::ReleaseMutex(m_hMutex);
        ::CloseHandle(m_hMutex);
        m_hMutex = nullptr;
    }
}

// ============================================================================
// 查找已运行的 FaceGuard 主窗口
// 通过窗口类名和窗口标题进行查找
// ============================================================================
HWND CSingleInstance::FindFaceGuardWindow()
{
    // 通过窗口标题查找
    HWND hWnd = ::FindWindow(nullptr, FACEGUARD_APP_TITLE);

    // 如果按标题找不到，尝试按窗口类名查找
    if (hWnd == nullptr)
    {
        hWnd = ::FindWindow(FACEGUARD_WINDOW_CLASS, nullptr);
    }

    return hWnd;
}

// ============================================================================
// 将窗口恢复到前台并激活
// ============================================================================
BOOL CSingleInstance::BringWindowToTop(HWND hWnd)
{
    if (hWnd == nullptr || !::IsWindow(hWnd))
    {
        return FALSE;
    }

    // 如果窗口最小化了，恢复它
    if (::IsIconic(hWnd))
    {
        ::ShowWindow(hWnd, SW_RESTORE);
    }

    // 如果窗口隐藏了，显示它
    if (!::IsWindowVisible(hWnd))
    {
        ::ShowWindow(hWnd, SW_SHOW);
    }

    // 将窗口置于 Z 序顶部
    ::BringWindowToTop(hWnd);
    ::SetForegroundWindow(hWnd);

    // 闪烁窗口以吸引用户注意
    ::FlashWindow(hWnd, TRUE);

    return TRUE;
}

// ============================================================================
// 激活已存在的程序实例
// 找到已运行的窗口并将其置于前台，同时发送恢复消息
// ============================================================================
BOOL CSingleInstance::ActivatePreviousInstance()
{
    HWND hWnd = FindFaceGuardWindow();

    if (hWnd == nullptr)
    {
        return FALSE;
    }

    BringWindowToTop(hWnd);

    // 向已存在的实例发送恢复消息
    ::PostMessage(hWnd, WM_USER + 200, 0, 0);  // 自定义恢复消息

    return TRUE;
}
