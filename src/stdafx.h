// stdafx.h
// 预编译头文件——包含标准系统头文件和常用项目头文件
// 凡是不经常变动、被多个源文件引用的头文件都放在此处进行预编译
//
// 注意：MFC 项目必须以 afxwin.h 为第一个 Windows 头文件，
//       因为 MFC 内部会自动包含 windows.h，不允许在 afxwin.h 之前包含 windows.h

#pragma once

#include "targetver.h"

// ======== VS2017 + Windows 10 SDK 10.0.19041 兼容性修复 ========
// SDK 的 winnt.h 使用了 VS2017 不识别的 __declspec(no_init_all) 属性。
// 关闭相关警告以避免 IntelliSense 报 E1097。
#if defined(_MSC_VER) && _MSC_VER < 1920
#pragma warning(disable: 5030)  // 无法识别的 __declspec 属性
#endif

// 关闭某些不常用的 Windows API 警告
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

// 资源ID定义
#include "resource.h"

// ======== MFC 头文件（必须在其他 Windows 头文件之前包含）========
#include <afxwin.h>         // MFC 核心和标准组件（内部包含 windows.h）
#include <afxext.h>         // MFC 扩展
#include <afxcmn.h>         // MFC 公共控件
#include <afxdlgs.h>        // MFC 通用对话框
#include <afxdialogex.h>    // MFC CDialogEx (扩展对话框，支持背景色)
#include <afxmt.h>          // MFC 同步对象

// ======== Windows API 头文件（在 MFC 之后包含）========
#include <shlobj.h>         // SHGetFolderPath
#include <shellapi.h>       // Shell_NotifyIcon
#include <shlwapi.h>        // PathFileExists, PathRemoveFileSpec
#include <wincrypt.h>       // CryptoAPI
#include <bcrypt.h>         // AES 加密 (CNG)
#include <powrprof.h>       // 电源管理（预留）
#include <winreg.h>         // 注册表操作
#include <tlhelp32.h>       // 进程快照

// ======== ATL 头文件（用于字符串转换等）========
#include <atlbase.h>
#include <atlstr.h>

// ======== C++ 标准库头文件 ========
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>

// ======== OpenCV 头文件 ========
// 注意：需要先在项目中配置 OpenCV 包含路径
// 如果 OpenCV 未安装，请从 https://opencv.org/releases/ 下载
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/objdetect.hpp>

// ======== SQLite3 头文件 ========
#include "sqlite3.h"

// 通用宏定义（已在 Unicode 配置中，使用 L 前缀宽字符串）
#define FACEGUARD_MUTEX_NAME    L"Global\\FaceGuard_SingleInstance_Mutex"
#define FACEGUARD_WINDOW_CLASS  L"FaceGuard_MainWnd"
#define FACEGUARD_APP_TITLE     L"FaceGuard"
#define FACEGUARD_APP_DATA_DIR  L""    // 数据文件与程序同目录
#define FACEGUARD_DB_FILENAME   L"faceguard.db"
#define FACEGUARD_INI_FILENAME  L"FaceGuard.ini"
#define FACEGUARD_FACE_DIR      L"faces"
#define FACEGUARD_INTRUDER_DIR  L"intruders"
#define FACEGUARD_THUMBNAIL_DIR L"thumbnails"
#define FACEGUARD_NO_FACE_DIR	L"noface"

// 默认配置值
#define DEFAULT_FACE_MATCH_THRESHOLD     80.0
#define DEFAULT_NORMAL_INTERVAL_SECONDS  60
#define DEFAULT_ALERT_INTERVAL_SECONDS   3
#define DEFAULT_ALERT_RETRY_COUNT        5
#define DEFAULT_SHUTDOWN_DELAY_SECONDS   60

// 人脸检测参数默认值（可通过 FaceGuard.ini [FaceDetection] 覆盖）
#define DEFAULT_MIN_FACE_WIDTH          50
#define DEFAULT_MIN_FACE_HEIGHT         50
#define DEFAULT_MIN_NEIGHBORS           2
#define DEFAULT_SCALE_FACTOR            1.1

// 托盘消息
#define WM_TRAY_NOTIFY      (WM_USER + 100)
#define WM_MONITOR_UPDATE   (WM_USER + 101)
#define WM_FACE_DETECTED    (WM_USER + 102)
#define WM_INTRUDER_ALERT   (WM_USER + 103)

// 托盘菜单命令ID
#define IDM_TRAY_SHOW       1000
#define IDM_TRAY_CHECK      1001
#define IDM_TRAY_VIEW_LOG   1002
#define IDM_TRAY_ABOUT      1003
#define IDM_TRAY_EXIT       1004

