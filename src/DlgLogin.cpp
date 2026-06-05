// DlgLogin.cpp
// 登录对话框实现

#include "stdafx.h"
#include "DlgLogin.h"
#include "DlgPwd.h"
#include "DlgFace.h"
#include "AuthManager.h"
#include "DatabaseManager.h"
#include "ConfigManager.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CDlgLogin, CDialogEx)

// ============================================================================
// 消息映射
// ============================================================================
BEGIN_MESSAGE_MAP(CDlgLogin, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_LOGIN, &CDlgLogin::OnBtnLogin)
    ON_BN_CLICKED(IDC_BTN_LOGIN_EXIT, &CDlgLogin::OnBtnExit)
END_MESSAGE_MAP()

// ============================================================================
// 拦截回车键——在用户名/密码框中按回车触发登录
// ============================================================================
BOOL CDlgLogin::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = GetFocus();
        if (pFocus == &m_editUsername || pFocus == &m_editPassword)
        {
            OnBtnLogin();
            return TRUE;  // 消息已处理
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

CDlgLogin::CDlgLogin(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DLG_LOGIN, pParent)
    , m_bLoginSuccess(FALSE)
    , m_nLoggedInUserId(0)
{
}

CDlgLogin::~CDlgLogin()
{
}

// ============================================================================
// 数据交换
// ============================================================================
void CDlgLogin::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_USERNAME, m_editUsername);
    DDX_Control(pDX, IDC_EDIT_PASSWORD, m_editPassword);
}

// ============================================================================
// 对话框初始化——每次打开登录窗口时调用
// ============================================================================
BOOL CDlgLogin::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 设置对话框标题
    SetWindowText(_T("FaceGuard - 登录"));

    // 设置密码框为密码模式（显示星号）
    m_editPassword.SetPasswordChar(_T('*'));

    // 尝试自动填充上次登录的用户名和密码
    CConfigManager& config = CConfigManager::GetInstance();
    if (config.GetRememberLogin())
    {
        m_editUsername.SetWindowText(config.GetLastUsername());
        // 密码已加密存储，需要解密
        // 此处不自动解密——用户需要在登录窗口手动输入（安全考虑）
    }

    // 设置焦点到用户名编辑框
    m_editUsername.SetFocus();

    return TRUE;  // 返回 TRUE 以设置焦点到指定控件
}

// ============================================================================
// 登录按钮点击事件
// ============================================================================
void CDlgLogin::OnBtnLogin()
{
    // 获取输入
    m_editUsername.GetWindowText(m_strUsername);
    m_editPassword.GetWindowText(m_strPassword);

    // 校验输入
    if (m_strUsername.IsEmpty())
    {
        AfxMessageBox(_T("请输入用户名。"), MB_ICONWARNING);
        m_editUsername.SetFocus();
        return;
    }

    if (m_strPassword.IsEmpty())
    {
        AfxMessageBox(_T("请输入密码。"), MB_ICONWARNING);
        m_editPassword.SetFocus();
        return;
    }

    // 执行登录
    if (DoLogin())
    {
        m_bLoginSuccess = TRUE;
        EndDialog(IDOK);
    }
}

// ============================================================================
// 退出按钮点击事件
// ============================================================================
void CDlgLogin::OnBtnExit()
{
    m_bLoginSuccess = FALSE;
    EndDialog(IDCANCEL);
}

// ============================================================================
// 执行登录流程
// 1. 如果数据库中无用户，创建默认 admin 账号
// 2. 验证用户名和密码
// 3. 检查是否首次登录 → 触发密码修改和人脸录入
// ============================================================================
BOOL CDlgLogin::DoLogin()
{
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    CAuthManager& auth = CAuthManager::GetInstance();

    // 检查数据库中是否有用户（判断是否首次运行程序）
    if (db.GetUserCount() == 0)
    {
        // 首次运行——创建默认管理员账号
        if (db.CreateDefaultAdmin())
        {
            AfxMessageBox(
                _T("首次启动程序，已创建默认账号。\n\n")
                _T("用户名: admin\n")
                _T("密码:   admin\n\n")
                _T("请使用默认账号登录后修改密码。"),
                MB_ICONINFORMATION);
        }
    }

    // 登录验证
    if (!auth.Login(m_strUsername, m_strPassword))
    {
        AfxMessageBox(_T("用户名或密码错误，请重新输入。"), MB_ICONERROR);
        m_editPassword.SetSel(0, -1);
        m_editPassword.SetFocus();
        return FALSE;
    }

    // 检查是否需要处理首次登录流程（修改密码 + 录入人脸）
    if (!HandleFirstLoginFlow())
    {
        return FALSE;  // 用户取消了某个环节
    }

    // 记录登录成功的用户ID
    m_nLoggedInUserId = db.GetUserId(m_strUsername);

    // 保存登录状态（用于下次开机自动登录）
    auth.SaveLoginState(m_strUsername, m_strPassword);

    return TRUE;
}

// ============================================================================
// 处理首次登录的后续流程
// 如果 is_first_login == 1：
//   1. 强制弹出密码修改对话框（不能取消）
//   2. 强制弹出人脸录入对话框（至少录入1张人脸）
// 如果 is_first_login == 0 但没有人脸数据：
//   直接进入人脸录入
// ============================================================================
BOOL CDlgLogin::HandleFirstLoginFlow()
{
    CAuthManager& auth = CAuthManager::GetInstance();
    CDatabaseManager& db = CDatabaseManager::GetInstance();

    if (auth.IsFirstLogin(m_strUsername))
    {
        // 首次登录——必须修改密码
        AfxMessageBox(
            _T("您是首次登录，出于安全考虑，请修改默认密码。\n新密码要求：长度至少6位，包含字母和数字。"),
            MB_ICONINFORMATION);

        // 弹出密码修改对话框（模态）
        CDlgPwd dlgPwd(m_strUsername, this);
        if (dlgPwd.DoModal() != IDOK)
        {
            // 用户不允许取消密码修改
            AfxMessageBox(_T("必须修改密码后才能继续使用本软件。程序即将退出。"), MB_ICONSTOP);
            return FALSE;
        }

        // 设置首次登录完成
        auth.SetFirstLoginDone(m_strUsername);
    }

    // 检查是否已录入人脸
    int nUserId = db.GetUserId(m_strUsername);
    int nFaceCount = db.GetFaceCount(nUserId);

    if (nFaceCount == 0)
    {
        // 没有人脸数据——需要录入
        AfxMessageBox(
            _T("尚未录入人脸数据，请进行人脸录入。\n提示：请正对摄像头，在光线充足的环境下录入。"),
            MB_ICONINFORMATION);

        CDlgFace dlgFace(nUserId, this);
        if (dlgFace.DoModal() != IDOK)
        {
            // 至少需要录入1张人脸
            AfxMessageBox(_T("必须录入至少一张人脸后才能继续使用本软件。程序即将退出。"), MB_ICONSTOP);
            return FALSE;
        }
    }

    return TRUE;
}
