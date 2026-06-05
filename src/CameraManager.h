// CameraManager.h
// 摄像头管理模块——基于 OpenCV VideoCapture 管理摄像头设备
// 支持摄像头打开/关闭、实时帧抓取、人脸检测定位、图像保存
// 使用 DirectShow 后端 (Windows 默认)，默认打开索引为 0 的摄像头

#pragma once

class CCameraManager
{
public:
    // 获取单例实例
    static CCameraManager& GetInstance();

    // 初始化：打开指定索引的摄像头并加载 Haar Cascade 模型
    // cameraIndex - 摄像头设备索引，0 为默认摄像头
    BOOL Initialize(int nCameraIndex = 0);

    // 释放摄像头资源
    void Release();

    // 检查摄像头是否已成功打开
    BOOL IsOpened() const;

    // 抓取单帧图像（原始彩色图像）
    BOOL CaptureFrame(cv::Mat& frame);

    // 抓取一帧并检测人脸区域，返回裁剪后的人脸图像
    // faceROI - 输出裁剪后的人脸区域图像
    BOOL CaptureFaceImage(cv::Mat& faceROI);

    // 检测图像中的人脸位置
    // frame    - 输入图像
    // faceRect - 输出检测到的人脸矩形区域
    BOOL DetectFace(const cv::Mat& frame, cv::Rect& faceRect);

    // 保存图像到文件
    BOOL SaveImage(const cv::Mat& image, const CString& strFilePath);

    // 获取摄像头索引
    int GetCameraIndex() const { return m_nCameraIndex; }

    // 获取 Haar Cascade 模型文件路径
    CString GetCascadePath() const { return m_strCascadePath; }

    // 从配置文件重新加载人脸检测参数
    void LoadDetectionParams();

private:
    CCameraManager();
    ~CCameraManager();
    CCameraManager(const CCameraManager&) = delete;
    CCameraManager& operator=(const CCameraManager&) = delete;

    cv::VideoCapture m_capture;          // OpenCV 摄像头捕获对象
    cv::CascadeClassifier m_faceCascade; // Haar 级联人脸检测分类器
    int m_nCameraIndex;                  // 摄像头设备索引
    BOOL m_bOpened;                      // 摄像头是否已打开
    CString m_strCascadePath;            // Haar Cascade 模型文件路径

    // 人脸检测参数（可从 INI 配置文件动态调整）
    int    m_nMinFaceWidth;              // 最小人脸宽度（像素）
    int    m_nMinFaceHeight;             // 最小人脸高度（像素）
    int    m_nMinNeighbors;              // 最小邻居检测窗口数
    double m_dScaleFactor;               // 缩放因子（越小检测越精细但越慢）
};
