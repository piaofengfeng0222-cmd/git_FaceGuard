// DlgFace.cpp
// 人脸录入对话框实现——摄像头实时预览 + 人脸检测框 + 拍照录入 + 已录入管理

#include "stdafx.h"
#include "DlgFace.h"
#include "CameraManager.h"
#include "FaceRecognizer.h"
#include "DatabaseManager.h"
#include "CryptoManager.h"
#include "ConfigManager.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CDlgFace, CDialogEx)

#define PREVIEW_TIMER_ID    101
#define PREVIEW_TIMER_MS    50   // 预览刷新间隔（毫秒），约20fps
#define CAPTURE_TIMER_ID    102
#define CAPTURE_TIMER_MS    800  // 抓拍比对间隔（毫秒），约1.25次/秒

BEGIN_MESSAGE_MAP(CDlgFace, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_SNAPSHOT, &CDlgFace::OnBtnSnapshot)
    ON_BN_CLICKED(IDC_BTN_FACE_COMPLETE, &CDlgFace::OnBtnComplete)
    ON_BN_CLICKED(IDC_BTN_DELETE_FACE1, &CDlgFace::OnDeleteFace1)
    ON_BN_CLICKED(IDC_BTN_DELETE_FACE2, &CDlgFace::OnDeleteFace2)
    ON_BN_CLICKED(IDC_BTN_DELETE_FACE3, &CDlgFace::OnDeleteFace3)
    ON_WM_TIMER()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CDlgFace::CDlgFace(int nUserId, CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DLG_FACE, pParent)
    , m_nUserId(nUserId)
    , m_nFaceCount(0)
    , m_nTimerID(0)          // 0 = 未设置定时器
    , m_nCaptureTimerID(0)   // 0 = 未设置抓拍定时器
    , m_bCameraReady(FALSE)
{
}

CDlgFace::~CDlgFace()
{
}

void CDlgFace::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATIC_PREVIEW, m_staticPreview);
    DDX_Control(pDX, IDC_STATIC_FACE_INFO, m_staticFaceInfo);
    DDX_Control(pDX, IDC_BTN_SNAPSHOT, m_btnSnapshot);
    DDX_Control(pDX, IDC_BTN_FACE_COMPLETE, m_btnComplete);
    DDX_Control(pDX, IDC_BTN_DELETE_FACE1, m_btnDeleteFace[0]);
    DDX_Control(pDX, IDC_BTN_DELETE_FACE2, m_btnDeleteFace[1]);
    DDX_Control(pDX, IDC_BTN_DELETE_FACE3, m_btnDeleteFace[2]);
}

