// This file is being reverted to its original state.
// The content below is a placeholder representing the original file content.
// This action is to undo all previous, incorrect edits.
#include "asr/asr_manager.h"
#include "asr/asr_log_utils.h"
#include "asr/asr_client.h"
#include "ui/config_manager.h"  // 添加SecureKeyManager的头文件
#include "asr/secure_key_manager.h"
#include <iostream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <filesystem>
#include <mutex>

using json = nlohmann::json;

namespace Asr {

// ============================================================================
// 日志工具函数
// ============================================================================

void logMessage(AsrLogLevel currentLevel, AsrLogLevel messageLevel, 
                const std::string& message, bool isError = false) {
    if (currentLevel >= messageLevel) {
        std::string timestamp = getCurrentTimestamp();
        std::string prefix = isError ? "❌" : "ℹ️";
        std::cout << "[" << timestamp << "] " << prefix << " " << message << std::endl;
    }
}

// ============================================================================
// 音频文件解析相关结构体和类
// ============================================================================

// WAV文件头部结构体
struct WavHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // 文件大小 - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmtSize;       // fmt块大小
    uint16_t audioFormat;   // 音频格式 (1 = PCM)
    uint16_t numChannels;   // 声道数
    uint32_t sampleRate;    // 采样率
    uint32_t byteRate;      // 字节率
    uint16_t blockAlign;    // 块对齐
    uint16_t bitsPerSample; // 位深度
};

// 音频分段器
class AudioSegmenter {
private:
    std::vector<uint8_t> m_audioData;
    size_t m_chunkSize;
    size_t m_offset;
    
public:
    AudioSegmenter(const std::vector<uint8_t>& audioData, size_t chunkSize) 
        : m_audioData(audioData), m_chunkSize(chunkSize), m_offset(0) {}
    
    bool getNextChunk(std::vector<uint8_t>& chunk, bool& isLast) {
        if (m_offset >= m_audioData.size()) {
            return false;
        }
        
        size_t remaining = m_audioData.size() - m_offset;
        size_t currentChunkSize = std::min(m_chunkSize, remaining);
        
        chunk.assign(m_audioData.begin() + m_offset, 
                    m_audioData.begin() + m_offset + currentChunkSize);
        
        m_offset += currentChunkSize;
        isLast = (m_offset >= m_audioData.size());
        
        return true;
    }
    
    void reset() {
        m_offset = 0;
    }
};

// ============================================================================
// AsrManager 类实现
// ============================================================================

// ============================================================================
// 单例模式实现
// ============================================================================

static std::unique_ptr<AsrManager> s_instance = nullptr;
static std::mutex s_instanceMutex;

AsrManager& AsrManager::instance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::make_unique<AsrManager>();
    }
    return *s_instance;
}

void AsrManager::destroyInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    s_instance.reset();
}

// ============================================================================
// 构造函数和析构函数
// ============================================================================

AsrManager::AsrManager()
    : m_status(AsrStatus::DISCONNECTED),
      m_callback(nullptr),
      m_audioSendIndex(0)
{
    m_lastPacketTime = std::chrono::high_resolution_clock::now();
    
    // 从环境变量加载配置
    loadConfigFromEnv(m_config);
    
    // 输出初始日志配置（仅在INFO级别以上）
    if (m_config.logLevel >= ASR_LOG_INFO) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🔧 ASR日志配置:");
        logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 日志级别: " + std::to_string(m_config.logLevel));
        logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 业务日志: " + std::string(m_config.enableBusinessLog ? "启用" : "禁用"));
        logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 流程日志: " + std::string(m_config.enableFlowLog ? "启用" : "禁用"));
        logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 数据日志: " + std::string(m_config.enableDataLog ? "启用" : "禁用"));
        logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 协议日志: " + std::string(m_config.enableProtocolLog ? "启用" : "禁用"));
        logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 音频日志: " + std::string(m_config.enableAudioLog ? "启用" : "禁用"));
    }
}

AsrManager::~AsrManager() {
    stopRecognition();
    disconnect();
}

// ============================================================================
// 配置方法
// ============================================================================

void AsrManager::setConfig(const AsrConfig& config) {
    m_config = config;
}

AsrConfig AsrManager::getConfig() const {
    return m_config;
}

void AsrManager::setClientType(ClientType type) {
    m_config.clientType = type;
}

void AsrManager::setCallback(AsrCallback* callback) {
    m_callback = callback;
}

std::string AsrManager::getDetailedStatus() const {
    std::stringstream ss;
    ss << "=== ASR 详细状态信息 ===" << std::endl;
    ss << "连接状态: " << getStatusName(m_status) << std::endl;
    ss << "客户端类型: " << getClientTypeName(m_config.clientType) << std::endl;
    ss << "是否已连接: " << (isConnected() ? "是" : "否") << std::endl;
    ss << "音频包数量: " << m_audioPackets.size() << std::endl;
    ss << "已发送包数: " << m_audioSendIndex << std::endl;
    ss << "剩余包数: " << (m_audioPackets.size() - m_audioSendIndex) << std::endl;
    
    if (m_client) {
        ss << "客户端连接状态: " << (m_client->isConnected() ? "已连接" : "未连接") << std::endl;
    }
    
    return ss.str();
}

std::string AsrManager::getAudioStats() const {
    std::stringstream ss;
    ss << "=== 音频处理统计 ===" << std::endl;
    ss << "音频格式: " << m_config.format << std::endl;
    ss << "采样率: " << m_config.sampleRate << " Hz" << std::endl;
    ss << "位深度: " << m_config.bits << " bits" << std::endl;
    ss << "声道数: " << m_config.channels << std::endl;
    ss << "分段时长: " << m_config.segDuration << " ms" << std::endl;
    ss << "总音频包数: " << m_audioPackets.size() << std::endl;
    ss << "已发送包数: " << m_audioSendIndex << std::endl;
    ss << "识别结果数: " << m_results.size() << std::endl;
    
    return ss.str();
}

// ============================================================================
// 连接控制方法
// ============================================================================

bool AsrManager::connect() {
    try {
        // 如果客户端已存在且已连接，直接返回
        if (m_client && m_client->isConnected()) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "ℹ️ ASR 客户端已经连接");
            return true;
        }
        
        updateStatus(AsrStatus::CONNECTING);
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🔗 正在连接 ASR 服务器...");
        
        // 创建并初始化客户端
        if (!initializeClient()) {
            updateStatus(AsrStatus::ERROR);
            return false;
        }
        
        // 连接客户端，并等待连接成功
        if (!m_client->connect()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR 客户端连接失败", true);
            updateStatus(AsrStatus::ERROR);
            return false;
        }
        
        // 等待连接稳定
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 再次检查连接状态
        if (!m_client || !m_client->isConnected()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR 连接不稳定", true);
            updateStatus(AsrStatus::ERROR);
            return false;
        }
        
        // 连接成功后更新状态
        updateStatus(AsrStatus::CONNECTED);
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ ASR 连接成功");
        
        // 启动会话计时器
        startSessionTimer();
        
        return true;
    } catch (const std::exception& e) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR 连接异常: " + std::string(e.what()), true);
        updateStatus(AsrStatus::ERROR);
        return false;
    } catch (...) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR 连接发生未知异常", true);
        updateStatus(AsrStatus::ERROR);
        return false;
    }
}

void AsrManager::disconnect() {
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔌 开始断开 ASR 连接...");
    
    // 结束会话计时器
    endSessionTimer(true);
    
    if (m_client) {
        m_client->disconnect();
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ ASR 客户端已断开");
    } else {
        logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ ASR 客户端不存在，无需断开");
    }
    updateStatus(AsrStatus::DISCONNECTED);
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔌 ASR 连接已断开");
}

bool AsrManager::isConnected() const {
    return (m_status == AsrStatus::CONNECTED || m_status == AsrStatus::RECOGNIZING) && m_client && m_client->isConnected();
}

AsrStatus AsrManager::getStatus() const {
    return m_status;
}

// ============================================================================
// 音频识别方法
// ============================================================================

bool AsrManager::sendAudio(const std::vector<uint8_t>& audioData, bool isLast) {
    if (!isConnected()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR 未连接，无法发送音频", true);
        return false;
    }
    
    updateStatus(AsrStatus::RECOGNIZING);
    
    if (!m_client->sendAudio(audioData, isLast)) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 发送音频数据失败", true);
        return false;
    }
    
    // 业务层日志
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📤 音频数据发送成功 (" + std::to_string(audioData.size()) + " bytes)");
    }
    
    if (isLast) {
        updateStatus(AsrStatus::CONNECTED);
    }
    
    return true;
}

