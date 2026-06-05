// FaceRecognizer.cpp
// 人脸识别模块实现——基于 LBP (Local Binary Patterns) 直方图的人脸特征提取和比对
//
// LBP 算法原理：
//   对于图像中的每个像素，将其周围 P 个邻域像素的灰度值与该像素比较，
//   大于中心像素的标记为 1，否则为 0，得到 P 位二进制数（LBP 码）。
//   统计全图所有像素的 LBP 码，构建直方图作为该图像的特征向量。
//
// 比对方法：
//   将人脸分割为多个网格，分别计算每个网格的 LBP 直方图，
//   拼接所有网格的直方图作为最终特征。
//   使用卡方距离 (Chi-Square Distance) 比较两个特征的相似度。

#include "stdafx.h"
#include "FaceRecognizer.h"
#include "ConfigManager.h"

// ============================================================================
// 单例实现
// ============================================================================
CFaceRecognizer& CFaceRecognizer::GetInstance()
{
    static CFaceRecognizer instance;
    return instance;
}

// ============================================================================
// 构造与析构
// ============================================================================
CFaceRecognizer::CFaceRecognizer()
    : m_dMatchThreshold(DEFAULT_FACE_MATCH_THRESHOLD)
    , m_bInitialized(FALSE)
{
}

CFaceRecognizer::~CFaceRecognizer()
{
}

// ============================================================================
// 初始化：从配置文件读取匹配阈值
// ============================================================================
BOOL CFaceRecognizer::Initialize()
{
    if (m_bInitialized)
    {
        return TRUE;
    }

    CConfigManager& config = CConfigManager::GetInstance();
    m_dMatchThreshold = config.GetFaceMatchThreshold();
    m_bInitialized = TRUE;

    return TRUE;
}

// ============================================================================
// 人脸图像预处理
// 1. 转换为灰度图
// 2. 直方图均衡化（改善光照不均匀的影响）
// 3. 缩放至统一尺寸 FACE_SIZE × FACE_SIZE
// ============================================================================
cv::Mat CFaceRecognizer::PreprocessFace(const cv::Mat& faceImage)
{
    if (faceImage.empty())
    {
        return cv::Mat();
    }

    cv::Mat gray;
    // 如果是彩色图，手动转换为灰度图
    if (faceImage.channels() == 3)
    {
        gray = cv::Mat(faceImage.rows, faceImage.cols, CV_8UC1);
        for (int y = 0; y < faceImage.rows; ++y) {
            const unsigned char* src = faceImage.ptr<unsigned char>(y);
            unsigned char* dst = gray.ptr<unsigned char>(y);
            for (int x = 0; x < faceImage.cols; ++x) {
                dst[x] = (unsigned char)(0.114 * src[x*3] + 0.587 * src[x*3+1] + 0.299 * src[x*3+2]);
            }
        }
    }
    else
    {
        gray = faceImage.clone();
    }

    // 直方图均衡化——增强对比度，减少光照变化的影响
    cv::equalizeHist(gray, gray);

    // 双线性插值缩放到统一尺寸
    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(FACE_SIZE, FACE_SIZE), 0, 0, cv::INTER_LINEAR);

    return resized;
}