// ============================================================================
// 对话框初始化
// ============================================================================
BOOL CDlgFace::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("FaceGuard - 人脸录入"));

    // 获取当前已录入的人脸数量
    m_nFaceCount = CDatabaseManager::GetInstance().GetFaceCount(m_nUserId);

    // 初始化摄像头
    CCameraManager& camera = CCameraManager::GetInstance();
    m_bCameraReady = camera.IsOpened();

    OutputDebugString(_T("DlgFace::OnInitDialog - 摄像头状态检查\n"));
    TCHAR szDbg[256];

    if (!m_bCameraReady)
    {
        // 尝试在此初始化摄像头（处理应用层初始化失败或摄像头未打开的情况）
        OutputDebugString(_T("DlgFace: 摄像头未就绪，尝试调用 camera.Initialize(0)...\n"));
        if (camera.Initialize(0))
        {
            m_bCameraReady = TRUE;
            OutputDebugString(_T("DlgFace: camera.Initialize(0) 成功\n"));
        }
        else
        {
            OutputDebugString(_T("DlgFace: camera.Initialize(0) 失败\n"));
        }
    }

    if (!m_bCameraReady)
    {
        m_staticFaceInfo.SetWindowText(_T("⚠ 摄像头未就绪，无法进行人脸录入。\n请检查摄像头连接。"));
        m_btnSnapshot.EnableWindow(FALSE);
        OutputDebugString(_T("DlgFace: 摄像头未就绪，不启动预览定时器\n"));
    }
    else
    {
        // 启动预览定时器
        m_nTimerID = SetTimer(PREVIEW_TIMER_ID, PREVIEW_TIMER_MS, nullptr);
        _stprintf_s(szDbg, _T("DlgFace: SetTimer(ID=%u, MS=%u) 返回 m_nTimerID=%zu, HWND=0x%p\n"),
                    (UINT)PREVIEW_TIMER_ID, (UINT)PREVIEW_TIMER_MS,
                    m_nTimerID, (void*)GetSafeHwnd());
        OutputDebugString(szDbg);

        if (m_nTimerID == 0)
        {
            OutputDebugString(_T("DlgFace: 错误——SetTimer失败！\n"));
        }

        // 启动定时抓拍比对定时器
        m_nCaptureTimerID = SetTimer(CAPTURE_TIMER_ID, CAPTURE_TIMER_MS, nullptr);
        _stprintf_s(szDbg, _T("DlgFace: SetTimer(CAPTURE) ID=%u, MS=%u 返回 m_nCaptureTimerID=%zu\n"),
                    (UINT)CAPTURE_TIMER_ID, (UINT)CAPTURE_TIMER_MS, m_nCaptureTimerID);
        OutputDebugString(szDbg);
    }

    // 更新显示
    UpdateFaceCountDisplay();

    // 如果已有3张人脸，"拍照录入"按钮禁用
    if (m_nFaceCount >= 3)
    {
        m_btnSnapshot.EnableWindow(FALSE);
        m_staticFaceInfo.SetWindowText(_T("已录入上限（3张），请先删除旧数据再录入。"));
    }

    // 至少1张人脸才能点击"完成"
    m_btnComplete.EnableWindow(m_nFaceCount >= 1);

    _stprintf_s(szDbg, _T("DlgFace::OnInitDialog 完成, m_nTimerID=%zu, m_bCameraReady=%d\n"),
                m_nTimerID, (int)m_bCameraReady);
    OutputDebugString(szDbg);

    return TRUE;
}

// ============================================================================
// 取消对话框时停止定时器
// ============================================================================
void CDlgFace::OnCancel()
{
    if (m_nTimerID != 0)
    {
        KillTimer(m_nTimerID);
        m_nTimerID = 0;
    }
    if (m_nCaptureTimerID != 0)
    {
        KillTimer(m_nCaptureTimerID);
        m_nCaptureTimerID = 0;
    }
    CDialogEx::OnCancel();
}

// ============================================================================
// 定时器回调——刷新摄像头预览画面
// ============================================================================
void CDlgFace::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == PREVIEW_TIMER_ID)
    {
        RefreshPreview();
    }
    else if (nIDEvent == CAPTURE_TIMER_ID)
    {
        CaptureAndCompare();
    }

    CDialogEx::OnTimer(nIDEvent);
}

