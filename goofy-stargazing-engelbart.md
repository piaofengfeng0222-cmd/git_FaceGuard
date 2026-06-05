# FaceGuard 人脸防盗监控系统 — 详细设计文档

> **最后更新**：2026-06-03

### 变更记录

| 日期 | 变更内容 |
|------|----------|
| 2026-06-03 | **全面更新设计文档**：修正数据存储位置（exe同目录）、新增 `[FaceDetection]` INI配置节、人脸检测参数可配置化、新增 MonitorManager 自定义消息机制、新增 CMainWnd 隐藏窗口类、新增 noface 无脸图像抓拍目录、完善模块初始化流程、补充 COM 初始化、新增托盘 `ShowContextMenu`、DlgAlert 改用 CListCtrl 实现 |
| 2026-06-02 | CDlgFace：新增双定时器（预览50ms + 抓拍比对800ms）+ CaptureAndCompare 实时比对 + 摄像头回退初始化 + OnPaint 修复 |
| 2026-06-02 | CFaceRecognizer：更新为自实现 LBP 直方图算法（非 OpenCV LBPH），卡方距离比对 |
| 2026-06-02 | CCameraManager：移动 DetectFace 至本模块，新增 Haar Cascade 加载逻辑和手动 BGR→Gray 转换 |

---

## 一、上下文

**项目背景**：开发一款基于人脸识别的 Windows 桌面防窥监控软件。当用户离开电脑时，程序自动通过摄像头定时抓拍，比对当前操作者是否为合法用户；发现非法用户时告警并强制关机，记录非法使用证据。

**开发环境**（自动检测结果）：
| 项目 | 版本 |
|------|------|
| 操作系统 | Windows 10 Education 10.0.18363 (Build 18363) x64 |
| IDE | Visual Studio Code 1.122.0 |
| Visual Studio | Visual Studio 2017 Professional |
| Windows SDK | 10.0.19041.0 |
| Git | 2.53.0.windows.3 |

**C++ 标准**：C++17（VS2017 完全支持）

**关键词**：MFC、OpenCV、SQLite、人脸识别、Windows 服务、模态对话框

---

## 二、总体架构

### 2.1 架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                      FaceGuard 应用程序                           │
├─────────────────────────────────────────────────────────────────┤
│  框架层      │ CMainWnd (隐藏主窗口) + CFaceGuardApp (生命周期)   │
│             │ 自定义消息：WM_FACE_DETECTED / WM_MONITOR_UPDATE   │
│             │            WM_INTRUDER_ALERT / WM_TRAY_NOTIFY      │
├─────────────┼───────────────────────────────────────────────────┤
│  UI 层      │ 登录对话框 │ 密码修改 │ 人脸录入  │ 告警弹窗       │
│  (MFC模态)  │ CDlgLogin  │ CDlgPwd  │ CDlgFace  │ CDlgAlert      │
├─────────────┼───────────────────────────────────────────────────┤
│  业务逻辑层  │ CMonitorManager │ CAuthManager │ CFaceRecognizer  │
├─────────────┼───────────────────────────────────────────────────┤
│  数据访问层  │           CDatabaseManager (SQLite)              │
│             │           CCryptoManager (AES-256-CBC)            │
├─────────────┼───────────────────────────────────────────────────┤
│  设备访问层  │ CCameraManager │ CTrayManager   │ CAutoStart      │
│             │  (OpenCV)      │  (Shell_Notify) │  (注册表)        │
├─────────────┼───────────────────────────────────────────────────┤
│  基础设施层  │ CConfigManager │ CSystemInfo │ CSingleInstance    │
└─────────────┴───────────────────────────────────────────────────┘
```

### 2.2 设计模式

| 设计模式 | 应用场景 |
|----------|----------|
| **单例 (Singleton)** | CDatabaseManager、CCryptoManager、CCameraManager、CConfigManager、CMonitorManager、CFaceRecognizer、CAuthManager、CTrayManager —— 全局唯一实例 |
| **策略 (Strategy)** | CFaceRecognizer 中可切换 LBPH / EigenFace 算法 |
| **观察者 (Observer)** | CMonitorManager 通过 PostMessage 自定义消息 + 回调函数双重机制通知 UI 状态变化 |
| **状态 (State)** | CMonitorManager 管理原子状态机：NORMAL→ALERT→SHUTDOWN，使用 `std::atomic` 保证跨线程安全 |

---

## 三、模块详细设计

### 3.1 系统信息检测模块 (CSystemInfo)

**职责**：自动检测并返回当前电脑的系统版本、SDK 版本和 IDE 版本。

**实现**：
```cpp
class CSystemInfo {
public:
    static CString GetOSVersion();      // 通过 RtlGetVersion 或 WMI 获取 Windows 版本
    static CString GetSDKVersion();     // 从注册表 HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots 读取
    static CString GetIDEVersion();     // 检测 VS 安装路径，读取 VC/Tools/MSVC 版本
    static CString GetAllInfo();        // 汇总格式化的系统信息字符串
};
```

**检测方式**：
- **OS 版本**：使用 `RtlGetVersion` API（比 GetVersionEx 更准确，不受兼容性清单影响）
- **SDK 版本**：读取注册表 `HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots` 下的 `KitsRoot10` 及 Include 目录版本号
- **IDE 版本**：检测 VS 2017 安装路径，读取 VC++ 工具链版本文件

---

### 3.2 配置管理模块 (CConfigManager)

**职责**：管理程序配置文件（INI 格式），控制程序行为开关。启动时自动补全缺失的默认配置项（`EnsureDefaults()`）。

**配置数据存储位置**：所有数据文件（数据库、配置文件、人脸图片等）与程序可执行文件位于相同目录，便于便携式部署。

**配置文件** (`FaceGuard.ini`)：
```ini
[General]
; 是否显示系统托盘图标 (1=显示, 0=隐藏)
ShowTrayIcon=1
; 是否开机自启动
AutoStart=1
; 人脸比对阈值 (卡方距离, 越小越严格)
FaceMatchThreshold=80
; 定时抓拍间隔 (秒)
NormalIntervalSeconds=60
; 不匹配时抓拍间隔 (秒)
AlertIntervalSeconds=3
; 不匹配重试次数
AlertRetryCount=5
; 强制关机前等待时间 (秒)
ShutdownDelaySeconds=60

[FaceDetection]
; 最小人脸宽度（像素）
MinFaceWidth=50
; 最小人脸高度（像素）
MinFaceHeight=50
; 最小邻居检测窗口数（Haar Cascade，越小越灵敏但误检率越高）
MinNeighbors=2
; 缩放因子（越小检测越精细但速度越慢）
ScaleFactor=1.1

[LastLogin]
; 上次成功登录的账号
LastUsername=
; 上次成功登录的密码（加密）
LastPassword=
; 是否记住登录状态
RememberLogin=0
```

**核心接口**（新增）：
```cpp
class CConfigManager {
public:
    static CConfigManager& GetInstance();

    BOOL Initialize();
    void EnsureDefaults();  // 检查并补全 [General] 和 [FaceDetection] 各节缺失的配置项

    // [General]
    int GetShowTrayIcon() const;
    // ... (所有 getter/setter 参见完整代码)

