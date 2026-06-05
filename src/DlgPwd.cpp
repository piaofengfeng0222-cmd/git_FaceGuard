// DlgPwd.cpp
// 密码修改对话框实现

#include "stdafx.h"
#include "DlgPwd.h"
#include "AuthManager.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CDlgPwd, CDialogEx)

BEGIN_MESSAGE_MAP(CDlgPwd, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_SAVE_PASSWORD, &CDlgPwd::OnBtnSave)
END_MESSAGE_MAP()

CDlgPwd::CDlgPwd(const CString& strUsername, CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DLG_PASSWORD, pParent)
    , m_strUsername(strUsername)
{
}

CDlgPwd::~CDlgPwd()
{
}

void CDlgPwd::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_OLD_PASSWORD, m_editOldPassword);
    DDX_Control(pDX, IDC_EDIT_NEW_PASSWORD, m_editNewPassword);
    DDX_Control(pDX, IDC_EDIT_CONFIRM_PASSWORD, m_editConfirmPassword);
}

BOOL CDlgPwd::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("FaceGuard - 修改密码"));

    // 设置所有密码框为密码模式
    m_editOldPassword.SetPasswordChar(_T('*'));
    m_editNewPassword.SetPasswordChar(_T('*'));
    m_editConfirmPassword.SetPasswordChar(_T('*'));

    m_editOldPassword.SetFocus();

    return TRUE;
}

// ============================================================================
// 保存按钮点击事件
// 1. 验证旧密码正确
// 2. 验证新密码复杂度
// 3. 验证两次输入一致
// 4. 更新数据库
// ============================================================================
void CDlgPwd::OnBtnSave()
{
    CString strOldPwd, strNewPwd, strConfirmPwd;

    m_editOldPassword.GetWindowText(strOldPwd);
    m_editNewPassword.GetWindowText(strNewPwd);
    m_editConfirmPassword.GetWindowText(strConfirmPwd);

    // ---- 校验旧密码 ----
    if (strOldPwd.IsEmpty())
    {
        AfxMessageBox(_T("请输入当前密码。"), MB_ICONWARNING);
        m_editOldPassword.SetFocus();
        return;
    }

    // ---- 校验新密码 ----
    if (strNewPwd.IsEmpty())
    {
        AfxMessageBox(_T("请输入新密码。"), MB_ICONWARNING);
        m_editNewPassword.SetFocus();
        return;
    }

    // 检查密码复杂度
    CString strErrorMsg;
    if (!CAuthManager::ValidatePasswordComplexity(strNewPwd, strErrorMsg))
    {
        AfxMessageBox(strErrorMsg, MB_ICONWARNING);
        m_editNewPassword.SetSel(0, -1);
        m_editNewPassword.SetFocus();
        return;
    }

    // ---- 校验确认密码 ----
    if (strNewPwd != strConfirmPwd)
    {
        AfxMessageBox(_T("两次输入的密码不一致，请重新输入。"), MB_ICONWARNING);
        m_editNewPassword.SetSel(0, -1);
        m_editNewPassword.SetFocus();
        return;
    }

    // ---- 执行密码修改 ----
    CAuthManager& auth = CAuthManager::GetInstance();

    if (!auth.ChangePassword(m_strUsername, strOldPwd, strNewPwd))
    {
        AfxMessageBox(_T("当前密码不正确，请重新输入。"), MB_ICONERROR);
        m_editOldPassword.SetSel(0, -1);
        m_editOldPassword.SetFocus();
        return;
    }

    AfxMessageBox(_T("密码修改成功！请妥善保管新密码。"), MB_ICONINFORMATION);

    EndDialog(IDOK);
}
