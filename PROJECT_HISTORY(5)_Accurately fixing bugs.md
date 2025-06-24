# PerfXAgent-app 项目开发笔记（第五季）

## <font color="blue">【本季的主要目标 2025/6/23 09:23】</font>
1. 主要琢磨如何精确fix bug🎯，顺便完善软件体验。
2. 主要琢磨如何精确优化代码🎯，顺便完善软件体验。
3. 这季的主题是“精确打击”😄
4. 界面统一化修改的尝试。


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
YT Demo Video:https://www.youtube.com/watch?v=RSnetYNAqog

## <font color="blue">【2025/6/23 22:17】</font>
to_ai>> 按照如图所示，修改一下界面。界面的外框尺寸要借鉴main_window的.
- 失败了，并不是我预期的。如何告诉AI，以及修改的一致性，在这种显性的事情上是个问题。
to_ai>> 我想和你讨论一下ui界面的问题。现在是有三个分离的界面，是否可以以主界面为主题，点击进入到的子界面不是建立一个新的页面，而是用换掉界面布局的方式进行呢？
你的想法是将当前的三个分离界面（比如主界面、音频转文本界面、实时转写界面等）整合为一个主界面，点击不同功能时，不是弹出新窗口或新页面，而是在主界面内部切换不同的布局或内容区域，对吗？
这种方式在桌面应用开发中很常见，通常有以下几种实现方式：
堆叠布局（Stacked Layout）
用一个QStackedWidget（如果你用的是Qt）或类似的控件，把所有子界面都作为不同的“页面”加进去，点击主界面的按钮时切换QStackedWidget的当前页面即可。
动态加载/卸载布局
主界面有一个内容区域，点击不同按钮时，把内容区域的布局或widget替换成对应的子界面widget。
Tab页（选项卡）
如果功能之间切换频繁，也可以用QTabWidget，不过这会让所有功能都以标签页的形式并列展示。
优点
用户体验更统一，界面切换流畅。
资源占用更低（不用频繁创建/销毁窗口）。
更容易做全局状态管理。
需要注意
子界面之间的状态切换、数据传递要设计好。
如果子界面很复杂，建议用懒加载（用到时才创建）。
如果你同意这种方式，可以告诉我你用的UI框架（比如Qt、wxWidgets、Electron等），我可以帮你具体设计一下结构和代码实现思路。
如果你有现有的主界面和子界面代码，也可以发给我，我帮你看看怎么改造。
to_ai>> 这只用改动ui部分的代码，而不会需要修改底层代码吧
...
to_ai>> 好，帮我对这个项目进行改造
...
- 编译出错fix 3次。
- 运行，出错。（我很奇怪，为什么改界面，会出现系统错误。不知道代码哪里关联了）
to_ai>> 运行出错。
[22:51:41.707] 🎯 成功获取 X-Tt-Logid: 20250623225141CC82F821B827851D9DA4
[22:51:41.707] ℹ️ ✅ WebSocket 连接已建立
[DEBUG] ASR connection opened
[22:51:41.800] ℹ️ ✅ ASR 连接成功
[22:51:41.800] ℹ️ ℹ️ ASR 客户端已经连接
[22:51:41.892] ℹ️ 🎯 识别结果:  (实时)
[DEBUG] JSON对象键: audio_info, result
[DEBUG] result对象键: additions, text
[DEBUG] 提取到text: , isFinal: 0
[22:51:41.899] ℹ️ ⚠️ 解析Full Server Response失败: [json.exception.parse_error.101] parse error at line 1, column 1: syntax error while parsing value - invalid literal; last read: 'S'
[22:51:41.899] ℹ️ ✅ 识别会话已开始
[DEBUG] ASR streaming recognition started
zsh: segmentation fault  ./bin/perfxagent-app

- (忘了干了什么了，我被解决完美的流式输出体验给吸引了，不过界面改好了。忘了过程)
<img width="438" alt="Pasted Graphic 7" src="https://github.com/user-attachments/assets/2eb759b4-e4bf-42db-8942-7d613d976e62" />
<img width="446" alt="0000 00" src="https://github.com/user-attachments/assets/865e09f6-b5cd-471a-9e65-0593a71aee60" />
<img width="429" alt="Pasted Graphic 6" src="https://github.com/user-attachments/assets/33a437ab-597d-459c-97a9-c4a3ccd9f4a8" />
- 
- ai提出了这种方式
a>> ...
1. 只显示“最终确定”的分句（definite=true）
理由：用户最关心的是“最终结果”，不希望看到反复变化、闪烁或撤回的内容。
做法：只把 definite=true 的分句追加到显示区，未确定的分句作为“临时预览”显示在最后一行（样式可区分，如灰色或斜体）。
2. 实时预览“未确定”分句
理由：用户希望看到系统正在识别的内容，哪怕还没最终确定。
做法：将当前未确定的分句（definite=false）以不同样式显示在最后一行，等它变成 definite=true 时再“固化”到主文本区。
3. 不要重复显示同一句话
理由：避免视觉噪音和困惑。
做法：每个 definite=true 的分句只显示一次，未确定的分句只显示最新的。
4. 可选：显示分句时间戳
理由：对需要回溯、定位的用户有帮助，但对普通用户可选。
做法：可在每句后面加上起止时间（如 00:01-00:03），用较小字体或灰色。
5. 只用 utterances 字段
理由：utterances 结构化、分句明确，适合逐句显示和管理。
做法：每次收到 utterances，遍历所有分句，按 definite 状态分别处理。
...

