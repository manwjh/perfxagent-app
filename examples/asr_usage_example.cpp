/**
 * ASR模块使用示例 - STEP调试版本 (纯Qt版本)
 * 
 * 本示例展示了如何使用PerfXAgent项目中的ASR模块进行语音识别
 * 按照STEP逻辑组织，便于调试和理解每个阶段
 * 使用Qt的QWebSocket实现，与火山ASR服务进行WebSocket通信
 */

// 全局凭据定义
#define ASR_APP_ID "8388344882"
#define ASR_ACCESS_TOKEN "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3"
#define ASR_SECRET_KEY "oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e"

#include "asr/asr_qt_client.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QThread>
#include <iostream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QSslSocket>
#include <QWebSocket>

// 全局变量用于控制流程
static bool g_connectionEstablished = false;
static bool g_fullRequestSent = false;
static bool g_audioSent = false;
static bool g_serverResponded = false;
static int g_step = 0;

// 回调类 - 适配纯Qt版本
class StepByStepQtCallback : public Asr::AsrQtCallback {
public:
    void onOpen(Asr::AsrQtClient* asr_client) override {
        (void)asr_client;
        std::cout << "\n✅ STEP1 完成: WebSocket连接已建立" << std::endl;
        std::cout << "   - WebSocket握手完成" << std::endl;
        std::cout << "   - 连接状态: 已连接" << std::endl;
        std::cout << "   - Full Client Request已自动发送" << std::endl;
        g_connectionEstablished = true;
        g_step = 1;
    }
    
    void onMessage(Asr::AsrQtClient* asr_client, const QString& msg) override {
        (void)asr_client;
        std::cout << "\n📨 收到服务器消息: " << msg.toStdString() << std::endl;
        
        // 尝试解析JSON消息
        QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            std::cout << "📋 解析后的JSON消息:" << std::endl;
            
            // 打印所有字段
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                QString key = it.key();
                QJsonValue value = it.value();
                
                if (value.isString()) {
                    std::cout << "   " << key.toStdString() << ": " << value.toString().toStdString() << std::endl;
                } else if (value.isDouble()) {
                    std::cout << "   " << key.toStdString() << ": " << value.toDouble() << std::endl;
                } else if (value.isBool()) {
                    std::cout << "   " << key.toStdString() << ": " << (value.toBool() ? "true" : "false") << std::endl;
                } else if (value.isObject()) {
                    std::cout << "   " << key.toStdString() << ": {object}" << std::endl;
                } else if (value.isArray()) {
                    std::cout << "   " << key.toStdString() << ": [array]" << std::endl;
                }
            }
            
            // 检查特定字段
            if (obj.contains("code")) {
                int code = obj["code"].toInt();
                std::cout << "🔢 响应代码: " << code << std::endl;
                if (code != 1000) {
                    std::cout << "⚠️  非成功响应代码" << std::endl;
                } else {
                    std::cout << "✅ 成功响应代码" << std::endl;
                }
            }
            
            if (obj.contains("message")) {
                std::cout << "💬 服务器消息: " << obj["message"].toString().toStdString() << std::endl;
            }
            
            if (obj.contains("sequence")) {
                int sequence = obj["sequence"].toInt();
                std::cout << "🔢 序列号: " << sequence << std::endl;
            }
            
            g_serverResponded = true;
        }
    }
    
    void onError(Asr::AsrQtClient* asr_client, const QString& error) override {
        (void)asr_client;
        std::cerr << "\n❌ 发生错误: " << error.toStdString() << std::endl;
        std::cerr << "🔍 错误详情: WebSocket连接或通信错误" << std::endl;
    }
    
    void onClose(Asr::AsrQtClient* asr_client) override {
        (void)asr_client;
        std::cout << "\n🔌 连接已关闭" << std::endl;
    }
};

