# PerfXAgent-app 项目发展记录

## 项目记录

【2025/5/24】   
我是一个"老"程序员，不过已经有十多年不写代码了。大模型唤醒了我写代码的灵魂，哈哈哈，因为你不需要再花时间去复习代码的编程规范，同时也不需要去自己update的最新的知识。所以，我想尝试用一个全新的方式"写"项目，试图不写"一行"实际代码。

【2025/5/24】   
我是从README.md描述这个项目开始。后续将让大模型总是回归到README.md这个文件，为基础目标。所以模式是：先和大模型一起修改README.md，然后再具体到让大模型进行coding工作。 

【2025/5/24】   
我今天计划编写完成audio_settings的界面部分。
有趣的是，我发现大模型并不会为这个模块建立初始化函数，由此造成一些状态的混乱。
包括全局性，界面和后端的相关性，独立性，大模型考虑得并不周全。也许不怪大模型，怪的是我们项目描述做得不够仔细。
我在小心翼翼地和大模型垒房子。

【2025/5/24 22:47】
程序在我的macbook上可以基本运行，先试试如何上传到github上去。方便我能找到和大模型一起潦潦草草搭建的项目。到目前为止，我没有写过一行代码。
<img width="796" alt="image" src="https://github.com/user-attachments/assets/f05e8cd3-91df-4f98-ac51-8e7ae166d692" />

我：我打算上传这个项目到我的git站点   https://github.com/manwjh/perfxagent-app
cursor: 好的，我来帮你准备上传代码到 GitHub 仓库。首先我们需要初始化 git 仓库并提交代码。


【2025/5/24 23:38】
我忍不住想构建第二个界面，服务器设定界面，server_settings.
我：增加一个界面，名称为：PerfXAgent服务器设置。
1. 参考/Users/wangjunhui/playcode/perfxagent-app/websocket-protocol.md文档，的“连接建立”部分内容。
2. 界面有用户填写
客户端连接Websocket服务器时需要携带的headers。
3. 一个测试按钮。看是否可以建立连接成功。
cursor：一顿操作
cursor：此处省略至少数千字。
我：有问题就改呀，笨蛋...
cursor：此处省略至少数千字。
我：全面检查项目，确保可以编译并运行。
（不断编译运行，不断出错，不断一顿操作猛如虎。受限于他的记忆长度问题，很多重复动作）
老婆：赶紧洗澡睡觉了。
我：编译运行
cursor: 依然有错误，依然要修复。
我：今天不陪你玩了，洗澡睡觉去。
=======
【2025/5/25 10:12】
加了一个服务器模块后，一堆错误。不过，不用自己动脑子，就是不断和大模型唠唠嗑，没啥强度。
如果一个稍微大一点的项目，模块是一个一个编写的。模块与模块之间风格的统一性，很难保证。
在代码不大加大的情况下，大模型无法进行全面交叉检验。稍微动一下，不是修修补补，是几乎再重构一次。
<img width="1508" alt="image" src="https://github.com/user-attachments/assets/940993b6-c00f-4d99-ae33-c1c5ef588cdd" />

也正常。以前我们自己手搓一个项目，写到后面，整个项目自己看到都想吐。恨不得重构，可以太过于“精巧”和“庞大”了，最终就是修修补补又一年。

【2025/5/25 10:18】
哈哈哈，终于fix了各种东西，并且将代码风格，目录结构也规整了一下。又一个界面写好了，至少初始化可以，能不能跑，周一就知道了。

<img width="792" alt="image" src="https://github.com/user-attachments/assets/d7c3c94a-f133-4258-8f12-db3c23251957" />

【2025/5/25 14:44】
再建立一个界面：Agent Zoom
1. 从服务器获取h5页面，在本机进行交互。
2. 但服务器部分还没协商，那就随便弄个页面试试。例如https://aiot.llmworld.net/#/login

【2025/5/25 15:45】
一口气增加了“开始页面”，“用户信息页面”，增加了系统配置文件。都不知道大模型堆了多少代码。按照这种搞法，每次版本升级都是翻天覆地的改变。
没事，塌了就塌了，大模型堆的很快。
等一下让大模型重新review所有代码，自己评估，自己修改。
和大模型一起工作的逻辑是：干就是了，别怕房子塌掉。

