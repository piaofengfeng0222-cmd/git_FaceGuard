// AuthManager.h
// 认证管理模块——管理用户登录、密码修改、自动登录等功能
// 与 CDatabaseManager 和 CCryptoManager 协作完成认证流程

#pragma once

class CAuthManager
{
public:
    // 获取单例实例
    static CAuthManager& GetInstance();

    // 用户登录验证
    // strUsername - 用户名（明文）
    // strPassword - 密码（明文）
    // 返回 TRUE 表示登录成功
    BOOL Login(const CString& strUsername, const CString& strPassword);

    // 修改密码
    // strUsername - 用户名
    // strOldPassword - 旧密码（明文）
    // strNewPassword - 新密码（明文）
    BOOL ChangePassword(const CString& strUsername,
                        const CString& strOldPassword,
                        const CString& strNewPassword);

    // 检查指定用户是否首次登录（is_first_login == 1）
    BOOL IsFirstLogin(const CString& strUsername);

    // 将首次登录标志设为完成（is_first_login = 0）
    BOOL SetFirstLoginDone(const CString& strUsername);

    // 保存登录状态到配置文件（用于下次开机自动登录）
    void SaveLoginState(const CString& strUsername, const CString& strPassword);

    // 从配置文件读取上次登录凭据，尝试自动登录
    // strUsername - 输出用户名
    // strPassword - 输出密码
    // 返回 TRUE 表示配置文件中存在有效的登录凭据
    BOOL AutoLogin(CString& strUsername, CString& strPassword);

    // 清除已保存的登录状态
    void ClearLoginState();

    // 验证密码复杂度
    // 要求：长度 ≥ 6 位，包含字母和数字
    static BOOL ValidatePasswordComplexity(const CString& strPassword, CString& strErrorMsg);

private:
    CAuthManager();
    ~CAuthManager();
    CAuthManager(const CAuthManager&) = delete;
    CAuthManager& operator=(const CAuthManager&) = delete;

    // 校验输入的明文密码是否与数据库中存储的加密密码匹配
    BOOL ValidatePassword(const CString& strInput, const CString& strEncrypted);
};
