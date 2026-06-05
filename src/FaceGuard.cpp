// FaceGuard.cpp
// FaceGuard 应用程序类实现——整个程序的入口和生命周期管理
//
// 应用启动流程：
//   1. 单实例检测（如果已有实例在运行，激活它并退出当前实例）
//   2. 初始化各模块（配置 → 加密 → 数据库 → 摄像头）
//   3. 自动检测系统信息（OS版本、SDK版本、IDE版本）
//   4. 显示登录对话框
//   5. 首次登录：密码修改 → 人脸录入
//   6. 启动监控服务
//   7. 创建隐藏主窗口（承载托盘图标，维持消息循环）
//   8. 进入消息循环（应用持续运行）

#include "stdafx.h"
#include "FaceGuard.h"
#include "DlgLogin.h"
#include "DlgAlert.h"
#include "SingleInstance.h"
#include "ConfigManager.h"
#include "CryptoManager.h"
#include "DatabaseManager.h"
#include "CameraManager.h"
#include "FaceRecognizer.h"
#include "AuthManager.h"
#include "MonitorManager.h"
#include "TrayManager.h"
#include "AutoStartManager.h"
#include "SystemInfo.h"
#include "resource.h"

// 全局唯一的应用程序对象
CFaceGuardApp theApp;

// ============================================================================
// CMainWnd 消息映射
// ============================================================================
BEGIN_MESSAGE_MAP(CMainWnd, CFrameWnd)
    ON_MESSAGE(WM_TRAY_NOTIFY, &CMainWnd::OnTrayNotify)
    ON_MESSAGE(WM_MONITOR_UPDATE, &CMainWnd::OnMonitorUpdate)
    ON_MESSAGE(WM_FACE_DETECTED, &CMainWnd::OnFaceDetected)
    ON_MESSAGE(WM_INTRUDER_ALERT, &CMainWnd::OnIntruderAlert)
    ON_COMMAND(IDM_TRAY_SHOW, &CMainWnd::OnTrayShow)
    ON_COMMAND(IDM_TRAY_CHECK, &CMainWnd::OnTrayCheck)
    ON_COMMAND(IDM_TRAY_VIEW_LOG, &CMainWnd::OnTrayViewLog)
    ON_COMMAND(IDM_TRAY_ABOUT, &CMainWnd::OnTrayAbout)
    ON_COMMAND(IDM_TRAY_EXIT, &CMainWnd::OnTrayExit)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

// ============================================================================
// CMainWnd 构造函数——创建隐藏窗口
// ============================================================================
CMainWnd::CMainWnd()
{
    // 注册窗口类
    LPCTSTR szClassName = AfxRegisterWndClass(0);

    // 创建一个隐藏的、无任务栏按钮的顶层窗口
    // WS_EX_TOOLWINDOW 确保窗口不显示在任务栏上
    CreateEx(WS_EX_TOOLWINDOW, szClassName, FACEGUARD_APP_TITLE,
             WS_OVERLAPPEDWINDOW,
             CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
             nullptr, nullptr);
}

CMainWnd::~CMainWnd()
{
}

// ============================================================================
// 初始化托盘图标
// ============================================================================
void CMainWnd::InitTrayIcon()
{
    CConfigManager& config = CConfigManager::GetInstance();
    if (config.GetShowTrayIcon())
    {
        CTrayManager::GetInstance().Create(this, WM_TRAY_NOTIFY);
    }
}

// ============================================================================
// 托盘消息回调——处理托盘图标的鼠标事件
// ============================================================================
LRESULT CMainWnd::OnTrayNotify(WPARAM wParam, LPARAM lParam)
{
    UINT uMsg = (UINT)lParam;

    switch (uMsg)
    {
    case WM_RBUTTONUP:
        // 右键点击托盘图标——显示上下文菜单
        CTrayManager::GetInstance().ShowContextMenu();
        break;

    case WM_LBUTTONDBLCLK:
        // 双击托盘图标——显示关于对话框
        OnTrayAbout();
        break;

    default:
        break;
    }

    return 0;
}

// ============================================================================
// 关闭窗口——退出应用程序
// ============================================================================
void CMainWnd::OnClose()
{
    // 销毁托盘图标
    CTrayManager::GetInstance().Destroy();

    // 销毁窗口
    CFrameWnd::OnClose();
}

// ============================================================================
// 窗口销毁后清理——MFC 框架会自动 delete this
// 通知应用程序对象主窗口已销毁
// ============================================================================
void CMainWnd::PostNcDestroy()
{
    // 清除应用程序对主窗口的引用，防止 ExitInstance 中重复释放
    AfxGetApp()->m_pMainWnd = nullptr;

    // 基类 CFrameWnd::PostNcDestroy() 会执行 delete this
    CFrameWnd::PostNcDestroy();
}