// 检测SSL支持
bool check_ssl_support() {
    std::cout << "\n🔒 SSL支持检测" << std::endl;
    std::cout << "=============" << std::endl;
    
    bool sslSupported = QSslSocket::supportsSsl();
    QString sslVersion = QSslSocket::sslLibraryBuildVersionString();
    QString sslRuntimeVersion = QSslSocket::sslLibraryVersionString();
    
    std::cout << "SSL支持: " << (sslSupported ? "✅ 是" : "❌ 否") << std::endl;
    std::cout << "SSL构建版本: " << sslVersion.toStdString() << std::endl;
    std::cout << "SSL运行时版本: " << sslRuntimeVersion.toStdString() << std::endl;
    
    if (!sslSupported) {
        std::cout << "⚠️  Qt没有SSL支持，无法建立wss连接" << std::endl;
        std::cout << "   请检查OpenSSL安装和Qt SSL配置" << std::endl;
    }
    
    return sslSupported;
}

// STEP0: 检查网络连接
bool step0_check_network() {
    std::cout << "\n🌐 STEP0: 检查网络连接" << std::endl;
    std::cout <<   "=====================" << std::endl;
    
    // 使用WebSocket连接测试，而不是HTTPS GET
    QWebSocket testSocket;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(5000); // 5秒超时
    
    bool connected = false;
    bool error = false;
    QString errorMsg;
    
    QObject::connect(&testSocket, &QWebSocket::connected, [&]() {
        connected = true;
        loop.quit();
    });
    
    QObject::connect(&testSocket, &QWebSocket::errorOccurred, [&](QAbstractSocket::SocketError socketError) {
        (void)socketError;
        error = true;
        errorMsg = testSocket.errorString();
        loop.quit();
    });
    
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (!connected && !error) {
            error = true;
            errorMsg = "连接超时";
            loop.quit();
        }
    });
    
    std::cout << "🔗 测试WebSocket连接到: wss://openspeech.bytedance.com/api/v2/asr" << std::endl;
    testSocket.open(QUrl("wss://openspeech.bytedance.com/api/v2/asr"));
    
    loop.exec();
    
    if (connected) {
        std::cout << "✅ 网络可达: WebSocket连接成功" << std::endl;
        testSocket.close();
        return true;
    } else {
        std::cout << "❌ 网络不可达: " << errorMsg.toStdString() << std::endl;
        return false;
    }
}

// STEP1: 连接和鉴权
void step1_connect_and_auth(Asr::AsrQtClient* asrClient) {
    std::cout << "\n🚀 STEP1: 连接和鉴权" << std::endl;
    std::cout << "==================" << std::endl;
    
    // 配置ASR客户端
    QString appId = ASR_APP_ID;
    QString token = ASR_ACCESS_TOKEN;
    QString secretKey = ASR_SECRET_KEY;
    
    asrClient->setAppId(appId);
    asrClient->setToken(token);
    asrClient->setSecretKey(secretKey);
    asrClient->setAuthType(Asr::AsrQtClient::TOKEN);
    asrClient->setAudioFormat("wav", 1, 16000, 16);
        
    // 连接到ASR服务器
    if (!asrClient->connect()) {
        std::cerr << "❌ 连接失败" << std::endl;
        return;
    }
    
    std::cout << "🔗 WebSocket连接信息:" << std::endl;
    std::cout << "   URL: wss://openspeech.bytedance.com/api/v2/asr" << std::endl;
    std::cout << "   Authorization: Bearer; " << token.toStdString() << std::endl;
    std::cout << "   User-Agent: PerfXAgent-ASR-Client/1.0" << std::endl;
    std::cout << std::endl;
    
    std::cout << "⏳ 等待连接建立..." << std::endl;
    
    // 等待连接建立
    int waitCount = 0;
    while (!g_connectionEstablished && waitCount < 100) { // 增加到10秒
        QThread::msleep(100);
        waitCount++;
        
        // 每2秒打印一次状态
        if (waitCount % 20 == 0) {
            std::cout << "⏳ 等待连接中... (" << (waitCount / 10) << "秒)" << std::endl;
        }
    }
    
    if (g_connectionEstablished) {
        std::cout << "✅ STEP1 成功: 连接和鉴权完成" << std::endl;
        std::cout << "🚚 实际发送的Full Client Request JSON:" << std::endl;
        std::cout << asrClient->getFullClientRequestJson().toStdString() << std::endl;
    } else {
        std::cout << "❌ STEP1 失败: 连接超时" << std::endl;
    }
}