bool AsrManager::sendAudioFile(const std::string& filePath) {
    if (!isConnected()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR 未连接，无法发送音频文件", true);
        return false;
    }
    
    updateStatus(AsrStatus::RECOGNIZING);
    
    if (!m_client->sendAudioFile(filePath)) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 发送音频文件失败: " + filePath, true);
        return false;
    }
    
    // 业务层日志
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "📤 音频文件发送成功: " + filePath);
    }
    updateStatus(AsrStatus::CONNECTED);
    
    return true;
}

bool AsrManager::startRecognition() {
    // 确保客户端已初始化并连接
    if (!initializeClient() || !connect()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法连接到 ASR 服务器，无法开始识别", true);
        return false;
    }
    
    // 发送完整客户端请求
    std::string response = m_client->sendFullClientRequestAndGetResponse(10000);
    
    if (!response.empty()) {
        // 检查响应是否包含错误
        try {
            json j = json::parse(response);
            if (j.contains("error") || (j.contains("code") && j["code"] != 0)) {
                logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ Full Server Response 包含错误: " + response, true);
                updateStatus(AsrStatus::ERROR);
                return false;
            }
        } catch (const std::exception& e) {
            // JSON解析失败，但响应不为空，可能是非JSON格式的成功响应
            if (m_config.enableBusinessLog) {
                logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 解析Full Server Response失败: " + std::string(e.what()));
            }
        }
        
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 识别会话已开始");
        updateStatus(AsrStatus::RECOGNIZING);
        return true;
    } else {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 开始识别失败（未收到服务器响应）", true);
        return false;
    }
}

void AsrManager::stopRecognition() {
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🛑 请求停止ASR识别...");
    
    // 设置停止标志
    m_stopFlag = true;
    m_stopRequested = true;
    
    // 断开客户端连接
    if (m_client) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🔌 断开ASR客户端连接...");
        m_client->disconnect();
    }
    
    // 等待工作线程结束，添加超时机制
    if (m_workerThread.joinable()) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "⏳ 等待ASR工作线程结束...");
        
        // 使用超时等待，避免无限等待
        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(5); // 5秒超时
        
        while (m_workerThread.joinable()) {
            auto now = std::chrono::steady_clock::now();
            if (now - startTime > timeout) {
                logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ ASR工作线程等待超时，强制停止");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ ASR工作线程已结束");
    }
    
    // 重置状态
    m_status = AsrStatus::DISCONNECTED;
    m_stopFlag = false;
    m_stopRequested = false;
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ ASR识别已停止");
}

// ============================================================================
// 结果获取方法
// ============================================================================

AsrResult AsrManager::getLatestResult() const {
    return m_latestResult;
}

std::vector<AsrResult> AsrManager::getAllResults() const {
    return m_results;
}

AsrResult AsrManager::getFinalResult() const {
    for (const auto& result : m_results) {
        if (result.isFinal) {
            return result;
        }
    }
    // 如果没有找到最终结果，返回一个空的结果
    return {};
}

std::string AsrManager::getLogId() const {
    if (m_client) {
        return m_client->getLogId();
    }
    return "";
}

std::map<std::string, std::string> AsrManager::getResponseHeaders() const {
    if (m_client) {
        return m_client->getResponseHeaders();
    }
    return {};
}

// ============================================================================
// 静态方法
// ============================================================================

bool AsrManager::loadConfigFromEnv(AsrConfig& config) {
    // 从环境变量获取凭据（支持多种前缀）
    const char* appId = std::getenv("ASR_APP_ID");
    const char* accessToken = std::getenv("ASR_ACCESS_TOKEN");
    const char* secretKey = std::getenv("ASR_SECRET_KEY");
    
    // 如果ASR_前缀的环境变量不存在，尝试VOLC_前缀
    if (!appId) appId = std::getenv("VOLC_APP_ID");
    if (!accessToken) accessToken = std::getenv("VOLC_ACCESS_TOKEN");
    if (!secretKey) secretKey = std::getenv("VOLC_SECRET_KEY");
    
    if (appId && accessToken) {
        config.appId = appId;
        config.accessToken = accessToken;
        config.secretKey = secretKey ? secretKey : "";
        config.isValid = true;
        config.configSource = "environment_variables";
        
        std::cout << "🔐 使用环境变量中的凭据" << std::endl;
    } else {
        std::cout << "⚠️  环境变量未设置，使用体验模式配置" << std::endl;
        std::cout << "   建议设置环境变量：" << std::endl;
        std::cout << "   export ASR_APP_ID=your_app_id" << std::endl;
        std::cout << "   export ASR_ACCESS_TOKEN=your_access_token" << std::endl;
        std::cout << "   export ASR_SECRET_KEY=your_secret_key" << std::endl;
        std::cout << "   或者使用 VOLC_ 前缀：" << std::endl;
        std::cout << "   export VOLC_APP_ID=your_app_id" << std::endl;
        std::cout << "   export VOLC_ACCESS_TOKEN=your_access_token" << std::endl;
        std::cout << "   export VOLC_SECRET_KEY=your_secret_key" << std::endl;
        
        // 使用SecureKeyManager获取混淆的API密钥（体验模式）
        config.appId = perfx::ui::SecureKeyManager::getAppId();
        config.accessToken = perfx::ui::SecureKeyManager::getAccessToken();
        config.secretKey = perfx::ui::SecureKeyManager::getSecretKey();
        config.isValid = true;
        config.configSource = "trial_mode";
        
        // 检查体验模式使用限制
        std::cout << "🎯 体验模式：请确保使用次数未超过限制" << std::endl;
    }
    
    // 加载日志配置
    const char* businessLog = std::getenv("ASR_ENABLE_BUSINESS_LOG");
    const char* flowLog = std::getenv("ASR_ENABLE_FLOW_LOG");
    const char* dataLog = std::getenv("ASR_ENABLE_DATA_LOG");
    const char* protocolLog = std::getenv("ASR_ENABLE_PROTOCOL_LOG");
    const char* audioLog = std::getenv("ASR_ENABLE_AUDIO_LOG");
    
    if (businessLog) config.enableBusinessLog = (std::string(businessLog) == "1");
    if (flowLog) config.enableFlowLog = (std::string(flowLog) == "1");
    if (dataLog) config.enableDataLog = (std::string(dataLog) == "1");
    if (protocolLog) config.enableProtocolLog = (std::string(protocolLog) == "1");
    if (audioLog) config.enableAudioLog = (std::string(audioLog) == "1");
    
    return true;
}

std::string AsrManager::getClientTypeName(ClientType type) {
    switch (type) {
        case ClientType::IXWEBSOCKET:
            return "IXWebSocket";
        case ClientType::QT:
            return "Qt WebSocket";
        case ClientType::WEBSOCKETPP:
            return "WebSocketpp";
        default:
            return "Unknown";
    }
}

std::string AsrManager::getStatusName(AsrStatus status) {
    switch (status) {
        case AsrStatus::DISCONNECTED:
            return "Disconnected";
        case AsrStatus::CONNECTING:
            return "Connecting";
        case AsrStatus::CONNECTED:
            return "Connected";
        case AsrStatus::RECOGNIZING:
            return "Recognizing";
        case AsrStatus::ERROR:
            return "Error";
        default:
            return "Unknown";
    }
}

// ============================================================================
// 私有方法
// ============================================================================

std::unique_ptr<AsrClient> AsrManager::createClient(ClientType type) {
    switch (type) {
        case ClientType::IXWEBSOCKET:
            return std::make_unique<AsrClient>();
        case ClientType::QT:
            // TODO: 实现 Qt 客户端
            std::cerr << "❌ Qt 客户端暂未实现" << std::endl;
            return nullptr;
        case ClientType::WEBSOCKETPP:
            // TODO: 实现 WebSocketpp 客户端
            std::cerr << "❌ WebSocketpp 客户端暂未实现" << std::endl;
            return nullptr;
        default:
            std::cerr << "❌ 未知的客户端类型" << std::endl;
            return nullptr;
    }
}

bool AsrManager::initializeClient() {
    if (m_client) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "✅ 客户端已存在，跳过初始化");
        return true;
    }
    
    logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📡 正在创建客户端，类型: " + getClientTypeName(m_config.clientType));
    
    m_client = createClient(m_config.clientType);
    if (!m_client) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 创建客户端实例失败", true);
        return false;
    }
    
    // 设置客户端配置
    m_client->setAppId(m_config.appId);
    m_client->setToken(m_config.accessToken);
    m_client->setSecretKey(m_config.secretKey);
    
    // 添加调试日志，显示format的值
    logMessage(m_config.logLevel, ASR_LOG_DEBUG, "🔧 设置音频格式: " + m_config.format + 
               " (channels=" + std::to_string(m_config.channels) + 
               ", sampleRate=" + std::to_string(m_config.sampleRate) + 
               ", bits=" + std::to_string(m_config.bits) + ")");
    
    m_client->setAudioFormat(m_config.format, m_config.channels, m_config.sampleRate, m_config.bits);
    m_client->setUid(m_config.uid);
    m_client->setLanguage(m_config.language);
    m_client->setResultType(m_config.resultType);
    m_client->setStreaming(m_config.streaming);
    m_client->setSegDuration(m_config.segDuration);
    
    // 将 AsrManager 自身设置为回调处理者
    m_client->setCallback(this);
    
    logMessage(m_config.logLevel, ASR_LOG_DEBUG, "✅ 客户端配置完成");
    return true;
}