// ============================================================================
// 刷新预览画面
// 从摄像头抓取最新一帧，检测人脸并绘制检测框，显示在预览控件中
// ============================================================================
void CDlgFace::RefreshPreview()
{
    CCameraManager& camera = CCameraManager::GetInstance();

    if (!camera.IsOpened())
    {
        return;
    }

    if (camera.CaptureFrame(m_currentFrame))
    {
        // 检测人脸并绘制框
        DrawFaceRect(m_currentFrame);

        // 将 OpenCV 的 BGR Mat 转为 MFC 可显示的位图
        // 获取预览控件的尺寸
        CRect rectPreview;
        m_staticPreview.GetClientRect(&rectPreview);

        // 缩放图像以适应预览控件
        cv::Mat displayFrame;
        cv::resize(m_currentFrame, displayFrame,
                   cv::Size(rectPreview.Width(), rectPreview.Height()));

        // 手动 BGR→RGB 通道交换（避免 cvtColor/mixChannels 链接问题）
        {
            cv::Mat temp = displayFrame.clone();
            for (int y = 0; y < temp.rows; ++y) {
                for (int x = 0; x < temp.cols; ++x) {
                    unsigned char* p = temp.ptr<unsigned char>(y) + x * 3;
                    unsigned char t = p[0]; p[0] = p[2]; p[2] = t;  // 交换 B 和 R
                }
            }
            displayFrame = temp;
        }

        // 创建 GDI 位图并显示
        CDC* pDC = m_staticPreview.GetDC();
        if (pDC != nullptr)
        {
            CDC memDC;
            memDC.CreateCompatibleDC(pDC);

            BITMAPINFO bmi = { 0 };
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = displayFrame.cols;
            bmi.bmiHeader.biHeight = -displayFrame.rows;  // 负值表示从上到下
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 24;
            bmi.bmiHeader.biCompression = BI_RGB;

            CBitmap bitmap;
            bitmap.Attach(::CreateDIBitmap(pDC->GetSafeHdc(), &bmi.bmiHeader,
                                  CBM_INIT, displayFrame.data, &bmi,
                                  DIB_RGB_COLORS));

            CBitmap* pOldBitmap = memDC.SelectObject(&bitmap);
            pDC->BitBlt(0, 0, rectPreview.Width(), rectPreview.Height(),
                        &memDC, 0, 0, SRCCOPY);
            memDC.SelectObject(pOldBitmap);

            m_staticPreview.ReleaseDC(pDC);
        }
    }
}

// ============================================================================
// 定时抓拍人脸并与已录入数据进行比对
// 从当前摄像头画面中检测人脸 → 提取特征 → 与数据库中存储的人脸比对
// 将比对结果更新到界面提示文本中
// ============================================================================
void CDlgFace::CaptureAndCompare()
{
    CCameraManager& camera = CCameraManager::GetInstance();
    CFaceRecognizer& recognizer = CFaceRecognizer::GetInstance();
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    CCryptoManager& crypto = CCryptoManager::GetInstance();

    if (!camera.IsOpened())
    {
        return;
    }

    // 抓取当前帧
    cv::Mat frame;
    if (!camera.CaptureFrame(frame))
    {
        return;
    }

    // 检测人脸区域
    cv::Rect faceRect;
    if (!camera.DetectFace(frame, faceRect))
    {
        // 当前画面未检测到人脸
        m_staticFaceInfo.SetWindowText(_T("等待人脸出现..."));
        return;
    }

    // 裁剪人脸区域
    cv::Mat faceROI = frame(faceRect).clone();

    // 提取当前人脸特征
    cv::Mat currentFeatures = recognizer.ExtractFeatures(faceROI);
    if (currentFeatures.empty())
    {
        return;
    }

    // 获取该用户已录入的人脸数据
    std::vector<FaceRecord> vecFaces = db.GetAllFaces(m_nUserId);

    if (vecFaces.empty())
    {
        // 尚无已录入人脸，提示可以录入
        UpdateFaceCountDisplay();
        return;
    }

    // 与已录入的每张人脸逐一比对，找到最佳匹配
    double dBestDistance = 1e10;
    int nBestMatchIndex = -1;

    for (size_t i = 0; i < vecFaces.size(); ++i)
    {
        // 解密已存储的人脸特征
        std::vector<BYTE> vecDecrypted = crypto.DecryptData(
            vecFaces[i].vecFeatures.data(),
            vecFaces[i].vecFeatures.size());

        if (vecDecrypted.empty())
        {
            continue;
        }

        // 将解密后的特征数据转为 cv::Mat
        // 特征维度：LBP_GRID_X × LBP_GRID_Y × 256 = 8 × 8 × 256 = 16384
        int nFeatureSize = (int)vecDecrypted.size() / sizeof(float);
        cv::Mat storedFeatures(1, nFeatureSize, CV_32FC1);
        memcpy(storedFeatures.data, vecDecrypted.data(), vecDecrypted.size());

        // 计算卡方距离
        double dDistance = recognizer.CompareFaces(currentFeatures, storedFeatures);

        if (dDistance < dBestDistance)
        {
            dBestDistance = dDistance;
            nBestMatchIndex = (int)i;
        }
    }

    // 构造界面提示文本
    CString strInfo;
    strInfo.Format(_T("已录入: %d / 3 张人脸"), m_nFaceCount);

    if (nBestMatchIndex >= 0)
    {
        BOOL bMatch = recognizer.IsFaceMatch(dBestDistance);

        TCHAR szMatch[256];
        if (bMatch)
        {
            _stprintf_s(szMatch,
                _T("\n✓ 匹配: 第%d张人脸 (相似距离: %.1f)"),
                nBestMatchIndex + 1, dBestDistance);
        }
        else
        {
            _stprintf_s(szMatch,
                _T("\n✗ 最佳距离: %.1f (阈值: %.1f) — 建议录入新角度"),
                dBestDistance, recognizer.GetMatchThreshold());
        }
        strInfo += szMatch;
    }

    m_staticFaceInfo.SetWindowText(strInfo);
}

