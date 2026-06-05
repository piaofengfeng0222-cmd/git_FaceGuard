// ConfigManager.cpp
// 配置文件管理模块实现

#include "stdafx.h"
#include "ConfigManager.h"

// ============================================================================
// 单例实现
// ============================================================================
CConfigManager& CConfigManager::GetInstance()
{
    static CConfigManager instance;
    return instance;
}

CConfigManager::CConfigManager()
{
}

CConfigManager::~CConfigManager()
{
}

// ============================================================================
// 初始化：确定应用程序数据目录和配置文件路径
// 所有数据文件与程序同目录
// 配置文件：[exe_dir]\FaceGuard.ini
// ============================================================================
BOOL CConfigManager::Initialize()
{
    // 获取程序所在目录
    TCHAR szExePath[MAX_PATH] = { 0 };
    ::GetModuleFileName(nullptr, szExePath, MAX_PATH);
    CString strExeDir = szExePath;
    int nPos = strExeDir.ReverseFind(_T('\\'));
    if (nPos >= 0)
    {
        strExeDir = strExeDir.Left(nPos);
    }

    m_strAppDataDir = strExeDir;

    // 确保数据目录存在
    if (!EnsureDirectoryExists(m_strAppDataDir))
    {
        return FALSE;
    }

    // 配置文件路径
    m_strConfigPath.Format(_T("%s\\%s"), (LPCTSTR)m_strAppDataDir, FACEGUARD_INI_FILENAME);

    return TRUE;
}

// ============================================================================
// 确保目录存在，如果不存在则创建
// ============================================================================
BOOL CConfigManager::EnsureDirectoryExists(const CString& strPath)
{
    // 使用 SHCreateDirectoryEx 可以递归创建多级目录
    int nResult = (int)::SHCreateDirectoryEx(nullptr, strPath, nullptr);
    return (nResult == ERROR_SUCCESS || nResult == ERROR_ALREADY_EXISTS ||
            nResult == ERROR_FILE_EXISTS);
}

// ============================================================================
// INI 文件读写辅助方法
// ============================================================================
CString CConfigManager::ReadString(LPCTSTR lpSection, LPCTSTR lpKey, LPCTSTR lpDefault) const
{
    TCHAR szBuffer[4096] = { 0 };
    ::GetPrivateProfileString(lpSection, lpKey, lpDefault, szBuffer,
                              sizeof(szBuffer) / sizeof(TCHAR), m_strConfigPath);
    return CString(szBuffer);
}

void CConfigManager::WriteString(LPCTSTR lpSection, LPCTSTR lpKey, LPCTSTR lpValue)
{
    ::WritePrivateProfileString(lpSection, lpKey, lpValue, m_strConfigPath);
}

int CConfigManager::ReadInt(LPCTSTR lpSection, LPCTSTR lpKey, int nDefault) const
{
    return (int)::GetPrivateProfileInt(lpSection, lpKey, nDefault, m_strConfigPath);
}

void CConfigManager::WriteInt(LPCTSTR lpSection, LPCTSTR lpKey, int nValue)
{
    CString strValue;
    strValue.Format(_T("%d"), nValue);
    WriteString(lpSection, lpKey, strValue);
}

double CConfigManager::ReadDouble(LPCTSTR lpSection, LPCTSTR lpKey, double dDefault) const
{
    CString strDefault;
    strDefault.Format(_T("%.2f"), dDefault);
    CString strResult = ReadString(lpSection, lpKey, strDefault);
    return _ttof(strResult);
}

void CConfigManager::WriteDouble(LPCTSTR lpSection, LPCTSTR lpKey, double dValue)
{
    CString strValue;
    strValue.Format(_T("%.2f"), dValue);
    WriteString(lpSection, lpKey, strValue);
}

// ============================================================================
// 检查 INI 中是否存在指定的节和键
// 使用唯一哨兵值判断：若读取结果与哨兵完全一致则键不存在
// ============================================================================
BOOL CConfigManager::KeyExists(LPCTSTR lpSection, LPCTSTR lpKey) const
{
    static const TCHAR* SENTINEL = _T("__FG_SENTINEL_7B3A9F1C__");
    CString strResult = ReadString(lpSection, lpKey, SENTINEL);
    return (strResult != SENTINEL);
}

