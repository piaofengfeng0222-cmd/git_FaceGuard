// CameraManager.cpp
// 摄像头管理模块实现——使用 OpenCV 进行摄像头操作和人脸检测

#include "stdafx.h"
#include "CameraManager.h"
#include "ConfigManager.h"

// ============================================================================
// 单例实现
// ============================================================================
CCameraManager& CCameraManager::GetInstance()
{
    static CCameraManager instance;
    return instance;
}

CCameraManager::CCameraManager()
    : m_nCameraIndex(0)
    , m_bOpened(FALSE)
    , m_nMinFaceWidth(DEFAULT_MIN_FACE_WIDTH)
    , m_nMinFaceHeight(DEFAULT_MIN_FACE_HEIGHT)
    , m_nMinNeighbors(DEFAULT_MIN_NEIGHBORS)
    , m_dScaleFactor(DEFAULT_SCALE_FACTOR)
{
}

CCameraManager::~CCameraManager()
{
    Release();
}

// ============================================================================
// 初始化摄像头
// 1. 打开指定索引的摄像头（使用 DirectShow 后端）
// 2. 加载 Haar Cascade 人脸检测模型
// 模型文件路径：优先在程序同目录下查找，其次在 res 目录下查找
// ============================================================================
BOOL CCameraManager::Initialize(int nCameraIndex)
{
    if (m_bOpened)
    {
        return TRUE;  // 已初始化
    }

    m_nCameraIndex = nCameraIndex;

    // 使用 DirectShow 后端打开摄像头（Windows 默认方式）
    // CAP_DSHOW 指定 DirectShow 后端，比默认的 VfW 更稳定
    if (!m_capture.open(nCameraIndex, cv::CAP_DSHOW))
    {
        // 尝试使用默认后端
        if (!m_capture.open(nCameraIndex))
        {
            return FALSE;
        }
    }

    // 设置摄像头分辨率（640x480 是比较可靠的默认分辨率）
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, 640.0);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480.0);

    // 记录摄像头实际分辨率（某些摄像头可能不支持请求的分辨率，返回较低分辨率）
    {
        double dActualWidth = m_capture.get(cv::CAP_PROP_FRAME_WIDTH);
        double dActualHeight = m_capture.get(cv::CAP_PROP_FRAME_HEIGHT);
        TCHAR szMsg[256] = { 0 };
        _stprintf_s(szMsg, _T("CameraManager: 摄像头实际分辨率 = %.0f x %.0f\n"),
                    dActualWidth, dActualHeight);
        OutputDebugString(szMsg);
    }

    // 加载人脸检测参数（从 INI 配置文件读取，使用默认值兜底）
    LoadDetectionParams();

    // 加载 Haar Cascade 人脸检测模型文件
    // 按优先级查找：程序同目录 → 程序同目录\res
    TCHAR szExePath[MAX_PATH] = { 0 };
    ::GetModuleFileName(nullptr, szExePath, MAX_PATH);
    CString strExeDir = szExePath;
    int nPos = strExeDir.ReverseFind(_T('\\'));
    if (nPos >= 0)
    {
        strExeDir = strExeDir.Left(nPos);
    }

    CString strCascadePaths[] = {
        strExeDir + _T("\\haarcascade_frontalface_default.xml"),
        strExeDir + _T("\\res\\haarcascade_frontalface_default.xml")
    };

    BOOL bCascadeLoaded = FALSE;
    for (const auto& strPath : strCascadePaths)
    {
        std::string strPathAnsi = CT2A(strPath);
        if (m_faceCascade.load(strPathAnsi))
        {
            m_strCascadePath = strPath;
            bCascadeLoaded = TRUE;
            break;
        }
    }

    if (!bCascadeLoaded)
    {
        // 模型文件未找到——摄像头可用但人脸检测不可用
        OutputDebugString(_T("CameraManager: 未找到 haarcascade_frontalface_default.xml 模型文件\n"));
        ::MessageBox(nullptr,
                     _T("FaceGuard 未能加载人脸检测模型文件！\n\n")
                     _T("请确保 haarcascade_frontalface_default.xml 文件存在于以下位置之一：\n")
                     _T("  1) 程序所在目录\n")
                     _T("  2) 程序所在目录\\res\\\n\n")
                     _T("人脸检测功能将不可用。"),
                     _T("FaceGuard - 警告"),
                     MB_OK | MB_ICONWARNING);
    }

    m_bOpened = TRUE;
    return TRUE;
}

// ============================================================================
// 释放摄像头资源
// ============================================================================
void CCameraManager::Release()
{
    if (m_bOpened && m_capture.isOpened())
    {
        m_capture.release();
    }
    m_bOpened = FALSE;
}

// ============================================================================
// 从配置文件加载人脸检测参数
// 读取 FaceGuard.ini 中 [FaceDetection] 节的参数，
// 若无配置文件则使用 stdafx.h 中定义的默认值
// ============================================================================
void CCameraManager::LoadDetectionParams()
{
    CConfigManager& config = CConfigManager::GetInstance();

    m_nMinFaceWidth  = config.GetMinFaceWidth();
    m_nMinFaceHeight = config.GetMinFaceHeight();
    m_nMinNeighbors  = config.GetMinNeighbors();
    m_dScaleFactor   = config.GetScaleFactor();

    TCHAR szMsg[256] = { 0 };
    _stprintf_s(szMsg, _T("CameraManager: 检测参数 minSize=%dx%d minNeighbors=%d scaleFactor=%.2f\n"),
                m_nMinFaceWidth, m_nMinFaceHeight, m_nMinNeighbors, m_dScaleFactor);
    OutputDebugString(szMsg);
}