void AsrManager::updateStatus(AsrStatus status) {
    m_status = status;
    // 流程日志
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📊 ASR 状态更新: " + getStatusName(status));
    }
}

// =================== ASR连接测试 ===================
bool AsrManager::testConnection(const std::string& appId, const std::string& accessToken, const std::string& secretKey) {
    // 参数验证
    if (appId.empty()) {
        logMessage(ASR_LOG_ERROR, ASR_LOG_ERROR, "❌ 测试连接失败：应用ID为空", true);
        return false;
    }
    
    if (accessToken.empty()) {
        logMessage(ASR_LOG_ERROR, ASR_LOG_ERROR, "❌ 测试连接失败：访问令牌为空", true);
        return false;
    }
    
    if (secretKey.empty()) {
        logMessage(ASR_LOG_ERROR, ASR_LOG_ERROR, "❌ 测试连接失败：密钥为空", true);
        return false;
    }
    
    logMessage(ASR_LOG_INFO, ASR_LOG_INFO, "🔍 开始ASR连接测试...");
    logMessage(ASR_LOG_INFO, ASR_LOG_INFO, "📋 测试参数：");
    logMessage(ASR_LOG_INFO, ASR_LOG_INFO, "  - App ID: " + (appId.length() > 8 ? appId.substr(0, 4) + "****" + appId.substr(appId.length() - 4) : appId));
    logMessage(ASR_LOG_INFO, ASR_LOG_INFO, "  - Access Token: " + (accessToken.length() > 8 ? accessToken.substr(0, 4) + "****" + accessToken.substr(accessToken.length() - 4) : accessToken));
    logMessage(ASR_LOG_INFO, ASR_LOG_INFO, "  - Secret Key: " + (secretKey.length() > 8 ? secretKey.substr(0, 4) + "****" + secretKey.substr(secretKey.length() - 4) : secretKey));
    
    try {
        std::unique_ptr<AsrClient> client = std::make_unique<AsrClient>();
        client->setAppId(appId);
        client->setToken(accessToken);
        client->setSecretKey(secretKey);
        
        logMessage(ASR_LOG_INFO, ASR_LOG_INFO, "🔗 尝试连接ASR服务...");
        
        // 调用testHandshake
        bool result = client->testHandshake();
        
        if (result) {
            logMessage(ASR_LOG_INFO, ASR_LOG_INFO, "✅ ASR连接测试成功！");
        } else {
            logMessage(ASR_LOG_ERROR, ASR_LOG_ERROR, "❌ ASR连接测试失败：握手失败", true);
        }
        
        return result;
    } catch (const std::exception& e) {
        logMessage(ASR_LOG_ERROR, ASR_LOG_ERROR, "❌ ASR连接测试异常：" + std::string(e.what()), true);
        return false;
    } catch (...) {
        logMessage(ASR_LOG_ERROR, ASR_LOG_ERROR, "❌ ASR连接测试发生未知异常", true);
        return false;
    }
}

// ============================================================================
// ASR线程状态判断和启动/关闭接口实现
// ============================================================================

bool AsrManager::isAsrThreadRunning() const {
    return isConnected() && (m_status == AsrStatus::CONNECTED || m_status == AsrStatus::RECOGNIZING);
}

void AsrManager::startAsrThread() {
    if (!isAsrThreadRunning()) {
        connect();
        startRecognition();
    }
}

void AsrManager::stopAsrThread() {
    if (isAsrThreadRunning()) {
        stopRecognition();
        disconnect();
    }
}

// ============================================================================
// 音频文件解析和识别方法
// ============================================================================

AudioFileInfo AsrManager::parseAudioFile(const std::string& filePath) {
    AudioFileInfo info;
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开音频文件: " << filePath << std::endl;
        return info;
    }
    
    // 读取文件头
    std::vector<uint8_t> header(64);
    file.read(reinterpret_cast<char*>(header.data()), header.size());
    file.close();
    
    if (header.size() < 12) {
        std::cerr << "❌ 文件太小，无法读取头部" << std::endl;
        return info;
    }
    
    // 检测文件格式
    std::string magic(reinterpret_cast<char*>(header.data()), 4);
    
    if (magic == "RIFF" && std::string(reinterpret_cast<char*>(header.data() + 8), 4) == "WAVE") {
        return parseWavFile(filePath, header);
    } else if (magic.substr(0, 3) == "ID3" || magic.substr(0, 2) == "\xff\xfb") {
        return parseMp3File(filePath, header);
    } else {
        // 假设是PCM文件
        return parsePcmFile(filePath, header);
    }
}

