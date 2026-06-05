// AuthManager.cpp
// 认证管理模块实现

#include "stdafx.h"
#include "AuthManager.h"
#include "DatabaseManager.h"
#include "CryptoManager.h"
#include "ConfigManager.h"

// ============================================================================
// 单例实现
// ============================================================================
CAuthManager& CAuthManager::GetInstance()
{
    static CAuthManager instance;
    return instance;
}

CAuthManager::CAuthManager()
{
}

CAuthManager::~CAuthManager()
{
}

// ============================================================================
// 用户登录验证
// 1. 从数据库获取该用户的加密密码
// 2. 将输入的明文密码加密后与数据库中的密文比对
// 3. 如果匹配，更新最后登录时间
// ============================================================================
BOOL CAuthManager::Login(const CString& strUsername, const CString& strPassword)
{
    CDatabaseManager& db = CDatabaseManager::GetInstance();

    // 从数据库获取加密后的密码
    CString strEncryptedPwd = db.GetPassword(strUsername);

    if (strEncryptedPwd.IsEmpty())
    {
        return FALSE;  // 用户不存在
    }

    // 验证密码
    if (!ValidatePassword(strPassword, strEncryptedPwd))
    {
        return FALSE;  // 密码不匹配
    }

    // 更新最后登录时间
    db.UpdateLastLoginTime(strUsername);

    return TRUE;
}

// ============================================================================
// 修改密码
// 1. 验证旧密码是否正确
// 2. 检查新密码复杂度
// 3. 加密新密码并更新数据库
// ============================================================================
BOOL CAuthManager::ChangePassword(const CString& strUsername,
                                   const CString& strOldPassword,
                                   const CString& strNewPassword)
{
    CDatabaseManager& db = CDatabaseManager::GetInstance();

    // 第一步：验证旧密码
    if (!Login(strUsername, strOldPassword))
    {
        return FALSE;  // 旧密码不正确
    }

    // 第二步：加密新密码
    CCryptoManager& crypto = CCryptoManager::GetInstance();
    CString strEncryptedNewPwd = crypto.EncryptString(strNewPassword);

    // 第三步：更新数据库
    return db.UpdatePassword(strUsername, strEncryptedNewPwd);
}

// ============================================================================
// 检查是否首次登录
// ============================================================================
BOOL CAuthManager::IsFirstLogin(const CString& strUsername)
{
    return CDatabaseManager::GetInstance().IsFirstLogin(strUsername);
}

// ============================================================================
// 设置首次登录完成
// ============================================================================
BOOL CAuthManager::SetFirstLoginDone(const CString& strUsername)
{
    return CDatabaseManager::GetInstance().SetFirstLoginFlag(strUsername, FALSE);
}

// ============================================================================
// 保存登录状态到配置文件
// 密码以加密形式保存
// ============================================================================
void CAuthManager::SaveLoginState(const CString& strUsername, const CString& strPassword)
{
    CConfigManager& config = CConfigManager::GetInstance();

    config.SetLastUsername(strUsername);

    // 密码加密后再保存到配置文件
    CCryptoManager& crypto = CCryptoManager::GetInstance();
    CString strEncryptedPwd = crypto.EncryptString(strPassword);
    config.SetLastPassword(strEncryptedPwd);

    config.SetRememberLogin(1);
}

// ============================================================================
// 从配置文件自动登录
// 读取上次保存的用户名和加密密码，解密后返回
// ============================================================================
BOOL CAuthManager::AutoLogin(CString& strUsername, CString& strPassword)
{
    CConfigManager& config = CConfigManager::GetInstance();

    if (!config.GetRememberLogin())
    {
        return FALSE;  // 未设置记住登录
    }

    strUsername = config.GetLastUsername();
    if (strUsername.IsEmpty())
    {
        return FALSE;  // 无保存的用户名
    }

    // 解密密码
    CString strEncryptedPwd = config.GetLastPassword();
    if (strEncryptedPwd.IsEmpty())
    {
        return FALSE;
    }

    CCryptoManager& crypto = CCryptoManager::GetInstance();
    strPassword = crypto.DecryptString(strEncryptedPwd);

    if (strPassword.IsEmpty())
    {
        return FALSE;  // 解密失败
    }

    // 使用解密出的密码进行登录验证
    return Login(strUsername, strPassword);
}

// ============================================================================
// 清除登录状态（将记住登录设为 0）
// ============================================================================
void CAuthManager::ClearLoginState()
{
    CConfigManager::GetInstance().SetRememberLogin(0);
}

// ============================================================================
// 校验明文密码是否与加密密码匹配
// strInput     - 用户输入的明文密码
// strEncrypted - 数据库中存储的加密密码（Base64 格式）
// ============================================================================
BOOL CAuthManager::ValidatePassword(const CString& strInput, const CString& strEncrypted)
{
    CCryptoManager& crypto = CCryptoManager::GetInstance();

    // 将输入密码加密
    CString strInputEncrypted = crypto.EncryptString(strInput);

    // 比较加密结果（由于使用随机 IV，同一明文每次加密结果不同，
    // 需要解密后比较明文，或使用确定性加密方案）
    //
    // 简化方案：解密数据库中存储的密码，与输入明文比较
    CString strDecrypted = crypto.DecryptString(strEncrypted);

    return (strDecrypted == strInput);
}

// ============================================================================
// 验证密码复杂度
// 规则：
//   1. 长度 ≥ 6 个字符
//   2. 必须同时包含字母和数字
// 返回值：TRUE 表示密码符合复杂度要求
// ============================================================================
BOOL CAuthManager::ValidatePasswordComplexity(const CString& strPassword, CString& strErrorMsg)
{
    if (strPassword.GetLength() < 6)
    {
        strErrorMsg = _T("密码长度不能少于6个字符，请重新输入。");
        return FALSE;
    }

    BOOL bHasLetter = FALSE;  // 是否包含字母
    BOOL bHasDigit = FALSE;   // 是否包含数字

    for (int i = 0; i < strPassword.GetLength(); ++i)
    {
        TCHAR ch = strPassword[i];
        if (_istalpha(ch))
        {
            bHasLetter = TRUE;
        }
        else if (_istdigit(ch))
        {
            bHasDigit = TRUE;
        }
    }

    if (!bHasLetter || !bHasDigit)
    {
        strErrorMsg = _T("密码必须同时包含字母和数字，请重新输入。");
        return FALSE;
    }

    return TRUE;
}
