# WarmNote

WarmNote 是一个面向《无限暖暖》的 Windows 自动演奏工具，目前处于开发阶段。

项目使用 C++20 和 Win32 API 实现游戏窗口检测、前台切换与键盘输入，并通过自定义 `.param` 谱面描述歌曲的节拍、分段、按键和特殊演奏动作。整体采用前后端分离思路；当前仓库主要实现后端核心，暂未提供图形界面。

## 当前功能

- 加载并解析 WarmNote `.param` 谱面。
- 按记录名指定演奏起点和结束位置。
- 支持单键、组合键以及同一拍内的连续按键。
- 内置 `KeyCodeInput` 普通按键动作层。
- 内置 `Tremolo` 颤音动作层，用于横笛的 `Shift + 音符键` 操作。
- 使用 Win32 API 查找游戏窗口、切换前台并发送键盘输入。
- 获取显示器分辨率、DPI、缩放比例和游戏客户区尺寸。
- 捕获游戏客户区画面，并可保存截图。
- 采用类似 ncnn Layer 的 ActionLayer 注册与分发机制，可通过 CMake 添加新动作层。

## 项目结构

```text
WarmNote/
├─ cmake/                 # ActionLayer 自动注册所需的 CMake 模板与函数
├─ include/               # 后端接口和动作层声明
│  └─ ActionLayer/        # 具体动作层
├─ score/                 # WarmNote param 谱面
├─ src/                   # 后端实现与当前命令行示例
├─ test/                  # 测试目录
├─ CMakeLists.txt
└─ LICENSE
```

## 环境要求

- Windows 10/11
- Visual Studio 的 MSVC 工具链
- CMake 3.24 或更高版本
- 支持 C++20 的编译器
- OpenCV 5，至少包含 `core` 组件

项目当前仅支持 MSVC。使用 CLion 时，请选择 Visual Studio/MSVC 工具链，并将 `OpenCV_DIR` 指向包含 `OpenCVConfig.cmake` 的目录。

## 构建

在已配置 MSVC 环境的终端中执行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR="D:/software/C++Lib/opencv/build"
cmake --build build
```

如果你的 OpenCV 安装目录不同，请相应修改 `OpenCV_DIR`。CMake 配置阶段会输出已加载的动作层，例如：

```text
Loaded ActionLayer: KeyCodeInput -> KeyCodeInputLayer
Loaded ActionLayer: Tremolo -> TremoloLayer
```

## 使用方式

当前入口位于 `src/main.cpp`。运行前需要在代码中选择要加载的 `.param` 文件，并通过 `MusicExtractor::input()` 设置开始记录名：

```cpp
Score score;
score.SetTicksPerBeat(480);
score.Load(score_path);

Core::MusicExtractor extractor;
extractor.input("part1_001", score);
extractor.extract();
```

也可以指定结束记录，执行完该记录后停止：

```cpp
extractor.extract("part2_060");
```

启动演奏前，请先在《无限暖暖》中进入对应乐器的演奏界面，并确认键位布局与谱面一致。程序会向当前游戏窗口发送真实键盘输入。

## 谱面格式

WarmNote 谱面采用便于人工阅读和生成的文本格式。下面是一个简化示例：

```text
name 示例歌曲
duration_ms 60000
instrument 10001
1464750897
1 1 1
meta 80 4 4 80
layout QWERTYU ASDFGHJ ZXCVBNM

section part1 1
1 part1_001 : KeyCodeInput 4 Q W E Q+E
```

- `instrument`：游戏内乐器编号。
- `meta`：依次表示 BPM、每小节拍数、拍号分母和默认动作持续时间。
- `section`：声明段落名称和该段落的记录数量。
- 每条记录由“动作块数量、记录名、动作类型、拍数、各拍载荷”组成。
- `+` 表示同时按下，例如 `Q+E`。
- `,` 表示一拍内依次执行多个动作。
- `-` 表示休止。
- `;` 用于分隔同一条记录中的多个动作块。

## 添加动作层

新增动作层时，在 `include/ActionLayer` 中实现继承自 `Control::ActionLayer` 的类，然后在 `CMakeLists.txt` 中注册：

```cmake
warmnote_add_action_layer(ActionType ActionLayerClass)
```

CMake 会自动生成创建函数和内置注册表，`MusicExtractor` 不需要为新动作类型增加 `if` 分支。

## 注意事项

- 本项目仍在开发中，谱面格式和接口可能继续调整。
- 游戏更新、键位设置、窗口模式、DPI 或分辨率变化都可能影响运行结果。
- 请遵守游戏服务条款，并自行承担使用自动化功能可能产生的风险。
- `score` 目录中的谱面仅用于程序格式测试与学习；歌曲本身的权利归各自权利人所有。

## 开源许可

WarmNote 采用 [GNU General Public License v3.0](LICENSE) 发布。分发本项目或其修改版本时，请遵守 GPL-3.0 的相关要求。