    // [FaceDetection] — 人脸检测参数
    int     GetMinFaceWidth() const;
    int     GetMinFaceHeight() const;
    int     GetMinNeighbors() const;
    double  GetScaleFactor() const;

    // [LastLogin]
    // ...

private:
    BOOL KeyExists(LPCTSTR lpSection, LPCTSTR lpKey) const;  // 使用哨兵值判断键是否存在
    // ...
};
```

> **设计说明**：`EnsureDefaults()` 在程序启动时调用，确保即使用户删除了配置文件中的某些项，所有配置项也有合理的默认值。`[FaceDetection]` 节允许用户在不修改代码的情况下调整 Haar Cascade 检测参数。

---

### 3.3 数据库模块 (CDatabaseManager) — 单例

**职责**：管理 SQLite 数据库的创建、连接、CRUD 操作。

**数据库文件**：`[exe_dir]\faceguard.db`（与程序 .exe 同目录，使用 WAL 模式 + 外键约束）

> **设计说明**：选择程序同目录而非 `%APPDATA%` 是为了便携性——用户可将整个文件夹复制到 U 盘或另一台电脑直接使用，无需重新配置。

**表结构**：

```sql
-- 用户账号表
CREATE TABLE IF NOT EXISTS user_account (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    username     TEXT    NOT NULL UNIQUE,
    password     TEXT    NOT NULL,  -- AES 加密存储
    is_first_login INTEGER DEFAULT 1, -- 0=已修改密码, 1=首次登录
    created_time TEXT    NOT NULL,
    last_login_time TEXT
);

-- 人脸数据表
CREATE TABLE IF NOT EXISTS face_data (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id      INTEGER NOT NULL,
    face_index   INTEGER NOT NULL CHECK(face_index BETWEEN 1 AND 3),
    face_features BLOB,               -- 人脸特征数据 (AES加密)
    face_image_path TEXT,             -- 人脸图像文件路径
    created_time TEXT    NOT NULL,
    FOREIGN KEY (user_id) REFERENCES user_account(id),
    UNIQUE(user_id, face_index)
);

-- 非法用户记录表
CREATE TABLE IF NOT EXISTS intruder_log (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    capture_time    TEXT    NOT NULL,
    face_image_path TEXT,
    thumbnail_path  TEXT,
    is_shutdown_triggered INTEGER DEFAULT 0,
    notes           TEXT
);
```

**核心接口**：
```cpp
class CDatabaseManager {
public:
    static CDatabaseManager& GetInstance();

    BOOL Initialize();         // 打开/创建数据库
    void Close();              // 关闭数据库连接

    // 用户操作
    BOOL CreateDefaultAdmin();           // 创建默认 admin/admin 账号
    BOOL UpdatePassword(const CString& username, const CString& newPassword);
    CString GetPassword(const CString& username);
    BOOL SetFirstLoginFlag(const CString& username, BOOL bFirst);
    BOOL IsFirstLogin(const CString& username);
    BOOL UpdateLastLoginTime(const CString& username);
    int  GetUserCount();                // 获取用户总数（判断是否首次运行）
    int  GetUserId(const CString& username);  // 通过用户名获取用户ID

    // 人脸操作
    int  GetFaceCount(int userId);
    BOOL InsertFaceData(int userId, int faceIndex, const std::vector<BYTE>& features, const CString& imagePath);
    BOOL DeleteFaceData(int userId, int faceIndex);
    std::vector<FaceRecord> GetAllFaces(int userId);
    BOOL DeleteAllFaces(int userId);

    // 非法用户操作
    BOOL InsertIntruderLog(const CString& captureTime, const CString& imagePath,
                           const CString& thumbnailPath, BOOL bShutdown);
    std::vector<IntruderRecord> GetAllIntruderLogs();
    int  GetIntruderLogCount();         // 获取非法用户记录数量
    BOOL DeleteAllIntruderLogs();       // 清空所有非法用户记录
    BOOL DeleteIntruderLog(int nId);    // 删除指定非法用户记录

private:
    sqlite3* m_pDB;
    CString m_strDBPath;
};
```

---

### 3.4 加密模块 (CCryptoManager) — 单例

**职责**：对密码和人脸特征数据进行 AES-256 加密/解密。

**方案**：使用 Windows CryptoAPI (BCrypt) 实现 AES-256-CBC 加密。
- 密钥：由固定盐值 + 设备唯一标识（MachineGuid）派生，通过 PBKDF2 生成
- 每次加密使用随机 IV，IV 拼接在密文前
- Base64 编码密文存储在 SQLite 中

```cpp
class CCryptoManager {
public:
    static CCryptoManager& GetInstance();

    BOOL Initialize();
    CString EncryptString(const CString& plainText);           // 字符串加密 → Base64
    CString DecryptString(const CString& encryptedBase64);     // Base64 → 解密 → 明文
    std::vector<BYTE> EncryptData(const BYTE* pData, size_t size); // 二进制加密
    std::vector<BYTE> DecryptData(const BYTE* pData, size_t size); // 二进制解密

private:
    std::vector<BYTE> m_key;  // 派生密钥
};
```

---

### 3.5 摄像头模块 (CCameraManager) — 单例

**职责**：管理摄像头设备，提供帧抓取、人脸检测、图像保存接口。

**实现**：基于 OpenCV `cv::VideoCapture`，使用 DirectShow 后端（Windows 默认）。人脸检测使用 Haar Cascade 级联分类器。

```cpp
class CCameraManager {
public:
    static CCameraManager& GetInstance();

    BOOL Initialize(int cameraIndex = 0);
    void Release();
    BOOL IsOpened() const;

    BOOL CaptureFrame(cv::Mat& frame);              // 抓取单帧原始图像 (BGR)
    BOOL CaptureFaceImage(cv::Mat& faceROI);        // 抓取并检测人脸区域 (含10%边距扩展)
    BOOL DetectFace(const cv::Mat& frame, cv::Rect& faceRect);  // Haar Cascade 人脸检测
    BOOL SaveImage(const cv::Mat& image, const CString& filePath); // 保存图像文件
    void LoadDetectionParams();           // 从配置文件重载人脸检测参数

    int GetCameraIndex() const;
    CString GetCascadePath() const;

private:
    cv::VideoCapture m_capture;           // OpenCV 摄像头
    cv::CascadeClassifier m_faceCascade;  // Haar 级联人脸检测分类器
    int m_nCameraIndex;
    BOOL m_bOpened;
    CString m_strCascadePath;             // Haar Cascade 模型文件路径

