// DlgAlert.cpp
// 非法用户告警对话框实现

#include "stdafx.h"
#include "DlgAlert.h"
#include "FaceGuard.h"
#include "DatabaseManager.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CDlgAlert, CDialogEx)

BEGIN_MESSAGE_MAP(CDlgAlert, CDialogEx)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_INTRUDERS, &CDlgAlert::OnDblclkListIntruders)
    ON_BN_CLICKED(IDC_BTN_VIEW_DETAIL, &CDlgAlert::OnBtnViewDetail)
    ON_BN_CLICKED(IDC_BTN_CLOSE_ALERT, &CDlgAlert::OnBtnClose)
    ON_BN_CLICKED(IDC_BTN_CLEAR_LOG, &CDlgAlert::OnBtnClearLog)
END_MESSAGE_MAP()

CDlgAlert::CDlgAlert(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DLG_ALERT, pParent)
    , m_pParentWnd(nullptr)
{
}

CDlgAlert::~CDlgAlert()
{
}

void CDlgAlert::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_INTRUDERS, m_listIntruders);
    DDX_Control(pDX, IDC_STATIC_ALERT_INFO, m_staticInfo);
}

// ============================================================================
// 初始化对话框——加载非法用户记录
// ============================================================================
BOOL CDlgAlert::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("FaceGuard - 非法用户入侵记录"));

    // 设置列表控件样式
    m_listIntruders.SetExtendedStyle(
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);

    // 添加列
    m_listIntruders.InsertColumn(0, _T("序号"), LVCFMT_CENTER, 50);
    m_listIntruders.InsertColumn(1, _T("抓拍时间"), LVCFMT_LEFT, 160);
    m_listIntruders.InsertColumn(2, _T("是否触发锁屏"), LVCFMT_CENTER, 90);
    m_listIntruders.InsertColumn(3, _T("缩略图路径"), LVCFMT_LEFT, 200);

    LoadIntruderRecords();

    return TRUE;
}

// ============================================================================
// 从数据库加载非法用户记录并填充列表
// ============================================================================
void CDlgAlert::LoadIntruderRecords()
{
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    std::vector<IntruderRecord> vecRecords = db.GetAllIntruderLogs();

    int nCount = (int)vecRecords.size();

    // 更新信息文本
    CString strInfo;
    if (nCount == 0)
    {
        strInfo = _T("没有检测到非法用户记录。");
    }
    else
    {
        strInfo.Format(_T("共检测到 %d 次非法访问！"), nCount);
    }
    m_staticInfo.SetWindowText(strInfo);

    // 填充列表
    for (size_t i = 0; i < vecRecords.size(); ++i)
    {
        const auto& record = vecRecords[i];

        CString strIndex;
        strIndex.Format(_T("%d"), (int)(i + 1));
        m_listIntruders.InsertItem((int)i, strIndex);

        m_listIntruders.SetItemText((int)i, 1, record.strCaptureTime);
        m_listIntruders.SetItemText((int)i, 2, record.bShutdownTriggered ? _T("是") : _T("否"));
        m_listIntruders.SetItemText((int)i, 3, record.strThumbnailPath);
    }
}

// ============================================================================
// 查看详情按钮（放大查看选中的图像）
// ============================================================================
void CDlgAlert::OnBtnViewDetail()
{
    int nSel = m_listIntruders.GetNextItem(-1, LVNI_SELECTED);
    if (nSel < 0)
    {
        AfxMessageBox(_T("请先选择一条记录。"), MB_ICONINFORMATION);
        return;
    }

    // 获取选中记录的图像路径
    CString strPath = m_listIntruders.GetItemText(nSel, 3);

    if (!strPath.IsEmpty() && ::PathFileExists(strPath))
    {
        // 通过 ShellExecute 用默认图片查看器打开
        ::ShellExecute(nullptr, _T("open"), strPath, nullptr, nullptr, SW_SHOW);
    }
    else
    {
        AfxMessageBox(_T("图像文件不存在或已被删除。"), MB_ICONWARNING);
    }
}

// ============================================================================
// 清除记录按钮——删除所有非法用户记录
// ============================================================================
void CDlgAlert::OnBtnClearLog()
{
    if (AfxMessageBox(_T("确定要清除所有非法用户记录吗？此操作不可撤销。"),
                      MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        return;
    }

    CDatabaseManager& db = CDatabaseManager::GetInstance();
    db.DeleteAllIntruderLogs();

    // 刷新显示
    m_listIntruders.DeleteAllItems();
    LoadIntruderRecords();
}

// ============================================================================
// 关闭按钮
// ============================================================================
void CDlgAlert::OnBtnClose()
{
    DestroyWindow();
}

// ============================================================================
// 窗口销毁后清理——通知父窗口指针已失效
// ============================================================================
void CDlgAlert::PostNcDestroy()
{
    // 通知父窗口对话框已销毁
    if (m_pParentWnd != nullptr)
    {
        m_pParentWnd->ResetDlgAlert();
    }

    CDialogEx::PostNcDestroy();
    delete this;
}

// ============================================================================
// 刷新非法用户记录（公共方法，供外部调用）
// ============================================================================
void CDlgAlert::RefreshRecords()
{
    // 清空现有列表项
    m_listIntruders.DeleteAllItems();

    // 重新加载记录
    LoadIntruderRecords();

    // 如果窗口最小化，恢复显示
    if (IsIconic())
    {
        ShowWindow(SW_RESTORE);
    }

    // 确保窗口在前台（SetForegroundWindow 在 Windows 上不可靠，用 SetWindowPos 替代）
    SetWindowPos(&wndTopMost, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow();
}

// ============================================================================
// 双击列表项——用默认图片查看器打开图像
// ============================================================================
void CDlgAlert::OnDblclkListIntruders(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    *pResult = 0;

    int nItem = pNMItemActivate->iItem;
    if (nItem < 0)
        return;

    CString strPath = m_listIntruders.GetItemText(nItem, 3);

    if (!strPath.IsEmpty() && ::PathFileExists(strPath))
    {
        ::ShellExecute(nullptr, _T("open"), strPath, nullptr, nullptr, SW_SHOW);
    }
    else
    {
        AfxMessageBox(_T("图像文件不存在或已被删除。"), MB_ICONWARNING);
    }
}