我：对于界面部分的代码结构和方法，请遵循统一原则。check 所有ui部分的代码，你认为怎么修改。
cursor: 
收到！我将自动统一和修正所有UI相关代码结构，主要包括：
所有UI类、头文件、实现文件都放入 perfx::ui 命名空间
头文件引用、成员变量类型全部用全限定名（如 perfx::ui::AudioSettingsWidget*）
成员变量加下划线结尾
信号与槽、方法命名风格统一
头文件引用只保留必要内容
StartPage 也纳入 perfx::ui 命名空间
我将依次修改以下文件：
include/ui/main_window.h / src/ui/main_window.cpp
include/ui/audio_settings.h / src/ui/audio_settings.cpp
include/ui/server_settings.h / src/ui/server_settings.cpp
include/ui/agent_zoom.h / src/ui/agent_zoom.cpp
include/ui/start_page.h / src/ui/start_page.cpp
include/ui/user_info.h / src/ui/user_info.cpp
我会先从 main_window.h 开始，逐步推进。
......
我：吃披萨🍕去了。偶尔需要不管不顾按一下“Accept“。

【2025/5/25 16:25】
这个大厦似乎要塌了，重复修改，重复编译运行。依然没有解决问题。我要赶高铁去了。

【2025/5/25 18:53】
🚄 高铁上...
我review了一下大模型写的代码和架构，程序可以跑，但有很多不完善的地方。大部分原因是，项目开始作者脑袋里并没有完整的描述，边提需求边想下一步。如果能越详细、且专业描述需求，AI编写的代码可靠性和统一性也会高许多。
我修改了一下README.md，思考下一步应该和大模型怎么合作 ：）。
当前版本几个大的问题：
1. 界面被AI大改了一番，改得惊天动地。（大模型看不到自己写的界面，没有反馈回路。它需要我专业的吐槽，才能修改。哈哈哈，我们合作的方法，是不是变成了我负责吐槽，它负责干活。）
2. 界面/功能的概念混在一起，功能不便于其他模块调用。例如start模块要调用server_settings模块的服务器握手功能，这就没法弄。这涉及到软件系统的架构规划，清晰地定义。大模型估计以为我只是在做一个界面显示为核心的软件。那么这个软件的核心架构应该怎么定义呢？

思考：界面/模块一个一个写，一个一个测试。全局性的东西，应该如何贯穿到整个项目实施过程呢？也许应该建一个单独的技术架构文档，每次提问都将这个文档带上。所以，我起草了technical_arch.md，打算试试这种想法。先写“## 界面风格“段落。

【2025/5/26 22:35】
我这个架构设计得太烂了，重来。合并audio_settings和server_settings的界面。
放飞自我，让cursor一旦操作。
💔💔💔
我发现一些代码被它修改过程掏空了，晕倒。

【2025/5/28 20:49】
实际上，我已经从5点，陪着大模型折腾到了现在。
我完全不相信一个没有较深编程基础的人，能玩得转现在的大模型编程。
对他所有的修改建议，请保持“怀疑”。全信他，你就完蛋了。

理由，好多好多。找个机会演示一下。

例如：WebSocket 和 Boost.Asio 的“不兼容问题”。（这个知识点，起初我是不懂的。）
fatal error: too many errors emitted, stopping now [-ferror-limit=]
20 errors generated.
make[2]: *** [CMakeFiles/perfxagent-app.dir/src/connection/websocket_client.cpp.o] Error 1
make[1]: *** [CMakeFiles/perfxagent-app.dir/all] Error 2

/* 妈的 */
是的,我完全理解这一点。从项目历史记录中,我看到你们遇到了 WebSocket 和 Boost.Asio 的兼容性问题。让我来解释一下这个问题和解决方案:
Boost.Beast 确实是官方推荐的 WebSocket 实现库,它完全基于 Boost.Asio 构建,提供了完整的 WebSocket 客户端和服务器功能。使用 Boost.Beast 的主要优势是:
与 Boost.Asio 完全兼容,因为它们都是 Boost 生态系统的一部分
提供了完整的 WebSocket 协议实现
性能优秀,因为直接基于 Boost.Asio 的异步 I/O
维护良好,是 Boost 官方支持的库

/* 妈的 */
那你怎么一遍一遍在Boost.Asio上打转呢？

【2025/5/28 22:46】
终于完成了编译和运行。我已经开始要骂娘了，but，如果我自己写这个代码，很显然我已经搞不定这么多代码了。太多新的知识没法短时间补全。
所以，我忍了！ ：）