    // 人脸检测参数（可从 FaceGuard.ini [FaceDetection] 动态调整）
    int    m_nMinFaceWidth;               // 最小人脸宽度（默认50）
    int    m_nMinFaceHeight;              // 最小人脸高度（默认50）
    int    m_nMinNeighbors;               // 最小邻居检测窗口数（默认2）
    double m_dScaleFactor;                // 缩放因子（默认1.1）
};
```

**摄像头初始化流程**：
1. 使用 `cv::CAP_DSHOW`（DirectShow）后端打开指定索引的摄像头
2. 若 DirectShow 失败，回退到默认后端重试
3. 设置分辨率 640×480
4. 按优先级查找 Haar Cascade 模型文件：`程序目录 → res目录 → 当前目录`
5. 模型文件未找到时：摄像头仍可用（抓拍），但人脸检测不可用

**人脸检测流程** (DetectFace)：
1. **手动 BGR→灰度转换**：Gray = 0.114×B + 0.587×G + 0.299×R（避免 `cvtColor` 链接问题）
2. **直方图均衡化**：提高不同光照条件下的检测率
3. **多尺度检测**：`detectMultiScale`，参数从 INI 配置文件 `[FaceDetection]` 节动态读取：
   - `scaleFactor`（默认 1.1）、`minNeighbors`（默认 2）
   - `minSize`（默认 50×50，可通过 `MinFaceWidth`/`MinFaceHeight` 调整）
4. **多脸处理**：若检测到多张人脸，返回面积最大的一张（通常最靠近摄像头）

**参数加载流程**：
- `Initialize()` → 调用 `LoadDetectionParams()` → 从 `CConfigManager` 读取 `[FaceDetection]` 节参数
- 若 INI 中不存在，使用 `stdafx.h` 中定义的 `DEFAULT_MIN_FACE_WIDTH/HEIGHT`（50）和 `DEFAULT_MIN_NEIGHBORS`（2）
- 参数可在运行时通过修改 INI 文件并重启程序来调整

**IsOpened() 双重检查**：
```cpp
BOOL IsOpened() const { return m_bOpened && m_capture.isOpened(); }
```
同时验证内部标志和 OpenCV 底层状态，防止标志与实际情况不一致。

---

### 3.6 人脸识别模块 (CFaceRecognizer)

**职责**：人脸特征提取、人脸比对（人脸检测由 CameraManager 提供）。

**实现方案**：
- **人脸检测**：由 `CCameraManager::DetectFace()` 提供（OpenCV Haar Cascade），识别模块自身不负责检测
- **特征提取**：自实现 LBP (Local Binary Patterns) 直方图算法，**不使用** OpenCV 内置的 `LBPHFaceRecognizer`
- **人脸比对**：卡方距离 (Chi-Square Distance)，值越小表示越相似
- **比对阈值**：默认值由 `DEFAULT_FACE_MATCH_THRESHOLD` 定义（见 ConfigManager），可根据配置文件调整

**LBP 算法参数**：
| 参数 | 值 | 说明 |
|------|-----|------|
| LBP_RADIUS | 2 | 邻域采样半径 |
| LBP_POINTS | 8 | 圆周采样点数 |
| FACE_SIZE | 100×100 | 人脸统一缩放尺寸 |
| LBP_GRID_X / LBP_GRID_Y | 8×8 | 空间网格划分 |
| 特征维度 | 16384 | 8×8×256 bins |

```cpp
class CFaceRecognizer {
public:
    static CFaceRecognizer& GetInstance();

    BOOL Initialize();                                    // 从配置文件读取匹配阈值

    cv::Mat PreprocessFace(const cv::Mat& faceImage);     // 灰度化 → 直方图均衡 → 缩放至100×100
    cv::Mat ComputeLBPHistogram(const cv::Mat& faceImage); // 计算 LBP 直方图特征 (Uniform LBP)
    cv::Mat ExtractFeatures(const cv::Mat& faceImage);    // 预处理 + LBP 直方图（便捷方法）

    double CompareFaces(const cv::Mat& features1, const cv::Mat& features2); // 卡方距离比对
    BOOL IsFaceMatch(double dConfidence);                 // 根据阈值判断是否匹配

    double GetMatchThreshold() const;
    void SetMatchThreshold(double dThreshold);

private:
    double m_dMatchThreshold;   // 匹配阈值（从配置文件读取）
    BOOL m_bInitialized;
};
```

**LBP 特征提取流程**：
1. **预处理**：BGR→灰度（手动通道加权）→ 直方图均衡化（`equalizeHist`）→ 双线性插值缩放至 100×100
2. **LBP 编码**：对每个像素计算其周围 8 个邻域的 Uniform LBP 编码（8-bit，256 种模式）
3. **空间直方图**：将人脸分为 8×8 网格，每格独立统计 256-bin LBP 直方图
4. **归一化**：每个网格直方图除以该网格像素数
5. **拼接**：64 个网格 × 256 bins = 16384 维特征向量（CV_32FC1）

**卡方距离公式**：
```
chi2 = Σ ( (H1[i] - H2[i])² / (H1[i] + H2[i]) )
```
- 返回值 0 表示完全相同
- 同一人通常在 20-80 之间，不同人通常在 100 以上
- 分母保护：`denominator > 1e-10` 防止除零

**人脸录入流程**：
1. 抓取摄像头帧 → 检测人脸（CameraManager） → 裁剪人脸ROI（10%边距扩展）
2. 预处理：灰度化 → 直方图均衡 → 缩放至 100×100
3. 提取 LBP 直方图特征（16384 维 float 向量）
4. 特征数据序列化为字节数组 → AES-256-CBC 加密 → 存入 SQLite
5. 人脸图片保存至 `[exe_dir]\faces\`（JPEG 格式），文件名格式：`face_<index>_<timestamp>.jpg`

**人脸比对流程**：
1. 抓取帧 → 检测人脸（CameraManager） → 裁剪ROI → 提取 LBP 直方图特征
2. 从数据库读取该用户所有已录入的人脸特征 → AES 解密
3. 将解密字节数组还原为 `cv::Mat(1, nFeatureSize, CV_32FC1)`
4. 逐一计算卡方距离，取最小距离作为最佳匹配
5. 与阈值比较：`dBestDistance < m_dMatchThreshold` → 匹配成功

---

### 3.7 监控管理模块 (CMonitorManager) — 单例

**职责**：核心监控逻辑，管理定时抓拍、人脸比对、状态切换。

**状态机**：
```
┌──────────┐  合法用户  ┌──────────┐
│ NORMAL   │────────────│ NORMAL   │ (循环)
│ 60秒间隔 │            │ 60秒间隔 │
└────┬─────┘            └──────────┘
     │ 不匹配
     ▼
┌──────────┐  5次中匹配 ┌──────────┐
│ ALERT    │────────────│ NORMAL   │
│ 3秒间隔  │            │ 60秒间隔 │
└────┬─────┘
     │ 5次都不匹配
     ▼
┌──────────┐
│ SHUTDOWN │ → 1分钟后强制关机
│ 记录入侵  │
└──────────┘
```

```cpp
class CMonitorManager {
public:
    static CMonitorManager& GetInstance();

    BOOL Start();    // 启动监控
    void Stop();     // 停止监控
    BOOL IsMonitoring() const;

    // 获取当前状态
    enum MonitorState { NORMAL, ALERT, SHUTDOWN };
    MonitorState GetState() const;
    int GetRetryCount() const;

    // 设置父窗口句柄（用于向 UI 线程发送状态消息）
    void SetParentWnd(HWND hWnd);
    // 设置当前登录用户 ID（监控将比对该用户的人脸数据）
    void SetUserId(int nUserId);
    int GetUserId() const;