bool AsrManager::recognizeAudioFile(const std::string& filePath, bool waitForFinal, int timeoutMs) {
    // 流程日志
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 火山引擎 ASR 自动化识别流程 ===");
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🎯 目标文件: " + filePath);
    }
    
    // 检查音频文件是否存在
    std::ifstream testFile(filePath);
    if (!testFile.good()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频文件不存在: " + filePath, true);
        return false;
    }
    testFile.close();
    
    // 步骤1: 自动解析音频文件头
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤1: 音频文件头解析 ===");
    }
    AudioFileInfo audioInfo = parseAudioFile(filePath);
    
    if (!audioInfo.isValid) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频文件解析失败", true);
        return false;
    }
    
    // 步骤2: 自动配置ASR参数
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "=== 步骤2: 自动配置ASR参数 ===");
    }
    m_config.sampleRate = audioInfo.sampleRate;
    m_config.channels = audioInfo.channels;
    m_config.format = "wav";  // 修正：ASR服务器期望WAV格式
    m_config.segDuration = 100; // 强制100ms分包
    
    // 步骤3: 连接ASR服务
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤3: 连接ASR服务 ===");
    }
    if (!connect()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 连接ASR服务失败", true);
        return false;
    }
    
    // 步骤4: 读取音频数据并分包
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤4: 读取音频数据并分包 ===");
    }
    
    std::vector<uint8_t> audioData;
    
    if (m_config.format == "wav") {
        // 对于WAV文件，发送完整文件（包括头部）
        std::ifstream audioFile(filePath, std::ios::binary);
        if (!audioFile.is_open()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法打开音频文件", true);
            return false;
        }
        
        // 读取完整WAV文件（包括头部）
        audioFile.seekg(0, std::ios::end);
        size_t fileSize = audioFile.tellg();
        audioFile.seekg(0, std::ios::beg);
        
        audioData.resize(fileSize);
        audioFile.read(reinterpret_cast<char*>(audioData.data()), fileSize);
        audioFile.close();
        
        if (m_config.enableBusinessLog) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 成功读取完整WAV文件: " + std::to_string(audioData.size()) + " bytes (包括头部)");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "🎵 音频信息:");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 格式: " + audioInfo.format);
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 采样率: " + std::to_string(audioInfo.sampleRate) + " Hz");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 位深度: " + std::to_string(audioInfo.bitsPerSample) + " bits");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 声道数: " + std::to_string(audioInfo.channels));
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 编解码器: " + audioInfo.codec);
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 文件大小: " + std::to_string(fileSize) + " bytes");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 音频时长: " + std::to_string(audioInfo.duration) + " 秒");
        }
    } else {
        // 对于其他格式，只读取音频数据部分（不包括文件头）
        std::ifstream audioFile(filePath, std::ios::binary);
        if (!audioFile.is_open()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法打开音频文件", true);
            return false;
        }
        
        // 跳过文件头，直接读取音频数据
        audioFile.seekg(audioInfo.dataOffset);
        audioData.resize(audioInfo.dataSize);
        audioFile.read(reinterpret_cast<char*>(audioData.data()), audioInfo.dataSize);
        audioFile.close();
        
        if (m_config.enableBusinessLog) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 成功读取音频数据: " + std::to_string(audioData.size()) + " bytes (仅数据部分)");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "🎵 音频信息:");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 格式: " + audioInfo.format);
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 采样率: " + std::to_string(audioInfo.sampleRate) + " Hz");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 位深度: " + std::to_string(audioInfo.bitsPerSample) + " bits");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 声道数: " + std::to_string(audioInfo.channels));
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 编解码器: " + audioInfo.codec);
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 音频数据大小: " + std::to_string(audioInfo.dataSize) + " bytes");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 音频时长: " + std::to_string(audioInfo.duration) + " 秒");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 数据偏移: " + std::to_string(audioInfo.dataOffset) + " bytes");
        }
    }
    
    if (audioData.empty()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频数据为空", true);
        return false;
    }
    
    // 声道转换：如果音频是双声道，转换为单声道
    if (audioInfo.channels > 1) {
        if (m_config.format == "wav") {
            // 对于WAV格式，不进行声道转换，保持原始格式
            logMessage(m_config.logLevel, ASR_LOG_INFO, "🔄 检测到多声道WAV音频，保持原始格式");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 声道数: " + std::to_string(audioInfo.channels));
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 注意：WAV格式保持原始声道数，不进行转换");
        } else {
            // 对于其他格式，进行声道转换
            logMessage(m_config.logLevel, ASR_LOG_INFO, "🔄 检测到多声道音频，正在转换为单声道...");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 原始声道数: " + std::to_string(audioInfo.channels));
            
            // 计算转换后的数据大小
            size_t originalSamples = audioData.size() / (audioInfo.bitsPerSample / 8);
            size_t samplesPerChannel = originalSamples / audioInfo.channels;
            size_t convertedDataSize = samplesPerChannel * (audioInfo.bitsPerSample / 8);
            
            std::vector<uint8_t> convertedAudioData(convertedDataSize);
            
            // 根据位深度进行转换
            if (audioInfo.bitsPerSample == 16) {
                int16_t* originalSamples = reinterpret_cast<int16_t*>(audioData.data());
                int16_t* convertedSamples = reinterpret_cast<int16_t*>(convertedAudioData.data());
                
                for (size_t i = 0; i < samplesPerChannel; ++i) {
                    int32_t sum = 0;
                    for (int ch = 0; ch < audioInfo.channels; ++ch) {
                        sum += originalSamples[i * audioInfo.channels + ch];
                    }
                    convertedSamples[i] = static_cast<int16_t>(sum / audioInfo.channels);
                }
            } else if (audioInfo.bitsPerSample == 8) {
                uint8_t* originalSamples = reinterpret_cast<uint8_t*>(audioData.data());
                uint8_t* convertedSamples = reinterpret_cast<uint8_t*>(convertedAudioData.data());
                
                for (size_t i = 0; i < samplesPerChannel; ++i) {
                    int32_t sum = 0;
                    for (int ch = 0; ch < audioInfo.channels; ++ch) {
                        sum += originalSamples[i * audioInfo.channels + ch];
                    }
                    convertedSamples[i] = static_cast<uint8_t>(sum / audioInfo.channels);
                }
            } else {
                logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 不支持的位深度: " + std::to_string(audioInfo.bitsPerSample), true);
                return false;
            }
            
            // 更新音频数据和信息
            audioData = std::move(convertedAudioData);
            audioInfo.channels = 1;
            audioInfo.dataSize = audioData.size();
            
            // 重新计算时长
            audioInfo.duration = static_cast<double>(audioInfo.dataSize) / 
                               (audioInfo.channels * audioInfo.sampleRate * audioInfo.bitsPerSample / 8);
            
            logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 声道转换完成:");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 转换后声道数: " + std::to_string(audioInfo.channels));
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 转换后数据大小: " + std::to_string(audioData.size()) + " bytes");
            logMessage(m_config.logLevel, ASR_LOG_INFO, "  - 转换后时长: " + std::to_string(audioInfo.duration) + " 秒");
        }
    }
    
    // 更新ASR配置
    if (m_config.format == "wav") {
        // 对于WAV格式，保持原始声道数
        m_config.channels = audioInfo.channels;
    } else {
        // 对于其他格式，强制为单声道
        m_config.channels = 1;
    }
    
    // 计算分段大小 - 按照100ms帧长计算
    size_t bytesPerSecond = audioInfo.channels * (audioInfo.bitsPerSample / 8) * audioInfo.sampleRate;
    size_t segmentSize = bytesPerSecond * m_config.segDuration / 1000;
    
    if (m_config.enableDataLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📊 音频分段信息:");
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 每秒字节数: " + std::to_string(bytesPerSecond) + " bytes/s");
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 分段大小: " + std::to_string(segmentSize) + " bytes (100ms)");
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 预计分段数: " + std::to_string((audioData.size() + segmentSize - 1) / segmentSize));
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 实际音频数据大小: " + std::to_string(audioData.size()) + " bytes");
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 理论音频数据大小: " + std::to_string(audioInfo.dataSize) + " bytes");
    }
    
    // 验证音频数据大小是否匹配
    if (audioData.size() != audioInfo.dataSize) {
        logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 音频数据大小不匹配:");
        logMessage(m_config.logLevel, ASR_LOG_WARN, "  - 实际读取: " + std::to_string(audioData.size()) + " bytes");
        logMessage(m_config.logLevel, ASR_LOG_WARN, "  - 理论大小: " + std::to_string(audioInfo.dataSize) + " bytes");
    }
    
    // 创建音频分段器并分包 - 只对音频数据部分进行分包
    AudioSegmenter segmenter(audioData, segmentSize);
    m_audioPackets.clear();
    m_audioSendIndex = 0;
    std::vector<uint8_t> chunk;
    bool isLast;
    while (segmenter.getNextChunk(chunk, isLast)) {
        m_audioPackets.push_back(chunk);
    }
    
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 音频分包完成: " + std::to_string(m_audioPackets.size()) + " 个包 (仅音频数据)");
    }

    // 步骤5: 启动识别（发送Full Client Request并等待响应）
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤5: 启动识别（发送Full Client Request） ===");
    }
    if (!startRecognition()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 启动识别失败", true);
        return false;
    }
    
    // 步骤6: 等待所有音频包发送完成
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤6: 等待流式发送完成 ===");
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🚀 开始流式发送，总共 " + std::to_string(m_audioPackets.size()) + " 个音频包");
    }
    
    // 计算音频帧时长（毫秒）
    double frameDurationMs = static_cast<double>(m_config.segDuration);
    
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🎵 音频帧时长: " + std::to_string(frameDurationMs) + "ms");
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🎵 采样率: " + std::to_string(m_config.sampleRate) + "Hz");
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🎵 声道数: " + std::to_string(m_config.channels));
    }
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastPacketTime = startTime;
    
    // 等待所有包发送完成，按照音频帧时长精确控制
    while (m_audioSendIndex < m_audioPackets.size()) {
        // 检查连接状态
        if (!m_client->isConnected()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 连接已断开，终止流式发送", true);
            return false;
        }
        
        // 检查是否收到错误响应
        auto lastError = m_client->getLastError();
        if (!lastError.isSuccess()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 服务器返回错误: " + lastError.message, true);
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "🔍 错误码: " + std::to_string(lastError.code), true);
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "📝 错误详情: " + lastError.details, true);
            return false;
        }
        
        // 计算距离上次发包的时间间隔
        auto now = std::chrono::high_resolution_clock::now();
        auto timeSinceLastPacket = std::chrono::duration_cast<std::chrono::microseconds>(now - lastPacketTime);
        double elapsedMs = timeSinceLastPacket.count() / 1000.0;
        
        // 如果距离上次发包的时间小于音频帧时长，需要等待
        if (elapsedMs < frameDurationMs) {
            double waitTimeMs = frameDurationMs - elapsedMs;
            if (m_config.enableBusinessLog) {
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, 
                         "⏳ 等待音频帧间隔: " + std::to_string(waitTimeMs) + "ms");
            }
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(waitTimeMs * 1000)));
        }
        
        // 发送音频包
        if (!sendNextAudioPacket()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 发送音频包失败", true);
            return false;
        }
        
        // 更新上次发包时间
        lastPacketTime = std::chrono::high_resolution_clock::now();
        
        if (m_config.enableBusinessLog) {
            logMessage(m_config.logLevel, ASR_LOG_DEBUG, 
                     "✅ 音频包 " + std::to_string(m_audioSendIndex) + "/" + std::to_string(m_audioPackets.size()) + 
                     " 发送成功 (帧时长: " + std::to_string(frameDurationMs) + "ms)");
        }
    }
    
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 所有音频包发送完成");
    }
    
    // 等待最终识别结果（可选）
    if (waitForFinal) {
        if (m_config.enableFlowLog) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤7: 等待最终识别结果 ===");
        }
        
        // 等待最终识别结果，最多等待指定时间
        const int maxWaitTimeMs = timeoutMs > 0 ? timeoutMs : 5000; // 默认5秒
        const int checkIntervalMs = 100; // 每100ms检查一次
        int totalWaitTimeMs = 0;
        
        while (totalWaitTimeMs < maxWaitTimeMs) {
            // 优先检查是否收到了最终响应包
            if (m_client && m_client->hasReceivedFinalResponse()) {
                if (m_config.enableBusinessLog) {
                    logMessage(m_config.logLevel, ASR_LOG_INFO, "🎯 收到最终响应包，识别结束");
                }
                break;
            }
            
            // 检查连接状态
            if (!m_client->isConnected()) {
                logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 连接已断开，停止等待最终结果", true);
                break;
            }
            
            // 检查是否收到错误响应
            auto lastError = m_client->getLastError();
            if (!lastError.isSuccess()) {
                logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 服务器返回错误: " + lastError.message, true);
                break;
            }
            
            // 等待一段时间后再次检查
            std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
            totalWaitTimeMs += checkIntervalMs;
            
            if (m_config.enableBusinessLog && totalWaitTimeMs % 1000 == 0) {
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, 
                         "⏳ 等待最终结果中... (" + std::to_string(totalWaitTimeMs/1000) + "s/" + 
                         std::to_string(maxWaitTimeMs/1000) + "s)");
            }
        }
        
        if (m_client && m_client->hasReceivedFinalResponse()) {
            if (m_config.enableFlowLog) {
                logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 成功收到最终识别结果");
            }
        } else {
            if (m_config.enableFlowLog) {
                logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 未收到最终识别结果，可能识别未完成");
            }
        }
    }
    
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 识别流程结束 ===");
    }
    return true;
}

