// CryptoManager.cpp
// 数据加密模块实现——使用 Windows BCrypt API (CNG) 进行 AES-256-CBC 加密

#include "stdafx.h"
#include "CryptoManager.h"

// 链接 BCrypt 库
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

// ============================================================================
// 单例实现
// ============================================================================
CCryptoManager& CCryptoManager::GetInstance()
{
    static CCryptoManager instance;
    return instance;
}

CCryptoManager::CCryptoManager()
    : m_hAesAlg(nullptr)
    , m_bInitialized(FALSE)
{
}

CCryptoManager::~CCryptoManager()
{
    if (m_hAesAlg != nullptr)
    {
        ::BCryptCloseAlgorithmProvider(m_hAesAlg, 0);
        m_hAesAlg = nullptr;
    }
}

// ============================================================================
// 初始化加密管理器
// 1. 打开 BCrypt AES 算法提供程序
// 2. 获取设备唯一标识 (MachineGuid) 作为加密种子
// 3. 使用 PBKDF2 派生 AES-256 密钥
// ============================================================================
BOOL CCryptoManager::Initialize()
{
    if (m_bInitialized)
    {
        return TRUE;
    }

    // 打开 AES 算法提供程序
    NTSTATUS status = ::BCryptOpenAlgorithmProvider(&m_hAesAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        return FALSE;
    }

    // 设置 CBC 模式
    status = ::BCryptSetProperty(m_hAesAlg, BCRYPT_CHAINING_MODE,
                                 (PBYTE)BCRYPT_CHAIN_MODE_CBC,
                                 sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    if (!BCRYPT_SUCCESS(status))
    {
        ::BCryptCloseAlgorithmProvider(m_hAesAlg, 0);
        m_hAesAlg = nullptr;
        return FALSE;
    }

    // 获取设备唯一标识作为加密种子
    CString strMachineGuid = GetMachineGuid();

    // 使用固定盐值（部分硬编码 + MachineGuid）
    std::string strSalt = "FaceGuard_Salt_2026_";
    std::string strGuid = CT2A(strMachineGuid);
    strSalt += strGuid;

    std::vector<BYTE> salt(strSalt.begin(), strSalt.end());

    // 从口令 + 盐值派生 AES-256 密钥
    DeriveKey(strMachineGuid, salt);

    m_bInitialized = TRUE;
    return TRUE;
}

// ============================================================================
// 从注册表获取 Windows 机器唯一标识 (MachineGuid)
// 位置：HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid
// ============================================================================
CString CCryptoManager::GetMachineGuid()
{
    HKEY hKey = nullptr;
    LONG lResult = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        _T("SOFTWARE\\Microsoft\\Cryptography"),
        0, KEY_READ | KEY_WOW64_64KEY, &hKey);

    if (lResult != ERROR_SUCCESS)
    {
        // 如果无法读取，使用计算机名作为后备标识
        TCHAR szComputerName[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
        ::GetComputerName(szComputerName, &dwSize);
        return CString(szComputerName);
    }

    TCHAR szValue[256] = { 0 };
    DWORD dwType = REG_SZ;
    DWORD dwSize = sizeof(szValue);

    lResult = ::RegQueryValueEx(hKey, _T("MachineGuid"), nullptr, &dwType,
                                (LPBYTE)szValue, &dwSize);
    ::RegCloseKey(hKey);

    if (lResult != ERROR_SUCCESS)
    {
        // 后备方案
        return _T("DefaultMachineGuid_FaceGuard");
    }

    return CString(szValue);
}

// ============================================================================
// 使用 PBKDF2 从口令和盐值派生 AES-256 密钥
// 参数：
//   strPassword - 派生的基础口令（设备 MachineGuid）
//   salt        - 盐值
// ============================================================================
void CCryptoManager::DeriveKey(const CString& strPassword, const std::vector<BYTE>& salt)
{
    m_key.resize(KEY_LENGTH);

    // 将宽字符口令转换为 UTF-8
    std::string strPwdUtf8 = CT2A(strPassword);

    // 使用 BCrypt PBKDF2 派生密钥
    NTSTATUS status = ::BCryptDeriveKeyPBKDF2(
        m_hAesAlg,
        (PUCHAR)strPwdUtf8.c_str(),
        (ULONG)strPwdUtf8.length(),
        (PUCHAR)salt.data(),
        (ULONG)salt.size(),
        PBKDF2_ITERATIONS,
        m_key.data(),
        KEY_LENGTH,
        0);

    if (!BCRYPT_SUCCESS(status))
    {
        // 派生失败时使用备用密钥（简单哈希）
        m_key.assign(KEY_LENGTH, 0);
        for (size_t i = 0; i < strPwdUtf8.length() && i < KEY_LENGTH; ++i)
        {
            m_key[i] = static_cast<BYTE>(strPwdUtf8[i]);
        }
    }
}

// ============================================================================
// 生成随机 16 字节初始化向量 (IV)
// ============================================================================
std::vector<BYTE> CCryptoManager::GenerateIV()
{
    std::vector<BYTE> iv(IV_LENGTH);
    NTSTATUS status = ::BCryptGenRandom(nullptr, iv.data(), IV_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status))
    {
        // 后备方案：使用基于时间的简单随机
        srand((unsigned int)::GetTickCount64());
        for (size_t i = 0; i < IV_LENGTH; ++i)
        {
            iv[i] = (BYTE)(rand() % 256);
        }
    }
    return iv;
}

