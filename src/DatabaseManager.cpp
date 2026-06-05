// DatabaseManager.cpp
// SQLite 数据库管理模块实现
// 负责所有数据库表的创建、增删改查操作

#include "stdafx.h"
#include "DatabaseManager.h"
#include "CryptoManager.h"

// ============================================================================
// 单例实现
// ============================================================================
CDatabaseManager& CDatabaseManager::GetInstance()
{
    static CDatabaseManager instance;
    return instance;
}

CDatabaseManager::CDatabaseManager()
    : m_pDB(nullptr)
    , m_bInitialized(FALSE)
{
}

CDatabaseManager::~CDatabaseManager()
{
    Close();
}

// ============================================================================
// 初始化数据库
// 1. 确定数据库文件路径（[exe_dir]\faceguard.db）
// 2. 打开或创建 SQLite 数据库
// 3. 创建所需的表结构
// ============================================================================
BOOL CDatabaseManager::Initialize()
{
    if (m_bInitialized)
    {
        return TRUE;
    }

    // 获取程序所在目录
    TCHAR szExePath[MAX_PATH] = { 0 };
    ::GetModuleFileName(nullptr, szExePath, MAX_PATH);
    CString strDir = szExePath;
    int nPos = strDir.ReverseFind(_T('\\'));
    if (nPos >= 0)
    {
        strDir = strDir.Left(nPos);
    }

    // 确保目录存在
    ::SHCreateDirectoryEx(nullptr, strDir, nullptr);

    // 完整数据库文件路径
    m_strDBPath.Format(_T("%s\\%s"), (LPCTSTR)strDir, FACEGUARD_DB_FILENAME);

    // 将宽字符路径转为 UTF-8
    std::string strPathUtf8 = CT2A(m_strDBPath, CP_UTF8);

    // 打开数据库连接
    int nResult = sqlite3_open(strPathUtf8.c_str(), &m_pDB);
    if (nResult != SQLITE_OK)
    {
        if (m_pDB != nullptr)
        {
            sqlite3_close(m_pDB);
            m_pDB = nullptr;
        }
        return FALSE;
    }

    // 启用 WAL 模式（提高并发读写性能）
    ExecuteSQL("PRAGMA journal_mode=WAL;");

    // 启用外键约束
    ExecuteSQL("PRAGMA foreign_keys=ON;");

    // 创建表结构
    if (!CreateTables())
    {
        Close();
        return FALSE;
    }

    m_bInitialized = TRUE;
    return TRUE;
}

// ============================================================================
// 关闭数据库连接
// ============================================================================
void CDatabaseManager::Close()
{
    if (m_pDB != nullptr)
    {
        sqlite3_close(m_pDB);
        m_pDB = nullptr;
    }
    m_bInitialized = FALSE;
}

// ============================================================================
// 执行SQL语句（无返回值的 DDL/DML 语句）
// ============================================================================
BOOL CDatabaseManager::ExecuteSQL(const char* szSQL)
{
    if (m_pDB == nullptr || szSQL == nullptr)
    {
        return FALSE;
    }

    char* szErrMsg = nullptr;
    int nResult = sqlite3_exec(m_pDB, szSQL, nullptr, nullptr, &szErrMsg);

    if (nResult != SQLITE_OK)
    {
        // 记录错误日志（调试用）
        if (szErrMsg != nullptr)
        {
            OutputDebugStringA("CDatabaseManager::ExecuteSQL 错误: ");
            OutputDebugStringA(szErrMsg);
            OutputDebugStringA("\n");
            sqlite3_free(szErrMsg);
        }
        return FALSE;
    }

    return TRUE;
}

// ============================================================================
// 创建所有数据库表
// 包括：user_account（用户账号表）、face_data（人脸数据表）、intruder_log（非法记录表）
// ============================================================================
BOOL CDatabaseManager::CreateTables()
{
    // 用户账号表
    const char* szCreateUserTable =
        "CREATE TABLE IF NOT EXISTS user_account ("
        "    id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    username        TEXT    NOT NULL UNIQUE,"
        "    password        TEXT    NOT NULL,"
        "    is_first_login  INTEGER DEFAULT 1,"
        "    created_time    TEXT    NOT NULL,"
        "    last_login_time TEXT"
        ");";

    if (!ExecuteSQL(szCreateUserTable))
    {
        return FALSE;
    }

    // 人脸数据表
    const char* szCreateFaceTable =
        "CREATE TABLE IF NOT EXISTS face_data ("
        "    id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    user_id         INTEGER NOT NULL,"
        "    face_index      INTEGER NOT NULL CHECK(face_index BETWEEN 1 AND 3),"
        "    face_features   BLOB,"
        "    face_image_path TEXT,"
        "    created_time    TEXT    NOT NULL,"
        "    FOREIGN KEY (user_id) REFERENCES user_account(id),"
        "    UNIQUE(user_id, face_index)"
        ");";

    if (!ExecuteSQL(szCreateFaceTable))
    {
        return FALSE;
    }

    // 非法用户记录表
    const char* szCreateIntruderTable =
        "CREATE TABLE IF NOT EXISTS intruder_log ("
        "    id                     INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    capture_time           TEXT    NOT NULL,"
        "    face_image_path        TEXT,"
        "    thumbnail_path         TEXT,"
        "    is_shutdown_triggered  INTEGER DEFAULT 0,"
        "    notes                  TEXT"
        ");";

    if (!ExecuteSQL(szCreateIntruderTable))
    {
        return FALSE;
    }

    return TRUE;
}

