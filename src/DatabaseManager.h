// DatabaseManager.h
// SQLite 数据库管理模块——管理所有数据的持久化存储
// 数据库文件位置：%APPDATA%\FaceGuard\faceguard.db
// 包含三张表：user_account（用户账号）、face_data（人脸数据）、intruder_log（非法用户记录）

#pragma once

// 人脸数据记录结构体
struct FaceRecord
{
    int nId;
    int nUserId;
    int nFaceIndex;
    std::vector<BYTE> vecFeatures;  // 加密后的人脸特征数据
    CString strImagePath;
    CString strCreatedTime;
};

// 非法用户记录结构体
struct IntruderRecord
{
    int nId;
    CString strCaptureTime;
    CString strImagePath;
    CString strThumbnailPath;
    BOOL bShutdownTriggered;
    CString strNotes;
};

class CDatabaseManager
{
public:
    // 获取单例实例
    static CDatabaseManager& GetInstance();

    // 初始化：打开/创建数据库并建表
    BOOL Initialize();

    // 关闭数据库连接
    void Close();

    // 获取数据库文件路径
    CString GetDatabasePath() const { return m_strDBPath; }

    // ======== 用户账号操作 ========

    // 创建默认管理员账号 (admin/admin)，仅在首次运行时调用
    BOOL CreateDefaultAdmin();

    // 更新用户密码（新密码以加密形式存储）
    BOOL UpdatePassword(const CString& strUsername, const CString& strNewPassword);

    // 获取用户密码（返回加密后的密码字符串）
    CString GetPassword(const CString& strUsername);

    // 设置/获取首次登录标志
    BOOL SetFirstLoginFlag(const CString& strUsername, BOOL bFirst);
    BOOL IsFirstLogin(const CString& strUsername);

    // 更新最后登录时间
    BOOL UpdateLastLoginTime(const CString& strUsername);

    // 检查数据库中是否存在用户账号（用于判断是否首次运行）
    int GetUserCount();

    // ======== 人脸数据操作 ========

    // 获取用户已录入的人脸数量
    int GetFaceCount(int nUserId);

    // 插入新的人脸数据
    BOOL InsertFaceData(int nUserId, int nFaceIndex,
                        const std::vector<BYTE>& vecFeatures,
                        const CString& strImagePath);

    // 删除指定位置的人脸数据
    BOOL DeleteFaceData(int nUserId, int nFaceIndex);

    // 获取用户的所有人脸数据
    std::vector<FaceRecord> GetAllFaces(int nUserId);

    // 删除用户的所有人脸数据
    BOOL DeleteAllFaces(int nUserId);

    // 获取用户ID（通过用户名）
    int GetUserId(const CString& strUsername);

    // ======== 非法用户记录操作 ========

    // 插入非法用户记录
    BOOL InsertIntruderLog(const CString& strCaptureTime,
                           const CString& strImagePath,
                           const CString& strThumbnailPath,
                           BOOL bShutdown);

    // 获取所有非法用户记录
    std::vector<IntruderRecord> GetAllIntruderLogs();

    // 获取非法用户记录数量
    int GetIntruderLogCount();

    // 删除所有非法用户记录
    BOOL DeleteAllIntruderLogs();

    // 删除指定的非法用户记录
    BOOL DeleteIntruderLog(int nId);

private:
    CDatabaseManager();
    ~CDatabaseManager();
    CDatabaseManager(const CDatabaseManager&) = delete;
    CDatabaseManager& operator=(const CDatabaseManager&) = delete;

    // 创建所有需要的表
    BOOL CreateTables();

    // 执行 SQL 语句（无返回值）
    BOOL ExecuteSQL(const char* szSQL);

    sqlite3* m_pDB;          // SQLite 数据库句柄
    CString m_strDBPath;     // 数据库文件路径
    BOOL m_bInitialized;     // 是否已初始化
};