- 我已经彻底把权限交给ai了，因为涉及到asr返回的数据结构样式，流的转换，界面的修改。
...
- 依然没达到我的要求，需要更准确的语言来描述我要的效果。
ai>> 帮我修正，包括现在渲染过于复杂，界面显示信息量太多。用户感官上希望的是音频和字幕同步跟进，可以在完整信息没有固化前显示字级别的累加信息，一旦句子被固化，立即覆盖掉前面的临时句子。

例如：字级别跟随（缓冲）-->确信可以固化句子时，将前面字用最终固化的信息去覆盖掉。

我描述清楚了吗？
...
- 底层信息扔给大模型。
to_ai>> 这是一段原始ASR消息，你能获得更多的解读，来解决分行问题吗？
 {"audio_info":{"duration":29800},"result":{"additions":{"log_id":"20250624002444473C75F770D1F2358394"},"text":"分，并且适用符合。Mega 的方式去得分，以 Mega 不是担心说要长期对以色列，对伊朗发动战争，这个东西你不干，那我保证是一击脱离。嗯嗯，这是我只炸一次，所以。特朗普这次非常重要点就是告诉你，伊朗你千万不要还手啊，对。你千万千万不要还手，你千万不要打美国人，那就是你打以色列，我先不管。哈哈哈。","utterances":[{"definite":true,"end_time":1540,"start_time":280,"text":"分，并且适用符合。","words":[{"end_time":360,"start_time":280,"text":"分"},{"end_time":520,"start_time":440,"text":"并且"},{"end_time":1080,"start_time":1000,"text":"适用"},{"end_time":1540,"start_time":1460,"text":"符合"}]},{"definite":true,"end_time":9140,"start_time":2600,"text":"Mega 的方式去得分，以 Mega 不是担心说要长期对以色列，对伊朗发动战争，这个东西你不干，那我保证是一击脱离。","words":[{"end_time":-1,"start_time":-1,"text":" "},{"end_time":2680,"start_time":2600,"text":"Mega"},{"end_time":-1,"start_time":-1,"text":" "},{"end_time":2920,"start_time":2840,"text":"的"},{"end_time":3080,"start_time":3000,"text":"方式"},{"end_time":3400,"start_time":3320,"text":"去"},{"end_time":3560,"start_time":3480,"text":"得分"},{"end_time":3880,"start_time":3800,"text":"以"},{"end_time":-1,"start_time":-1,"text":" "},{"end_time":3960,"start_time":3880,"text":"Mega"},{"end_time":-1,"start_time":-1,"text":" "},{"end_time":4200,"start_time":4120,"text":"不是"},{"end_time":4360,"start_time":4280,"text":"担心"},{"end_time":4600,"start_time":4520,"text":"说"},{"end_time":4760,"start_time":4680,"text":"要"},{"end_time":5080,"start_time":5000,"text":"长期"},{"end_time":5640,"start_time":5560,"text":"对"},{"end_time":5800,"start_time":5720,"text":"以色列"},{"end_time":6280,"start_time":6200,"text":"对"},{"end_time":6360,"start_time":6280,"text":"伊朗"},{"end_time":6760,"start_time":6680,"text":"发动"},{"end_time":6920,"start_time":6840,"text":"战争"},{"end_time":7240,"start_time":7160,"text":"这个"},{"end_time":7320,"start_time":7240,"text":"东西"},{"end_time":7480,"start_time":7400,"text":"你"},{"end_time":7720,"start_time":7640,"text":"不"},{"end_time":7800,"start_time":7720,"text":"干"},{"end_time":7880,"start_time":7800,"text":"那"},{"end_time":8040,"start_time":7960,"text":"我"},{"end_time":8120,"start_time":8040,"text":"保证"},{"end_time":8440,"start_time":8360,"text":"是"},{"end_time":8600,"start_time":8520,"text":"一"},{"end_time":8840,"start_time":8760,"text":"击"},{"end_time":9140,"start_time":9060,"text":"脱离"}]},{"definite":true,"end_time":11060,"start_time":9400,"text":"嗯嗯，这是我只炸一次，所以。","words":[{"end_time":9480,"start_time":9400,"text":"嗯"},{"end_time":9640,"start_time":9560,"text":"嗯"},{"end_time":10120,"start_time":10040,"text":"这"},{"end_time":10200,"start_time":10120,"text":"是我"},{"end_time":10520,"start_time":10440,"text":"只"},{"end_time":10680,"start_time":10600,"text":"炸"},{"end_time":10760,"start_time":10680,"text":"一"},{"end_time":10840,"start_time":10760,"text":"次"},{"end_time":11060,"start_time":10980,"text":"所以"}]},{"definite":true,"end_time":14980,"start_time":11560,"text":"特朗普这次非常重要点就是告诉你，伊朗你千万不要还手啊，对。","words":[{"end_time":11640,"start_time":11560,"text":"特"},{"end_time":11800,"start_time":11720,"text":"朗"},{"end_time":11880,"start_time":11800,"text":"普"},{"end_time":12040,"start_time":11960,"text":"这次"},{"end_time":12200,"start_time":12120,"text":"非常"},{"end_time":12520,"start_time":12440,"text":"重要"},{"end_time":12760,"start_time":12680,"text":"点"},{"end_time":12840,"start_time":12760,"text":"就是"},{"end_time":13000,"start_time":12920,"text":"告诉"},{"end_time":13160,"start_time":13080,"text":"你"},{"end_time":13320,"start_time":13240,"text":"伊朗"},{"end_time":13560,"start_time":13480,"text":"你"},{"end_time":13720,"start_time":13640,"text":"千"},{"end_time":13800,"start_time":13720,"text":"万"},{"end_time":13880,"start_time":13800,"text":"不要"},{"end_time":14120,"start_time":14040,"text":"还"},{"end_time":14200,"start_time":14120,"text":"手"},{"end_time":14280,"start_time":14200,"text":"啊"},{"end_time":14980,"start_time":14900,"text":"对"}]},{"definite":true,"end_time":19300,"start_time":15240,"text":"你千万千万不要还手，你千万不要打美国人，那就是你打以色列，我先不管。","words":[{"end_time":15320,"start_time":15240,"text":"你"},{"end_time":15560,"start_time":15480,"text":"千"},{"end_time":15640,"start_time":15560,"text":"万"},{"end_time":15880,"start_time":15800,"text":"千"},{"end_time":15960,"start_time":15880,"text":"万"},{"end_time":16120,"start_time":16040,"text":"不要"},{"end_time":16280,"start_time":16200,"text":"还"},{"end_time":16360,"start_time":16280,"text":"手"},{"end_time":16520,"start_time":16440,"text":"你"},{"end_time":16680,"start_time":16600,"text":"千"},{"end_time":16760,"start_time":16680,"text":"万"},{"end_time":16840,"start_time":16760,"text":"不要"},{"end_time":17160,"start_time":17080,"text":"打"},{"end_time":17320,"start_time":17240,"text":"美国"},{"end_time":17480,"start_time":17400,"text":"人"},{"end_time":18120,"start_time":18040,"text":"那"},{"end_time":18280,"start_time":18200,"text":"就是"},{"end_time":18440,"start_time":18360,"text":"你"},{"end_time":18520,"start_time":18440,"text":"打"},{"end_time":18680,"start_time":18600,"text":"以色列"},{"end_time":18920,"start_time":18840,"text":"我"},{"end_time":19080,"start_time":19000,"text":"先"},{"end_time":19300,"start_time":19220,"text":"不管"}]},{"definite":true,"end_time":20180,"start_time":20100,"text":"哈哈哈。","words":[{"end_time":20180,"start_time":20100,"text":"哈哈哈"}]},{"definite":false,"end_time":-1,"start_time":-1,"text":""}]}}

 - 终于达到我心里预期了。🙏
 - 再让ai总结一下。