// ============================================================================
// 确保配置文件包含所有默认配置项
// 启动时调用，检查每项配置是否存在：
//   - 若文件不存在，WritePrivateProfileString 会自动创建
//   - 若节/键不存在，写入默认值
//   - 若已存在，保留现有值不做修改
// 涵盖 [General] 和 [FaceDetection] 两个节
// [LastLogin] 节不在此处处理，属于运行时数据
// ============================================================================
void CConfigManager::EnsureDefaults()
{
    // ======== [General] 通用配置 ========
    if (!KeyExists(_T("General"), _T("ShowTrayIcon")))
        WriteInt(_T("General"), _T("ShowTrayIcon"), 1);

    if (!KeyExists(_T("General"), _T("AutoStart")))
        WriteInt(_T("General"), _T("AutoStart"), 1);

    if (!KeyExists(_T("General"), _T("FaceMatchThreshold")))
        WriteDouble(_T("General"), _T("FaceMatchThreshold"), DEFAULT_FACE_MATCH_THRESHOLD);

    if (!KeyExists(_T("General"), _T("NormalIntervalSeconds")))
        WriteInt(_T("General"), _T("NormalIntervalSeconds"), DEFAULT_NORMAL_INTERVAL_SECONDS);

    if (!KeyExists(_T("General"), _T("AlertIntervalSeconds")))
        WriteInt(_T("General"), _T("AlertIntervalSeconds"), DEFAULT_ALERT_INTERVAL_SECONDS);

    if (!KeyExists(_T("General"), _T("AlertRetryCount")))
        WriteInt(_T("General"), _T("AlertRetryCount"), DEFAULT_ALERT_RETRY_COUNT);

    if (!KeyExists(_T("General"), _T("ShutdownDelaySeconds")))
        WriteInt(_T("General"), _T("ShutdownDelaySeconds"), DEFAULT_SHUTDOWN_DELAY_SECONDS);

    // ======== [FaceDetection] 人脸检测参数 ========
    if (!KeyExists(_T("FaceDetection"), _T("MinFaceWidth")))
        WriteInt(_T("FaceDetection"), _T("MinFaceWidth"), DEFAULT_MIN_FACE_WIDTH);

    if (!KeyExists(_T("FaceDetection"), _T("MinFaceHeight")))
        WriteInt(_T("FaceDetection"), _T("MinFaceHeight"), DEFAULT_MIN_FACE_HEIGHT);

    if (!KeyExists(_T("FaceDetection"), _T("MinNeighbors")))
        WriteInt(_T("FaceDetection"), _T("MinNeighbors"), DEFAULT_MIN_NEIGHBORS);

    if (!KeyExists(_T("FaceDetection"), _T("ScaleFactor")))
        WriteDouble(_T("FaceDetection"), _T("ScaleFactor"), DEFAULT_SCALE_FACTOR);

    OutputDebugString(_T("ConfigManager: 默认配置项检查/补全完成\n"));
}

// ======== [General] 通用配置 ========
int CConfigManager::GetShowTrayIcon() const
{
    return ReadInt(_T("General"), _T("ShowTrayIcon"), 1);
}

void CConfigManager::SetShowTrayIcon(int nValue)
{
    WriteInt(_T("General"), _T("ShowTrayIcon"), nValue);
}

int CConfigManager::GetAutoStart() const
{
    return ReadInt(_T("General"), _T("AutoStart"), 1);
}

void CConfigManager::SetAutoStart(int nValue)
{
    WriteInt(_T("General"), _T("AutoStart"), nValue);
}

double CConfigManager::GetFaceMatchThreshold() const
{
    return ReadDouble(_T("General"), _T("FaceMatchThreshold"), DEFAULT_FACE_MATCH_THRESHOLD);
}

void CConfigManager::SetFaceMatchThreshold(double dValue)
{
    WriteDouble(_T("General"), _T("FaceMatchThreshold"), dValue);
}

int CConfigManager::GetNormalIntervalSeconds() const
{
    return ReadInt(_T("General"), _T("NormalIntervalSeconds"), DEFAULT_NORMAL_INTERVAL_SECONDS);
}

void CConfigManager::SetNormalIntervalSeconds(int nValue)
{
    WriteInt(_T("General"), _T("NormalIntervalSeconds"), nValue);
}

int CConfigManager::GetAlertIntervalSeconds() const
{
    return ReadInt(_T("General"), _T("AlertIntervalSeconds"), DEFAULT_ALERT_INTERVAL_SECONDS);
}

void CConfigManager::SetAlertIntervalSeconds(int nValue)
{
    WriteInt(_T("General"), _T("AlertIntervalSeconds"), nValue);
}

int CConfigManager::GetAlertRetryCount() const
{
    return ReadInt(_T("General"), _T("AlertRetryCount"), DEFAULT_ALERT_RETRY_COUNT);
}

void CConfigManager::SetAlertRetryCount(int nValue)
{
    WriteInt(_T("General"), _T("AlertRetryCount"), nValue);
}

int CConfigManager::GetShutdownDelaySeconds() const
{
    return ReadInt(_T("General"), _T("ShutdownDelaySeconds"), DEFAULT_SHUTDOWN_DELAY_SECONDS);
}

void CConfigManager::SetShutdownDelaySeconds(int nValue)
{
    WriteInt(_T("General"), _T("ShutdownDelaySeconds"), nValue);
}

// ======== [FaceDetection] 人脸检测参数 ========
int CConfigManager::GetMinFaceWidth() const
{
    return ReadInt(_T("FaceDetection"), _T("MinFaceWidth"), DEFAULT_MIN_FACE_WIDTH);
}

int CConfigManager::GetMinFaceHeight() const
{
    return ReadInt(_T("FaceDetection"), _T("MinFaceHeight"), DEFAULT_MIN_FACE_HEIGHT);
}

int CConfigManager::GetMinNeighbors() const
{
    return ReadInt(_T("FaceDetection"), _T("MinNeighbors"), DEFAULT_MIN_NEIGHBORS);
}

double CConfigManager::GetScaleFactor() const
{
    return ReadDouble(_T("FaceDetection"), _T("ScaleFactor"), DEFAULT_SCALE_FACTOR);
}

// ======== [LastLogin] 上次登录信息 ========
CString CConfigManager::GetLastUsername() const
{
    return ReadString(_T("LastLogin"), _T("LastUsername"), _T(""));
}

void CConfigManager::SetLastUsername(const CString& strValue)
{
    WriteString(_T("LastLogin"), _T("LastUsername"), strValue);
}

CString CConfigManager::GetLastPassword() const
{
    return ReadString(_T("LastLogin"), _T("LastPassword"), _T(""));
}

void CConfigManager::SetLastPassword(const CString& strValue)
{
    WriteString(_T("LastLogin"), _T("LastPassword"), strValue);
}

int CConfigManager::GetRememberLogin() const
{
    return ReadInt(_T("LastLogin"), _T("RememberLogin"), 0);
}

void CConfigManager::SetRememberLogin(int nValue)
{
    WriteInt(_T("LastLogin"), _T("RememberLogin"), nValue);
}
