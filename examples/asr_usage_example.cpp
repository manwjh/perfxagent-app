/**
 * ASR模块使用示例 - STEP调试版本 (纯Qt版本)
 * 
 * 本示例展示了如何使用PerfXAgent项目中的ASR模块进行语音识别
 * 按照STEP逻辑组织，便于调试和理解每个阶段
 * 使用Qt的QWebSocket实现，与火山ASR服务进行WebSocket通信
 * 
 * 作者: PerfXAgent Team
 * 版本: 1.0.1
 * 日期: 2024
 */

// ============================================================================
// 配置参数
// ============================================================================

// 连接配置
#define ASR_WS_URL "wss://openspeech.bytedance.com/api/v2/asr"
#define CONNECTION_TIMEOUT_MS 10000
#define NETWORK_TEST_TIMEOUT_MS 5000
#define RESPONSE_WAIT_TIMEOUT_MS 5000

// 默认凭据（仅作为备用，建议使用环境变量）
#define DEFAULT_ASR_APP_ID "8388344882"
#define DEFAULT_ASR_ACCESS_TOKEN "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3"
#define DEFAULT_ASR_SECRET_KEY "oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e"

// ============================================================================
// 头文件包含
// ============================================================================

#include "asr/asr_qt_client.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QThread>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslSocket>
#include <QWebSocket>
#include <iostream>

// ============================================================================
// 全局状态变量
// ============================================================================

namespace {
    // 连接状态标志
    bool g_connectionEstablished = false;
    bool g_fullRequestSent = false;
    bool g_audioSent = false;
    bool g_serverResponded = false;
    int g_currentStep = 0;
    
    // 统计信息
    int g_totalSteps = 5;
    int g_successfulSteps = 0;
}

// ============================================================================
// 回调类 - 处理ASR客户端事件
// ============================================================================

class StepByStepQtCallback : public Asr::AsrQtCallback {
public:
    void onOpen(Asr::AsrQtClient* asr_client) override {
        (void)asr_client;
        std::cout << "\n✅ STEP1 完成: WebSocket连接已建立" << std::endl;
        std::cout << "   - WebSocket握手完成" << std::endl;
        std::cout << "   - 连接状态: 已连接" << std::endl;
        std::cout << "   - Full Client Request已自动发送" << std::endl;
        g_connectionEstablished = true;
        g_currentStep = 1;
        g_successfulSteps++;
    }
    
