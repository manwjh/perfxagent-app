#【第二季】
## 【2025-06-15 16:55】
用AI来实现音频转文字功能。
第一步：让它画了一个简单的界面。实现音频文件的导入。
第二步：所有音频格式文件需要转换为opus编码（采样率 16K，INT16，帧大小为60ms）。并且是流式的。这个是为了匹配火山的ASR api能接收的格式。

问题1：加一个模块，基本就会面临环境问题。耗费时间和token费用。
问题2：cursor的连接也不问题，较长出现slow request, get fast access here。尽管我是pro账号（代价是20美金）
问题3：cursor的费用实际挺高的，20美金消耗速度很快。（你越懒，越费钱。计费机制按照响应次数？）
问题4: 摘要--
你的 src/CMakeLists.txt 文件中，file_importer.cpp 没有被加入到 perfxagent-app 的可执行文件编译列表中。这就是链接时找不到 FileImporter 相关符号的根本原因。
解决方法：
需要将 audio/file_importer.cpp 加入 add_executable(perfxagent-app ...) 的源文件列表中。
我将为你修正 src/CMakeLists.txt 文件。

问题5: AI容易将问题扩大化。解决一个问题，估计因为记忆的长度问题。有可能会莫名其妙帮你延伸到另外一个问题上去。如果不直至，问题也许不会收敛。例如这样的提示，鬼知道它的逻辑是什么呢？
1. 只用主 CMakeLists.txt，集中管理所有源文件
2. 用 add_subdirectory(src)，让 src/CMakeLists.txt 负责可执行文件构建
推荐第2种（更清晰、易维护）。请告知你的选择，或直接让我帮你修正。

我对于编译错误，已经无力自己fix了。只能靠AI了。

## 【2025-06-15 19:46】
没有完成一个小里程碑，肚子饿了。
AI 10%的时间在写代码，看起来很爽。但90%的时间，陪着AI去fix问题。

再次发了这个目录“检查整个项目，清理。如果发现明显问题，修改”
我猜这会是一场灾难。

还好，错误并没有急剧增加。

## 【2025-06-17 21:44】
实现导入文件，转换为临时文件（.WAV）可以播放；进一步准备OPUS编码，输出.opus文件，不能播放。
至少编译通过了，至少项目能一瘸一拐跑。更新一下github

## 【2025-06-18 13:56】
目标音频文件-->WAV临时文件（可以）-->opus编码文件OK


## 【2025-06-19 12:18】
调试火山asr接口，参考https://www.volcengine.com/docs/6561/80818 的c++实现代码

to_ai: 参考该目录下的官方示例代码/Users/wangjunhui/playcode/火山api
在项目的src/asr 和include/asr 下实现调用模块和示例代码。websocket通讯部分用QT的
(全盘接受，即使这样，代码也并没有一次通过。修改3轮，为了减少复杂度，让它去掉了asr_demo,只保留asr_usage_example.cpp。很快编译通过了。)

to_ai: 我来帮你实现 full client request 功能。首先让我查看一下官方demo示例的结构，然后了解当前项目的结构。

遇到ai认为QT的websocket无法直接控制 HTTP 握手和 header，需要修改为websocketapp库替代。
ai: 
核心思路：用 websocketpp 实现 ASR 客户端的 WebSocket 连接、认证、二进制协议收发，完全参考官方 demo。
Qt 只负责 UI/业务逻辑，ASR 通信全部交给 websocketpp。
你可以用 CMake 管理 websocketpp 依赖，和 Qt 工程无冲突。

（ai执行了移植官方代码到我这个项目的做法）

ai: 看起来websocketpp版本与Boost版本不兼容。这是一个常见问题，websocketpp 0.8.2是为较老的Boost版本设计的。让我们尝试一个更简单的解决方案：
总结
迁移 websocketpp 遇到的技术障碍：
版本兼容性问题：websocketpp 0.8.2 与 Boost 1.88.0 不兼容，主要是 io_service 被重命名为 io_context，以及一些API变化。
编译复杂性：需要处理大量的依赖关系和版本匹配问题。
建议的解决方案
既然你要求保持 asr_usage_example 的关键流程和结构不变，我建议采用以下方案：...

(完蛋了，ai忙乎了半天，到编译它才发现问题。)

ai:这样 Qt connect 报错一定会消失，ASR 代码能100%编译通过！
(100%个锤子呀，这是给用户打气)


wangjunhui@MacBook-Pro-5 build % camke ..
zsh: command not found: camke
wangjunhui@MacBook-Pro-5 build % cmake ..
-- Finding Qt6...
-- Qt6 found: 6.9.0
-- Finding OpenCV...
-- OpenCV found: 4.11.0
-- Finding PkgConfig...
-- PkgConfig found: 1
-- Finding PortAudio...
-- PortAudio found: 19
-- Finding Opus...
-- Opus found: 1.5.2
-- Finding OGG...
-- OGG found: 1.3.5
-- Finding SndFile...
-- SndFile found: 1.2.2
-- Finding nlohmann_json...
-- nlohmann_json found: 3.12.0
CMake Warning (dev) at CMakeLists.txt:263 (find_package):
  Policy CMP0167 is not set: The FindBoost module is removed.  Run "cmake
  --help-policy CMP0167" for policy details.  Use the cmake_policy command to
  set the policy and suppress this warning.

This warning is for project developers.  Use -Wno-dev to suppress it.

-- Configuring done (0.7s)
-- Generating done (0.1s)
-- Build files have been written to: /Users/wangjunhui/playcode/perfxagent-app-1.0.1/build
wangjunhui@MacBook-Pro-5 build % make
[  0%] Built target perfx_audio_autogen_timestamp_deps
[  3%] Built target perfx_audio_autogen
[ 34%] Built target perfx_audio
[ 34%] Built target perfx_asr_qt_autogen_timestamp_deps
[ 37%] Built target perfx_asr_qt_autogen
[ 51%] Built target perfx_asr_qt
[ 51%] Built target perfxagent-app_autogen_timestamp_deps
[ 55%] Built target perfxagent-app_autogen
[ 82%] Built target perfxagent-app
[ 82%] Built target asr_usage_example_autogen_timestamp_deps
[ 86%] Built target asr_usage_example_autogen
[100%] Built target asr_usage_example
wangjunhui@MacBook-Pro-5 build % ./bin/asr_usage_example 
ASR模块使用示例 - STEP调试版本
===============================

🔒 SSL支持检测
=============
SSL支持: ✅ 是
SSL构建版本: Secure Transport, macOS Sequoia (15.5)
SSL运行时版本: Secure Transport, macOS Sequoia (15.5)

🌐 STEP0: 检查网络连接
=====================
🔗 测试WebSocket连接到: wss://openspeech.bytedance.com/api/v2/asr
✅ 网络可达: WebSocket连接成功

🚀 STEP1: 连接和鉴权
==================
Connecting to ASR server: "wss://openspeech.bytedance.com/api/v2/asr"
Final URL: "wss://openspeech.bytedance.com/api/v2/asr?token=vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3"
🔗 WebSocket连接信息:
   URL: wss://openspeech.bytedance.com/api/v2/asr
   Authorization: Bearer; vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3
   User-Agent: PerfXAgent-ASR-Client/1.0

⏳ 等待连接建立...
⏳ 等待连接中... (2秒)
⏳ 等待连接中... (4秒)
⏳ 等待连接中... (6秒)
^Z
zsh: suspended  ./bin/asr_usage_example
wangjunhui@MacBook-Pro-5 build % 


依然没有解决鉴权的问题。先上传一个版本先。