// ============================================================================
// 监控更新消息处理——wParam=MonitorState, lParam=重试次数
// ============================================================================
LRESULT CMainWnd::OnMonitorUpdate(WPARAM wParam, LPARAM lParam)
{
    CMonitorManager::MonitorState state = (CMonitorManager::MonitorState)wParam;
    int nRetryCount = (int)lParam;

    TCHAR szTip[256];
    if (state == CMonitorManager::ALERT)
    {
        _stprintf_s(szTip, _T("⚠ 检测到可疑人员！第 %d 次重试..."), nRetryCount);
        CTrayManager::GetInstance().ShowBalloonTip(
            _T("FaceGuard - 告警"), szTip, NIIF_WARNING);
    }

    return 0;
}

// ============================================================================
// 人脸检测消息处理——wParam: 1=匹配, 0=不匹配; lParam: 重试次数
// ============================================================================
LRESULT CMainWnd::OnFaceDetected(WPARAM wParam, LPARAM lParam)
{
    if (wParam == 1)
    {
        // 人脸匹配——合法用户确认（静默，不打扰用户）
        CTrayManager::GetInstance().UpdateTooltip(_T("FaceGuard - 监控中 ✓"));
    }
    else
    {
        // 人脸不匹配
        int nRetryCount = (int)lParam;
        TCHAR szTooltip[128];
        _stprintf_s(szTooltip, _T("FaceGuard - ⚠ 可疑人员 (%d/5)"), nRetryCount);
        CTrayManager::GetInstance().UpdateTooltip(szTooltip);
    }
    return 0;
}

// ============================================================================
// 非法用户告警消息处理——wParam: 1=触发关机, 0=用户回归查看记录
// ============================================================================
LRESULT CMainWnd::OnIntruderAlert(WPARAM wParam, LPARAM lParam)
{
    if (wParam == 1)
    {
        // 触发强制关机——弹出告警窗口
        CTrayManager::GetInstance().ShowBalloonTip(
            _T("FaceGuard - 严重告警"),
            _T("检测到非法用户！系统将在60秒后强制关机。"),
            NIIF_ERROR);

        // 弹出非法用户告警窗口（模态，阻止非法用户操作）
        CDlgAlert dlgAlert(this);
        dlgAlert.DoModal();
    }
    else
    {
        // 合法用户回归，弹出之前的非法记录
        CTrayManager::GetInstance().ShowBalloonTip(
            _T("FaceGuard"),
            _T("检测到您离开期间有非法用户操作。点击查看详情。"),
            NIIF_INFO);

        CDlgAlert dlgAlert(this);
        dlgAlert.DoModal();
    }
    return 0;
}

// ============================================================================
// 托盘菜单：显示主窗口（隐藏窗口无界面，显示关于对话框代替）
// ============================================================================
void CMainWnd::OnTrayShow()
{
    OnTrayAbout();
}

// ============================================================================
// 托盘菜单：立即检查人脸
// ============================================================================
void CMainWnd::OnTrayCheck()
{
    // 监控正在运行中，下一轮循环会自动检测
    CTrayManager::GetInstance().ShowBalloonTip(
        _T("FaceGuard"),
        _T("人脸监控正在运行中，每隔一段时间会自动检测。"),
        NIIF_INFO);
}

// ============================================================================
// 托盘菜单：查看非法记录
// ============================================================================
void CMainWnd::OnTrayViewLog()
{
    CDlgAlert dlgAlert(this);
    dlgAlert.DoModal();
}

// ============================================================================
// 托盘菜单：关于 FaceGuard
// ============================================================================
void CMainWnd::OnTrayAbout()
{
    AfxMessageBox(
        _T("FaceGuard v1.0\n\n")
        _T("基于人脸识别的电脑防盗监控系统。\n\n")
        _T("功能：\n")
        _T("  - 人脸录入与识别\n")
        _T("  - 非法用户检测与告警\n")
        _T("  - 自动关机保护\n\n")
        _T("运行时请保持摄像头连接。"),
        MB_ICONINFORMATION);
}