// STEP2: 等待Full Client Request响应
void step2_wait_full_client_response(Asr::AsrQtClient* asrClient) {
    (void)asrClient; // 标记参数为已使用
    std::cout << "\n📤 STEP2: 等待Full Client Request响应" << std::endl;
    std::cout << "=================================" << std::endl;
    
    if (!g_connectionEstablished) {
        std::cout << "❌ 连接未建立，跳过STEP2" << std::endl;
        return;
    }
    
    std::cout << "⏳ 等待Full Client Request响应..." << std::endl;
    std::cout << "   - 连接建立后已自动发送Full Client Request" << std::endl;
    std::cout << "   - 等待服务器确认响应" << std::endl;
    
    // 等待服务器响应
    int waitCount = 0;
    while (!g_serverResponded && waitCount < 50) { // 等待5秒
        QThread::msleep(100);
        waitCount++;
    }
    
    if (g_serverResponded) {
        std::cout << "✅ STEP2 成功: 收到Full Client Request响应" << std::endl;
        g_fullRequestSent = true;
        g_step = 2;
    } else {
        std::cout << "⚠️  STEP2 警告: 未收到Full Client Request响应" << std::endl;
        std::cout << "   尝试手动发送Full Client Request..." << std::endl;
        
        // 尝试手动发送
        if (asrClient->sendAudio(QByteArray(), false)) {
            std::cout << "✅ 手动发送Full Client Request成功" << std::endl;
            g_fullRequestSent = true;
            g_step = 2;
            
            // 再次等待响应
            waitCount = 0;
            while (!g_serverResponded && waitCount < 30) {
                QThread::msleep(100);
                waitCount++;
            }
            
            if (g_serverResponded) {
                std::cout << "✅ 收到手动发送的Full Client Request响应" << std::endl;
            } else {
                std::cout << "⚠️  仍未收到响应" << std::endl;
            }
        } else {
            std::cout << "❌ 手动发送Full Client Request失败" << std::endl;
        }
    }
}

// STEP3: 发送音频数据
void step3_send_audio_data(Asr::AsrQtClient* asrClient, const QString& audioFile = "") {
    std::cout << "\n🎵 STEP3: 发送音频数据" << std::endl;
    std::cout << "=====================" << std::endl;
    
    if (!g_fullRequestSent) {
        std::cout << "❌ Full Client Request未发送，跳过STEP3" << std::endl;
        return;
    }
    
    if (!audioFile.isEmpty()) {
        // 发送音频文件
        if (QFile::exists(audioFile)) {
            std::cout << "📁 发送音频文件: " << audioFile.toStdString() << std::endl;
            
            QFile file(audioFile);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray audioData = file.readAll();
                file.close();
                
                if (asrClient->sendAudio(audioData, true)) {
                    std::cout << "✅ 音频文件发送成功" << std::endl;
                    g_audioSent = true;
                    g_step = 3;
                } else {
                    std::cout << "❌ 音频文件发送失败" << std::endl;
                }
            } else {
                std::cout << "❌ 无法打开音频文件" << std::endl;
            }
        } else {
            std::cout << "❌ 音频文件不存在: " << audioFile.toStdString() << std::endl;
        }
    } else {
        // 发送测试音频数据
        std::cout << "🎵 发送测试音频数据..." << std::endl;
        
        // 创建简单的测试音频数据（静音）
        QByteArray testAudio(1024, 0); // 1KB的静音数据
        
        if (asrClient->sendAudio(testAudio, true)) {
            std::cout << "✅ 测试音频数据发送成功" << std::endl;
            g_audioSent = true;
            g_step = 3;
        } else {
            std::cout << "❌ 测试音频数据发送失败" << std::endl;
        }
    }
    
    // 等待处理结果
    std::cout << "⏳ 等待音频处理结果..." << std::endl;
    QThread::msleep(3000);
    
    if (g_serverResponded) {
        std::cout << "✅ STEP3 成功: 音频数据已处理" << std::endl;
    } else {
        std::cout << "⚠️  STEP3 警告: 未收到音频处理结果" << std::endl;
    }
}

