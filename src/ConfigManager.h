// ConfigManager.h
// 配置文件管理模块——读写程序配置文件 (FaceGuard.ini)
// 使用 Windows INI 文件 API 进行配置管理
// 配置文件位于程序和用户数据目录下

#pragma once

class CConfigManager
{
public:
    // 获取单例实例
    static CConfigManager& GetInstance();

    // 初始化：确定配置文件的完整路径并确保目录存在
    BOOL Initialize();

    // 确保配置文件存在且包含所有默认配置项
    // 检查 [General] 和 [FaceDetection] 各节的所有键，
    // 对缺失的键自动补写默认值
    void EnsureDefaults();

    // 获取配置文件完整路径
    CString GetConfigFilePath() const { return m_strConfigPath; }

    // 获取应用程序数据目录路径
    CString GetAppDataDir() const { return m_strAppDataDir; }

    // ======== [General] 通用配置 ========
    int     GetShowTrayIcon() const;
    void    SetShowTrayIcon(int nValue);

    int     GetAutoStart() const;
    void    SetAutoStart(int nValue);

    double  GetFaceMatchThreshold() const;
    void    SetFaceMatchThreshold(double dValue);

    int     GetNormalIntervalSeconds() const;
    void    SetNormalIntervalSeconds(int nValue);

    int     GetAlertIntervalSeconds() const;
    void    SetAlertIntervalSeconds(int nValue);

    int     GetAlertRetryCount() const;
    void    SetAlertRetryCount(int nValue);

    int     GetShutdownDelaySeconds() const;
    void    SetShutdownDelaySeconds(int nValue);

    // ======== [FaceDetection] 人脸检测参数 ========
    int     GetMinFaceWidth() const;
    int     GetMinFaceHeight() const;
    int     GetMinNeighbors() const;
    double  GetScaleFactor() const;

    // ======== [LastLogin] 上次登录信息 ========
    CString GetLastUsername() const;
    void    SetLastUsername(const CString& strValue);

    CString GetLastPassword() const;  // 加密后的密码
    void    SetLastPassword(const CString& strValue);

    int     GetRememberLogin() const;
    void    SetRememberLogin(int nValue);

private:
    CConfigManager();
    ~CConfigManager();
    CConfigManager(const CConfigManager&) = delete;
    CConfigManager& operator=(const CConfigManager&) = delete;

    // 确保目录存在
    BOOL EnsureDirectoryExists(const CString& strPath);

    // 辅助方法：读写 INI
    CString ReadString(LPCTSTR lpSection, LPCTSTR lpKey, LPCTSTR lpDefault) const;
    void    WriteString(LPCTSTR lpSection, LPCTSTR lpKey, LPCTSTR lpValue);
    int     ReadInt(LPCTSTR lpSection, LPCTSTR lpKey, int nDefault) const;
    void    WriteInt(LPCTSTR lpSection, LPCTSTR lpKey, int nValue);
    double  ReadDouble(LPCTSTR lpSection, LPCTSTR lpKey, double dDefault) const;
    void    WriteDouble(LPCTSTR lpSection, LPCTSTR lpKey, double dValue);

    // 检查 INI 中是否存在指定的节和键
    BOOL    KeyExists(LPCTSTR lpSection, LPCTSTR lpKey) const;

    CString m_strConfigPath;   // 配置文件完整路径
    CString m_strAppDataDir;   // 应用程序数据目录
};
