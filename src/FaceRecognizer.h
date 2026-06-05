// FaceRecognizer.h
// 人脸识别模块——使用 LBP (Local Binary Patterns) 直方图进行人脸特征提取和比对
// 方案说明：
//   1. 人脸检测：使用 OpenCV Haar Cascade（由 CameraManager 提供）
//   2. 特征提取：LBP (Local Binary Patterns) 直方图
//   3. 人脸比对：直方图相似度比较（卡方距离 Chi-Square）
// 优点：无需 opencv_contrib 模块，仅依赖标准 OpenCV 即可运行
// LBP 方法对光照变化具有较强的鲁棒性

#pragma once

class CFaceRecognizer
{
public:
    // 获取单例实例
    static CFaceRecognizer& GetInstance();

    CFaceRecognizer();
    ~CFaceRecognizer();

    // 初始化识别器（从配置文件读取匹配阈值）
    BOOL Initialize();

    // 对图像进行预处理（灰度化 → 直方图均衡 → 缩放至统一尺寸）
    // faceImage - 输入彩色人脸图像，预处理后原地修改
    cv::Mat PreprocessFace(const cv::Mat& faceImage);

    // 计算人脸图像的 LBP 直方图特征
    // faceImage - 预处理后的人脸灰度图像
    // 返回 LBP 直方图（一维 float 数组）
    cv::Mat ComputeLBPHistogram(const cv::Mat& faceImage);

    // 提取人脸特征（预处理 + LBP 直方图）
    cv::Mat ExtractFeatures(const cv::Mat& faceImage);

    // 比较两个人脸特征，返回相似度距离
    // 使用卡方距离 (Chi-Square)，值越小表示越相似，0 表示完全相同
    // features1, features2 - LBP 直方图特征
    double CompareFaces(const cv::Mat& features1, const cv::Mat& features2);

    // 根据阈值判断两个人脸是否匹配
    BOOL IsFaceMatch(double dConfidence);

    // 获取/设置匹配阈值
    double GetMatchThreshold() const { return m_dMatchThreshold; }
    void SetMatchThreshold(double dThreshold) { m_dMatchThreshold = dThreshold; }

private:
    // LBP 算子参数
    static const int LBP_RADIUS = 2;        // LBP 半径
    static const int LBP_POINTS = 8;        // LBP 采样点数
    static const int FACE_SIZE = 100;       // 人脸统一缩放尺寸 (100x100)
    static const int LBP_GRID_X = 8;        // 水平方向网格数
    static const int LBP_GRID_Y = 8;        // 垂直方向网格数

    double m_dMatchThreshold;               // 匹配阈值（从配置文件读取）
    BOOL m_bInitialized;
};
