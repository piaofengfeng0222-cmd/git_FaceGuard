// MonitorManager.cpp
// 监控管理模块实现——核心监控逻辑
//
// 工作流程：
//   1. NORMAL 状态：每 60 秒抓拍一次，检测人脸并与数据库中的录入人脸比对
//   2. 如果不匹配，转入 ALERT 状态
//   3. ALERT 状态：每 3 秒抓拍一次，最多重试 5 次
//   4. 如果 5 次都不匹配，确认非法用户，记录信息并安排 1 分钟后强制关机
//   5. 在任何时候如果匹配成功，回到 NORMAL 状态
//   6. 合法用户回来时，如果之前有非法用户记录，弹出告警窗口

#include "stdafx.h"
#include "MonitorManager.h"
#include "CameraManager.h"
#include "FaceRecognizer.h"
#include "DatabaseManager.h"
#include "CryptoManager.h"
#include "ConfigManager.h"

// ============================================================================
// 单例实现
// ============================================================================
CMonitorManager& CMonitorManager::GetInstance()
{
    static CMonitorManager instance;
    return instance;
}

CMonitorManager::CMonitorManager()
    : m_state(NORMAL)
    , m_bRunning(FALSE)
    , m_nRetryCount(0)
    , m_bHadIntruder(FALSE)
    , m_hParentWnd(nullptr)
    , m_nUserId(1)                         // 默认用户ID（单用户场景）
    , m_nNormalInterval(DEFAULT_NORMAL_INTERVAL_SECONDS)
    , m_nAlertInterval(DEFAULT_ALERT_INTERVAL_SECONDS)
    , m_nAlertRetryCount(DEFAULT_ALERT_RETRY_COUNT)
    , m_nShutdownDelay(DEFAULT_SHUTDOWN_DELAY_SECONDS)
{
}

CMonitorManager::~CMonitorManager()
{
    Stop();
}

// ============================================================================
// 启动监控
// 1. 从配置文件读取参数
// 2. 启动后台监控线程
// ============================================================================
BOOL CMonitorManager::Start()
{
    if (m_bRunning)
    {
        return TRUE;  // 已在运行
    }

    // 检查摄像头是否就绪
    CCameraManager& camera = CCameraManager::GetInstance();
    if (!camera.IsOpened())
    {
        return FALSE;  // 摄像头未初始化
    }

    // 读取配置
    CConfigManager& config = CConfigManager::GetInstance();
    m_nNormalInterval = config.GetNormalIntervalSeconds();
    m_nAlertInterval = config.GetAlertIntervalSeconds();
    m_nAlertRetryCount = config.GetAlertRetryCount();
    m_nShutdownDelay = config.GetShutdownDelaySeconds();

    // 重置状态
    m_state = NORMAL;
    m_nRetryCount = 0;
    m_bHadIntruder = FALSE;
    m_bRunning = TRUE;

    // 启动监控线程
    m_monitorThread = std::thread(&CMonitorManager::MonitorLoop, this);

    return TRUE;
}

// ============================================================================
// 停止监控
// ============================================================================
void CMonitorManager::Stop()
{
    if (!m_bRunning)
    {
        return;
    }

    m_bRunning = FALSE;

    // 等待监控线程退出
    if (m_monitorThread.joinable())
    {
        m_monitorThread.join();
    }

    // 如果有关机计划，取消它
    CancelShutdown();
}

