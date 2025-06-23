# PerfXAgent-app 项目开发笔记（第五季）

## <font color="blue">【本季的主要目标 2025/6/23 09:23】</font>
1. 主要琢磨如何精确fix bug🎯，顺便完善软件体验。
2. 主要琢磨如何精确优化代码🎯，顺便完善软件体验。
3. 这季的主题是“精确打击”😄


## <font color="blue">【2025/6/23 11:35】</font>
- 处理了一些公司的事情，用碎片化时间一个一个分解我需要进行修改的模块。
- 目前录音转文本的流式输出是正常的，借鉴它的实现fix实时语音流式识别存在的问题。
[NewChat]
to_ai>> 给我分析一下这个audio_to_text_window模块如何实现流式文本的显示，要求解释从哪个模块获取什么结构的数据，经过什么转化步骤，UI使用什么策略来实现的。
ai>> 现在我来分析这个模块的流式文本显示实现机制：
流式文本显示实现分析
1. 数据来源模块
ASR服务器 → ASR客户端 → ASR管理器 → UI回调
数据流向如下：
ASR服务器：火山引擎ASR服务，通过WebSocket发送JSON格式的识别结果
ASR客户端 (AsrClient)：接收WebSocket消息，解析二进制/文本协议
ASR管理器 (AsrManager)：统一管理ASR客户端，处理识别结果
UI回调 (EnhancedAsrCallback)：处理ASR结果并更新UI
...
- 具体就不贴出来了，你可以使用我上方的提示词试试便知

to_ai>> 给我分析一下这个realtime_audio_to_text_window模块如何实现流式文本的显示，要求解释从哪个模块获取什么结构的数据，经过什么转化步骤，UI使用什么策略来实现的。并和上方实现流程进行对比，优劣势。
...
- 我看如何精准修改我要改的内容。
to_ai>> 分析一下，audio_to_text_window的流式输出从UI显示内容是正确和响应也OK。但realtime_audio_to_text_window的UI界面流式输出的文本显示出现重叠和各种问题。为什么？
ai>> ...
realtime_audio_to_text_window的流式文本显示问题主要源于：
缺乏线程安全机制
文本累积策略不当（空格vs换行）
中间结果缺乏标识
状态管理过于简化
而audio_to_text_window之所以工作正常，是因为它采用了更完善的线程安全机制、更合理的文本分隔策略和更清晰的状态管理。
...
to_ai>> 好，帮我fix realtime_audio_to_text_window的这个问题，用最小侵入策略修改。
...
- 只修改了realtime_audio_to_text_window（5）个问题，收敛在这个层面修改，应该不用太担心
- 第一次编译没有任何错误。
- 运行没有错误，但效果并不是预期的。

- copy了log信息给ai作为附件
to_ai>> 显示还是存在重复问题。
- 一次编译通过
- 运行一下，系统奔溃
to_ai>> 系统奔溃
[12:04:07.617] 📨 收到文本消息: {"error":"payload unmarshal: no request object before data"}
[12:04:07.617] ❌ ❌ 检测到错误: 客户端错误
[12:04:07.617] 🔍 错误码: 45000007
[12:04:07.617] 📝 错误详情: payload unmarshal: no request object before data
libc++abi: terminating due to uncaught exception of type std::__1::system_error: thread::join failed: Resource deadlock avoided
zsh: abort      ./bin/perfxagent-app
ai>> ...
- 直接让我realtime_transcription_controller（15）个错误。
- 我有点担心，复制了这个cpp到记事本。同意它，万一不行，可以立即恢复。
make
- 编译居然没有错误，一次过。结果依然不对。
- CURSOR提示“Start a new chat for better results.",这个应该是新增的。
ai>> 依然不对，我所期望的结果是“123456789”，但UI显示的是
12
123
1234
12345
123456
1234567
12345678
123456789
...
- 只修改了realtime_audio_to_text_window（5）个问题，我依然对这个文件用ctrl+c, ctrl+v进行快速记事本备份。
- 编译一次过。
image.png
- 【2025/6/23 12:19】总结一下，整个过程化了20来分钟。因为我在不断写这个历史记录笔记，实际上过程比这个可以更短一下。