// ============================================================================
// 托盘菜单：退出程序
// ============================================================================
void CMainWnd::OnTrayExit()
{
    if (AfxMessageBox(_T("确定要退出 FaceGuard 吗？\n退出后将无法检测非法用户操作。"),
                      MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        // 使用 PostMessage 而非 SendMessage 避免 delete this 后访问已释放内存
        PostMessage(WM_CLOSE);
    }
}

// ============================================================================
// CFaceGuardApp 构造函数
// ============================================================================
CFaceGuardApp::CFaceGuardApp()
    : m_pMainHiddenWnd(nullptr)
    , m_nLoggedInUserId(0)
{
}

CFaceGuardApp::~CFaceGuardApp()
{
}

// ============================================================================
// 应用程序初始化——MFC 框架在程序启动时自动调用
// 如果返回 FALSE，程序将退出
// ============================================================================
BOOL CFaceGuardApp::InitInstance()
{
    // ---- 第一步：单实例检测 ----
    // 确保系统只有一个 FaceGuard 实例在运行（每个终端只能有一个账号）
    CSingleInstance singleInstance(FACEGUARD_MUTEX_NAME);

    if (singleInstance.IsAlreadyRunning())
    {
        // 已有实例在运行——尝试激活它
        singleInstance.ActivatePreviousInstance();

        AfxMessageBox(_T("FaceGuard 已经在运行中。已切换到已运行的实例。"),
                      MB_ICONINFORMATION);
        return FALSE;  // 退出当前实例
    }

    // ---- 第二步：初始化 COM 库 ----
    // 某些 OpenCV 后端需要 COM 支持
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // ---- 第三步：初始化所有业务模块 ----
    if (!InitializeModules())
    {
        AfxMessageBox(_T("FaceGuard 初始化失败，程序即将退出。\n请检查以下内容：\n1. 磁盘空间是否充足\n2. 是否有写入权限\n3. 摄像头是否连接"),
                      MB_ICONERROR);
        ::CoUninitialize();
        return FALSE;
    }

    // ---- 第四步：输出系统信息（调试用） ----
    CString strSysInfo = CSystemInfo::GetAllInfo();
    OutputDebugString(_T("\n"));
    OutputDebugString(strSysInfo);
    OutputDebugString(_T("\n"));

    // ---- 第五步：显示登录对话框 ----
    if (!ShowLoginDialog())
    {
        // 用户取消登录或登录失败——退出程序
        CleanupModules();
        ::CoUninitialize();
        return FALSE;
    }

    // ---- 第六步：启动人脸监控服务 ----
    if (!StartMonitoring())
    {
        AfxMessageBox(_T("无法启动人脸监控服务。\n请检查摄像头是否已连接并可用。"),
                      MB_ICONERROR);
        CleanupModules();
        ::CoUninitialize();
        return FALSE;
    }

    // ---- 第七步：检查开机自启动 ----
    CheckAutoStart();

    // ---- 第八步：创建隐藏主窗口（维持消息循环） ----
    // 程序以托盘方式运行，不显示主窗口
    // 但 MFC 需要一个主窗口来维持消息循环，否则应用会立即退出
    m_pMainHiddenWnd = new CMainWnd();
    if (m_pMainHiddenWnd == nullptr)
    {
        AfxMessageBox(_T("无法创建主窗口。"), MB_ICONERROR);
        CleanupModules();
        ::CoUninitialize();
        return FALSE;
    }

    // 设置为主窗口（MFC 框架用它来维持消息循环）
    m_pMainWnd = m_pMainHiddenWnd;

    // 将主窗口 HWND 传给监控管理器（用于向 UI 线程发送状态消息）
    CMonitorManager::GetInstance().SetParentWnd(m_pMainHiddenWnd->GetSafeHwnd());

    // 初始化托盘图标
    m_pMainHiddenWnd->InitTrayIcon();

    // 注意：不调用 ShowWindow，窗口默认隐藏到托盘

    return TRUE;
}

// ============================================================================
// 退出时的清理工作
// ============================================================================
int CFaceGuardApp::ExitInstance()
{
    // 清理主窗口
    // 注意：如果窗口通过托盘菜单"退出"关闭，PostNcDestroy 已将 m_pMainWnd 置空
    if (m_pMainWnd != nullptr && m_pMainHiddenWnd != nullptr)
    {
        // 窗口尚未销毁——主动销毁它
        CTrayManager::GetInstance().Destroy();
        m_pMainHiddenWnd->DestroyWindow();
        // DestroyWindow 会触发 PostNcDestroy → delete this → m_pMainWnd = nullptr
    }
    m_pMainHiddenWnd = nullptr;

    CleanupModules();
    ::CoUninitialize();

    return CWinApp::ExitInstance();
}

// ============================================================================
// 初始化所有业务模块（按依赖顺序初始化）
// 返回 TRUE 表示所有模块初始化成功
// ============================================================================
BOOL CFaceGuardApp::InitializeModules()
{
    // 1. 配置文件管理（最先初始化，其他模块依赖配置）
    CConfigManager& config = CConfigManager::GetInstance();
    if (!config.Initialize())
    {
        OutputDebugString(_T("FaceGuard: 配置文件管理初始化失败\n"));
        return FALSE;
    }

    // 检查并补全默认配置项（首次运行时创建完整的 INI 文件）
    config.EnsureDefaults();

    // 2. 加密管理（数据库依赖加密功能）
    CCryptoManager& crypto = CCryptoManager::GetInstance();
    if (!crypto.Initialize())
    {
        OutputDebugString(_T("FaceGuard: 加密模块初始化失败\n"));
        return FALSE;
    }

    // 3. 数据库管理
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    if (!db.Initialize())
    {
        OutputDebugString(_T("FaceGuard: 数据库初始化失败\n"));
        return FALSE;
    }

    // 4. 人脸识别器（从配置读取阈值）
    CFaceRecognizer& recognizer = CFaceRecognizer::GetInstance();
    if (!recognizer.Initialize())
    {
        OutputDebugString(_T("FaceGuard: 人脸识别器初始化失败\n"));
        // 人脸识别初始化失败不是致命错误（摄像头可能不可用）
        // 继续运行，在需要时再报错
    }

    // 5. 摄像头管理（最后初始化，因为它依赖硬件）
    CCameraManager& camera = CCameraManager::GetInstance();
    if (!camera.Initialize(0))  // 使用默认摄像头（索引0）
    {
        OutputDebugString(_T("FaceGuard: 摄像头初始化失败——程序将在无摄像头模式下运行\n"));
        // 摄像头不可用不是致命错误——登录和人脸录入可以稍后重试
    }

    OutputDebugString(_T("FaceGuard: 所有模块初始化完成\n"));
    return TRUE;
}

// ============================================================================
// 清理所有业务模块（按初始化的相反顺序释放）
// ============================================================================
void CFaceGuardApp::CleanupModules()
{
    // 停止监控
    CMonitorManager::GetInstance().Stop();

    // 释放摄像头
    CCameraManager::GetInstance().Release();

    // 关闭数据库
    CDatabaseManager::GetInstance().Close();

    OutputDebugString(_T("FaceGuard: 所有模块已清理\n"));
}

// ============================================================================
// 显示登录对话框并处理完整登录流程
// 返回 TRUE 表示用户成功登录
// ============================================================================
BOOL CFaceGuardApp::ShowLoginDialog()
{
    CConfigManager& config = CConfigManager::GetInstance();
    CAuthManager& auth = CAuthManager::GetInstance();
    CDatabaseManager& db = CDatabaseManager::GetInstance();

    // 先尝试自动登录（如果配置了记住密码）
    CString strUsername, strPassword;
    if (config.GetRememberLogin())
    {
        if (auth.AutoLogin(strUsername, strPassword))
        {
            // 自动登录成功——记录用户ID
            m_nLoggedInUserId = db.GetUserId(strUsername);
            int nFaceCount = db.GetFaceCount(m_nLoggedInUserId);

            if (nFaceCount > 0)
            {
                // 已有人脸数据，可直接进入监控
                return TRUE;
            }
            // 否则需要进入人脸录入流程（但不需要重新验证密码）
        }
    }

    // 弹出登录对话框
    CDlgLogin dlgLogin;
    INT_PTR nResult = dlgLogin.DoModal();

    if (nResult == IDOK && dlgLogin.IsLoginSuccess())
    {
        // 记录登录成功的用户ID
        m_nLoggedInUserId = dlgLogin.GetLoggedInUserId();
        return TRUE;
    }

    return FALSE;
}

// ============================================================================
// 启动人脸监控服务
// 返回 TRUE 表示监控成功启动
// ============================================================================
BOOL CFaceGuardApp::StartMonitoring()
{
    CMonitorManager& monitor = CMonitorManager::GetInstance();

    // 设置当前登录用户的ID（监控将比对该用户的人脸数据）
    monitor.SetUserId(m_nLoggedInUserId);

    if (!monitor.Start())
    {
        return FALSE;
    }

    OutputDebugString(_T("FaceGuard: 人脸监控服务已启动\n"));
    return TRUE;
}

// ============================================================================
// 检查并设置开机自启动
// 根据配置文件中的设置决定是否启用
// ============================================================================
void CFaceGuardApp::CheckAutoStart()
{
    CConfigManager& config = CConfigManager::GetInstance();

    if (config.GetAutoStart())
    {
        if (!CAutoStartManager::IsAutoStartEnabled())
        {
            CAutoStartManager::EnableAutoStart();
        }
    }
    else
    {
        if (CAutoStartManager::IsAutoStartEnabled())
        {
            CAutoStartManager::DisableAutoStart();
        }
    }
}