// ============================================================================
// 监控主循环
// 在独立线程中运行，通过 Sleep 控制抓拍间隔
// ============================================================================
void CMonitorManager::MonitorLoop()
{
    CCameraManager& camera = CCameraManager::GetInstance();
    CFaceRecognizer& recognizer = CFaceRecognizer::GetInstance();
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    CCryptoManager& crypto = CCryptoManager::GetInstance();

    while (m_bRunning)
    {
        int nSleepInterval = (m_state == ALERT) ? m_nAlertInterval : m_nNormalInterval;

        // 等待指定时间（可响应停止信号）
        // 使用小间隔轮询以便及时响应停止
        for (int i = 0; i < nSleepInterval && m_bRunning; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (!m_bRunning)
        {
            break;
        }

        // 检查摄像头状态
        if (!camera.IsOpened())
        {
            // 摄像头不可用，跳过本次检查
            continue;
        }

        // 执行人脸检查
        BOOL bFaceDetected = DoFaceCheck();
    }
}

// ============================================================================
// 执行一次人脸检查流程
// 1. 从摄像头抓取一帧
// 2. 检测人脸
// 3. 如果检测到人脸，与数据库中所有录入的人脸逐一比对
// 4. 根据比对结果更新监控状态
// ============================================================================
BOOL CMonitorManager::DoFaceCheck()
{
    CCameraManager& camera = CCameraManager::GetInstance();
    CFaceRecognizer& recognizer = CFaceRecognizer::GetInstance();
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    CCryptoManager& crypto = CCryptoManager::GetInstance();

    // 抓取图像帧
    cv::Mat frame;
    if (!camera.CaptureFrame(frame))
    {
        return FALSE;  // 抓取失败
    }

    // 检测人脸
    cv::Rect faceRect;
    if (!camera.DetectFace(frame, faceRect))
    {
        // 没有检测到人脸——无人使用电脑，保持正常状态
        // 将抓取到的图像保存到程序同目录下的 image 文件夹中
        TCHAR szExePath[MAX_PATH] = { 0 };
        ::GetModuleFileName(nullptr, szExePath, MAX_PATH);
        CString strExeDir = szExePath;
        int nPos = strExeDir.ReverseFind(_T('\\'));
        if (nPos >= 0)
        {
            strExeDir = strExeDir.Left(nPos);
        }
        CString strImageDir;
        strImageDir.Format(_T("%s\\%s"), (LPCTSTR)strExeDir, FACEGUARD_NO_FACE_DIR);

		// 没有检测到人脸图像保存路径
		::SHCreateDirectoryEx(nullptr, strImageDir, nullptr);

        time_t tNow = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &tNow);
        TCHAR szTimestamp[64] = { 0 };
        _tcsftime(szTimestamp, 64, _T("%Y%m%d_%H%M%S"), &timeinfo);

        CString strFilePath;
        strFilePath.Format(_T("%s\\no_face_%s.jpg"), (LPCTSTR)strImageDir, szTimestamp);

        camera.SaveImage(frame, strFilePath);
        return FALSE;
    }

    // 提取人脸 ROI 和特征
    cv::Mat faceROI = frame(faceRect).clone();
    cv::Mat currentFeatures = recognizer.ExtractFeatures(faceROI);

    if (currentFeatures.empty())
    {
        return FALSE;  // 特征提取失败
    }

    // 获取当前登录用户的所有录入人脸特征
    std::vector<FaceRecord> vecFaces = db.GetAllFaces(m_nUserId);

    if (vecFaces.empty())
    {
        // 没有录入人脸——无法比对，视为正常
        return FALSE;
    }

    // 逐一比对：解密特征数据 → 比较 → 任一匹配即为合法用户
    BOOL bMatched = FALSE;
    for (const auto& record : vecFaces)
    {
        // 解密人脸特征数据
        std::vector<BYTE> decryptedFeatures = crypto.DecryptData(
            record.vecFeatures.data(),
            record.vecFeatures.size());

        if (decryptedFeatures.empty())
        {
            continue;  // 解密失败，跳过此记录
        }

        // 将解密的特征数据还原为 cv::Mat
        // 特征维度：1 × (LBP_GRID_X * LBP_GRID_Y * 256)
        // 即 1 × (8 * 8 * 256) = 1 × 16384
        cv::Mat storedFeatures(1, (int)(decryptedFeatures.size() / sizeof(float)), CV_32FC1,
                               decryptedFeatures.data());

        double dDistance = recognizer.CompareFaces(currentFeatures, storedFeatures);

        if (recognizer.IsFaceMatch(dDistance))
        {
            bMatched = TRUE;
            break;  // 匹配成功
        }
    }

    // ---- 根据比对结果更新状态 ----
    if (bMatched)
    {
        // === 人脸匹配：合法用户 ===
        if (m_state == ALERT && m_bHadIntruder)
        {
            // 之前有非法用户，现在合法用户回来了——通知 UI 弹出告警窗口
            OutputDebugString(_T("MonitorManager: 合法用户回归，之前有非法入侵记录\n"));
            if (m_hParentWnd != nullptr)
            {
                ::PostMessage(m_hParentWnd, WM_INTRUDER_ALERT, 0, 0);
            }
            if (m_onUserReturn)
            {
                m_onUserReturn();
            }
        }

        // 重置状态
        m_state = NORMAL;
        m_nRetryCount = 0;

        // 取消可能存在的关机计划
        CancelShutdown();

        // 通知 UI：人脸匹配成功
        if (m_hParentWnd != nullptr)
        {
            ::PostMessage(m_hParentWnd, WM_FACE_DETECTED, 1, 0);  // wParam=1 表示匹配
        }
        if (m_onFaceMatch)
        {
            m_onFaceMatch();
        }
    }
    else
    {
        // === 人脸不匹配：可能的非法用户 ===
        if (m_state == NORMAL)
        {
            // 从不匹配的第一帧开始进入告警状态
            m_state = ALERT;
            m_nRetryCount = 0;
            OutputDebugString(_T("MonitorManager: 检测到不匹配人脸，进入 ALERT 状态\n"));
        }

        m_nRetryCount++;
        m_bHadIntruder = TRUE;

        // 通知 UI：人脸不匹配
        if (m_hParentWnd != nullptr)
        {
            ::PostMessage(m_hParentWnd, WM_FACE_DETECTED, 0, m_nRetryCount);  // wParam=0=不匹配, lParam=重试次数
            ::PostMessage(m_hParentWnd, WM_MONITOR_UPDATE, (WPARAM)m_state, m_nRetryCount);
        }
        if (m_onFaceMismatch)
        {
            m_onFaceMismatch(m_nRetryCount);
        }

        // 检查是否达到最大重试次数
        if (m_nRetryCount >= m_nAlertRetryCount)
        {
            // === 确认非法用户 ===
            m_state = SHUTDOWN;
            m_bRunning = FALSE;

            OutputDebugString(_T("MonitorManager: 确认非法用户入侵！准备强制关机...\n"));

            // 记录非法用户信息
            LogIntruder(faceROI, TRUE);

            // 通知 UI：确认非法用户
            if (m_hParentWnd != nullptr)
            {
                ::PostMessage(m_hParentWnd, WM_INTRUDER_ALERT, 1, 0);  // wParam=1 表示触发关机
            }
            if (m_onIntruderConfirmed)
            {
                m_onIntruderConfirmed();
            }

            // 安排强制关机
            ScheduleShutdown();
        }
        else
        {
            // 记录非法用户图像（非关机触发）
            LogIntruder(faceROI, FALSE);
        }
    }

    return bMatched;
}