// ============================================================================
// 创建默认管理员账号 (admin / admin)
// 密码以 AES 加密形式存储
// 仅在程序首次运行时调用
// ============================================================================
BOOL CDatabaseManager::CreateDefaultAdmin()
{
    // 检查是否已有用户
    if (GetUserCount() > 0)
    {
        return FALSE;  // 已有用户，不创建默认账号
    }

    // 加密默认密码
    CCryptoManager& crypto = CCryptoManager::GetInstance();
    CString strEncryptedPwd = crypto.EncryptString(_T("admin"));

    // 获取当前时间
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    TCHAR szTime[64] = { 0 };
    _tcsftime(szTime, 64, _T("%Y-%m-%d %H:%M:%S"), &timeinfo);

    // 构建SQL语句（使用参数化查询防止SQL注入）
    const char* szSQL =
        "INSERT INTO user_account (username, password, is_first_login, created_time) "
        "VALUES (?, ?, 1, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    // 绑定参数
    sqlite3_bind_text(stmt, 1, "admin", -1, SQLITE_STATIC);

    std::string strPwdUtf8 = CT2A(strEncryptedPwd, CP_UTF8);
    sqlite3_bind_text(stmt, 2, strPwdUtf8.c_str(), -1, SQLITE_TRANSIENT);

    std::string strTimeUtf8 = CT2A(CString(szTime), CP_UTF8);
    sqlite3_bind_text(stmt, 3, strTimeUtf8.c_str(), -1, SQLITE_TRANSIENT);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 更新用户密码
// ============================================================================
BOOL CDatabaseManager::UpdatePassword(const CString& strUsername, const CString& strNewPassword)
{
    if (m_pDB == nullptr)
    {
        return FALSE;
    }

    const char* szSQL = "UPDATE user_account SET password = ? WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    std::string strPwdUtf8 = CT2A(strNewPassword, CP_UTF8);
    std::string strUserUtf8 = CT2A(strUsername, CP_UTF8);

    sqlite3_bind_text(stmt, 1, strPwdUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, strUserUtf8.c_str(), -1, SQLITE_TRANSIENT);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 获取用户密码（返回加密后的 Base64 字符串）
// ============================================================================
CString CDatabaseManager::GetPassword(const CString& strUsername)
{
    CString strResult;

    if (m_pDB == nullptr)
    {
        return strResult;
    }

    const char* szSQL = "SELECT password FROM user_account WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return strResult;
    }

    std::string strUserUtf8 = CT2A(strUsername, CP_UTF8);
    sqlite3_bind_text(stmt, 1, strUserUtf8.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* szText = sqlite3_column_text(stmt, 0);
        if (szText != nullptr)
        {
            strResult = CA2T(reinterpret_cast<const char*>(szText), CP_UTF8);
        }
    }

    sqlite3_finalize(stmt);
    return strResult;
}

// ============================================================================
// 设置首次登录标志
// ============================================================================
BOOL CDatabaseManager::SetFirstLoginFlag(const CString& strUsername, BOOL bFirst)
{
    if (m_pDB == nullptr)
    {
        return FALSE;
    }

    const char* szSQL = "UPDATE user_account SET is_first_login = ? WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, bFirst ? 1 : 0);

    std::string strUserUtf8 = CT2A(strUsername, CP_UTF8);
    sqlite3_bind_text(stmt, 2, strUserUtf8.c_str(), -1, SQLITE_TRANSIENT);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 检查是否首次登录
// ============================================================================
BOOL CDatabaseManager::IsFirstLogin(const CString& strUsername)
{
    if (m_pDB == nullptr)
    {
        return TRUE;
    }

    const char* szSQL = "SELECT is_first_login FROM user_account WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return TRUE;
    }

    std::string strUserUtf8 = CT2A(strUsername, CP_UTF8);
    sqlite3_bind_text(stmt, 1, strUserUtf8.c_str(), -1, SQLITE_TRANSIENT);

    BOOL bResult = TRUE;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        bResult = (sqlite3_column_int(stmt, 0) != 0);
    }

    sqlite3_finalize(stmt);
    return bResult;
}