// STEP4: 处理服务器响应
void step4_handle_server_response(Asr::AsrQtClient* asrClient) {
    (void)asrClient; // 标记参数为已使用
    std::cout << "\n📨 STEP4: 处理服务器响应" << std::endl;
    std::cout << "=========================" << std::endl;
    
    if (!g_audioSent) {
        std::cout << "❌ 音频数据未发送，跳过STEP4" << std::endl;
        return;
    }
    
    std::cout << "⏳ 等待服务器响应..." << std::endl;
    
    // 等待更多响应
    int waitCount = 0;
    while (waitCount < 50) { // 等待5秒
        QThread::msleep(100);
        waitCount++;
        
        if (g_serverResponded) {
            std::cout << "✅ 收到服务器响应" << std::endl;
            break;
        }
    }
    
    if (g_serverResponded) {
        std::cout << "✅ STEP4 成功: 服务器响应处理完成" << std::endl;
        g_step = 4;
    } else {
        std::cout << "⚠️  STEP4 警告: 未收到服务器响应" << std::endl;
    }
}

// STEP5: 断开连接和清理
void step5_disconnect_and_cleanup(Asr::AsrQtClient* asrClient) {
    std::cout << "\n🔌 STEP5: 断开连接和清理" << std::endl;
    std::cout << "=========================" << std::endl;
    
    std::cout << "🔌 断开WebSocket连接..." << std::endl;
    asrClient->disconnect();
    
    // 等待连接关闭
    QThread::msleep(1000);
    
    std::cout << "🧹 清理资源..." << std::endl;
    
    // 重置全局状态
    g_connectionEstablished = false;
    g_fullRequestSent = false;
    g_audioSent = false;
    g_serverResponded = false;
    g_step = 0;
    
    std::cout << "✅ STEP5 完成: 连接已断开，资源已清理" << std::endl;
}

// 主函数
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    std::cout << "ASR模块使用示例 - STEP调试版本" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // 检测SSL支持
    if (!check_ssl_support()) {
        std::cout << "程序无法继续，SSL支持缺失" << std::endl;
        return 1;
    }
    
    // STEP0: 检查网络
    if (!step0_check_network()) {
        std::cout << "请检查本机网络或代理设置，确保能访问 https://openspeech.bytedance.com/api/v2/asr" << std::endl;
        return 1;
    }
    
    // 检查命令行参数
    QString audioFile = "";
    if (argc > 1) {
        audioFile = argv[1];
        if (!audioFile.isEmpty() && !QFile::exists(audioFile)) {
            std::cout << "⚠️  音频文件不存在: " << audioFile.toStdString() << std::endl;
            std::cout << "   将使用测试音频数据" << std::endl;
            audioFile = "";
        }
    }
        
    // 创建ASR客户端
    auto asrClient = std::make_unique<Asr::AsrQtClient>();
    
    // 创建回调对象
    auto callback = std::make_unique<StepByStepQtCallback>();
    asrClient->setCallback(callback.get());
    
    // 执行STEP流程
    try {
        step1_connect_and_auth(asrClient.get());
        
        if (g_connectionEstablished) {
            step2_wait_full_client_response(asrClient.get());
            
            if (g_fullRequestSent) {
                step3_send_audio_data(asrClient.get(), audioFile);
                
                if (g_audioSent) {
                    step4_handle_server_response(asrClient.get());
                }
            }
        }
        
        step5_disconnect_and_cleanup(asrClient.get());
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 执行过程中发生异常: " << e.what() << std::endl;
        step5_disconnect_and_cleanup(asrClient.get());
    }
    
    std::cout << "\n🎉 所有STEP执行完成" << std::endl;
    std::cout << "最终状态:" << std::endl;
    std::cout << "  - 连接状态: " << (g_connectionEstablished ? "✅ 已连接" : "❌ 未连接") << std::endl;
    std::cout << "  - Full Request: " << (g_fullRequestSent ? "✅ 已发送" : "❌ 未发送") << std::endl;
    std::cout << "  - 音频数据: " << (g_audioSent ? "✅ 已发送" : "❌ 未发送") << std::endl;
    std::cout << "  - 服务器响应: " << (g_serverResponded ? "✅ 已收到" : "❌ 未收到") << std::endl;
    std::cout << "  - 当前STEP: " << g_step << std::endl;
    
    return 0;
} 