    // 回调
    void SetOnFaceMatchCallback(std::function<void()> callback);
    void SetOnFaceMismatchCallback(std::function<void(int retryCount)> callback);
    void SetOnIntruderConfirmedCallback(std::function<void()> callback);
    void SetOnUserReturnCallback(std::function<void()> callback);  // 合法用户回归

private:
    void MonitorLoop();        // 监控主循环（在独立线程中运行）
    BOOL DoFaceCheck();        // 执行一次人脸检查
    void ScheduleShutdown();   // 计划强制关机（获取 SE_SHUTDOWN_NAME 权限后调用 InitiateSystemShutdownEx）
    void CancelShutdown();     // 取消已计划的关机（AbortSystemShutdown）
    void LogIntruder(const cv::Mat& faceImage, BOOL bShutdown);  // 记录入侵证据
    CString GetCurrentTimeString();

    std::atomic<MonitorState> m_state;   // 当前监控状态
    std::atomic<BOOL> m_bRunning;        // 监控是否运行中
    std::atomic<int> m_nRetryCount;      // 当前重试次数
    std::atomic<BOOL> m_bHadIntruder;    // 本次会话是否有过非法用户

    std::thread m_monitorThread;         // 监控工作线程
    std::mutex m_mutex;
    HWND m_hParentWnd;                   // 父窗口句柄（用于 PostMessage 通知 UI）
    int m_nUserId;                       // 当前监控的用户 ID

    // 配置参数（从配置文件读取，默认值见 stdafx.h 宏）
    int m_nNormalInterval;
    int m_nAlertInterval;
    int m_nAlertRetryCount;
    int m_nShutdownDelay;

    // 回调函数
    std::function<void()> m_onFaceMatch;
    std::function<void(int)> m_onFaceMismatch;
    std::function<void()> m_onIntruderConfirmed;
    std::function<void()> m_onUserReturn;
};
```

**监控循环逻辑**（1秒粒度轮询，确保及时响应停止信号）：
```cpp
while (m_bRunning) {
    int nSleepInterval = (m_state == ALERT) ? m_nAlertInterval : m_nNormalInterval;

    // 使用1秒轮询，确保 Stop() 调用后最迟1秒内退出循环
    for (int i = 0; i < nSleepInterval && m_bRunning; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!m_bRunning) break;

    // 检查摄像头状态
    if (!camera.IsOpened()) continue;

    DoFaceCheck();
}
```

**DoFaceCheck() 单次检查流程**：
```
1. 抓取摄像头帧
2. 检测人脸区域（Haar Cascade）
   ├─ 未检测到人脸 → 保存帧至 noface\no_face_YYYYMMDD_HHMMSS.jpg → 返回 FALSE
   │   (用于记录"无人使用"时间段的证据)
   └─ 检测到人脸 →
3. 裁剪人脸 ROI → 提取 LBP 直方图特征 (16384维)
4. 从 SQLite 读取该用户的所有录入人脸数据
5. 逐一 AES 解密 → 还原为 cv::Mat → 卡方距离比对
6. 任一录入人脸匹配 → bMatched = TRUE
7. 根据比对结果更新状态：
   ├─ 匹配：
   │   ├─ 若之前有非法用户 (m_bHadIntruder) → PostMessage WM_INTRUDER_ALERT(0) 通知UI
   │   ├─ m_state = NORMAL, m_retryCount = 0
   │   └─ CancelShutdown()
   └─ 不匹配：
       ├─ 首次不匹配 → m_state = ALERT, m_retryCount = 0
       ├─ m_retryCount++, m_bHadIntruder = TRUE
       ├─ PostMessage WM_FACE_DETECTED(0) 和 WM_MONITOR_UPDATE(ALERT)
       ├─ LogIntruder(faceROI, FALSE) 记录入侵证据
       └─ 若 m_retryCount >= m_nAlertRetryCount：
           ├─ m_state = SHUTDOWN, m_bRunning = FALSE
           ├─ LogIntruder(faceROI, TRUE)
           ├─ PostMessage WM_INTRUDER_ALERT(1) 触发关机UI
           └─ ScheduleShutdown()
```

**自定义消息机制**（线程安全通信）：
监控线程通过 `::PostMessage(m_hParentWnd, ...)` 向 UI 线程发送以下消息：

| 消息 ID | wParam | lParam | 含义 |
|---------|--------|--------|------|
| `WM_FACE_DETECTED` | 1=匹配 / 0=不匹配 | 重试次数 | 人脸比对结果通知 |
| `WM_MONITOR_UPDATE` | MonitorState 枚举值 | 重试次数 | 监控状态变化通知（主要用于 ALERT 状态） |
| `WM_INTRUDER_ALERT` | 1=触发关机 / 0=用户回归 | 0 | 非法用户确认或合法用户回归通知 |

---

### 3.8 认证管理模块 (CAuthManager) — 单例

**职责**：管理用户登录、密码修改、记住登录状态。

```cpp
class CAuthManager {
public:
    static CAuthManager& GetInstance();

    BOOL Login(const CString& username, const CString& password);  // 登录验证
    BOOL ChangePassword(const CString& username, const CString& oldPwd, const CString& newPwd);
    BOOL IsFirstLogin(const CString& username);
    BOOL SetFirstLoginDone(const CString& username);
    void SaveLoginState(const CString& username, const CString& password); // 保存到配置文件
    BOOL AutoLogin(CString& username, CString& password);  // 自动登录（从配置文件读取）
    void ClearLoginState();

private:
    BOOL ValidatePassword(const CString& input, const CString& encrypted);
};
```

---

### 3.9 用户界面模块 (UI Dialogs)

**所有窗口均为模态对话框**，基于 MFC `CDialogEx`。

#### 3.9.1 登录对话框 (CDlgLogin)

```
┌─────────────────────────────────┐
│         FaceGuard 登录           │
│                                  │
│    用户名: [_______________]     │
│    密  码: [_______________]     │
│                                  │
│    [  登录  ]   [  退出  ]       │
└─────────────────────────────────┘
```

**逻辑**：
- 读取配置文件，如果 `RememberLogin=1`，自动填充上次登录凭据
- 第一次启动（数据库无账号），使用默认 admin/admin 创建账号
- 登录成功后，检查 `is_first_login` 标志，若为 1 则弹出密码修改窗口
- 密码修改完成后弹出人脸录入窗口
- 登录成功，保存登录状态到配置文件

#### 3.9.2 密码修改对话框 (CDlgPwd)

```
┌─────────────────────────────────┐
│        修改账号密码              │
│                                  │
│    当前密码: [_______________]   │
│    新 密 码: [_______________]   │
│    确认密码: [_______________]   │
│                                  │
│    [  确认修改  ]  [  取消  ]    │
└─────────────────────────────────┘
```

**逻辑**：
- 验证当前密码正确
- 新密码长度 ≥ 6 位，包含字母和数字
- 两次输入一致
- 修改成功后更新数据库，设置 `is_first_login=0`

#### 3.9.3 人脸录入对话框 (CDlgFace)

```
┌──────────────────────────────────────────────────┐
│              FaceGuard - 人脸录入                  │
│    ┌────────────────────────┐                    │
│    │                        │                    │
│    │    摄像头实时预览区域    │  ← 绿色人脸检测框   │
│    │    (定时器 50ms 刷新)   │                    │
│    │                        │                    │
│    └────────────────────────┘                    │
│    已录入: 2 / 3 张人脸                           │
│    ✓ 匹配: 第1张人脸 (相似距离: 45.3)             │
│                                                   │
│    [  拍照录入  ]  [  删除1  ]  [  完成  ]        │
│    [  删除2  ]    [  删除3  ]                     │
└──────────────────────────────────────────────────┘
```

**双定时器驱动**：

| 定时器 ID | 间隔 | 功能 |
|-----------|------|------|
| `PREVIEW_TIMER_ID` (101) | 50ms (~20fps) | 刷新摄像头预览画面 + 人脸检测框绘制 |
| `CAPTURE_TIMER_ID` (102) | 800ms (~1.25次/秒) | 抓拍人脸 → 提取特征 → 与已录入数据比对 |

**摄像头初始化策略**（OnInitDialog）：
1. 优先检查应用层已初始化的摄像头：`camera.IsOpened()`
2. 若未就绪，在此对话框内尝试 `camera.Initialize(0)` 回退初始化
3. 初始化成功 → 启动双定时器；失败 → 禁用拍照按钮，提示检查摄像头

**OnTimer 分发逻辑**：
```cpp
void CDlgFace::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == PREVIEW_TIMER_ID)
        RefreshPreview();          // 预览刷新
    else if (nIDEvent == CAPTURE_TIMER_ID)
        CaptureAndCompare();       // 抓拍比对
    CDialogEx::OnTimer(nIDEvent);
}
```

**CaptureAndCompare() 抓拍比对流程**：
```
定时器触发 (每800ms)
  → 抓取摄像头当前帧
  → Haar Cascade 检测人脸区域
     ├─ 未检测到人脸 → 显示 "等待人脸出现..."
     └─ 检测到人脸 →
         ├─ 裁剪人脸 ROI → 提取 LBP 直方图特征 (16384维)
         ├─ 从 SQLite 读取该用户已录入的人脸数据
         ├─ AES 解密每条特征 → 还原为 cv::Mat
         ├─ 逐一计算卡方距离，找到最佳匹配
         └─ 更新 UI 显示：
              ✓ 匹配: 第N张人脸 (相似距离: XX.X)     ← 低于阈值
              ✗ 最佳距离: XX.X (阈值: YY) — 建议录入  ← 高于阈值
