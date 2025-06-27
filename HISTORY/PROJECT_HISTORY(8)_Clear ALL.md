# PerfXAgent-app 项目开发笔记（第八季）

## <font color="blue">【本季的主要目标 2025/6/27 11:35】</font>
1. 小心.清理.所有.问题。
2. 完成Apple dmg版本和发行版
3. 这可能需要很长时间，为了保持文档的可读性，将不再写和ai的对话内容。我将摘录一些有趣的提示词出来，以及界面修改的状态。

- 上一季，梳理LOG信息，改动太大了，一开始报错非常多，我以为这个版本要废了。刚刚发现，至少编译错误居然没有了，估计是最后一个fix起到了关键作用。
- 调整代码结构，例如：
    - 职责不清：这个函数承担了太多责任，违反了单一职责原则
    - 公共功能和界面耦合的。（造成上层调用，与之无关的部分也会启动一次）。

- 有趣，确实省事呀。
to_ai>> statusBar_ 显示方式需要统一
1. 修改为中文显示，不要有文字底色。
2. 成功类信息，显示为白字
3. 失败类信息，显示为黄字
4. 警告类信息，显示为红字

- 差不过改好一个界面，上传一个视频。
https://www.bilibili.com/video/BV1o8KizFEvF/?vd_source=4ab49101e323bf3a37e7b81812524444
<img width="1490" alt="image" src="https://github.com/user-attachments/assets/154c55f4-260c-4b91-83a5-88613e2b5170" />

- 主界面也展示一下。
<img width="691" alt="image" src="https://github.com/user-attachments/assets/b8fccbff-20bf-4ab6-ab02-595167ea795c" />

- 不再在本地编写PROJECT HISTORY，不好插图，不好预览。文字多了增加AI工作难度。后续在线上直接写✍️。

- 系统互锁的问题。


