// SingleInstance.h
// 单实例检测模块——确保程序在同一台电脑上只运行一个实例
// 使用 Windows 命名互斥体 (Named Mutex) 实现

#pragma once

class CSingleInstance
{
public:
    // 构造函数：创建命名互斥体
    // strMutexName - 互斥体名称（用于全局唯一标识）
    explicit CSingleInstance(const CString& strMutexName);

    // 析构函数：释放互斥体
    ~CSingleInstance();

    // 检查是否已有另一个实例正在运行
    BOOL IsAlreadyRunning() const { return m_bAlreadyRunning; }

    // 尝试激活已存在的程序实例（将其主窗口置于前台）
    BOOL ActivatePreviousInstance();

private:
    HANDLE m_hMutex;            // 互斥体句柄
    BOOL m_bAlreadyRunning;     // 是否已存在运行实例
    CString m_strMutexName;     // 互斥体名称

    // 查找已存在的 FaceGuard 主窗口
    static HWND FindFaceGuardWindow();

    // 将窗口恢复到前台
    static BOOL BringWindowToTop(HWND hWnd);
};
