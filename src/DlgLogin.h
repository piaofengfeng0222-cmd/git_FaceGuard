// DlgLogin.h
// 登录对话框——用户输入账号密码进行身份认证
// 首次启动时使用默认账号 admin/admin 登录
// 登录成功后可能触发密码修改和人脸录入流程

#pragma once

class CDlgLogin : public CDialogEx
{
    DECLARE_DYNAMIC(CDlgLogin)

public:
    CDlgLogin(CWnd* pParent = nullptr);
    virtual ~CDlgLogin();

    // 获取登录结果
    BOOL IsLoginSuccess() const { return m_bLoginSuccess; }
    CString GetUsername() const { return m_strUsername; }
    int GetLoggedInUserId() const { return m_nLoggedInUserId; }

    // 对话框ID
    enum { IDD = IDD_DLG_LOGIN };

protected:
    // 数据交换
    virtual void DoDataExchange(CDataExchange* pDX) override;

    // 消息映射
    DECLARE_MESSAGE_MAP()

    // 初始化对话框
    virtual BOOL OnInitDialog() override;

    // 键盘事件——拦截回车键触发登录
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

    // 按钮点击事件
    afx_msg void OnBtnLogin();
    afx_msg void OnBtnExit();

private:
    // 执行登录流程（验证 → 检查首次登录 → 触发后续流程）
    BOOL DoLogin();

    // 检查并处理首次登录的后续流程
    BOOL HandleFirstLoginFlow();

    // 控件成员变量
    CEdit m_editUsername;
    CEdit m_editPassword;

    // 数据成员
    CString m_strUsername;
    CString m_strPassword;
    BOOL m_bLoginSuccess;
    int m_nLoggedInUserId;
};
