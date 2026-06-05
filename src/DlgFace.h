// DlgFace.h
// 人脸录入对话框——用户通过摄像头采集并存储人脸数据
// 每个账号最多录入 3 张不同角度的人脸
// 人脸特征经过 AES 加密后存储在 SQLite 中

#pragma once

class CDlgFace : public CDialogEx
{
    DECLARE_DYNAMIC(CDlgFace)

public:
    // nUserId - 当前登录用户的数据库ID
    CDlgFace(int nUserId, CWnd* pParent = nullptr);
    virtual ~CDlgFace();

    enum { IDD = IDD_DLG_FACE };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog() override;
    virtual void OnCancel() override;

    // 按钮点击事件
    afx_msg void OnBtnSnapshot();       // 拍照录入当前摄像头画面中的人脸
    afx_msg void OnBtnComplete();       // 完成人脸录入（退出对话框）
    afx_msg void OnTimer(UINT_PTR nIDEvent);  // 定时刷新摄像头预览

    // 删除已录入的人脸
    afx_msg void OnDeleteFace1();
    afx_msg void OnDeleteFace2();
    afx_msg void OnDeleteFace3();

    // 绘制预览画面
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

private:
    // 更新已录入人脸数量显示
    void UpdateFaceCountDisplay();

    // 获取当前摄像头预览帧并在静态控件中显示
    void RefreshPreview();

    // 定时抓拍人脸并与已录入数据进行比对
    void CaptureAndCompare();

    // 绘制人脸检测框
    void DrawFaceRect(cv::Mat& frame);

    int m_nUserId;               // 当前用户ID
    int m_nFaceCount;            // 已录入的人脸数量（0-3）
    UINT_PTR m_nTimerID;         // 预览定时器ID（用于OnTimer）
    UINT_PTR m_nCaptureTimerID;  // 抓拍比对定时器ID
    BOOL m_bCameraReady;         // 摄像头是否就绪

    // 控件
    CStatic m_staticPreview;
    CStatic m_staticFaceInfo;
    CButton m_btnSnapshot;
    CButton m_btnComplete;
    CButton m_btnDeleteFace[3];  // 删除按钮（索引0-2对应face_index 1-3）
    CStatic m_staticFaces[3];    // 人脸缩略图显示

    // 预览帧缓存
    cv::Mat m_currentFrame;
};
