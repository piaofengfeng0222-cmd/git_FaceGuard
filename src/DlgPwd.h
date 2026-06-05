// DlgPwd.h
// 密码修改对话框——用户修改登录密码
// 首次登录时必须修改默认密码，后续可随时修改
// 要求：验证旧密码正确后，设置新密码（长度 ≥ 6，包含字母和数字）

#pragma once

class CDlgPwd : public CDialogEx
{
    DECLARE_DYNAMIC(CDlgPwd)

public:
    // strUsername - 当前登录的用户名（用于验证和更新密码）
    CDlgPwd(const CString& strUsername, CWnd* pParent = nullptr);
    virtual ~CDlgPwd();

    enum { IDD = IDD_DLG_PASSWORD };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog() override;

    // 按钮点击事件
    afx_msg void OnBtnSave();

private:
    CString m_strUsername;  // 当前用户名

    // 控件成员
    CEdit m_editOldPassword;
    CEdit m_editNewPassword;
    CEdit m_editConfirmPassword;
};