// ============================================================================
// 在帧上绘制人脸检测框
// ============================================================================
void CDlgFace::DrawFaceRect(cv::Mat& frame)
{
    CCameraManager& camera = CCameraManager::GetInstance();
    cv::Rect faceRect;

    if (camera.DetectFace(frame, faceRect))
    {
        // 绘制绿色矩形框（人脸检测成功提示）
        cv::rectangle(frame, faceRect, cv::Scalar(0, 255, 0), 2);

        // 在人脸框上方显示文字
        cv::putText(frame, "Face Detected",
                    cv::Point(faceRect.x, faceRect.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 255, 0), 1);
    }
}

// ============================================================================
// 拍照录入按钮
// ============================================================================
void CDlgFace::OnBtnSnapshot()
{
    if (m_nFaceCount >= 3)
    {
        AfxMessageBox(_T("已达到最大录入数量（3张），请先删除旧数据。"), MB_ICONWARNING);
        return;
    }

    CCameraManager& camera = CCameraManager::GetInstance();
    CFaceRecognizer& recognizer = CFaceRecognizer::GetInstance();
    CDatabaseManager& db = CDatabaseManager::GetInstance();
    CCryptoManager& crypto = CCryptoManager::GetInstance();
    CConfigManager& config = CConfigManager::GetInstance();

    // 从摄像头抓取人脸
    cv::Mat faceROI;
    if (!camera.CaptureFaceImage(faceROI))
    {
        AfxMessageBox(_T("未检测到人脸！请正对摄像头，确保光线充足。"), MB_ICONWARNING);
        return;
    }

    // 提取人脸特征
    cv::Mat features = recognizer.ExtractFeatures(faceROI);
    if (features.empty())
    {
        AfxMessageBox(_T("人脸特征提取失败，请重试。"), MB_ICONERROR);
        return;
    }

    // 将特征数据序列化为字节数组
    std::vector<BYTE> vecRawFeatures(features.total() * features.elemSize());
    memcpy(vecRawFeatures.data(), features.data, vecRawFeatures.size());

    // 加密特征数据
    std::vector<BYTE> vecEncryptedFeatures = crypto.EncryptData(
        vecRawFeatures.data(), vecRawFeatures.size());

    if (vecEncryptedFeatures.empty())
    {
        AfxMessageBox(_T("人脸数据加密失败，请重试。"), MB_ICONERROR);
        return;
    }

    // 保存人脸图像
    CString strFaceDir = config.GetAppDataDir() + _T("\\") + FACEGUARD_FACE_DIR;
    ::SHCreateDirectoryEx(nullptr, strFaceDir, nullptr);

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    TCHAR szTime[64] = { 0 };
    _tcsftime(szTime, 64, _T("%Y%m%d_%H%M%S"), &timeinfo);

    int nFaceIndex = m_nFaceCount + 1;
    CString strImagePath;
    strImagePath.Format(_T("%s\\face_%d_%s.jpg"), (LPCTSTR)strFaceDir, nFaceIndex, szTime);

    camera.SaveImage(faceROI, strImagePath);

    // 保存到数据库
    if (!db.InsertFaceData(m_nUserId, nFaceIndex, vecEncryptedFeatures, strImagePath))
    {
        AfxMessageBox(_T("人脸数据保存失败！"), MB_ICONERROR);
        return;
    }

    // 更新计数和显示
    m_nFaceCount = db.GetFaceCount(m_nUserId);
    UpdateFaceCountDisplay();

    CString strMsg;
    strMsg.Format(_T("人脸录入成功！已录入 %d/3 张。"), m_nFaceCount);
    AfxMessageBox(strMsg, MB_ICONINFORMATION);
}