```

**按钮逻辑**：
- **拍照录入** (`OnBtnSnapshot`)：检测人脸 → 提取特征 → AES 加密 → 存入 SQLite → 保存人脸图片 → 更新计数。最多 3 张，满额后禁用
- **删除按钮** (1/2/3)：删除指定索引的人脸数据，更新计数和按钮状态
- **完成** (`OnBtnComplete`)：停止两个定时器，关闭对话框。至少 1 张人脸才可点击
- **取消** (`OnCancel`)：停止两个定时器，关闭对话框

**预览刷新流程** (RefreshPreview)：
1. 抓取当前帧 → 调用 `DrawFaceRect` 绘制绿色检测框和 "Face Detected" 文字
2. 缩放至预览控件尺寸 → 手动 BGR→RGB 通道交换
3. 通过 `CreateDIBitmap` + `BitBlt` 渲染到 `IDC_STATIC_PREVIEW` 控件

**绘制与背景**：
- `OnEraseBkgnd`：填充白色背景 `RGB(255, 255, 255)`
- `OnPaint`：委托 `CDialogEx::OnPaint()` 处理默认绘制

#### 3.9.4 非法用户告警对话框 (CDlgAlert)

```
┌─────────────────────────────────────────────────────────┐
│              FaceGuard - 非法用户入侵记录                  │
│                                                          │
│  共检测到 N 次非法访问！                                   │
│                                                          │
│  ┌──────┬────────────────────┬──────┬──────────────────┐ │
│  │ 序号 │      抓拍时间        │ 关机  │    缩略图路径     │ │
│  ├──────┼────────────────────┼──────┼──────────────────┤ │
│  │  1   │ 2026-06-03 14:30   │  否  │ thumb_xxx.jpg   │ │
│  │  2   │ 2026-06-03 14:30   │  是  │ thumb_xxx.jpg   │ │
│  └──────┴────────────────────┴──────┴──────────────────┘ │
│                                                          │
│  [  查看详情  ]  [  清除记录  ]  [  关闭  ]               │
└──────────────────────────────────────────────────────────┘
```

**实现方式**：使用 MFC `CListCtrl` 报表视图（Report View），以列表形式展示非法入侵记录。

**逻辑**：
- 从 `intruder_log` 表按时间倒序读取所有记录
- 通过 `CListCtrl` 四列显示：序号 | 抓拍时间 | 是否触发关机 | 缩略图路径
- **查看详情**：选中列表行后，通过 `ShellExecute` 调用系统默认图片查看器打开原图
- **清除记录**：弹窗确认后调用 `DeleteAllIntruderLogs()` 清除数据库记录，刷新列表
- **关闭**：关闭对话框

---

### 3.10 系统托盘模块 (CTrayManager)

**职责**：管理系统托盘图标和右键菜单。

**托盘菜单**：
```
┌─────────────────┐
│ 显示主窗口       │
│ 立即检查         │
│ ─────────────── │
│ 查看非法记录     │
│ ─────────────── │
│ 关于 FaceGuard   │
│ 退出程序         │
└─────────────────┘
```

```cpp
class CTrayManager {
public:
    static CTrayManager& GetInstance();

