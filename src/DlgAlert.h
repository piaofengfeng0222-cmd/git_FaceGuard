// DlgAlert.h
// 非法用户告警对话框——显示所有检测到的非法用户记录
// 合法用户回归后自动弹出，显示入侵次数、缩略图和时间信息

#pragma once

class CDlgAlert : public CDialogEx
{
    DECLARE_DYNAMIC(CDlgAlert)

public:
    CDlgAlert(CWnd* pParent = nullptr);
    virtual ~CDlgAlert();

    enum { IDD = IDD_DLG_ALERT };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog() override;
    virtual void PostNcDestroy() override;  // 窗口销毁后通知父窗口

    // 按钮点击事件
    afx_msg void OnBtnViewDetail();    // 查看详情（放大查看某张图片）
    afx_msg void OnBtnClearLog();      // 清除所有记录
    afx_msg void OnBtnClose();         // 关闭对话框
    afx_msg void OnDblclkListIntruders(NMHDR* pNMHDR, LRESULT* pResult);  // 双击列表项

public:
    // 刷新非法用户记录（公共方法，供外部调用）
    void RefreshRecords();

    // 父窗口通知指针（用于窗口关闭时通知父窗口）
    class CMainWnd* m_pParentWnd;

private:
    // 加载非法用户记录到列表控件
    void LoadIntruderRecords();

    CListCtrl m_listIntruders;         // 非法用户记录列表
    CStatic m_staticInfo;              // 信息提示文本
};