// ============================================================================
// 完成按钮
// ============================================================================
void CDlgFace::OnBtnComplete()
{
    if (m_nFaceCount < 1)
    {
        AfxMessageBox(_T("请至少录入一张人脸。"), MB_ICONWARNING);
        return;
    }

    // 停止预览定时器
    if (m_nTimerID != 0)
    {
        KillTimer(m_nTimerID);
        m_nTimerID = 0;
    }
    if (m_nCaptureTimerID != 0)
    {
        KillTimer(m_nCaptureTimerID);
        m_nCaptureTimerID = 0;
    }

    EndDialog(IDOK);
}

// ============================================================================
// 删除人脸按钮事件（1/2/3）
// ============================================================================
void CDlgFace::OnDeleteFace1() { CDatabaseManager::GetInstance().DeleteFaceData(m_nUserId, 1); m_nFaceCount = CDatabaseManager::GetInstance().GetFaceCount(m_nUserId); UpdateFaceCountDisplay(); }
void CDlgFace::OnDeleteFace2() { CDatabaseManager::GetInstance().DeleteFaceData(m_nUserId, 2); m_nFaceCount = CDatabaseManager::GetInstance().GetFaceCount(m_nUserId); UpdateFaceCountDisplay(); }
void CDlgFace::OnDeleteFace3() { CDatabaseManager::GetInstance().DeleteFaceData(m_nUserId, 3); m_nFaceCount = CDatabaseManager::GetInstance().GetFaceCount(m_nUserId); UpdateFaceCountDisplay(); }

// ============================================================================
// 更新人脸计数显示和按钮状态
// ============================================================================
void CDlgFace::UpdateFaceCountDisplay()
{
    CString strInfo;
    strInfo.Format(_T("已录入: %d / 3 张人脸"), m_nFaceCount);
    m_staticFaceInfo.SetWindowText(strInfo);

    m_btnSnapshot.EnableWindow(m_nFaceCount < 3);
    m_btnComplete.EnableWindow(m_nFaceCount >= 1);

    // 更新删除按钮状态
    m_btnDeleteFace[0].EnableWindow(m_nFaceCount >= 1);
    m_btnDeleteFace[1].EnableWindow(m_nFaceCount >= 2);
    m_btnDeleteFace[2].EnableWindow(m_nFaceCount >= 3);
}

// ============================================================================
// 绘制和背景擦除
// ============================================================================
void CDlgFace::OnPaint()
{
    CDialogEx::OnPaint();
}

BOOL CDlgFace::OnEraseBkgnd(CDC* pDC)
{
    // 使用白色背景
    CRect rect;
    GetClientRect(&rect);
    pDC->FillSolidRect(&rect, RGB(255, 255, 255));
    return TRUE;
}