// ============================================================================
// 更新最后登录时间
// ============================================================================
BOOL CDatabaseManager::UpdateLastLoginTime(const CString& strUsername)
{
    if (m_pDB == nullptr)
    {
        return FALSE;
    }

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    TCHAR szTime[64] = { 0 };
    _tcsftime(szTime, 64, _T("%Y-%m-%d %H:%M:%S"), &timeinfo);

    const char* szSQL = "UPDATE user_account SET last_login_time = ? WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    std::string strTimeUtf8 = CT2A(CString(szTime), CP_UTF8);
    std::string strUserUtf8 = CT2A(strUsername, CP_UTF8);

    sqlite3_bind_text(stmt, 1, strTimeUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, strUserUtf8.c_str(), -1, SQLITE_TRANSIENT);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 获取用户总数
// ============================================================================
int CDatabaseManager::GetUserCount()
{
    if (m_pDB == nullptr)
    {
        return 0;
    }

    const char* szSQL = "SELECT COUNT(*) FROM user_account;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int nCount = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        nCount = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return nCount;
}

// ============================================================================
// 获取用户ID
// ============================================================================
int CDatabaseManager::GetUserId(const CString& strUsername)
{
    if (m_pDB == nullptr)
    {
        return -1;
    }

    const char* szSQL = "SELECT id FROM user_account WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return -1;
    }

    std::string strUserUtf8 = CT2A(strUsername, CP_UTF8);
    sqlite3_bind_text(stmt, 1, strUserUtf8.c_str(), -1, SQLITE_TRANSIENT);

    int nId = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        nId = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return nId;
}

// ============================================================================
// 获取用户已录入的人脸数量
// ============================================================================
int CDatabaseManager::GetFaceCount(int nUserId)
{
    if (m_pDB == nullptr || nUserId <= 0)
    {
        return 0;
    }

    const char* szSQL = "SELECT COUNT(*) FROM face_data WHERE user_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, nUserId);

    int nCount = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        nCount = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return nCount;
}

// ============================================================================
// 插入人脸数据
// ============================================================================
BOOL CDatabaseManager::InsertFaceData(int nUserId, int nFaceIndex,
                                       const std::vector<BYTE>& vecFeatures,
                                       const CString& strImagePath)
{
    if (m_pDB == nullptr || nUserId <= 0)
    {
        return FALSE;
    }

    // 如果该位置已有人脸数据，先删除旧的
    DeleteFaceData(nUserId, nFaceIndex);

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    TCHAR szTime[64] = { 0 };
    _tcsftime(szTime, 64, _T("%Y-%m-%d %H:%M:%S"), &timeinfo);

    const char* szSQL =
        "INSERT INTO face_data (user_id, face_index, face_features, face_image_path, created_time) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, nUserId);
    sqlite3_bind_int(stmt, 2, nFaceIndex);

    // 绑定加密后的人脸特征数据（BLOB）
    if (!vecFeatures.empty())
    {
        sqlite3_bind_blob(stmt, 3, vecFeatures.data(), (int)vecFeatures.size(), SQLITE_TRANSIENT);
    }
    else
    {
        sqlite3_bind_null(stmt, 3);
    }

    std::string strPathUtf8 = CT2A(strImagePath, CP_UTF8);
    sqlite3_bind_text(stmt, 4, strPathUtf8.c_str(), -1, SQLITE_TRANSIENT);

    std::string strTimeUtf8 = CT2A(CString(szTime), CP_UTF8);
    sqlite3_bind_text(stmt, 5, strTimeUtf8.c_str(), -1, SQLITE_TRANSIENT);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 删除指定位置的人脸数据
// ============================================================================
BOOL CDatabaseManager::DeleteFaceData(int nUserId, int nFaceIndex)
{
    if (m_pDB == nullptr)
    {
        return FALSE;
    }

    const char* szSQL = "DELETE FROM face_data WHERE user_id = ? AND face_index = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, nUserId);
    sqlite3_bind_int(stmt, 2, nFaceIndex);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 获取用户所有人脸数据
// ============================================================================
std::vector<FaceRecord> CDatabaseManager::GetAllFaces(int nUserId)
{
    std::vector<FaceRecord> vecResult;

    if (m_pDB == nullptr || nUserId <= 0)
    {
        return vecResult;
    }

    const char* szSQL =
        "SELECT id, user_id, face_index, face_features, face_image_path, created_time "
        "FROM face_data WHERE user_id = ? ORDER BY face_index;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return vecResult;
    }

    sqlite3_bind_int(stmt, 1, nUserId);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        FaceRecord record;
        record.nId = sqlite3_column_int(stmt, 0);
        record.nUserId = sqlite3_column_int(stmt, 1);
        record.nFaceIndex = sqlite3_column_int(stmt, 2);

        // 读取 BLOB 特征数据
        const void* pBlob = sqlite3_column_blob(stmt, 3);
        int nBlobSize = sqlite3_column_bytes(stmt, 3);
        if (pBlob != nullptr && nBlobSize > 0)
        {
            const BYTE* pBytes = static_cast<const BYTE*>(pBlob);
            record.vecFeatures.assign(pBytes, pBytes + nBlobSize);
        }

        // 读取图像路径
        const unsigned char* szText = sqlite3_column_text(stmt, 4);
        if (szText != nullptr)
        {
            record.strImagePath = CA2T(reinterpret_cast<const char*>(szText), CP_UTF8);
        }

        szText = sqlite3_column_text(stmt, 5);
        if (szText != nullptr)
        {
            record.strCreatedTime = CA2T(reinterpret_cast<const char*>(szText), CP_UTF8);
        }

        vecResult.push_back(record);
    }

    sqlite3_finalize(stmt);
    return vecResult;
}

