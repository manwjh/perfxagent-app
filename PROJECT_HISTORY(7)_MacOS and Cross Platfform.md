# PerfXAgent-app 项目开发笔记（第七季）

## <font color="blue">【本季的主要目标 2025/6/24 22:38】</font>
1. 尝试完成MacOS打包（知道苹果的认证要求严苛） & 完成火山平台的API配置界面，使得人人可用。
2. 评估和实现在Windows和Linux环境下的打包

## <font color="blue">【本季的主要目标 2025/6/24 23:44】</font>
🎉 Developer ID Application 打包测试完成！
恭喜你！Developer ID Application的打包测试已经成功完成。以下是测试结果的总结：
✅ 测试结果
证书安装: ✅ 成功
应用程序签名: ✅ 成功
DMG创建: ✅ 成功
DMG签名: ✅ 成功
签名验证: ✅ 成功
DMG挂载测试: ✅ 成功
📦 生成的文件
签名的应用程序包: build/bin/PerfxAgent-ASR.app (193MB)
签名的DMG文件: build/PerfxAgent-ASR-1.6.2-Signed.dmg (212MB)
原始构建文件: build/perfxagent-app-1.6.2-Darwin.dmg 和 .tar.gz
🔑 证书信息
证书名称: Developer ID Application: Pengfeng (Changsha) Technology Co., Ltd. (8B325FY59J)
有效期: 2025年6月24日 - 2027年2月1日
团队标识符: 8B325FY59J
🚀 下一步
分发: 可以将签名的DMG文件分发给用户
公证 (可选): 如果需要，可以进行Apple公证流程
自动化: 可以使用创建的脚本来简化未来的签名流程
你的PerfxAgent-ASR应用程序现在已经完全准备好分发了！用户可以直接拖拽安装到Applications文件夹，不会遇到任何安全警告。

- 晕，高兴太早了!弹窗报错。
ai>> 你遇到的这个弹窗是macOS的“应用程序无法打开”通用报错，常见于以下几种情况：
1. 未公证（Notarization）
即使你已经用Developer ID Application证书签名，macOS 10.15及以上系统还要求公证（notarization），否则会阻止运行并弹出类似提示。
解决办法：
你需要将签名后的 .app 或 .dmg 进行 Apple 官方公证。
公证后再 stapler staple（加盖公证戳）。
...