// ============================================================================
// 计算 LBP (Local Binary Patterns) 直方图特征
//
// 采用 Uniform LBP 算子：
//   - 半径为 LBP_RADIUS，采样点数为 LBP_POINTS
//   - 输出 LBP 码图像中每个像素的 LBP 值
//   - 将人脸分为 LBP_GRID_X × LBP_GRID_Y 个网格
//   - 分别统计每个网格的 LBP 直方图（256 维）
//   - 拼接所有网格的直方图作为最终特征（256 × GRID_X × GRID_Y 维）
// ============================================================================
cv::Mat CFaceRecognizer::ComputeLBPHistogram(const cv::Mat& faceImage)
{
    if (faceImage.empty() || faceImage.channels() != 1)
    {
        return cv::Mat();
    }

    // ---- 第一步：计算每个像素的 LBP 码 ----
    cv::Mat lbpImage(faceImage.size(), CV_8UC1, cv::Scalar(0));

    for (int y = LBP_RADIUS; y < faceImage.rows - LBP_RADIUS; ++y)
    {
        for (int x = LBP_RADIUS; x < faceImage.cols - LBP_RADIUS; ++x)
        {
            unsigned char center = faceImage.at<unsigned char>(y, x);
            unsigned char code = 0;

            for (int p = 0; p < LBP_POINTS; ++p)
            {
                // 计算采样点在圆上的坐标
                double angle = 2.0 * CV_PI * p / LBP_POINTS;
                int sx = (int)(x + LBP_RADIUS * cos(angle) + 0.5);
                int sy = (int)(y - LBP_RADIUS * sin(angle) + 0.5);  // 图像坐标 y 向下

                // 边界检查
                sx = (std::max)(0, (std::min)(faceImage.cols - 1, sx));
                sy = (std::max)(0, (std::min)(faceImage.rows - 1, sy));

                unsigned char neighbor = faceImage.at<unsigned char>(sy, sx);

                if (neighbor >= center)
                {
                    code |= (1 << p);
                }
            }

            lbpImage.at<unsigned char>(y, x) = code;
        }
    }

    // ---- 第二步：分网格计算 LBP 直方图 ----
    int gridWidth = faceImage.cols / LBP_GRID_X;
    int gridHeight = faceImage.rows / LBP_GRID_Y;
    int histogramBins = 256;  // 8-bit LBP 有 256 个可能的编码值

    // 最终特征向量：LBP_GRID_X × LBP_GRID_Y × 256 维
    cv::Mat histogram(1, LBP_GRID_X * LBP_GRID_Y * histogramBins, CV_32FC1, cv::Scalar(0));

    for (int gy = 0; gy < LBP_GRID_Y; ++gy)
    {
        for (int gx = 0; gx < LBP_GRID_X; ++gx)
        {
            // 当前网格的区域
            int startX = gx * gridWidth;
            int startY = gy * gridHeight;
            int endX = (gx == LBP_GRID_X - 1) ? faceImage.cols : (gx + 1) * gridWidth;
            int endY = (gy == LBP_GRID_Y - 1) ? faceImage.rows : (gy + 1) * gridHeight;

            // 统计当前网格内每个 LBP 码出现的频率
            std::vector<float> gridHist(histogramBins, 0.0f);
            int pixelCount = 0;

            for (int y = startY; y < endY; ++y)
            {
                for (int x = startX; x < endX; ++x)
                {
                    unsigned char code = lbpImage.at<unsigned char>(y, x);
                    gridHist[code]++;
                    pixelCount++;
                }
            }

            // 归一化（除以像素总数，转为人脸频率分布）
            if (pixelCount > 0)
            {
                for (int bin = 0; bin < histogramBins; ++bin)
                {
                    gridHist[bin] /= (float)pixelCount;
                }
            }

            // 将当前网格的直方图写入特征向量的对应位置
            int offset = (gy * LBP_GRID_X + gx) * histogramBins;
            for (int bin = 0; bin < histogramBins; ++bin)
            {
                histogram.at<float>(0, offset + bin) = gridHist[bin];
            }
        }
    }

    return histogram;
}

// ============================================================================
// 提取人脸特征
// ============================================================================
cv::Mat CFaceRecognizer::ExtractFeatures(const cv::Mat& faceImage)
{
    cv::Mat preprocessed = PreprocessFace(faceImage);
    if (preprocessed.empty())
    {
        return cv::Mat();
    }

    return ComputeLBPHistogram(preprocessed);
}

// ============================================================================
// 比较两个人脸特征（LBP 直方图）的相似度
//
// 使用卡方距离 (Chi-Square Distance)：
//   chi2 = Σ ( (H1[i] - H2[i])² / (H1[i] + H2[i]) )
//
// 返回值越小表示越相似，0 表示两个直方图完全相同
// 注：在实际使用中，同一人的 LBP 直方图卡方距离通常在 20-80 之间，
//      不同人之间的距离通常在 100 以上
// ============================================================================
double CFaceRecognizer::CompareFaces(const cv::Mat& features1, const cv::Mat& features2)
{
    if (features1.empty() || features2.empty())
    {
        return 1e10;  // 返回一个很大的值表示无法比较
    }

    if (features1.cols != features2.cols || features1.rows != features2.rows)
    {
        return 1e10;  // 特征维度不一致
    }

    double chiSquare = 0.0;
    const float* p1 = features1.ptr<float>(0);
    const float* p2 = features2.ptr<float>(0);
    int totalBins = features1.cols;

    for (int i = 0; i < totalBins; ++i)
    {
        double numerator = (p1[i] - p2[i]) * (p1[i] - p2[i]);
        double denominator = p1[i] + p2[i];

        if (denominator > 1e-10)  // 避免除以零
        {
            chiSquare += numerator / denominator;
        }
    }

    return chiSquare;
}

// ============================================================================
// 根据阈值判断是否匹配
// ============================================================================
BOOL CFaceRecognizer::IsFaceMatch(double dConfidence)
{
    return (dConfidence < m_dMatchThreshold);
}