    BOOL Create(CWnd* pParentWnd, UINT uCallbackMessage);
    void Destroy();
    void ShowBalloonTip(const CString& title, const CString& text, DWORD dwIcon = NIIF_INFO);
    void UpdateTooltip(const CString& text);
    void SetVisible(BOOL bVisible);
    BOOL IsCreated() const;
    void ShowContextMenu();  // 显示托盘右键菜单

private:
    NOTIFYICONDATA m_nid;
    BOOL m_bCreated;
    CWnd* m_pParentWnd;      // 父窗口指针
    UINT m_uCallbackMsg;     // 回调消息 ID
};
```

---

### 3.11 开机自启动模块 (CAutoStartManager)

**职责**：管理程序的开机自启动注册表设置。

**实现**：在注册表 `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run` 下写入程序路径。

```cpp
class CAutoStartManager {
public:
    static BOOL IsAutoStartEnabled();
    static BOOL EnableAutoStart(const CString& appPath);
    static BOOL DisableAutoStart();
};
```

---

### 3.12 单实例检测模块 (CSingleInstance)

**职责**：确保程序只运行一个实例（每个终端只能一个账号）。

**实现**：使用命名互斥体（Named Mutex）+ 窗口标题查找。

```cpp
class CSingleInstance {
public:
    CSingleInstance(const CString& mutexName);
    ~CSingleInstance();
    BOOL IsAlreadyRunning();
    BOOL ActivatePreviousInstance();  // 激活已运行的实例窗口

private:
    HANDLE m_hMutex;
};
```

---

### 3.13 应用程序主窗口 (CMainWnd) — 隐藏窗口

**职责**：作为 MFC 框架的主窗口，承载系统托盘图标和维持消息循环。窗口始终保持隐藏状态，程序通过托盘图标与用户交互。

**设计要点**：
- 继承自 `CFrameWnd`，使用 `WS_EX_TOOLWINDOW` 扩展样式确保不显示在任务栏
- 创建后窗口被隐藏，用户看不到主窗口
- 处理托盘消息和来自监控线程的自定义消息

```cpp
class CMainWnd : public CFrameWnd
{
public:
    CMainWnd();
    virtual ~CMainWnd();
    void InitTrayIcon();  // 初始化托盘图标（依赖 ConfigManager）

protected:
    afx_msg LRESULT OnTrayNotify(WPARAM wParam, LPARAM lParam);      // 托盘鼠标事件
    afx_msg void OnClose();                                           // 窗口关闭
    afx_msg LRESULT OnMonitorUpdate(WPARAM wParam, LPARAM lParam);   // WM_MONITOR_UPDATE
    afx_msg LRESULT OnFaceDetected(WPARAM wParam, LPARAM lParam);    // WM_FACE_DETECTED
    afx_msg LRESULT OnIntruderAlert(WPARAM wParam, LPARAM lParam);   // WM_INTRUDER_ALERT
    virtual void PostNcDestroy() override;  // 窗口销毁后 delete this + m_pMainWnd = nullptr

private:
    void OnTrayShow();      // 托盘菜单：显示主窗口（→显示关于对话框）
    void OnTrayCheck();     // 托盘菜单：立即检查（→提示监控正在运行）
    void OnTrayViewLog();   // 托盘菜单：查看非法记录（→打开 CDlgAlert）
    void OnTrayAbout();     // 托盘菜单：关于 FaceGuard
    void OnTrayExit();      // 托盘菜单：退出程序（→PostMessage WM_CLOSE）
};
```

**消息处理逻辑**：

| 消息 | 处理方式 |
|------|----------|
| `WM_TRAY_NOTIFY` + 右键 | 调用 `CTrayManager::ShowContextMenu()` 弹出托盘菜单 |
| `WM_TRAY_NOTIFY` + 双击 | 显示"关于"对话框 |
| `WM_FACE_DETECTED` (wParam=1) | 更新托盘提示为"监控中 ✓" |
| `WM_FACE_DETECTED` (wParam=0) | 更新托盘提示为"⚠ 可疑人员 (N/5)" |
| `WM_MONITOR_UPDATE` (ALERT) | 弹出气泡提示"检测到可疑人员" |
| `WM_INTRUDER_ALERT` (wParam=1) | 弹出气泡提示 + 打开 CDlgAlert（触发关机） |
| `WM_INTRUDER_ALERT` (wParam=0) | 弹出气泡提示 + 打开 CDlgAlert（用户回归查看记录） |

---

### 3.14 应用程序类 (CFaceGuardApp) — 启动流程

**职责**：协调所有模块的初始化和生命周期管理。

**启动流程** (`InitInstance()`)：
```
1. 单实例检测 (CSingleInstance)
   └─ 若已有实例 → ActivatePreviousInstance() → 退出
2. COM 初始化 (CoInitializeEx, APARTMENTTHREADED)
   └─ 某些 OpenCV 后端需要 COM 支持
3. 模块初始化 (InitializeModules)
   ├─ CConfigManager::Initialize() → EnsureDefaults()
   ├─ CCryptoManager::Initialize() (PBKDF2 密钥派生)
   ├─ CDatabaseManager::Initialize() (创建/打开 SQLite)
   ├─ CFaceRecognizer::Initialize() (读取匹配阈值)
   └─ CCameraManager::Initialize(0) (摄像头 + Haar Cascade)
4. 系统信息输出 (CSystemInfo::GetAllInfo → OutputDebugString)
5. 显示登录对话框 (ShowLoginDialog)
   ├─ 检查 RememberLogin → AutoLogin
   └─ 否则弹出 CDlgLogin → 首次登录流程 (密码修改 + 人脸录入)
6. 启动监控服务 (StartMonitoring)
   └─ CMonitorManager::SetUserId() → Start()
7. 检查开机自启动 (CheckAutoStart)
   └─ 根据配置启用/禁用注册表 Run 项
8. 创建隐藏主窗口 (new CMainWnd)
   ├─ m_pMainWnd = m_pMainHiddenWnd (MFC 消息循环)
   ├─ SetParentWnd(m_pMainHiddenWnd->GetSafeHwnd()) → 监控管理器
   └─ InitTrayIcon() → 创建托盘图标
9. 进入消息循环 (MFC Run)
```

**模块清理** (`ExitInstance` / `CleanupModules`)：
```
逆序清理：
  CMonitorManager::Stop() → CCameraManager::Release() → CDatabaseManager::Close()
COM 反初始化 (CoUninitialize)
```
```

---

## 四、数据流图