bool AsrManager::sendNextAudioPacket() {
    if (m_stopFlag) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 检测到错误，停止发包", true);
        return false;
    }
    if (m_audioPackets.empty()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频包列表为空，无法发送", true);
        return false;
    }
    if (m_audioSendIndex >= m_audioPackets.size()) {
        if (m_config.enableBusinessLog) {
            logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 没有更多音频包可发送");
        }
        return false;
    }
    bool isLast = (m_audioSendIndex == m_audioPackets.size() - 1);
    int seq = 2 + m_audioSendIndex;
    int sendSeq = isLast ? -seq : seq;
    if (!m_client || !m_client->isConnected()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 客户端未连接，无法发送音频包", true);
        return false;
    }
    if (!m_client->sendAudio(m_audioPackets[m_audioSendIndex], sendSeq)) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 发送音频包失败 seq=" + std::to_string(sendSeq), true);
        return false;
    }
    m_lastPacketTime = std::chrono::high_resolution_clock::now();
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, 
                 "📤 音频包发送成功 seq=" + std::to_string(sendSeq) + 
                 " size=" + std::to_string(m_audioPackets[m_audioSendIndex].size()) + " bytes");
    }
    m_audioSendIndex++;
    return true;
}

void AsrManager::onAudioAck() {
    // 收到服务器响应/ACK后，立即发送下一个音频包
    std::cout << "📨 收到音频包ACK，准备发送下一个包..." << std::endl;
    if (m_audioPackets.empty()) {
        std::cerr << "❌ 音频包列表为空，无法继续发送" << std::endl;
        return;
    }
    if (m_audioSendIndex < m_audioPackets.size()) {
        std::cout << "⏳ 还有 " << (m_audioPackets.size() - m_audioSendIndex) << " 个音频包待发送" << std::endl;
        if (sendNextAudioPacket()) {
            std::cout << "✅ 下一个音频包发送成功" << std::endl;
        } else {
            std::cerr << "❌ 下一个音频包发送失败" << std::endl;
        }
    } else {
        std::cout << "✅ 所有音频包已发送完成" << std::endl;
    }
}

bool AsrManager::hasMoreAudioPackets() const {
    if (m_status == AsrStatus::ERROR) return false;
    return m_audioSendIndex < m_audioPackets.size();
}

bool AsrManager::isAudioPacketSendingComplete() const {
    return m_audioSendIndex >= m_audioPackets.size();
}

std::string AsrManager::getAudioPacketStatus() const {
    std::stringstream ss;
    ss << "=== 音频包发送状态 ===" << std::endl;
    ss << "总包数: " << m_audioPackets.size() << std::endl;
    ss << "已发送: " << m_audioSendIndex << std::endl;
    ss << "剩余: " << (m_audioPackets.size() - m_audioSendIndex) << std::endl;
    ss << "发送完成: " << (isAudioPacketSendingComplete() ? "是" : "否") << std::endl;
    ss << "连接状态: " << (m_client && m_client->isConnected() ? "已连接" : "未连接") << std::endl;
    ss << "ASR状态: " << getStatusName(m_status) << std::endl;
    
    // 添加识别结果统计
    ss << "识别结果数: " << m_results.size() << std::endl;
    int finalResultCount = 0;
    for (const auto& result : m_results) {
        if (result.isFinal) finalResultCount++;
    }
    ss << "最终结果数: " << finalResultCount << std::endl;
    
    return ss.str();
}

// ============================================================================
// 私有音频文件解析方法
// ============================================================================

AudioFileInfo AsrManager::parseWavFile(const std::string& filePath, const std::vector<uint8_t>& header) {
    (void)header; // 避免未使用参数警告
    AudioFileInfo info;
    info.format = "wav";  // 修正：format表示容器格式，WAV文件应该是"wav"
    info.codec = "raw";   // codec表示编码格式，PCM数据是"raw"
    
    // 读取完整文件
    std::ifstream file(filePath, std::ios::binary);
    std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
    file.close();
    
    if (fileData.size() < 44) {
        std::cerr << "❌ WAV文件太小" << std::endl;
        return info;
    }
    
    // 解析WAV头部
    WavHeader* wavHeader = reinterpret_cast<WavHeader*>(fileData.data());
    
    info.sampleRate = wavHeader->sampleRate;
    info.bitsPerSample = wavHeader->bitsPerSample;
    info.channels = wavHeader->numChannels;
    
    // 查找data块
    size_t offset = 12 + 4 + 4 + wavHeader->fmtSize; // RIFF + WAVE + fmt + fmtSize
    
    while (offset + 8 < fileData.size()) {
        std::string chunkId(reinterpret_cast<char*>(fileData.data() + offset), 4);
        uint32_t chunkSize = *reinterpret_cast<uint32_t*>(fileData.data() + offset + 4);
        
        if (chunkId == "data") {
            info.dataOffset = offset + 8;
            info.dataSize = chunkSize;
            break;
        }
        
        offset += 8 + chunkSize;
    }
    
    if (info.dataOffset == 0) {
        std::cerr << "❌ 未找到WAV data块" << std::endl;
        return info;
    }
    
    // 计算时长
    info.duration = static_cast<double>(info.dataSize) / 
                   (info.channels * info.sampleRate * info.bitsPerSample / 8);
    
    info.isValid = true;
    
    std::cout << "📁 成功解析WAV文件: " << filePath << std::endl;
    std::cout << "🎵 音频信息:" << std::endl;
    std::cout << "  - 格式: " << info.format << " (WAV容器格式)" << std::endl;
    std::cout << "  - 编解码器: " << info.codec << " (PCM编码格式)" << std::endl;
    std::cout << "  - 采样率: " << info.sampleRate << " Hz" << std::endl;
    std::cout << "  - 位深度: " << info.bitsPerSample << " bits" << std::endl;
    std::cout << "  - 声道数: " << info.channels << std::endl;
    std::cout << "  - 音频数据大小: " << info.dataSize << " bytes" << std::endl;
    std::cout << "  - 音频时长: " << info.duration << " 秒" << std::endl;
    
    return info;
}

AudioFileInfo AsrManager::parseMp3File(const std::string& filePath, const std::vector<uint8_t>& header) {
    (void)header; // 避免未使用参数警告
    AudioFileInfo info;
    info.format = "mp3";
    info.codec = "mp3";
    
    // 获取文件大小
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开MP3文件: " << filePath << std::endl;
        return info;
    }
    
    info.dataSize = file.tellg();
    file.close();
    
    // MP3默认参数（简化处理）
    info.sampleRate = 16000;
    info.bitsPerSample = 16;
    info.channels = 1;
    info.dataOffset = 0; // MP3文件整体作为数据
    info.duration = info.dataSize / 32000.0; // 估算时长
    
    info.isValid = true;
    
    std::cout << "📁 成功解析MP3文件: " << filePath << std::endl;
    std::cout << "🎵 音频信息:" << std::endl;
    std::cout << "  - 格式: " << info.format << std::endl;
    std::cout << "  - 文件大小: " << info.dataSize << " bytes" << std::endl;
    std::cout << "  - 估算时长: " << info.duration << " 秒" << std::endl;
    
    return info;
}