## <font color="blue">【2025/6/23 13:05】</font>
- 中午不休息，继续推进。下午有个外出客户拜访，准备安排2个小会。
- 琢磨从一个泛的但比较准确的问题开始（不好描述，但问题肯定存在），让ai找到精确的位置，然后再精准打击。

- 刚才在测试时，发现退出时，确实存在一个问题，但问题不好描述（懒）。
to_ai>> 运行发现，STOP后的文件保存流程存在问题，全面检查一下。画出在录音状态下，按下STOP的流程出来。
ai>> …（忽略过程）…
通过检查代码，我发现了以下几个关键问题：
1. 双重保存对话框：在UI的stopRecording()方法中调用了saveRecordingDialog()，同时在控制器的stopRecording()方法中也调用了saveRecordingFiles()，这会导致用户看到两个保存对话框。
2. UI的saveRecordingDialog方法没有实际保存功能：UI中的saveRecordingDialog()方法只是显示了一个对话框，但实际的文件保存逻辑被注释掉了（// controller_->saveRecordingToFile(fileName);）。
3. 控制器的saveRecordingFiles方法使用nullptr作为父窗口：在控制器中使用QFileDialog::getExistingDirectory(nullptr, ...)会导致对话框没有正确的父窗口，可能在某些系统上显示异常。
4. 转录内容没有保存：控制器的保存方法中，转录内容部分写着"[实时转录功能待实现]"，没有保存实际的转录文本。
…
- 它确实找到了问题点。就是两次保存逻辑
- 涉及数个文件，涉及实际代码估计几十行，删除掉了一个函数和一个调用。
- 一次编译通过
- 一次运行成功，fix了一个问题。
- UI交互分支有点多，下面就不一一列举了。采用同样的精准打击🎯方式。
- 流程：泛问题-->ai分析->如果准确命中你所预期的-->fix（不过依然要提防问题扩大化）

to_ai>> 将这个界面的颜色修改为这个颜色。
to_ai>> 子界面因为存在资源冲突，所以每次只允许打开唯一的子界面。
to_ai>> 权限不对，除非子界面已经自己关闭，否则不运行切换子界面。
- 有个谭姓朋友提出，希望流式的文字输出更快一点。我试试。
to_ai>> 如果我希望加快流式文字的输出速度，中间有环节可以节约时间吗？
- 它提出了主要延迟环节：A. 音频分包延迟 (100ms)，B. UI更新延迟，C. 中间结果处理. D.减少LOG信息（我加的）
- 我挑选了减少DEBUG信息输出的选项，看看影响多大。看看打印信息就知道哪些打印信息频率高。我需要保留一些出错，减少正确情况下的打印信息。
- 我估计这个才是主要问题，因为时间太短，不会断句。
 // 当前只显示最终结果，中间结果被忽略
to_ai>> 关于你描述的这个问题，我觉得才是重要的UI视觉延迟的问题。如下位置。
if (isFinal) {
    // 显示最终文本
} else {
    // 只显示"正在识别..."，浪费了中间结果
    partialText_ = "正在识别...";
}
- 修改了数次，并不成功，文字处理逻辑ai似乎没有理解透彻。文字重复再现。
- 将刚才的临时文件 ctrl+c, ctrl+v覆盖。完美恢复到起点。
- 成功快速修改了数个问题。

- 【2025/6/23 14:00】 俗（公）事外出，暂停任务。

## <font color="blue">【2025/6/23 21:49】</font>
- 俗事结束，继续。
- 我发现需要应对音源设备可能主动断开的情况：USB、蓝牙类这样的音频设备。当音源断开时，上层应用可能不知道设备不存在了。我怀疑类似如下情况就是。此时录音应该暂停，ASR线程应该停止。
||PaMacCore (AUHAL)|| Error on line 2523: err='-50', msg=Unknown Error
[21:43:34.236] 🏓 收到 Ping
[21:43:39.183] 🏓 收到 Ping
[21:43:44.187] 🏓 收到 Ping
[21:43:49.186] 🏓 收到 Ping
to_ai>> （...忘了怎么提问的了...）
- 修改完成。先合并一个版本到云端。看看是否还有时间统一一下UI设计。这个比较复杂，担心玩砸😓！