```
┌──────────────────────────────────────────────────────────────────┐
│                          程序启动                                  │
│                              │                                    │
│                    ┌─────────▼──────────┐                         │
│                    │  检查是否首次运行    │                         │
│                    └─────┬─────────┬────┘                         │
│                    首次  │         │ 非首次                         │
│                    ┌─────▼──┐  ┌───▼──────────┐                   │
│                    │创建默认 │  │ 从配置文件读取 │                   │
│                    │admin账号│  │ 上次登录凭据   │                   │
│                    └─────┬──┘  └───┬──────────┘                   │
│                          │         │                              │
│                    ┌─────▼─────────▼──────┐                       │
│                    │     登录验证          │                       │
│                    └─────┬────────────────┘                       │
│                          │ 成功                                    │
│                    ┌─────▼──────────┐                             │
│                    │ is_first_login?│                             │
│                    └──┬──────────┬──┘                             │
│                     是│          │否                                │
│              ┌───────▼──┐  ┌────▼────────┐                        │
│              │ 强制修改  │  │ 检查是否已   │                        │
│              │ 密码      │  │ 录入人脸     │                        │
│              └───────┬──┘  └──┬─────────┬─┘                       │
│                      │        │无人脸    │有人脸                     │
│              ┌───────▼──┐ ┌──▼────┐ ┌───▼────────┐                │
│              │ 人脸录入  │ │人脸录入│ │启动监控     │                │
│              │ (必录)    │ │对话框  │ │隐藏到托盘   │                │
│              └───────┬──┘ └───┬───┘ └───┬────────┘                │
│                      │        │         │                          │
│                      └────────┴─────────┘                          │
│                               │                                    │
│                    ┌──────────▼───────────┐                        │
│                    │   监控循环开始         │                        │
│                    │   每60秒抓拍图像       │                        │
│                    └──────────┬───────────┘                        │
│                               │                                    │
│                    ┌──────────▼───────────┐                        │
│                    │   检测人脸并比对      │                        │
│                    └─────┬──────────┬─────┘                        │
│                     匹配 │          │ 不匹配                         │
│                    ┌─────▼──┐  ┌────▼─────────┐                   │
│                    │继续监控 │  │ 每3秒重试5次  │                   │
│                    │(如有非法 │  │ 记录非法用户  │                   │
│                    │ 记录则   │  │ 强制关机      │                   │
│                    │ 弹窗告警)│  └──────────────┘                   │
│                    └─────────┘                                     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 五、文件目录结构

```
FaceGuard/
├── src/
│   ├── main.cpp                       # 程序入口
│   ├── stdafx.h / stdafx.cpp          # 预编译头（含所有全局宏、常量、系统头文件）
│   ├── FaceGuard.h / FaceGuard.cpp   # 应用程序类 CFaceGuardApp + 隐藏窗口 CMainWnd
│   ├── DlgLogin.h / DlgLogin.cpp     # 登录对话框
│   ├── DlgPwd.h / DlgPwd.cpp         # 密码修改对话框
│   ├── DlgFace.h / DlgFace.cpp       # 人脸录入对话框（双定时器驱动）
│   ├── DlgAlert.h / DlgAlert.cpp     # 非法用户告警对话框（CListCtrl 列表视图）
│   ├── SystemInfo.h / SystemInfo.cpp # 系统信息检测
│   ├── DatabaseManager.h/cpp         # SQLite 数据库管理
│   ├── CryptoManager.h / CryptoManager.cpp # AES-256-CBC 加密管理（Windows BCrypt）
│   ├── CameraManager.h / CameraManager.cpp # 摄像头管理 + 人脸检测
│   ├── FaceRecognizer.h/cpp          # LBP 直方图人脸特征提取与比对
│   ├── ConfigManager.h / ConfigManager.cpp # INI 配置文件管理
│   ├── AuthManager.h / AuthManager.cpp     # 认证管理
│   ├── MonitorManager.h/cpp          # 监控管理（核心，独立线程 + 自定义消息）
│   ├── TrayManager.h / TrayManager.cpp     # 系统托盘管理
│   ├── AutoStartManager.h/cpp        # 开机自启动管理
│   └── SingleInstance.h/cpp          # 单实例检测（命名互斥体）
├── bin/                              # 编译输出目录
│   ├── faceguard.db                  # SQLite 数据库（运行时自动创建）
│   ├── FaceGuard.ini                 # 配置文件（运行时自动创建）
│   ├── faces/                        # 已录入的人脸图像
│   ├── intruders/                    # 非法用户的全尺寸图像
│   ├── thumbnails/                   # 非法用户的缩略图
│   └── noface/                       # 未检测到人脸时保存的抓拍图像
├── res/
│   ├── faceguard.ico                 # 程序图标
│   ├── faceguard.rc                  # 资源文件（对话框模板、菜单等）
│   ├── resource.h                    # 资源ID定义
│   └── haarcascade_frontalface_default.xml  # OpenCV Haar Cascade 人脸检测模型
├── third_party/
│   ├── opencv/                       # OpenCV 4.3.0 预编译库
│   │   ├── include/                  # 头文件
│   │   └── lib/                      # opencv_world430.lib (Release CRT)
│   └── sqlite3/
│       ├── sqlite3.h                 # SQLite3 头文件
│       └── sqlite3.c                 # SQLite3 amalgamation 源文件
├── FaceGuard.sln                     # Visual Studio 解决方案
├── FaceGuard.vcxproj                 # 项目文件 (v141, C++17, Unicode)
├── goofy-stargazing-engelbart.md     # 本详细设计文档
├── Debug链接问题说明.md               # Debug x64 CRT 不匹配问题及解决方案
└── OpenCV_DebugCompat.props          # MSBuild 属性表（修复 Debug CRT 兼容性）
```

### 5.1 全局宏与常量定义 (stdafx.h)

| 宏/常量 | 值 | 说明 |
|---------|------|------|
| `FACEGUARD_MUTEX_NAME` | `Global\FaceGuard_SingleInstance_Mutex` | 单实例互斥体名称 |
| `FACEGUARD_APP_TITLE` | `FaceGuard` | 应用程序标题 |
| `FACEGUARD_DB_FILENAME` | `faceguard.db` | 数据库文件名 |
| `FACEGUARD_INI_FILENAME` | `FaceGuard.ini` | 配置文件名 |
| `FACEGUARD_FACE_DIR` | `faces` | 已录入人脸图像子目录 |
| `FACEGUARD_INTRUDER_DIR` | `intruders` | 非法用户图像子目录 |
| `FACEGUARD_THUMBNAIL_DIR` | `thumbnails` | 非法用户缩略图子目录 |
| `FACEGUARD_NO_FACE_DIR` | `noface` | 无人脸抓拍图像子目录 |
| `DEFAULT_FACE_MATCH_THRESHOLD` | `80.0` | 默认人脸匹配阈值（卡方距离） |
| `DEFAULT_NORMAL_INTERVAL_SECONDS` | `60` | 默认正常抓拍间隔 |
| `DEFAULT_ALERT_INTERVAL_SECONDS` | `3` | 默认告警抓拍间隔 |
| `DEFAULT_ALERT_RETRY_COUNT` | `5` | 默认告警重试次数 |
| `DEFAULT_SHUTDOWN_DELAY_SECONDS` | `60` | 默认关机延迟 |
| `DEFAULT_MIN_FACE_WIDTH` | `50` | 默认最小人脸宽度 |
| `DEFAULT_MIN_FACE_HEIGHT` | `50` | 默认最小人脸高度 |
| `DEFAULT_MIN_NEIGHBORS` | `2` | 默认 Haar Cascade 最小邻居数 |
| `DEFAULT_SCALE_FACTOR` | `1.1` | 默认 Haar Cascade 缩放因子 |

### 5.2 自定义窗口消息

| 消息 ID | 值 | 用途 |
|---------|------|------|
| `WM_TRAY_NOTIFY` | `WM_USER + 100` | 系统托盘图标事件通知 |
| `WM_MONITOR_UPDATE` | `WM_USER + 101` | 监控状态变化通知（wParam=MonitorState, lParam=重试次数） |
| `WM_FACE_DETECTED` | `WM_USER + 102` | 人脸比对结果通知（wParam: 1=匹配/0=不匹配） |
| `WM_INTRUDER_ALERT` | `WM_USER + 103` | 非法用户告警（wParam: 1=触发关机/0=用户回归） |

### 5.3 托盘菜单命令

| 命令 ID | 值 | 功能 |
|---------|------|------|
| `IDM_TRAY_SHOW` | `1000` | 显示主窗口 |
| `IDM_TRAY_CHECK` | `1001` | 立即检查 |
| `IDM_TRAY_VIEW_LOG` | `1002` | 查看非法记录 |
| `IDM_TRAY_ABOUT` | `1003` | 关于 FaceGuard |
| `IDM_TRAY_EXIT` | `1004` | 退出程序 |

---

## 六、关键技术决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| C++ 标准 | C++17 | VS2017 完全支持，提供结构化绑定、if constexpr、string_view 等 |
| 字符集 | Unicode (UTF-16) | MFC Unicode 配置，中文注释使用 UTF-8 with BOM |
| UI 框架 | MFC 原生控件 | 用户决定暂时跳过 DuiLib，先完成功能 |
| 数据库 | SQLite3 (amalgamation) | 单文件嵌入式数据库，无需安装，适合本地桌面应用 |
| 数据库位置 | 程序同目录 (`[exe_dir]\faceguard.db`) | 便携部署，整个文件夹可拷贝使用 |
| 加密 | Windows BCrypt AES-256-CBC | 系统自带，无需第三方加密库，安全性高 |
| 密钥派生 | PBKDF2 (MachineGuid + 盐值, 10000次迭代) | 绑定设备，不同机器密钥不同 |
| 摄像头 | OpenCV VideoCapture (DirectShow) | 跨摄像头兼容性好，支持 DirectShow |
| 人脸检测 | OpenCV Haar Cascade (手动BGR→灰度) | 轻量级，无需 GPU，适合定时检测场景。手动实现通道转换避免 `cvtColor` 链接问题 |
| 人脸检测参数 | INI 配置文件 `[FaceDetection]` 可调 | 默认 minSize=50×50, minNeighbors=2, scaleFactor=1.1，用户可按需调整 |
| 人脸识别 | 自实现 LBP 直方图 + 卡方距离 | 仅依赖标准 OpenCV，无需 opencv_contrib 模块。Uniform LBP (R=2, P=8)，8×8网格×256bins=16384维特征 |
| 图像存储 | 文件系统（程序同目录） | 人脸图片存为 JPEG 文件，数据库只存路径和加密特征 |
| 配置存储 | INI 文件 | 使用 Windows GetPrivateProfileString/WritePrivateProfileString API |
| 定时器（UI预览） | MFC CWnd::SetTimer + WM_TIMER | 双定时器：50ms 预览刷新 + 800ms 抓拍比对。UI 线程内执行，可直接操作控件 |
| 定时器（监控循环） | std::thread + 1秒粒度轮询 | 确保 Stop() 调用后最迟 1 秒内退出循环 |
| 跨线程通信 | Windows PostMessage + 自定义消息 | 监控线程 → UI 线程：WM_FACE_DETECTED / WM_MONITOR_UPDATE / WM_INTRUDER_ALERT |
| 强制关机 | InitiateSystemShutdownEx | Windows API，支持延迟关机和原因记录 |
| 主窗口策略 | 隐藏 CFrameWnd + 托盘图标 | 窗口使用 WS_EX_TOOLWINDOW 不显示在任务栏 |
| 无脸抓拍 | 保存至 `noface/` 目录 | 无人脸场景仍抓拍留证，记录"无人使用"时间段的证据 |

---

## 七、异常与边界情况处理

| 场景 | 处理方式 |
|------|----------|
| 摄像头未连接/被占用 | 日志记录，托盘显示"摄像头不可用"，继续尝试重连 |
| 摄像头在应用层初始化失败 | CDlgFace 在 OnInitDialog 中自动调用 `camera.Initialize(0)` 回退初始化 |
| 监控中摄像头断开 | 监控循环检测 `camera.IsOpened()` 返回 FALSE 时跳过本轮检查，1秒后重试 |
| SQLite 数据库损坏 | 自动备份，启动时检测并修复或重建 |
| 磁盘空间不足 | 监控图像文件夹大小，超出上限时自动清理旧记录 |
| 程序被强制结束 | 下次启动时检测上次运行状态，恢复监控 |
| 多实例启动 | 互斥体检测，激活已运行实例，拒绝新实例 |
| 人脸录入时未检测到人脸 | 提示用户调整位置，实时显示 "等待人脸出现..." |
| 定时抓拍比对未检测到人脸 | 保存当前帧至 `noface/` 目录作为证据留痕，不触发状态变更 |
| 已录入人脸特征解密失败 | 跳过该条记录，继续比对下一条 |
| 定时器创建失败 | 输出 Debug 日志，预览/抓拍功能静默降级（不弹错误框） |
| 加密密钥丢失 | 重新生成密钥（原有数据不可恢复，提示用户重新录入人脸） |
| 强制关机倒计时 | 提供取消按钮（需输入合法用户密码） |
| Haar Cascade 模型文件未找到 | 弹窗告警，人脸检测不可用但摄像头仍可抓拍 |
| COM 初始化失败 | InitInstance 中检查返回值，失败时退出程序 |
| 配置项缺失 | `EnsureDefaults()` 自动补全所有 `[General]` 和 `[FaceDetection]` 节默认值 |
| BCrypt API 调用失败 | `DeriveKey` 回退到简单 XOR 密钥派生，保证程序可继续运行

---

## 八、编译与构建

### 8.1 构建配置

- **平台**：Win32 (x86) / x64
- **配置**：Debug / Release
- **工具集**：Visual Studio 2017 (v141)
- **Windows SDK**：10.0.19041.0

### 8.2 依赖库

| 库 | 版本 | 用途 |
|----|------|------|
| OpenCV | 4.5.x (VS2017 兼容) | 摄像头访问、人脸检测与识别 |
| SQLite3 | 3.36+ (amalgamation) | 本地数据存储 |

### 8.3 项目配置要点

- 字符集：使用 Unicode 字符集
- 运行时库：/MD (Release) / /MDd (Debug)
- MFC 使用方式：在共享 DLL 中使用 MFC
- 预编译头：使用 stdafx.h
- 附加包含目录：`third_party\opencv\include; third_party\sqlite3`
- 附加库目录：`third_party\opencv\lib`
- 附加依赖项：`opencv_world4xx.lib`

---

## 九、验证方案

1. **编译验证**：在 VS2017 命令行下使用 MSBuild 编译 Debug 和 Release 配置，确认无编译错误和警告
2. **环境检测验证**：启动程序，在日志/关于对话框中确认自动检测的系统版本、SDK 版本正确
3. **首次启动流程**：删除配置文件和数据目录，验证 admin/admin 默认登录 → 密码修改 → 人脸录入流程
4. **自动登录验证**：重启程序，验证自动使用上次凭据登录
5. **摄像头回退验证**：程序级初始化失败时，验证 CDlgFace 自动调用 `camera.Initialize(0)` 成功恢复
6. **双定时器验证**：打开人脸录入对话框，验证预览流畅（~20fps）且抓拍比对定时触发（每800ms）
7. **实时比对验证**：坐在摄像头前，验证实时显示与已录入人脸的最佳匹配结果和相似距离
8. **人脸识别验证**：合法用户坐在摄像头前，验证每 60 秒自动识别通过
9. **非法用户检测**：换人坐在摄像头前，验证 3 秒抓拍 × 5 次后触发关机倒计时
10. **无脸抓拍验证**：无人在摄像头前时，验证 `noface/` 目录下有按时间戳命名的抓拍图像
11. **入侵证据验证**：非法用户检测后，验证 `intruders/` 和 `thumbnails/` 目录下有对应图像文件
12. **系统托盘验证**：验证窗口隐藏、托盘图标显示、右键菜单功能（5 个菜单项）
13. **托盘气泡提示**：验证告警状态下的气泡提示正常弹出
14. **开机启动验证**：重启电脑，验证程序自动启动
15. **数据加密验证**：直接查看 SQLite 数据库，确认密码和人脸数据为密文
16. **单实例验证**：尝试运行第二个程序实例，确认被阻止并激活已有实例
17. **配置补全验证**：删除 INI 部分配置项后重启，验证 `EnsureDefaults()` 自动补全缺失项
18. **配置化检测参数验证**：修改 `[FaceDetection]` 参数后重启，验证检测行为变化
19. **CListCtrl 告警窗口验证**：验证非法用户告警窗口以列表形式正确显示记录