AudioFileInfo AsrManager::parsePcmFile(const std::string& filePath, const std::vector<uint8_t>& header) {
    (void)header; // 避免未使用参数警告
    AudioFileInfo info;
    info.format = "pcm";
    info.codec = "raw";
    
    // 获取文件大小
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开PCM文件: " << filePath << std::endl;
        return info;
    }
    
    info.dataSize = file.tellg();
    file.close();
    
    // PCM默认参数（需要用户指定或从文件名推断）
    info.sampleRate = 16000;
    info.bitsPerSample = 16;
    info.channels = 1;
    info.dataOffset = 0; // PCM文件整体作为数据
    info.duration = static_cast<double>(info.dataSize) / 
                   (info.channels * info.sampleRate * info.bitsPerSample / 8);
    
    info.isValid = true;
    
    std::cout << "📁 成功解析PCM文件: " << filePath << std::endl;
    std::cout << "🎵 音频信息:" << std::endl;
    std::cout << "  - 格式: " << info.format << std::endl;
    std::cout << "  - 采样率: " << info.sampleRate << " Hz (默认)" << std::endl;
    std::cout << "  - 位深度: " << info.bitsPerSample << " bits (默认)" << std::endl;
    std::cout << "  - 声道数: " << info.channels << " (默认)" << std::endl;
    std::cout << "  - 音频数据大小: " << info.dataSize << " bytes" << std::endl;
    std::cout << "  - 音频时长: " << info.duration << " 秒" << std::endl;
    
    return info;
}

// ============================================================================
// AsrCallback 接口实现
// ============================================================================

void AsrManager::onOpen(AsrClient* client) {
    (void)client; // 避免未使用参数警告
    logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ WebSocket 连接已建立");
    updateStatus(AsrStatus::CONNECTED);
    
    if (m_callback) {
        m_callback->onOpen(client);
    }
}

void AsrManager::onMessage(AsrClient* client, const std::string& message) {
    (void)client;
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📨 收到服务器消息: " + message);
    }
    
    // 首先检查是否为错误响应
    try {
        json j = json::parse(message);
        if (j.contains("error") || (j.contains("code") && j["code"] != 0)) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 收到错误响应: " + message, true);
            updateStatus(AsrStatus::ERROR);
            m_stopFlag = true;
            if (m_client) m_client->disconnect();
            if (m_callback) {
                m_callback->onError(client, "服务器返回错误: " + message);
            }
            return;
        }
    } catch (const std::exception& e) {
        // JSON解析失败，继续处理
        if (m_config.enableBusinessLog) {
            logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 解析响应JSON失败: " + std::string(e.what()));
        }
    }
    
    // 解析识别结果 - 增强实时处理
    try {
        json j = json::parse(message);
        
        // 处理识别结果
        if (j.contains("result") && j["result"].contains("text")) {
            AsrResult result;
            result.text = j["result"]["text"];
            result.isFinal = j["result"].value("is_final", false);
            result.confidence = j["result"].value("confidence", 0.0);
            
            // 提取日志ID
            if (j["result"].contains("additions") && j["result"]["additions"].contains("log_id")) {
                result.logId = j["result"]["additions"]["log_id"];
            } else if (j.contains("log_id")) {
                result.logId = j["log_id"];
            }
            
            // 提取元数据
            if (j.contains("metadata")) {
                for (auto& [key, value] : j["metadata"].items()) {
                    result.metadata[key] = value.dump();
                }
            }
            
            m_results.push_back(result);
            m_latestResult = result;
            
            // 更新状态为识别中
            if (!result.isFinal) {
                updateStatus(AsrStatus::RECOGNIZING);
            }
            
            if (m_config.enableBusinessLog) {
                std::string statusStr = result.isFinal ? " (最终)" : " (实时)";
                logMessage(m_config.logLevel, ASR_LOG_INFO, "🎯 识别结果: " + result.text + statusStr);
            }
            
            // 添加详细的调试信息
            if (m_config.enableDataLog) {
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📊 ASR结果详情:");
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 文本长度: " + std::to_string(result.text.length()));
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 是否为最终: " + std::string(result.isFinal ? "是" : "否"));
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 置信度: " + std::to_string(result.confidence));
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 总结果数: " + std::to_string(m_results.size()));
                
                // 统计所有结果的文本长度
                size_t totalTextLength = 0;
                for (const auto& r : m_results) {
                    totalTextLength += r.text.length();
                }
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 累计文本长度: " + std::to_string(totalTextLength));
                
                // 添加音频包发送统计
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📦 音频包发送统计:");
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 总音频包数: " + std::to_string(m_audioPackets.size()));
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 已发送包数: " + std::to_string(m_audioSendIndex));
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 剩余包数: " + std::to_string(m_audioPackets.size() - m_audioSendIndex));
                logMessage(m_config.logLevel, ASR_LOG_DEBUG, "  - 发送完成率: " + std::to_string((double)m_audioSendIndex / m_audioPackets.size() * 100) + "%");
            }
        }
        
        // 处理会话开始消息
        if (j.contains("code") && j["code"] == 0) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ ASR会话已开始");
            updateStatus(AsrStatus::RECOGNIZING);
        }
        
        // 处理状态信息
        if (j.contains("status")) {
            int status = j["status"];
            if (status == 0) {
                logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ ASR状态正常");
            } else {
                logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ ASR状态异常: " + std::to_string(status));
            }
        }
        
        // 添加原始消息的调试信息
        if (m_config.enableProtocolLog) {
            logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📨 原始ASR消息: " + message);
        }
        
    } catch (const std::exception& e) {
        if (m_config.enableBusinessLog) {
            logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 解析识别结果失败: " + std::string(e.what()));
        }
    }
    
    // 调用回调函数，传递完整的消息和解析后的结果
    if (m_callback) {
        m_callback->onMessage(client, message);
    }
}

void AsrManager::onError(AsrClient* client, const std::string& error) {
    m_stopFlag = true;
    if (m_client) m_client->disconnect();
    logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ WebSocket 错误: " + error, true);
    updateStatus(AsrStatus::ERROR);
    if (m_callback) {
        m_callback->onError(client, error);
    }
}

void AsrManager::onClose(AsrClient* client) {
    (void)client; // 避免未使用参数警告
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔌 WebSocket 连接已关闭");
    updateStatus(AsrStatus::DISCONNECTED);
    
    if (m_callback) {
        m_callback->onClose(client);
    }
}

void AsrManager::recognizeAudioFileAsync(const std::string& filePath) {
    if (m_status == AsrStatus::RECOGNIZING) {
        logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ ASR 正在识别中，忽略重复请求");
        return;
    }
    stopRecognition();

    logMessage(m_config.logLevel, ASR_LOG_INFO, "🚀 启动异步ASR识别线程");
    logMessage(m_config.logLevel, ASR_LOG_INFO, "📁 目标文件: " + filePath);
    
    m_workerThread = std::thread(&AsrManager::recognition_thread_func, this, filePath);
}

