// MonitorManager.h
// 监控管理模块——核心监控逻辑
// 负责定时抓拍、人脸比对、状态切换、非法用户检测和锁屏调度
//
// 状态机：
//   NORMAL (60s间隔) ──不匹配──→ ALERT (3s间隔×5次)
//     ↑                              │
//     └─────匹配──────────────────────┘
//                                    │ 5次都不匹配
//                                    ↓
//                                 SHUTDOWN (立即锁屏)

#pragma once

class CMonitorManager
{
public:
    // 获取单例实例
    static CMonitorManager& GetInstance();

    // 启动监控（在后台线程中运行监控循环）
    BOOL Start();

    // 停止监控
    void Stop();

    // 是否正在监控
    BOOL IsMonitoring() const { return m_bRunning; }

    // 获取当前监控状态
    enum MonitorState { NORMAL, ALERT, SHUTDOWN };
    MonitorState GetState() const { return m_state; }

    // 获取重试计数
    int GetRetryCount() const { return m_nRetryCount; }

    // ======== 回调设置（由 UI 层注册） ========
    // 人脸匹配时的回调
    void SetOnFaceMatchCallback(std::function<void()> callback)
    {
        m_onFaceMatch = callback;
    }

    // 人脸不匹配时的回调（retryCount 为当前重试次数）
    void SetOnFaceMismatchCallback(std::function<void(int nRetryCount)> callback)
    {
        m_onFaceMismatch = callback;
    }

    // 非法用户确认（5次都不匹配）时的回调
    void SetOnIntruderConfirmedCallback(std::function<void()> callback)
    {
        m_onIntruderConfirmed = callback;
    }

    // 合法用户回来时的回调（之前有非法用户记录）
    void SetOnUserReturnCallback(std::function<void()> callback)
    {
        m_onUserReturn = callback;
    }

    // 设置父窗口句柄（用于向 UI 线程发送消息）
    void SetParentWnd(HWND hWnd) { m_hParentWnd = hWnd; }

    // 设置当前登录用户ID（监控将比对该用户的人脸数据）
    void SetUserId(int nUserId) { m_nUserId = nUserId; }
    int GetUserId() const { return m_nUserId; }

private:
    CMonitorManager();
    ~CMonitorManager();
    CMonitorManager(const CMonitorManager&) = delete;
    CMonitorManager& operator=(const CMonitorManager&) = delete;

    // 监控主循环（在独立线程中运行）
    void MonitorLoop();

    // 执行一次完整的人脸检查流程
    BOOL DoFaceCheck();

    // 锁定工作站
    void LockWorkStation();

    // 安排延迟锁屏（倒计时结束后执行锁屏）
    void ScheduleLock(int nDelaySeconds);

    // 取消延迟锁屏
    void CancelScheduleLock();

    // 记录非法用户信息
    void LogIntruder(const cv::Mat& faceImage, BOOL bShutdown);

    // 获取当前时间字符串
    CString GetCurrentTimeString();

    std::atomic<MonitorState> m_state;   // 当前监控状态
    std::atomic<BOOL> m_bRunning;        // 监控是否运行中
    std::atomic<int> m_nRetryCount;      // 当前重试次数
    std::atomic<BOOL> m_bHadIntruder;    // 本次会话是否有过非法用户

    std::thread m_monitorThread;         // 监控工作线程
    std::mutex m_mutex;                  // 互斥锁
    HWND m_hParentWnd;                   // 父窗口句柄
    int m_nUserId;                       // 当前监控的用户ID

    // 配置参数（从配置文件读取）
    int m_nNormalInterval;
    int m_nAlertInterval;
    int m_nAlertRetryCount;
    int m_nLockDelaySeconds;        // 锁屏延迟秒数（5次失败后倒计时）

    // 回调函数
    std::function<void()> m_onFaceMatch;
    std::function<void(int)> m_onFaceMismatch;
    std::function<void()> m_onIntruderConfirmed;
    std::function<void()> m_onUserReturn;
};