// ============================================================================
// 检查摄像头是否已打开
// ============================================================================
BOOL CCameraManager::IsOpened() const
{
    return m_bOpened && m_capture.isOpened();
}

// ============================================================================
// 抓取单帧图像
// frame - 输出的原始彩色图像 (BGR 格式)
// 返回 TRUE 表示成功抓取
// ============================================================================
BOOL CCameraManager::CaptureFrame(cv::Mat& frame)
{
    if (!IsOpened())
    {
        return FALSE;
    }

    if (!m_capture.read(frame))
    {
        return FALSE;
    }

    if (frame.empty())
    {
        return FALSE;
    }

    return TRUE;
}

// ============================================================================
// 抓取一帧并检测其中的人脸区域
// faceROI - 输出裁剪后的人脸图像（彩色，仅包含人脸区域）
// 如果检测到多张人脸，返回最大的一张
// ============================================================================
BOOL CCameraManager::CaptureFaceImage(cv::Mat& faceROI)
{
    cv::Mat frame;
    if (!CaptureFrame(frame))
    {
        return FALSE;
    }

    cv::Rect faceRect;
    if (!DetectFace(frame, faceRect))
    {
        return FALSE;  // 未检测到人脸
    }

    // 使人脸区域稍微扩大一些（10%边距），避免裁切过紧
    int nMarginX = (int)(faceRect.width * 0.1);
    int nMarginY = (int)(faceRect.height * 0.1);

    int x = (std::max)(0, faceRect.x - nMarginX);
    int y = (std::max)(0, faceRect.y - nMarginY);
    int w = (std::min)(frame.cols - x, faceRect.width + 2 * nMarginX);
    int h = (std::min)(frame.rows - y, faceRect.height + 2 * nMarginY);

    cv::Rect safeRect(x, y, w, h);
    faceROI = frame(safeRect).clone();

    return TRUE;
}

// ============================================================================
// 检测图像中的人脸位置
// 使用 Haar Cascade 级联分类器进行人脸检测
// 如果检测到多张人脸，返回最大的一张（通常最靠近摄像头的人）
// ============================================================================
BOOL CCameraManager::DetectFace(const cv::Mat& frame, cv::Rect& faceRect)
{
    if (frame.empty() || m_faceCascade.empty())
    {
        return FALSE;
    }

    // 转换为灰度图（Haar Cascade 需要在灰度图上检测）
    // 手动 BGR→GRAY 转换: Gray = 0.299*R + 0.587*G + 0.114*B
    cv::Mat gray(frame.rows, frame.cols, CV_8UC1);
    for (int y = 0; y < frame.rows; ++y) {
        const unsigned char* src = frame.ptr<unsigned char>(y);
        unsigned char* dst = gray.ptr<unsigned char>(y);
        for (int x = 0; x < frame.cols; ++x) {
            dst[x] = (unsigned char)(0.114 * src[x*3] + 0.587 * src[x*3+1] + 0.299 * src[x*3+2]);
        }
    }

    // 直方图均衡化——提高不同光照条件下的检测率
    cv::equalizeHist(gray, gray);

    // 多尺度检测人脸（参数从 INI 配置文件读取，可通过调整 FaceGuard.ini 动态优化）
    std::vector<cv::Rect> faces;
    m_faceCascade.detectMultiScale(
        gray,                                               // 灰度图像
        faces,                                              // 输出检测到的人脸矩形列表
        m_dScaleFactor,                                     // 缩放因子（默认 1.1，越小检测越精细）
        m_nMinNeighbors,                                    // 最小邻居数（默认 2，越小越灵敏）
        0,                                                  // 标志位（0 = 默认）
        cv::Size(m_nMinFaceWidth, m_nMinFaceHeight),       // 最小人脸尺寸（默认 50x50）
        cv::Size()                                         // 最大人脸尺寸（不限制）
    );

    if (faces.empty())
    {
        return FALSE;
    }

    // 如果检测到多张人脸，返回面积最大的一张
    int nMaxArea = 0;
    int nMaxIndex = 0;
    for (size_t i = 0; i < faces.size(); ++i)
    {
        int nArea = faces[i].width * faces[i].height;
        if (nArea > nMaxArea)
        {
            nMaxArea = nArea;
            nMaxIndex = (int)i;
        }
    }

    faceRect = faces[nMaxIndex];
    return TRUE;
}

// ============================================================================
// 保存图像到文件
// 自动创建所需的子目录
// ============================================================================
BOOL CCameraManager::SaveImage(const cv::Mat& image, const CString& strFilePath)
{
    if (image.empty() || strFilePath.IsEmpty())
    {
        return FALSE;
    }

    // 确保目标目录存在
    CString strDir = strFilePath;
    int nPos = strDir.ReverseFind(_T('\\'));
    if (nPos >= 0)
    {
        strDir = strDir.Left(nPos);
        ::SHCreateDirectoryEx(nullptr, strDir, nullptr);
    }

    // 保存图像（OpenCV 根据扩展名自动选择编码格式）
    std::string strPathAnsi = CT2A(strFilePath);
    return cv::imwrite(strPathAnsi, image);
}