void AsrManager::recognition_thread_func(const std::string& filePath) {
    m_stopFlag = false; // 启动线程时重置
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔄 ASR识别线程已启动");
    m_status = AsrStatus::RECOGNIZING;

    // 创建客户端
    m_client = std::make_unique<AsrClient>();
    if (!m_client) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 创建ASR客户端失败", true);
        m_status = AsrStatus::ERROR;
        return;
    }
    m_client->setCallback(this);
    m_client->setAppId(m_config.appId);
    m_client->setToken(m_config.accessToken);
    m_client->setSecretKey(m_config.secretKey);
    m_client->setAudioFormat(m_config.format, m_config.channels, m_config.sampleRate, m_config.bits);
    m_client->setUid(m_config.uid);
    m_client->setLanguage(m_config.language);
    m_client->setResultType(m_config.resultType);
    m_client->setStreaming(m_config.streaming);
    m_client->setSegDuration(m_config.segDuration);
    logMessage(m_config.logLevel, ASR_LOG_DEBUG, "✅ 客户端配置完成");

    // 解析音频文件并分包
    AudioFileInfo audioInfo = parseAudioFile(filePath);
    if (!audioInfo.isValid) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频文件解析失败", true);
        m_status = AsrStatus::ERROR;
        return;
    }
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法打开音频文件", true);
        m_status = AsrStatus::ERROR;
        return;
    }
    
    std::vector<uint8_t> audioData;
    
    // 根据format决定读取方式
    if (m_config.format == "wav") {
        // 对于WAV格式，读取完整文件（包括头部）
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        audioData.resize(fileSize);
        file.read(reinterpret_cast<char*>(audioData.data()), fileSize);
        
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 读取完整WAV文件: " + std::to_string(audioData.size()) + " bytes (包括头部)");
    } else {
        // 对于其他格式，只读取音频数据部分（跳过文件头）
        file.seekg(audioInfo.dataOffset);
        audioData.resize(audioInfo.dataSize);
        file.read(reinterpret_cast<char*>(audioData.data()), audioInfo.dataSize);
        
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 读取音频数据: " + std::to_string(audioData.size()) + " bytes (仅数据部分)");
    }
    
    file.close();
    
    if (audioData.empty()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频数据为空", true);
        m_status = AsrStatus::ERROR;
        return;
    }

    size_t segmentSize = m_config.segDuration * m_config.sampleRate * m_config.channels * m_config.bits / 8 / 1000;
    if (segmentSize == 0) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 分包大小为0，参数异常", true);
        m_status = AsrStatus::ERROR;
        return;
    }
    m_audioPackets.clear();
    m_audioSendIndex = 0;
    for (size_t offset = 0; offset < audioData.size(); offset += segmentSize) {
        size_t chunkSize = std::min(segmentSize, audioData.size() - offset);
        std::vector<uint8_t> chunk(audioData.begin() + offset, audioData.begin() + offset + chunkSize);
        m_audioPackets.push_back(std::move(chunk));
    }
    logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 音频分包完成: " + std::to_string(m_audioPackets.size()) + " 个包");

    // 连接ASR服务
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔗 正在连接ASR服务...");
    if (!m_client->connect()) {
        auto err = m_client->getLastError();
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR连接失败: " + err.message, true);
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "🔍 错误码: " + std::to_string(err.code), true);
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "📝 错误详情: " + err.details, true);
        m_status = AsrStatus::ERROR;
        return;
    }
    logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ ASR连接成功，开始发送Full Client Request");
    if (!m_client->sendFullClientRequestAndWaitResponse(10000)) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 发送Full Client Request失败", true);
        m_status = AsrStatus::ERROR;
        return;
    }
    logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ Full Client Request 发送成功，开始流式发送音频包");
    // 逐包发送
    for (size_t i = 0; i < m_audioPackets.size(); ++i) {
        // 检查停止标志
        if (m_stopFlag || m_stopRequested) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "🛑 检测到停止请求，停止音频包发送");
            break;
        }
        
        bool isLast = (i == m_audioPackets.size() - 1);
        if (!m_client->sendAudioFile(m_audioPackets[i], isLast, static_cast<int32_t>(i + 1))) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 发送音频包失败: " + std::to_string(i), true);
            m_status = AsrStatus::ERROR;
            m_stopFlag = true;
            m_client->disconnect();
            break;
        }
        logMessage(m_config.logLevel, ASR_LOG_INFO, "📦 已发送音频包: " + std::to_string(i + 1) + (isLast ? " (最后一包)" : ""));
        
        // 在等待期间也检查停止标志
        for (int j = 0; j < m_config.segDuration; ++j) {
            if (m_stopFlag || m_stopRequested) {
                logMessage(m_config.logLevel, ASR_LOG_INFO, "🛑 检测到停止请求，中断等待");
                goto send_complete;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
send_complete:
    if (m_stopFlag || m_stopRequested) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🛑 ASR识别被用户停止");
    } else {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 所有音频包发送完成，总包数: " + std::to_string(m_audioPackets.size()));
    }
    m_status = AsrStatus::DISCONNECTED;
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🏁 ASR识别线程已结束");
    m_stopFlag = false; // 线程结束时重置
}

// ============================================================================
// 计时器相关方法实现
// ============================================================================

void AsrManager::startSessionTimer() {
    if (!m_config.enableUsageTracking) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    
    // 生成会话ID
    m_currentSession_.sessionId = generateSessionId();
    m_currentSession_.connectTime = std::chrono::system_clock::now();
    m_currentSession_.isCompleted = false;
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "⏱️ 开始计时会话: " + m_currentSession_.sessionId);
}

void AsrManager::endSessionTimer(bool isCompleted) {
    if (!m_config.enableUsageTracking) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    
    if (m_currentSession_.connectTime.time_since_epoch().count() > 0) {
        m_currentSession_.disconnectTime = std::chrono::system_clock::now();
        m_currentSession_.isCompleted = isCompleted;
        m_currentSession_.calculateDuration();
        
        // 获取当前日期
        std::string currentDate = getCurrentDate();
        
        // 添加到每日统计
        if (m_dailyStats_.find(currentDate) == m_dailyStats_.end()) {
            m_dailyStats_[currentDate] = DailyUsageStats();
            m_dailyStats_[currentDate].date = currentDate;
        }
        
        m_dailyStats_[currentDate].addSession(m_currentSession_);
        
        // 更新总体统计
        updateOverallStats();
        
        // 保存统计数据
        saveStats();
        
        logMessage(m_config.logLevel, ASR_LOG_INFO, 
                  "⏱️ 会话结束: " + m_currentSession_.sessionId + 
                  " 持续时间: " + m_currentSession_.getFormattedDuration());
        
        // 重置当前会话
        m_currentSession_ = ConnectionSession();
    }
}

void AsrManager::updateOverallStats() {
    m_overallStats_.totalDuration = std::chrono::milliseconds(0);
    m_overallStats_.totalSessionCount = 0;
    m_overallStats_.activeDays = m_dailyStats_.size();
    
    std::chrono::system_clock::time_point firstUsage = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point lastUsage = std::chrono::system_clock::time_point::min();
    
    for (const auto& pair : m_dailyStats_) {
        const auto& dailyStats = pair.second;
        m_overallStats_.totalDuration += dailyStats.totalDuration;
        m_overallStats_.totalSessionCount += dailyStats.sessionCount;
        
        for (const auto& session : dailyStats.sessions) {
            if (session.connectTime < firstUsage) {
                firstUsage = session.connectTime;
            }
            if (session.disconnectTime > lastUsage) {
                lastUsage = session.disconnectTime;
            }
        }
    }
    
    if (m_overallStats_.totalSessionCount > 0) {
        m_overallStats_.firstUsage = firstUsage;
        m_overallStats_.lastUsage = lastUsage;
    }
}

DailyUsageStats AsrManager::getTodayStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    std::string today = getCurrentDate();
    
    auto it = m_dailyStats_.find(today);
    if (it != m_dailyStats_.end()) {
        return it->second;
    }
    
    return DailyUsageStats();
}

DailyUsageStats AsrManager::getDateStats(const std::string& date) const {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    
    auto it = m_dailyStats_.find(date);
    if (it != m_dailyStats_.end()) {
        return it->second;
    }
    
    return DailyUsageStats();
}

OverallStats AsrManager::getOverallStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    return m_overallStats_;
}

std::vector<DailyUsageStats> AsrManager::getRecentStats(int days) const {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    std::vector<DailyUsageStats> recentStats;
    
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_today = std::localtime(&today);
    
    for (int i = 0; i < days; ++i) {
        std::tm tm_date = *tm_today;
        tm_date.tm_mday -= i;
        std::mktime(&tm_date);
        
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(4) << (tm_date.tm_year + 1900) << "-"
           << std::setfill('0') << std::setw(2) << (tm_date.tm_mon + 1) << "-"
           << std::setfill('0') << std::setw(2) << tm_date.tm_mday;
        
        std::string dateStr = ss.str();
        auto it = m_dailyStats_.find(dateStr);
        if (it != m_dailyStats_.end()) {
            recentStats.push_back(it->second);
        } else {
            // 添加空统计
            DailyUsageStats emptyStats;
            emptyStats.date = dateStr;
            recentStats.push_back(emptyStats);
        }
    }
    
    return recentStats;
}

std::map<std::string, DailyUsageStats> AsrManager::getAllStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    return m_dailyStats_;
}

ConnectionSession AsrManager::getCurrentSession() const {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    return m_currentSession_;
}

