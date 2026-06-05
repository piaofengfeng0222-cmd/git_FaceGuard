// TrayManager.h
// 系统托盘管理模块——管理系统托盘的图标和右键菜单
// 使用 Windows Shell_NotifyIcon API 创建和管理托盘图标
// 支持气泡提示通知

#pragma once

class CTrayManager
{
public:
    // 获取单例实例
    static CTrayManager& GetInstance();

    // 创建托盘图标
    // pParentWnd      - 接收托盘消息的父窗口
    // uCallbackMessage - 托盘事件回调消息 ID
    BOOL Create(CWnd* pParentWnd, UINT uCallbackMessage);

    // 销毁托盘图标
    void Destroy();

    // 显示气泡提示
    // strTitle - 提示标题
    // strText  - 提示正文
    // dwIcon   - 图标类型 (NIIF_INFO, NIIF_WARNING, NIIF_ERROR)
    void ShowBalloonTip(const CString& strTitle, const CString& strText,
                        DWORD dwIcon = NIIF_INFO);

    // 更新托盘鼠标悬停提示文字
    void UpdateTooltip(const CString& strText);

    // 设置托盘图标的可见性
    void SetVisible(BOOL bVisible);

    // 是否已创建托盘图标
    BOOL IsCreated() const { return m_bCreated; }

    // 显示托盘右键菜单
    void ShowContextMenu();

private:
    CTrayManager();
    ~CTrayManager();
    CTrayManager(const CTrayManager&) = delete;
    CTrayManager& operator=(const CTrayManager&) = delete;

    NOTIFYICONDATA m_nid;    // 托盘通知数据结构
    BOOL m_bCreated;         // 是否已成功创建
    CWnd* m_pParentWnd;      // 父窗口指针
    UINT m_uCallbackMsg;     // 回调消息 ID
};