// ============================================================================
// 计划强制关机
// 使用 InitiateSystemShutdownEx API，延迟指定秒数后关机
// ============================================================================
void CMonitorManager::ScheduleShutdown()
{
    // 获取关机权限
    HANDLE hToken = nullptr;
    if (::OpenProcessToken(::GetCurrentProcess(),
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        TOKEN_PRIVILEGES tkp;
        ::LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, nullptr);
        ::CloseHandle(hToken);
    }

    // 获取关机原因消息
    CString strMessage;
    strMessage.Format(_T("FaceGuard 检测到非法用户操作，系统将在 %d 秒后自动关机。"), m_nShutdownDelay);

    // 调用关机 API
    // 参数：关机消息、超时（秒）、强制关闭应用程序、重启后不重启
    BOOL bResult = ::InitiateSystemShutdownEx(
        nullptr,                          // 本机
        (LPTSTR)(LPCTSTR)strMessage,      // 关机消息
        (DWORD)m_nShutdownDelay,          // 超时（秒）
        TRUE,                             // 强制关闭应用程序
        FALSE,                            // 不重启
        SHTDN_REASON_FLAG_PLANNED        // 计划内关机
    );

    if (!bResult)
    {
        DWORD dwError = ::GetLastError();
        TCHAR szError[256] = { 0 };
        _stprintf_s(szError, _T("关机调度失败，错误码: %lu"), dwError);
        OutputDebugString(szError);
    }
}

// ============================================================================
// 取消关机计划
// ============================================================================
void CMonitorManager::CancelShutdown()
{
    // 获取权限
    HANDLE hToken = nullptr;
    if (::OpenProcessToken(::GetCurrentProcess(),
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        TOKEN_PRIVILEGES tkp;
        ::LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        ::AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, nullptr);
        ::CloseHandle(hToken);
    }

    // 取消本机上的关机计划
    ::AbortSystemShutdown(nullptr);
}

// ============================================================================
// 记录非法用户信息到数据库和文件
// bShutdown - 是否触发了关机
// ============================================================================
void CMonitorManager::LogIntruder(const cv::Mat& faceImage, BOOL bShutdown)
{
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    CConfigManager& config = CConfigManager::GetInstance();

    CString strTime = GetCurrentTimeString();

    // 生成文件路径
    CString strAppDataDir = config.GetAppDataDir();

    // 非法用户图像保存路径
    CString strIntruderDir = strAppDataDir + _T("\\") + FACEGUARD_INTRUDER_DIR;
    ::SHCreateDirectoryEx(nullptr, strIntruderDir, nullptr);

    // 文件名：intruder_YYYYMMDD_HHMMSS.jpg
    CString strFileName;
    strFileName.Format(_T("intruder_%s.jpg"), (LPCTSTR)strTime);
    strFileName.Replace(_T(':'), _T('_'));
    strFileName.Replace(_T(' '), _T('_'));

    CString strFullImagePath = strIntruderDir + _T("\\") + strFileName;

    // 缩略图保存
    CString strThumbDir = strAppDataDir + _T("\\") + FACEGUARD_THUMBNAIL_DIR;
    ::SHCreateDirectoryEx(nullptr, strThumbDir, nullptr);
    CString strThumbPath = strThumbDir + _T("\\thumb_") + strFileName;

    // 保存原图
    CCameraManager::GetInstance().SaveImage(faceImage, strFullImagePath);

    // 生成缩略图（缩放到 150px 宽）
    cv::Mat thumbnail;
    double dScale = 150.0 / faceImage.cols;
    cv::resize(faceImage, thumbnail,
               cv::Size(150, (int)(faceImage.rows * dScale)),
               0, 0, cv::INTER_AREA);
    CCameraManager::GetInstance().SaveImage(thumbnail, strThumbPath);

    // 写入数据库
    db.InsertIntruderLog(strTime, strFullImagePath, strThumbPath, bShutdown);
}

// ============================================================================
// 获取当前时间字符串 (YYYY-MM-DD HH:MM:SS)
// ============================================================================
CString CMonitorManager::GetCurrentTimeString()
{
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    TCHAR szTime[64] = { 0 };
    _tcsftime(szTime, 64, _T("%Y-%m-%d %H:%M:%S"), &timeinfo);
    return CString(szTime);
}