// ============================================================================
// 加密字符串（宽字符 CString）
// 流程：UTF-8 编码 → AES 加密 → IV + 密文 → Base64 编码
// ============================================================================
CString CCryptoManager::EncryptString(const CString& strPlainText)
{
    if (strPlainText.IsEmpty())
    {
        return _T("");
    }

    // 将宽字符串转为 UTF-8
    std::string strUtf8 = CT2A(strPlainText, CP_UTF8);

    // 加密
    std::vector<BYTE> encrypted = EncryptData(
        reinterpret_cast<const BYTE*>(strUtf8.c_str()),
        strUtf8.length());

    // Base64 编码
    return Base64Encode(encrypted.data(), encrypted.size());
}

// ============================================================================
// 解密字符串（返回宽字符 CString）
// 流程：Base64 解码 → 分离 IV + 密文 → AES 解密 → UTF-8 解码 → CString
// ============================================================================
CString CCryptoManager::DecryptString(const CString& strEncryptedBase64)
{
    if (strEncryptedBase64.IsEmpty())
    {
        return _T("");
    }

    // Base64 解码
    std::vector<BYTE> encryptedData = Base64Decode(strEncryptedBase64);
    if (encryptedData.empty())
    {
        return _T("");
    }

    // 解密
    std::vector<BYTE> decrypted = DecryptData(encryptedData.data(), encryptedData.size());
    if (decrypted.empty())
    {
        return _T("");
    }

    // 将 UTF-8 字节流转为 CString
    std::string strUtf8(reinterpret_cast<const char*>(decrypted.data()), decrypted.size());
    return CString(CA2T(strUtf8.c_str(), CP_UTF8));
}

// ============================================================================
// AES-256-CBC 加密二进制数据
// 输出格式：[IV 16字节][密文 N字节]（使用 PKCS#7 填充）
// ============================================================================
std::vector<BYTE> CCryptoManager::EncryptData(const BYTE* pData, size_t nSize)
{
    std::vector<BYTE> result;

    if (!m_bInitialized || pData == nullptr || nSize == 0)
    {
        return result;
    }

    // 生成随机 IV
    std::vector<BYTE> iv = GenerateIV();

    // 计算 PKCS#7 填充后的数据长度
    size_t nPaddingLen = BLOCK_SIZE - (nSize % BLOCK_SIZE);
    size_t nPaddedSize = nSize + nPaddingLen;

    std::vector<BYTE> paddedData(nPaddedSize);
    memcpy(paddedData.data(), pData, nSize);
    // PKCS#7 填充：每个填充字节的值等于填充长度
    memset(paddedData.data() + nSize, (int)nPaddingLen, nPaddingLen);

    // 创建加密用的密钥对象
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status = ::BCryptGenerateSymmetricKey(m_hAesAlg, &hKey, nullptr, 0,
                                                    m_key.data(), KEY_LENGTH, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        return result;
    }

    // 加密
    ULONG nCipherTextLen = 0;
    status = ::BCryptEncrypt(hKey,
                             paddedData.data(), (ULONG)paddedData.size(),
                             nullptr,           // 不使用额外的认证数据
                             iv.data(), IV_LENGTH,
                             nullptr, 0,        // 第一次调用获取输出大小
                             &nCipherTextLen,
                             BCRYPT_BLOCK_PADDING);

    if (!BCRYPT_SUCCESS(status))
    {
        ::BCryptDestroyKey(hKey);
        return result;
    }

    // 输出 = IV + 密文
    result.resize(IV_LENGTH + nCipherTextLen);
    memcpy(result.data(), iv.data(), IV_LENGTH);

    status = ::BCryptEncrypt(hKey,
                             paddedData.data(), (ULONG)paddedData.size(),
                             nullptr,
                             iv.data(), IV_LENGTH,
                             result.data() + IV_LENGTH, nCipherTextLen,
                             &nCipherTextLen,
                             BCRYPT_BLOCK_PADDING);

    ::BCryptDestroyKey(hKey);

    if (!BCRYPT_SUCCESS(status))
    {
        result.clear();
        return result;
    }

    // 调整到实际大小
    result.resize(IV_LENGTH + nCipherTextLen);
    return result;
}