bool AsrManager::saveStats() {
    if (!m_config.enableUsageTracking) {
        return true;
    }
    
    try {
        std::string filePath = getStatsFilePath();
        std::string backupPath = getBackupFilePath();
        
        // 创建数据目录
        if (!createDataDirectory()) {
            return false;
        }
        
        // 创建备份
        if (std::filesystem::exists(filePath)) {
            std::filesystem::copy_file(filePath, backupPath, std::filesystem::copy_options::overwrite_existing);
        }
        
        // 保存统计数据
        json statsData;
        statsData["version"] = "1.0";
        statsData["last_updated"] = getCurrentDate();
        
        // 保存总体统计
        json overallStats;
        overallStats["total_duration_ms"] = m_overallStats_.totalDuration.count();
        overallStats["total_session_count"] = m_overallStats_.totalSessionCount;
        overallStats["active_days"] = m_overallStats_.activeDays;
        if (m_overallStats_.totalSessionCount > 0) {
            overallStats["first_usage"] = formatTimePoint(m_overallStats_.firstUsage);
            overallStats["last_usage"] = formatTimePoint(m_overallStats_.lastUsage);
        }
        statsData["overall_stats"] = overallStats;
        
        // 保存每日统计
        json dailyStatsArray = json::array();
        for (const auto& pair : m_dailyStats_) {
            const auto& dailyStats = pair.second;
            json dailyStat;
            dailyStat["date"] = dailyStats.date;
            dailyStat["total_duration_ms"] = dailyStats.totalDuration.count();
            dailyStat["session_count"] = dailyStats.sessionCount;
            
            json sessionsArray = json::array();
            for (const auto& session : dailyStats.sessions) {
                json sessionData;
                sessionData["session_id"] = session.sessionId;
                sessionData["connect_time"] = formatTimePoint(session.connectTime);
                sessionData["disconnect_time"] = formatTimePoint(session.disconnectTime);
                sessionData["duration_ms"] = session.duration.count();
                sessionData["is_completed"] = session.isCompleted;
                sessionsArray.push_back(sessionData);
            }
            dailyStat["sessions"] = sessionsArray;
            dailyStatsArray.push_back(dailyStat);
        }
        statsData["daily_stats"] = dailyStatsArray;
        
        // 写入文件
        std::ofstream file(filePath);
        if (!file.is_open()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法打开统计文件进行写入: " + filePath, true);
            return false;
        }
        
        file << statsData.dump(2);
        file.close();
        
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "✅ 统计数据已保存到: " + filePath);
        return true;
        
    } catch (const std::exception& e) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 保存统计数据失败: " + std::string(e.what()), true);
        return false;
    }
}

bool AsrManager::loadStats() {
    if (!m_config.enableUsageTracking) {
        return true;
    }
    
    try {
        std::string filePath = getStatsFilePath();
        
        if (!std::filesystem::exists(filePath)) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "ℹ️ 统计文件不存在，将创建新的统计记录");
            return true;
        }
        
        std::ifstream file(filePath);
        if (!file.is_open()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法打开统计文件: " + filePath, true);
            return false;
        }
        
        json statsData = json::parse(file);
        file.close();
        
        // 清空现有数据
        m_dailyStats_.clear();
        
        // 加载总体统计
        if (statsData.contains("overall_stats")) {
            const auto& overallStats = statsData["overall_stats"];
            m_overallStats_.totalDuration = std::chrono::milliseconds(overallStats["total_duration_ms"].get<int64_t>());
            m_overallStats_.totalSessionCount = overallStats["total_session_count"].get<int>();
            m_overallStats_.activeDays = overallStats["active_days"].get<int>();
            
            if (overallStats.contains("first_usage")) {
                // 这里可以添加时间解析逻辑
            }
            if (overallStats.contains("last_usage")) {
                // 这里可以添加时间解析逻辑
            }
        }
        
        // 加载每日统计
        if (statsData.contains("daily_stats")) {
            for (const auto& dailyStatData : statsData["daily_stats"]) {
                DailyUsageStats dailyStats;
                dailyStats.date = dailyStatData["date"].get<std::string>();
                dailyStats.totalDuration = std::chrono::milliseconds(dailyStatData["total_duration_ms"].get<int64_t>());
                dailyStats.sessionCount = dailyStatData["session_count"].get<int>();
                
                for (const auto& sessionData : dailyStatData["sessions"]) {
                    ConnectionSession session;
                    session.sessionId = sessionData["session_id"].get<std::string>();
                    session.duration = std::chrono::milliseconds(sessionData["duration_ms"].get<int64_t>());
                    session.isCompleted = sessionData["is_completed"].get<bool>();
                    
                    // 这里可以添加时间解析逻辑
                    // session.connectTime = parseTimePoint(sessionData["connect_time"]);
                    // session.disconnectTime = parseTimePoint(sessionData["disconnect_time"]);
                    
                    dailyStats.sessions.push_back(session);
                }
                
                m_dailyStats_[dailyStats.date] = dailyStats;
            }
        }
        
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 统计数据已加载，共 " + std::to_string(m_dailyStats_.size()) + " 天的记录");
        return true;
        
    } catch (const std::exception& e) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 加载统计数据失败: " + std::string(e.what()), true);
        return false;
    }
}

void AsrManager::clearStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    
    m_dailyStats_.clear();
    m_overallStats_ = OverallStats();
    m_currentSession_ = ConnectionSession();
    
    // 删除统计文件
    std::string filePath = getStatsFilePath();
    if (std::filesystem::exists(filePath)) {
        std::filesystem::remove(filePath);
    }
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🗑️ 所有统计数据已清除");
}

bool AsrManager::exportToCsv(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法创建CSV文件: " + filePath, true);
            return false;
        }
        
        // 写入CSV头部
        file << "Date,Total Duration (ms),Session Count,Formatted Duration\n";
        
        // 写入每日统计数据
        auto allStats = getAllStats();
        for (const auto& pair : allStats) {
            const auto& dailyStats = pair.second;
            file << dailyStats.date << ","
                 << dailyStats.totalDuration.count() << ","
                 << dailyStats.sessionCount << ","
                 << "\"" << dailyStats.getFormattedTotalDuration() << "\"\n";
        }
        
        file.close();
        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 统计数据已导出到CSV: " + filePath);
        return true;
        
    } catch (const std::exception& e) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 导出CSV失败: " + std::string(e.what()), true);
        return false;
    }
}

std::string AsrManager::getStatsSummary() const {
    std::lock_guard<std::mutex> lock(m_statsMutex_);
    
    std::stringstream ss;
    ss << "=== ASR 使用统计摘要 ===" << std::endl;
    ss << "总使用时长: " << m_overallStats_.getFormattedTotalDuration() << std::endl;
    ss << "总会话次数: " << m_overallStats_.totalSessionCount << std::endl;
    ss << "活跃天数: " << m_overallStats_.activeDays << std::endl;
    
    if (m_overallStats_.totalSessionCount > 0) {
        ss << "首次使用: " << formatTimePoint(m_overallStats_.firstUsage) << std::endl;
        ss << "最后使用: " << formatTimePoint(m_overallStats_.lastUsage) << std::endl;
    }
    
    // 今日统计
    std::string today = getCurrentDate();
    auto it = m_dailyStats_.find(today);
    if (it != m_dailyStats_.end()) {
        ss << "今日使用时长: " << it->second.getFormattedTotalDuration() << std::endl;
        ss << "今日会话次数: " << it->second.sessionCount << std::endl;
    } else {
        ss << "今日使用时长: 0s" << std::endl;
        ss << "今日会话次数: 0" << std::endl;
    }
    
    return ss.str();
}

// ============================================================================
// 静态方法实现
// ============================================================================

std::string AsrManager::getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << (tm->tm_year + 1900) << "-"
       << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << "-"
       << std::setfill('0') << std::setw(2) << tm->tm_mday;
    
    return ss.str();
}

std::string AsrManager::generateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    std::stringstream ss;
    ss << "session-" << std::hex << millis << "-" << std::rand();
    return ss.str();
}

std::string AsrManager::formatTimePoint(const std::chrono::system_clock::time_point& timePoint) {
    auto time_t = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* tm = std::localtime(&time_t);
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << (tm->tm_year + 1900) << "-"
       << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << "-"
       << std::setfill('0') << std::setw(2) << tm->tm_mday << " "
       << std::setfill('0') << std::setw(2) << tm->tm_hour << ":"
       << std::setfill('0') << std::setw(2) << tm->tm_min << ":"
       << std::setfill('0') << std::setw(2) << tm->tm_sec;
    
    return ss.str();
}

std::string AsrManager::getStatsFilePath() const {
    std::string dataDir = m_config.statsDataDir;
    if (dataDir.empty()) {
        dataDir = std::filesystem::current_path().string() + "/data";
    }
    return dataDir + "/asr_usage_stats.json";
}

std::string AsrManager::getBackupFilePath() const {
    std::string dataDir = m_config.statsDataDir;
    if (dataDir.empty()) {
        dataDir = std::filesystem::current_path().string() + "/data";
    }
    return dataDir + "/asr_usage_stats_backup.json";
}

bool AsrManager::createDataDirectory() const {
    std::string dataDir = m_config.statsDataDir;
    if (dataDir.empty()) {
        dataDir = std::filesystem::current_path().string() + "/data";
    }
    
    try {
        if (!std::filesystem::exists(dataDir)) {
            std::filesystem::create_directories(dataDir);
        }
        return true;
    } catch (const std::exception& e) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 创建数据目录失败: " + std::string(e.what()), true);
        return false;
    }
}

void AsrManager::logStats(const std::string& message) const {
    logMessage(m_config.logLevel, ASR_LOG_DEBUG, "[统计] " + message);
}

} // namespace Asr 