// CryptoManager.h
// 数据加密模块——使用 Windows BCrypt API 进行 AES-256-CBC 加密/解密
// 用于保护用户密码和人脸特征数据在 SQLite 中的存储安全
//
// 加密方案：
//   - 算法：AES-256-CBC
//   - 密钥派生：PBKDF2（使用固定盐值 + 设备 MachineGuid，迭代 10000 次）
//   - 每次加密使用随机 16 字节 IV，IV 拼接在密文前部
//   - 字符串加密后使用 Base64 编码存储

#pragma once

class CCryptoManager
{
public:
    // 获取单例实例
    static CCryptoManager& GetInstance();

    // 初始化加密管理器：派生密钥、初始化 BCrypt 算法提供程序
    BOOL Initialize();

    // ======== 字符串加密/解密（返回 Base64 编码的密文）========
    // 加密明文 → 返回 Base64(IV + 密文)
    CString EncryptString(const CString& strPlainText);
    // 解密 Base64(IV + 密文) → 返回明文
    CString DecryptString(const CString& strEncryptedBase64);

    // ======== 二进制数据加密/解密 ========
    // 加密二进制数据，返回 IV + 密文
    std::vector<BYTE> EncryptData(const BYTE* pData, size_t nSize);
    // 解密 IV + 密文，返回明文数据
    std::vector<BYTE> DecryptData(const BYTE* pData, size_t nSize);

    // Base64 编解码辅助方法
    static CString Base64Encode(const BYTE* pData, size_t nSize);
    static std::vector<BYTE> Base64Decode(const CString& strBase64);

private:
    CCryptoManager();
    ~CCryptoManager();
    CCryptoManager(const CCryptoManager&) = delete;
    CCryptoManager& operator=(const CCryptoManager&) = delete;

    // 从系统获取唯一的设备标识（MachineGuid）作为密钥派生基础
    static CString GetMachineGuid();

    // 使用 PBKDF2 从口令派生 AES-256 密钥
    void DeriveKey(const CString& strPassword, const std::vector<BYTE>& salt);

    // 生成随机 IV (16 字节)
    std::vector<BYTE> GenerateIV();

    BCRYPT_ALG_HANDLE m_hAesAlg;       // BCrypt AES 算法句柄
    std::vector<BYTE> m_key;           // 派生的 AES-256 密钥 (32 字节)
    BOOL m_bInitialized;               // 是否已成功初始化

    // AES 参数
    static const DWORD KEY_LENGTH = 32;        // AES-256: 32 字节密钥
    static const DWORD IV_LENGTH = 16;         // CBC 模式: 16 字节 IV
    static const DWORD PBKDF2_ITERATIONS = 10000;  // PBKDF2 迭代次数
    static const DWORD BLOCK_SIZE = 16;        // AES 块大小
};