to_ai>> 你遇到的问题
实时语音转写字幕分行/换行体验不佳，有时换行过于频繁，有时内容碎片化，影响用户阅读。
ASR 原始消息中 blank_duration 字段基本为 0，无法用静音时长智能分句。
你希望字幕每句自然分行，短句自动合并，预览区实时跟随，整体体验更接近主流字幕产品。
解决方案与实现
分析 ASR 原始消息结构
每个 utterance 有 definite、text、words 等字段。
definite=true 表示该分句已固化，可以作为主区显示。
definite=false 表示当前正在识别的分句，用于预览区。
分行/换行逻辑优化
每个 definite=true 的分句作为一行。
如果分句很短（如少于6字），自动合并到上一行，避免字幕碎片化。
预览区只显示当前未固化分句的字级别拼接，灰色斜体实时跟随。
实现效果
字幕主区每句自然分行，短句不会单独成行。
预览区实时显示正在识别的内容，体验流畅。
不依赖 blank_duration 字段，兼容你的实际 ASR 数据。
你现在获得的体验
主区：每句自然分行，短句自动合并，阅读流畅。
预览区：实时字级别跟随，灰色斜体，体验接近主流字幕产品。
整体界面简洁、自然、无重复、无跳动。

## <font color="blue">【2025/6/24 00:43】</font>
- 结束今天的工作。
- 也结束本季。
- 就这么滴。
<img width="508" alt="image" src="https://github.com/user-attachments/assets/6dfcdf03-d827-463a-8040-11e575a42b97" />
这个视频是本季过程代码，新的就没有录了。
https://www.youtube.com/shorts/RSnetYNAqog








