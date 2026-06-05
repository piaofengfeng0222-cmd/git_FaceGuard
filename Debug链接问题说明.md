# Debug x64 链接错误 LNK2001 说明

## 错误现象

```
error LNK2001: 无法解析的外部符号 "public: void __cdecl cv::Mat::copyTo(...)"
```

**Release x64 编译成功，Debug x64 编译失败。**

---

## 根本原因

**CRT（C 运行时库）不匹配。**

| | Debug x64 | Release x64 |
|---|---|---|
| 运行时库 | `/MDd` (MultiThreadedDebugDLL) | `/MD` (MultiThreadedDLL) |
| 链接的 OpenCV | `opencv_world430.lib` | `opencv_world430.lib` |

`third_party\opencv\lib\opencv_world430.lib` 是以 **Release CRT (`/MD`)** 编译的，项目中**没有**对应的 Debug 版本（`opencv_world430d.lib`）。

当 Debug 配置使用 `/MDd` 编译你的代码时：
- Debug CRT（`/MDd`）中的 STL 容器（`std::vector`、`std::string` 等）包含额外的**迭代器调试成员**
- 这些额外成员改变了对象的内存布局，导致 C++ 符号的**修饰名（decorated name）**与 Release 版不同
- 链接器在 `opencv_world430.lib` 中找不到与 `/MDd` 修饰名匹配的符号 → **LNK2001**

`cv::Mat::copyTo()` 内部使用了 STL 类型，所以成为触发点。

---

## 解决方案

### 方案一：使用属性表（推荐，已提供）

项目根目录下已生成 `OpenCV_DebugCompat.props`，按以下步骤导入：

1. Visual Studio 中打开项目
2. **视图 → 属性管理器**（View → Property Manager）
3. 展开 **Debug | x64**
4. 右键 → **添加现有属性表**（Add Existing Property Sheet）
5. 选择 `OpenCV_DebugCompat.props`

该属性表默认使用**策略 A**（保留 `/MDd`，仅关闭 STL 迭代器调试）。

如需同时修复 Win32 平台，对 **Debug | Win32** 重复上述步骤。

### 方案二：手动修改项目属性

#### 策略 A：关闭 STL 迭代器调试（推荐）

保留 `/MDd` 的堆检查功能，只让 STL 容器与 Release ABI 兼容：

- **项目属性 → C/C++ → 预处理器 → 预处理器定义**，添加：
  ```
  _HAS_ITERATOR_DEBUGGING=0
  _ITERATOR_DEBUG_LEVEL=0
  ```

#### 策略 B：Debug 改用 Release CRT

- **项目属性 → C/C++ → 代码生成 → 运行库**，改为：
  ```
  多线程 DLL (/MD)
  ```

### 方案三：获取 Debug 版 OpenCV 库

下载或自行编译 OpenCV 4.3.0 的 Debug 版本（`opencv_world430d.lib`），放入 `third_party\opencv\lib\`，然后在 `.vcxproj` 中按配置条件链接不同库。

---

## 相关文件

| 文件 | 说明 |
|---|---|
| `OpenCV_DebugCompat.props` | MSBuild 属性表，修复 Debug CRT 不匹配 |
| `FaceGuard.vcxproj` | 项目文件（第127行 `/MDd` vs 第169行 `/MD`） |
| `third_party\opencv\lib\opencv_world430.lib` | Release CRT 编译的 OpenCV 库 |

---

## 补充说明

- 此问题**不是代码 bug**，OpenCV API 调用方式在 Debug/Release 下完全相同。
- `opencv_world430.lib` 是 OpenCV 的"world"模块（所有模块合并为单个库），官方预编译包通常只提供 Release 版本。
- 此问题在 VS2017/VS2019/VS2022 中均有出现，是使用预编译第三方 C++ 库时的通用问题。