// ============================================================================
// AES-256-CBC 解密二进制数据
// 输入格式：[IV 16字节][密文 N字节]
// ============================================================================
std::vector<BYTE> CCryptoManager::DecryptData(const BYTE* pData, size_t nSize)
{
    std::vector<BYTE> result;

    if (!m_bInitialized || pData == nullptr || nSize <= IV_LENGTH)
    {
        return result;
    }

    // 前 16 字节为 IV
    std::vector<BYTE> iv(pData, pData + IV_LENGTH);

    // 剩余为密文
    const BYTE* pCipherText = pData + IV_LENGTH;
    size_t nCipherTextLen = nSize - IV_LENGTH;

    // 创建解密用的密钥对象
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status = ::BCryptGenerateSymmetricKey(m_hAesAlg, &hKey, nullptr, 0,
                                                    m_key.data(), KEY_LENGTH, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        return result;
    }

    // 获取解密后数据长度
    ULONG nPlainTextLen = 0;
    status = ::BCryptDecrypt(hKey,
                             (PUCHAR)pCipherText, (ULONG)nCipherTextLen,
                             nullptr,
                             iv.data(), IV_LENGTH,
                             nullptr, 0,
                             &nPlainTextLen,
                             BCRYPT_BLOCK_PADDING);

    if (!BCRYPT_SUCCESS(status))
    {
        ::BCryptDestroyKey(hKey);
        return result;
    }

    // 解密
    result.resize(nPlainTextLen);
    status = ::BCryptDecrypt(hKey,
                             (PUCHAR)pCipherText, (ULONG)nCipherTextLen,
                             nullptr,
                             iv.data(), IV_LENGTH,
                             result.data(), nPlainTextLen,
                             &nPlainTextLen,
                             BCRYPT_BLOCK_PADDING);

    ::BCryptDestroyKey(hKey);

    if (!BCRYPT_SUCCESS(status))
    {
        result.clear();
        return result;
    }

    // 将 result 调整为实际解密后大小
    result.resize(nPlainTextLen);

    // 移除 PKCS#7 填充
    if (!result.empty())
    {
        BYTE nPaddingLen = result.back();
        if (nPaddingLen > 0 && nPaddingLen <= BLOCK_SIZE && nPaddingLen <= result.size())
        {
            result.resize(result.size() - nPaddingLen);
        }
    }

    return result;
}

// ============================================================================
// Base64 编码
// 使用 Windows CryptBinaryToStringA API
// ============================================================================
CString CCryptoManager::Base64Encode(const BYTE* pData, size_t nSize)
{
    if (pData == nullptr || nSize == 0)
    {
        return _T("");
    }

    DWORD dwBase64Len = 0;
    if (!::CryptBinaryToStringA(pData, (DWORD)nSize,
                                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                nullptr, &dwBase64Len))
    {
        return _T("");
    }

    std::vector<CHAR> buffer(dwBase64Len + 1, 0);
    if (!::CryptBinaryToStringA(pData, (DWORD)nSize,
                                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                buffer.data(), &dwBase64Len))
    {
        return _T("");
    }

    return CString(CA2T(buffer.data(), CP_UTF8));
}

// ============================================================================
// Base64 解码
// 使用 Windows CryptStringToBinaryA API
// ============================================================================
std::vector<BYTE> CCryptoManager::Base64Decode(const CString& strBase64)
{
    if (strBase64.IsEmpty())
    {
        return {};
    }

    std::string strAnsi = CT2A(strBase64);

    DWORD dwBinaryLen = 0;
    if (!::CryptStringToBinaryA(strAnsi.c_str(), (DWORD)strAnsi.length(),
                                CRYPT_STRING_BASE64,
                                nullptr, &dwBinaryLen, nullptr, nullptr))
    {
        return {};
    }

    std::vector<BYTE> result(dwBinaryLen);
    if (!::CryptStringToBinaryA(strAnsi.c_str(), (DWORD)strAnsi.length(),
                                CRYPT_STRING_BASE64,
                                result.data(), &dwBinaryLen, nullptr, nullptr))
    {
        return {};
    }

    return result;
}
