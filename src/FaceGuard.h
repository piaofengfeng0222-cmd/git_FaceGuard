// FaceGuard.h
// FaceGuard 应用程序类——管理应用程序的初始化、运行和退出
// 负责协调各模块的启动顺序，处理命令行参数

#pragma once

// ============================================================================
// CMainWnd - 隐藏主窗口，用于承载托盘图标和维持消息循环
// 应用程序以托盘方式运行，此窗口始终保持隐藏状态
// ============================================================================
class CMainWnd : public CFrameWnd
{
public:
    CMainWnd();
    virtual ~CMainWnd();

    // 初始化托盘图标（需要在窗口创建后调用）
    void InitTrayIcon();

protected:
    afx_msg LRESULT OnTrayNotify(WPARAM wParam, LPARAM lParam);
    afx_msg void OnClose();
    afx_msg LRESULT OnMonitorUpdate(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFaceDetected(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnIntruderAlert(WPARAM wParam, LPARAM lParam);

    // 窗口销毁后的清理（MFC 会 delete this）
    virtual void PostNcDestroy() override;

    DECLARE_MESSAGE_MAP()

private:
    void OnTrayShow();
    void OnTrayCheck();
    void OnTrayViewLog();
    void OnTrayAbout();
    void OnTrayExit();
};

// ============================================================================
// CFaceGuardApp - 应用程序类
// ============================================================================
class CFaceGuardApp : public CWinApp
{
public:
    CFaceGuardApp();
    virtual ~CFaceGuardApp();

    // 应用程序初始化（在 MFC 框架中自动调用）
    virtual BOOL InitInstance() override;

    // 应用程序退出时的清理工作
    virtual int ExitInstance() override;

private:
    // 初始化所有业务模块
    BOOL InitializeModules();

    // 清理所有业务模块
    void CleanupModules();

    // 显示登录对话框并处理登录流程
    BOOL ShowLoginDialog();

    // 启动人脸监控服务
    BOOL StartMonitoring();

    // 检查并启用开机自启动
    void CheckAutoStart();

    // 隐藏主窗口指针（用于托盘运行）
    CMainWnd* m_pMainHiddenWnd;

    // 当前登录用户的ID（用于监控服务人脸比对）
    int m_nLoggedInUserId;
};