// ============================================================================
// 删除用户所有人脸数据
// ============================================================================
BOOL CDatabaseManager::DeleteAllFaces(int nUserId)
{
    if (m_pDB == nullptr)
    {
        return FALSE;
    }

    const char* szSQL = "DELETE FROM face_data WHERE user_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, nUserId);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 插入非法用户记录
// ============================================================================
BOOL CDatabaseManager::InsertIntruderLog(const CString& strCaptureTime,
                                          const CString& strImagePath,
                                          const CString& strThumbnailPath,
                                          BOOL bShutdown)
{
    if (m_pDB == nullptr)
    {
        return FALSE;
    }

    const char* szSQL =
        "INSERT INTO intruder_log (capture_time, face_image_path, thumbnail_path, "
        "is_shutdown_triggered) VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    std::string strTimeUtf8 = CT2A(strCaptureTime, CP_UTF8);
    std::string strImgUtf8 = CT2A(strImagePath, CP_UTF8);
    std::string strThumbUtf8 = CT2A(strThumbnailPath, CP_UTF8);

    sqlite3_bind_text(stmt, 1, strTimeUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, strImgUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, strThumbUtf8.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, bShutdown ? 1 : 0);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}

// ============================================================================
// 获取所有非法用户记录
// ============================================================================
std::vector<IntruderRecord> CDatabaseManager::GetAllIntruderLogs()
{
    std::vector<IntruderRecord> vecResult;

    if (m_pDB == nullptr)
    {
        return vecResult;
    }

    const char* szSQL =
        "SELECT id, capture_time, face_image_path, thumbnail_path, "
        "is_shutdown_triggered, notes FROM intruder_log ORDER BY capture_time DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return vecResult;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        IntruderRecord record;
        record.nId = sqlite3_column_int(stmt, 0);

        const unsigned char* szText = sqlite3_column_text(stmt, 1);
        if (szText != nullptr)
        {
            record.strCaptureTime = CA2T(reinterpret_cast<const char*>(szText), CP_UTF8);
        }

        szText = sqlite3_column_text(stmt, 2);
        if (szText != nullptr)
        {
            record.strImagePath = CA2T(reinterpret_cast<const char*>(szText), CP_UTF8);
        }

        szText = sqlite3_column_text(stmt, 3);
        if (szText != nullptr)
        {
            record.strThumbnailPath = CA2T(reinterpret_cast<const char*>(szText), CP_UTF8);
        }

        record.bShutdownTriggered = (sqlite3_column_int(stmt, 4) != 0);

        szText = sqlite3_column_text(stmt, 5);
        if (szText != nullptr)
        {
            record.strNotes = CA2T(reinterpret_cast<const char*>(szText), CP_UTF8);
        }

        vecResult.push_back(record);
    }

    sqlite3_finalize(stmt);
    return vecResult;
}

// ============================================================================
// 获取非法用户记录数量
// ============================================================================
int CDatabaseManager::GetIntruderLogCount()
{
    if (m_pDB == nullptr)
    {
        return 0;
    }

    const char* szSQL = "SELECT COUNT(*) FROM intruder_log;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int nCount = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        nCount = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return nCount;
}

// ============================================================================
// 删除所有非法用户记录
// ============================================================================
BOOL CDatabaseManager::DeleteAllIntruderLogs()
{
    return ExecuteSQL("DELETE FROM intruder_log;");
}

// ============================================================================
// 删除指定非法用户记录
// ============================================================================
BOOL CDatabaseManager::DeleteIntruderLog(int nId)
{
    if (m_pDB == nullptr)
    {
        return FALSE;
    }

    const char* szSQL = "DELETE FROM intruder_log WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, nId);

    int nResult = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (nResult == SQLITE_DONE);
}
