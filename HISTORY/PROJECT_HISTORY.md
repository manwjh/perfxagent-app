# 99%的代码使用AI Coding完成一个项目

这是我的项目笔记目录，在同一目录下,分成数个篇章，请参考！
## PROJECT_HISTORY(1)_Exploring AI Coding.md
链接：/PROJECT_HISTORY(1)_Exploring%20AI%20Coding.md)
摘要：这是一个关于AI Coding编程实验的项目开发笔记，记录了作者在完全依赖大模型开发音频处理应用的经历，最终成功构建了音频、摄像头、连接等核心模块，但也发现了AI编程在代码一致性和架构设计方面的局限性，最终以精简版应用结束第一季开发。

## PROJECT_HISTORY(2)_Integration with AI Coding.md
链接：./PROJECT_HISTORY(2)_Integration%20with%20AI%20Coding.md
摘要：记录了作者使用AI Coding实现火山引擎ASR（语音识别）API的C++客户端开发过程。主要成果包括成功集成了音频文件导入、格式转换（WAV到Opus）、WebSocket通信协议实现，以及完整的火山ASR流式语音识别功能。开发过程中遇到了Qt6 WebSocket兼容性、协议解析错误、音频包发送时机等技术挑战，通过反复调试和AI辅助最终成功实现了实时语音转文字功能，为项目添加了核心的AI语音识别能力。

## PROJECT_HISTORY(3)_Audio and ASR threads.md
连接：./PROJECT_HISTORY(3)_Audio%20and%20ASR%20threads.md
记录了作者使用AI Coding将音频处理模块与语音识别功能深度集成的开发过程。主要成果包括实现了ASR独立线程架构、完善了音频转文字界面功能、添加了音频播放控制和歌词同步格式输出，以及解决了流式ASR识别结果的实时显示问题。开发过程中遇到了音频格式转换、WebSocket连接稳定性、UI线程同步等技术挑战，通过AI辅助最终成功实现了完整的音频文件导入→格式转换→ASR识别→歌词同步的完整工作流程，为项目建立了稳定的音频处理核心功能。

## PROJECT_HISTORY(4)_Bulding UI.md
链接：./PROJECT_HISTORY(4)_Bulding%20UI.md
摘要：记录了作者使用AI Coding进行UI界面构建和实时音频转录功能开发的项目笔记，通过AI辅助实现现代化界面设计和实时音频转录功能的开发过程。主要成果包括完成了仿手机界面的主界面设计、实现了实时录音转文本功能（包含音频设备选择、实时波形显示、流式WAV录音、ASR实时转录），以及建立了UI与功能代码分离的架构模式。开发过程中遇到了AI代码修改过于激进、模块间相互影响、编译错误连锁反应等技术挑战，通过建立"重要备份"参考机制和最小侵入性修改原则，最终成功实现了完整的实时音频转录工作流程，为项目建立了现代化的用户界面和核心功能。

## PROJECT_HISTORY(5)_Accurately fixing bugs.md
链接：./PROJECT_HISTORY(5)_Accurately%20fixing%20bugs.md
摘要：记录了作者使用AI Coding进行精确修复bug和优化用户体验的项目开发笔记，记录了作者在2025年6月23日至24日期间，通过"精确打击"策略系统性地修复软件问题和完善用户体验的开发过程。主要成果包括修复了流式文本显示重复问题、解决了双重保存对话框问题、优化了音频设备断开检测机制、实现了统一的单界面布局设计，以及完善了实时语音转写字幕的分行显示逻辑。开发过程中采用了"泛问题→AI分析→精准修复"的工作模式，通过最小侵入性修改原则避免了代码扩散，最终实现了流畅的字幕同步显示效果，为项目建立了稳定的用户体验和可靠的错误处理机制。

## PROJECT_HISTORY(6)_installation package.md
链接：./PROJECT_HISTORY(6)_installation%20package.md
摘要：记录了作者使用AI Coding进行项目打包发布和资源管理的开发笔记，记录了作者在2025年6月24日期间，为PerfxAgent-ASR项目建立完整的macOS安装包打包系统和图标资源管理体系的开发过程。主要成果包括清理了CMakeLists.txt中的跨平台代码、建立了完整的图标生成体系（支持多种尺寸的PNG、ICO、ICNS格式）、实现了基于CPack的macOS DMG/TGZ安装包自动构建、修复了版本号不一致问题，以及完善了项目的文档体系。开发过程中遇到了依赖库名称不匹配、版本号冲突等技术挑战，通过AI辅助最终成功建立了完整的项目发布流程，为项目建立了专业级的打包发布能力和资源管理体系。

## PROJECT_HISTORY(7)_MacOS and Cross Platfform.md
链接：./ROJECT_HISTORY(7)_MacOS%20and%20Cross%20Platfform.md
摘要：成功完成 macOS 应用程序的 Developer ID 签名和打包，解决了 OpenCV 残留依赖库问题，这个是造成本季进度不如预期的重要原因之一。

## PROJECT_HISTORY(8)_ Clear ALL.md
链接：./PROJECT_HISTORY(8)_Clear%20ALL.md
摘要：清理bug，清理不合理的地方，清理架构荣誉，清理不归一，清理安全等问题

## 结束语
（暂无）