    void onMessage(Asr::AsrQtClient* asr_client, const QString& msg) override {
        (void)asr_client;
        std::cout << "\n📨 收到服务器消息: " << msg.toStdString() << std::endl;
        
        // 解析并显示JSON消息
        parseAndDisplayJsonMessage(msg);
        g_serverResponded = true;
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

private:
    void parseAndDisplayJsonMessage(const QString& message) {
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        if (!doc.isObject()) {
            std::cout << "⚠️  消息不是有效的JSON格式" << std::endl;
            return;
        }
        
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
        
        // 检查关键字段
        checkKeyFields(obj);
    }
    
    void checkKeyFields(const QJsonObject& obj) {
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
    }
};

// ============================================================================
// 工具函数
// ============================================================================

/**
 * 检测SSL支持
 * @return 是否支持SSL
 */
bool checkSslSupport() {
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

/**
 * 重置全局状态
 */
void resetGlobalState() {
    g_connectionEstablished = false;
    g_fullRequestSent = false;
    g_audioSent = false;
    g_serverResponded = false;
    g_currentStep = 0;
    g_successfulSteps = 0;
}

/**
 * 打印步骤标题
 * @param step 步骤编号
 * @param title 步骤标题
 */
void printStepHeader(int step, const std::string& title) {
    std::cout << "\n";
    if (step == 1) std::cout << "🚀";
    else if (step == 2) std::cout << "📤";
    else if (step == 3) std::cout << "🎵";
    else if (step == 4) std::cout << "📨";
    else if (step == 5) std::cout << "🔌";
    else std::cout << "📋";
    
    std::cout << " STEP" << step << ": " << title << std::endl;
    std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// ============================================================================
// STEP函数实现
// ============================================================================

/**
 * STEP0: 检查网络连接
 * @return 网络是否可达
 */
bool step0_check_network() {
    printStepHeader(0, "检查网络连接");
    
    QWebSocket testSocket;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(NETWORK_TEST_TIMEOUT_MS);
    
    bool connected = false;
    bool error = false;
    QString errorMsg;
    
    // 连接信号
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
    
    std::cout << "🔗 测试WebSocket连接到: " << ASR_WS_URL << std::endl;
    testSocket.open(QUrl(ASR_WS_URL));
    
    loop.exec();
    
    if (connected) {
        std::cout << "✅ 网络可达: WebSocket连接成功" << std::endl;
        testSocket.close();
        g_successfulSteps++;
        return true;
    } else {
        std::cout << "❌ 网络不可达: " << errorMsg.toStdString() << std::endl;
        return false;
    }
}

/**
 * STEP1: 连接和鉴权
 * @param asrClient ASR客户端实例
 */
void step1_connect_and_auth(Asr::AsrQtClient* asrClient) {
    printStepHeader(1, "连接和鉴权");
    
    // 安全获取凭据
    Asr::AsrQtClient::Credentials creds = Asr::AsrQtClient::getCredentialsFromEnv();
    if (!creds.isValid) {
        std::cerr << "❌ 凭据无效，无法继续" << std::endl;
        return;
    }
    
    // 配置ASR客户端
    asrClient->setAppId(creds.appId);
    asrClient->setToken(creds.accessToken);
    asrClient->setSecretKey(creds.secretKey);
    asrClient->setAuthType(Asr::AsrQtClient::TOKEN);
    asrClient->setAudioFormat("wav", 1, 16000, 16);
        
    // 连接到ASR服务器
    if (!asrClient->connect()) {
        std::cerr << "❌ 连接失败" << std::endl;
        return;
    }
        
    std::cout << "⏳ 等待连接建立..." << std::endl;
    
    // 使用非阻塞方式等待连接建立
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(CONNECTION_TIMEOUT_MS);
    
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (!g_connectionEstablished) {
            std::cout << "⏰ 连接超时" << std::endl;
            loop.quit();
        }
    });
    
    int waitCount = 0;
    while (!g_connectionEstablished && timer.isActive()) {
        loop.processEvents(); // 处理Qt事件，让WebSocket信号能够被处理
        QThread::msleep(100);
        waitCount++;
        
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

/**
 * STEP2: 等待Full Client Request响应
 * @param asrClient ASR客户端实例
 */
void step2_wait_full_client_response(Asr::AsrQtClient* asrClient) {
    printStepHeader(2, "等待Full Client Request响应");
    
    if (!g_connectionEstablished) {
        std::cout << "❌ 连接未建立，跳过STEP2" << std::endl;
        return;
    }
    
    std::cout << "⏳ 等待Full Client Request响应..." << std::endl;
    std::cout << "   - 连接建立后已自动发送Full Client Request" << std::endl;
    std::cout << "   - 等待服务器确认响应" << std::endl;
    
    // 等待服务器响应
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(RESPONSE_WAIT_TIMEOUT_MS);
    
    int waitCount = 0;
    while (!g_serverResponded && timer.isActive()) {
        loop.processEvents();
        QThread::msleep(100);
        waitCount++;
    }
    
    if (g_serverResponded) {
        std::cout << "✅ STEP2 成功: 收到Full Client Request响应" << std::endl;
        g_fullRequestSent = true;
        g_currentStep = 2;
        g_successfulSteps++;
    } else {
        std::cout << "⚠️  STEP2 警告: 未收到Full Client Request响应" << std::endl;
        std::cout << "   尝试手动发送Full Client Request..." << std::endl;
        
        // 尝试手动发送
        if (asrClient->sendAudio(QByteArray(), false)) {
            std::cout << "✅ 手动发送Full Client Request成功" << std::endl;
            g_fullRequestSent = true;
            g_currentStep = 2;
            
            // 再次等待响应
            waitCount = 0;
            while (!g_serverResponded && waitCount < 30) {
                loop.processEvents();
                QThread::msleep(100);
                waitCount++;
            }
            
            if (g_serverResponded) {
                std::cout << "✅ 收到手动发送的Full Client Request响应" << std::endl;
                g_successfulSteps++;
            } else {
                std::cout << "⚠️  仍未收到响应" << std::endl;
            }
        } else {
            std::cout << "❌ 手动发送Full Client Request失败" << std::endl;
        }
    }
}

/**
 * STEP3: 发送音频数据
 * @param asrClient ASR客户端实例
 * @param audioFile 音频文件路径（可选）
 */
void step3_send_audio_data(Asr::AsrQtClient* asrClient, const QString& audioFile = "") {
    printStepHeader(3, "发送音频数据");
    
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
                    g_currentStep = 3;
                    g_successfulSteps++;
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
            g_currentStep = 3;
            g_successfulSteps++;
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

/**
 * STEP4: 处理服务器响应
 * @param asrClient ASR客户端实例
 */
void step4_handle_server_response(Asr::AsrQtClient* asrClient) {
    (void)asrClient; // 标记参数为已使用
    printStepHeader(4, "处理服务器响应");
    
    if (!g_audioSent) {
        std::cout << "❌ 音频数据未发送，跳过STEP4" << std::endl;
        return;
    }
    
    std::cout << "⏳ 等待服务器响应..." << std::endl;
    
    // 等待更多响应
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(RESPONSE_WAIT_TIMEOUT_MS);
    
    while (timer.isActive()) {
        loop.processEvents();
        QThread::msleep(100);
        
        if (g_serverResponded) {
            std::cout << "✅ 收到服务器响应" << std::endl;
            break;
        }
    }
    
    if (g_serverResponded) {
        std::cout << "✅ STEP4 成功: 服务器响应处理完成" << std::endl;
        g_currentStep = 4;
        g_successfulSteps++;
    } else {
        std::cout << "⚠️  STEP4 警告: 未收到服务器响应" << std::endl;
    }
}

/**
 * STEP5: 断开连接和清理
 * @param asrClient ASR客户端实例
 */
void step5_disconnect_and_cleanup(Asr::AsrQtClient* asrClient) {
    printStepHeader(5, "断开连接和清理");
    
    std::cout << "🔌 断开WebSocket连接..." << std::endl;
    asrClient->disconnect();
    
    // 等待连接关闭
    QThread::msleep(1000);
    
    std::cout << "🧹 清理资源..." << std::endl;
    
    // 重置全局状态
    resetGlobalState();
    
    std::cout << "✅ STEP5 完成: 连接已断开，资源已清理" << std::endl;
    g_successfulSteps++;
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    std::cout << "ASR模块使用示例 - STEP调试版本" << std::endl;
    std::cout << "===============================" << std::endl;
    std::cout << std::endl;
    
    // 重置全局状态
    resetGlobalState();
    
    // 检查SSL支持
    if (!checkSslSupport()) {
        std::cerr << "❌ SSL支持检查失败，程序退出" << std::endl;
        return -1;
    }
    
    // 检查网络连接
    if (!step0_check_network()) {
        std::cerr << "❌ 网络连接检查失败，程序退出" << std::endl;
        return -1;
    }
    
    // 创建ASR客户端
    Asr::AsrQtClient asrClient;
    StepByStepQtCallback callback;
    asrClient.setCallback(&callback);
    
    // 执行各个步骤
    step1_connect_and_auth(&asrClient);
    step2_wait_full_client_response(&asrClient);
    step3_send_audio_data(&asrClient);
    step4_handle_server_response(&asrClient);
    step5_disconnect_and_cleanup(&asrClient);
    
    // 输出最终结果
    std::cout << "\n🎉 所有STEP执行完成" << std::endl;
    std::cout << "最终状态:" << std::endl;
    std::cout << "  - 连接状态: " << (g_connectionEstablished ? "✅ 已连接" : "❌ 未连接") << std::endl;
    std::cout << "  - Full Request: " << (g_fullRequestSent ? "✅ 已发送" : "❌ 未发送") << std::endl;
    std::cout << "  - 音频数据: " << (g_audioSent ? "✅ 已发送" : "❌ 未发送") << std::endl;
    std::cout << "  - 服务器响应: " << (g_serverResponded ? "✅ 已收到" : "❌ 未收到") << std::endl;
    std::cout << "  - 当前STEP: " << g_currentStep << std::endl;
    std::cout << "  - 成功步骤: " << g_successfulSteps << "/" << g_totalSteps << std::endl;
    
    return 0;
} 