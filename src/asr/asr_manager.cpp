// This file is being reverted to its original state.
// The content below is a placeholder representing the original file content.
// This action is to undo all previous, incorrect edits.
#include "audio/audio_types.h"  // 主要是一些音频相关的定义，例如WavHeader
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
#include <thread>
#include <string>
#include <cstring>
#include <chrono>
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

void AsrManager::setCallback(AsrCallback* callback) {
    m_callback = callback;
}

// 获取ASR详细状态信息
std::string AsrManager::getDetailedStatus() const {
    std::stringstream ss;
    ss << "=== ASR 详细状态信息 ===" << std::endl;
    ss << "连接状态: " << getStatusName(m_status) << std::endl;
    ss << "客户端类型: IXWebSocket" << std::endl;
    ss << "是否已连接: " << (isConnected() ? "是" : "否") << std::endl;
    ss << "音频包数量: " << m_audioPackets.size() << std::endl;
    ss << "已发送包数: " << m_audioSendIndex << std::endl;
    ss << "剩余包数: " << (m_audioPackets.size() - m_audioSendIndex) << std::endl;
    
    if (m_client) {
        ss << "客户端连接状态: " << (m_client->isConnected() ? "已连接" : "未连接") << std::endl;
    }
    
    return ss.str();
}

// 获取音频处理统计信息
std::string AsrManager::getAudioStats() const {
    std::stringstream ss;
    ss << "=== 音频处理统计 ===" << std::endl;
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

bool AsrManager::startRecognition() {
    // 确保客户端已初始化并连接
    if (!initializeClient() || !connect()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法连接到 ASR 服务器，无法开始识别", true);
        return false;
    }
    
    // 发送完整客户端请求
    std::string response = m_client->sendFullClientRequestAndGetResponse(10000);
    
    if (!response.empty()) {
        // 检查客户端是否已准备好接收音频
        if (m_client->isReadyForAudio()) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 识别会话已开始");
            updateStatus(AsrStatus::RECOGNIZING);
            return true;
        } else {
            // 如果响应不为空但客户端未准备好，可能是错误响应
            try {
                json j = json::parse(response);
                if (j.contains("error") || (j.contains("code") && j["code"] != 0)) {
                    logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ Full Server Response 包含错误: " + response, true);
                    updateStatus(AsrStatus::ERROR);
                    return false;
                }
            } catch (const std::exception& e) {
                // JSON 解析失败，可能是非 JSON 格式的响应
                logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 无法解析服务器响应: " + response);
            }
            
            logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 识别会话已开始");
            updateStatus(AsrStatus::RECOGNIZING);
            return true;
        }
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
    // 由于移除了isFinal字段，返回最后一个结果作为最终结果
    if (!m_results.empty()) {
        return m_results.back();
    }
    // 如果没有结果，返回一个空的结果
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
    (void)type; // 忽略参数，当前只支持IXWebSocket
    return std::make_unique<AsrClient>();
}

bool AsrManager::initializeClient() {
    if (m_client) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "✅ 客户端已存在，跳过初始化");
        return true;
    }
    
    logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📡 正在创建IXWebSocket客户端");
    
    m_client = createClient(m_config.clientType);
    if (!m_client) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 创建客户端实例失败", true);
        return false;
    }
    
    // 设置客户端配置 - 使用AsrApiConfig
    // 注意：所有API相关的配置都由AsrClient内部管理，这里只传递必要的认证信息
    m_client->setAppId(m_config.appId);
    m_client->setToken(m_config.accessToken);
    m_client->setSecretKey(m_config.secretKey);
    
    // 设置默认音频格式 (这些配置现在由AsrClient管理)
    m_client->setAudioFormat("pcm", 1, 16000, 16, "raw");
    
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
        // 设置认证信息
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
    
    // 步骤2: 验证音频格式是否符合ASR API要求
    if (m_config.enableBusinessLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "=== 步骤2: 验证音频格式 ===");
    }
    
    // 检测到的音频格式信息
    std::string formatInfo = std::string("🔍 检测到音频格式: ") + 
                             "format=" + audioInfo.format + 
                             ", channels=" + std::to_string(audioInfo.channels) + 
                             ", sampleRate=" + std::to_string(audioInfo.sampleRate) + 
                             ", bitsPerSample=" + std::to_string(audioInfo.bitsPerSample) + 
                             ", codec=" + audioInfo.codec;
    logMessage(m_config.logLevel, ASR_LOG_INFO, formatInfo);
    
    // 验证音频格式是否符合ASR API要求
    // 注意：这里需要先创建客户端来验证格式，但实际连接在后面
    std::unique_ptr<AsrClient> tempClient = std::make_unique<AsrClient>();
    auto validation = tempClient->validateAudioFormat("pcm", audioInfo.channels, 
                                                     audioInfo.sampleRate, audioInfo.bitsPerSample, "raw");
    
    if (!validation.isValid) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频格式不符合ASR API要求: " + validation.errorMessage, true);
        logMessage(m_config.logLevel, ASR_LOG_INFO, "📋 " + tempClient->getSupportedAudioFormats());
        return false;
    }
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 音频格式验证通过");
    
    // 步骤2.5: 读取音频文件并分包（新增关键步骤）
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤2.5: 音频文件读取和分包 ===");
    }
    
    // 读取整个音频文件
    std::ifstream audioFile(filePath, std::ios::binary);
    if (!audioFile.is_open()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 无法打开音频文件: " + filePath, true);
        return false;
    }
    
    // 跳过文件头，只读取音频数据
    audioFile.seekg(audioInfo.dataOffset);
    std::vector<uint8_t> audioData;
    audioData.resize(audioInfo.dataSize);
    audioFile.read(reinterpret_cast<char*>(audioData.data()), audioInfo.dataSize);
    audioFile.close();
    
    if (audioData.empty()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频数据为空", true);
        return false;
    }
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "📊 读取音频数据: " + std::to_string(audioData.size()) + " 字节");
    
    // 计算100ms对应的字节数
    size_t bytesPerSecond = audioInfo.sampleRate * audioInfo.channels * (audioInfo.bitsPerSample / 8);
    size_t bytesPer100ms = bytesPerSecond / 10; // 100ms
    
    // 分包
    m_audioPackets.clear();
    size_t offset = 0;
    while (offset < audioData.size()) {
        size_t chunkSize = std::min(bytesPer100ms, audioData.size() - offset);
        std::vector<uint8_t> packet(audioData.begin() + offset, audioData.begin() + offset + chunkSize);
        m_audioPackets.push_back(std::move(packet));
        offset += chunkSize;
    }
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "📦 音频分包完成: " + std::to_string(m_audioPackets.size()) + " 个包");
    
    // 添加调试信息确认分包结果
    if (m_audioPackets.empty()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 音频分包失败：m_audioPackets为空", true);
        return false;
    }
    
    // 显示前几个包的信息
    for (size_t i = 0; i < std::min(size_t(3), m_audioPackets.size()); ++i) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "📦 音频包[" + std::to_string(i) + "]: " + std::to_string(m_audioPackets[i].size()) + " 字节");
    }
    
    // 步骤3: 连接ASR服务
    if (m_config.enableFlowLog) {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤3: 连接ASR服务 ===");
    }
    
    // 确保客户端使用更新后的配置重新初始化
    if (m_client) {
        m_client.reset(); // 重置客户端，强制重新初始化
    }
    
    if (!connect()) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 连接ASR服务失败", true);
        return false;
    }
    
    // 根据实际检测到的音频格式设置客户端配置
    // 注意：这里使用检测到的实际格式，而不是硬编码
    std::string format = audioInfo.format;
    if (format == "wav") {
        format = "pcm"; // WAV文件内部是PCM数据，但格式标识为pcm
    }
    
    m_client->setAudioFormat(format, audioInfo.channels, audioInfo.sampleRate, audioInfo.bitsPerSample, audioInfo.codec);
    
    // 步骤4: 发送Full Client Request（初始化包），并等待服务器响应
    std::string response;
    if (!m_client->sendFullClientRequestAndWaitResponse(10000, &response)) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 初始化包发送失败或未收到服务器响应", true);
        m_client->disconnect();
        return false;
    }
    // 检查响应是否有错误
    try {
        json j = json::parse(response);
        if (j.contains("error") || (j.contains("code") && j["code"] != 0)) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ Full Server Response 包含错误: " + response, true);
            m_client->disconnect();
            return false;
        }
    } catch (...) {
        // 忽略解析失败，假定成功
    }

    // 步骤5: 分包发送音频，每包都等待服务器响应
    logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 步骤5: 开始发送音频包 ===");
    for (size_t i = 0; i < m_audioPackets.size(); ++i) {
        bool isLast = (i == m_audioPackets.size() - 1);
        int seq = 2 + i;
        int sendSeq = isLast ? -seq : seq;

        logMessage(m_config.logLevel, ASR_LOG_INFO, "📤 发送音频包 " + std::to_string(i+1) + "/" + std::to_string(m_audioPackets.size()) + " (seq=" + std::to_string(sendSeq) + ")");

        if (!m_client->isConnected()) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 连接已断开，终止流式发送", true);
            m_client->disconnect();
            return false;
        }
        if (!m_client->sendAudio(m_audioPackets[i], sendSeq)) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 发送音频包失败 seq=" + std::to_string(sendSeq), true);
            m_client->disconnect();
            return false;
        }
        // 等待服务器响应
        std::string audioResp;
        if (!m_client->waitForResponse(3000, &audioResp)) {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 等待音频包响应超时 seq=" + std::to_string(sendSeq), true);
            m_client->disconnect();
            return false;
        }
        // 可加最小帧间隔sleep (使用固定的100ms间隔)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 步骤6: 等待最终识别结果（可选）
    if (waitForFinal) {
        int totalWait = 0, maxWait = timeoutMs > 0 ? timeoutMs : 5000;
        while (totalWait < maxWait) {
            if (m_client->hasReceivedFinalResponse()) break;
            if (!m_client->isConnected()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            totalWait += 100;
        }
    }

    m_client->disconnect();
    logMessage(m_config.logLevel, ASR_LOG_INFO, "=== 识别流程结束 ===");
    return true;
}

// ============================================================================
// 异步音频识别方法实现
// ============================================================================

void AsrManager::recognizeAudioFileAsync(const std::string& filePath) {
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔄 开始异步音频识别: " + filePath);
    
    // 如果已有工作线程在运行，先停止它
    if (m_workerThread.joinable()) {
        logMessage(m_config.logLevel, ASR_LOG_WARN, "⚠️ 检测到正在运行的工作线程，正在停止...");
        m_stopFlag = true;
        m_workerThread.join();
        m_stopFlag = false;
    }
    
    // 启动新的工作线程
    m_workerThread = std::thread(&AsrManager::recognition_thread_func, this, filePath);
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 异步识别任务已启动");
}

void AsrManager::recognition_thread_func(const std::string& filePath) {
    try {
        logMessage(m_config.logLevel, ASR_LOG_INFO, "🧵 识别线程开始处理: " + filePath);
        
        // 执行同步识别
        bool success = recognizeAudioFile(filePath, true, 30000);
        
        if (success) {
            logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 异步识别完成: " + filePath);
        } else {
            logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 异步识别失败: " + filePath, true);
        }
        
    } catch (const std::exception& e) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 识别线程异常: " + std::string(e.what()), true);
    } catch (...) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 识别线程发生未知异常", true);
    }
    
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🧵 识别线程结束");
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

//裸PCM文件
AudioFileInfo Asr::AsrManager::parsePcmFile(const std::string& filePath, const std::vector<uint8_t>& header) {
    (void)header; // 避免未使用参数警告
    AudioFileInfo info;
    info.format = "pcm";
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开PCM文件" << std::endl;
        return info;
    }
    
    file.seekg(0, std::ios::end);
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
    std::cout << "  - 编解码器: " << info.codec << std::endl;
    std::cout << "  - 音频数据大小: " << info.dataSize << " bytes" << std::endl;
    std::cout << "  - 音频时长: " << info.duration << " 秒" << std::endl;
    
    return info;
}

//WAV文件,带header
AudioFileInfo Asr::AsrManager::parseWavFile(const std::string& filePath, const std::vector<uint8_t>& header) {
    AudioFileInfo info;
    info.format = "wav";
    
    if (header.size() < 12) {  // 至少需要RIFF头
        std::cerr << "❌ WAV文件头太小" << std::endl;
        return info;
    }
    
    // 检查WAV文件标识
    std::string riff(reinterpret_cast<const char*>(header.data()), 4);
    std::string wave(reinterpret_cast<const char*>(header.data() + 8), 4);
    
    if (riff != "RIFF" || wave != "WAVE") {
        std::cerr << "❌ 无效的WAV文件格式" << std::endl;
        return info;
    }
    
    // 打开文件进行详细解析
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开WAV文件" << std::endl;
        return info;
    }
    
    // 从文件开头开始解析所有块
    file.seekg(0);
    char chunkId[4];
    uint32_t chunkSize;
    bool foundFmt = false;
    bool foundData = false;
    
    // 读取RIFF头
    file.read(chunkId, 4);
    file.read(reinterpret_cast<char*>(&chunkSize), 4);
    file.read(chunkId, 4);  // 读取"WAVE"
    
    if (std::string(chunkId, 4) != "WAVE") {
        std::cerr << "❌ 无效的WAV文件格式" << std::endl;
        file.close();
        return info;
    }
    
    // 解析所有块
    while (file.read(chunkId, 4) && file.read(reinterpret_cast<char*>(&chunkSize), 4)) {
        std::string chunkName(chunkId, 4);
        
        if (chunkName == "fmt ") {
            // 解析fmt块
            uint16_t format, channels, bitsPerSample;
            uint32_t sampleRate, byteRate;
            uint16_t blockAlign;
            
            file.read(reinterpret_cast<char*>(&format), 2);
            file.read(reinterpret_cast<char*>(&channels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            
            info.sampleRate = sampleRate;
            info.bitsPerSample = bitsPerSample;
            info.channels = channels;
            info.codec = "PCM";
            foundFmt = true;
            
            // 跳过fmt块的剩余部分
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }
        } else if (chunkName == "data") {
            // 找到data块
            info.dataOffset = file.tellg();
            info.dataSize = chunkSize;
            foundData = true;
            break;  // 找到data块后就可以停止了
        } else {
            // 跳过其他块（如LIST、INFO等元数据块）
            // 注意：chunkSize可能包含填充字节，需要确保正确跳过
            file.seekg(chunkSize, std::ios::cur);
            
            // 如果chunkSize是奇数，需要跳过填充字节
            if (chunkSize % 2 != 0) {
                file.seekg(1, std::ios::cur);
            }
        }
    }
    
    file.close();
    
    if (!foundFmt) {
        std::cerr << "❌ 未找到WAV格式块" << std::endl;
        return info;
    }
    
    if (!foundData) {
        std::cerr << "❌ 未找到WAV数据块" << std::endl;
        return info;
    }
    
    if (info.dataSize == 0) {
        std::cerr << "❌ WAV数据块大小为0" << std::endl;
        return info;
    }
    
    info.duration = static_cast<double>(info.dataSize) / 
                   (info.channels * info.sampleRate * info.bitsPerSample / 8);
    info.isValid = true;
    
    std::cout << "📁 成功解析WAV文件: " << filePath << std::endl;
    std::cout << "🎵 音频信息:" << std::endl;
    std::cout << "  - 格式: " << info.format << std::endl;
    std::cout << "  - 采样率: " << info.sampleRate << " Hz" << std::endl;
    std::cout << "  - 位深度: " << info.bitsPerSample << " bits" << std::endl;
    std::cout << "  - 声道数: " << info.channels << std::endl;
    std::cout << "  - 编解码器: " << info.codec << std::endl;
    std::cout << "  - 音频数据大小: " << info.dataSize << " bytes" << std::endl;
    std::cout << "  - 音频时长: " << info.duration << " 秒" << std::endl;
    std::cout << "  - 数据偏移: " << info.dataOffset << " bytes" << std::endl;
    
    return info;
}

// ============================================================================
// AsrCallback 虚函数实现
// ============================================================================

void Asr::AsrManager::onOpen(AsrClient* client) {
    (void)client;
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔗 ASR连接已打开");
    updateStatus(AsrStatus::CONNECTED);
    
    // 开始会话计时
    startSessionTimer();
}

void Asr::AsrManager::onClose(AsrClient* client) {
    (void)client;
    logMessage(m_config.logLevel, ASR_LOG_INFO, "🔌 ASR连接已关闭");
    updateStatus(AsrStatus::DISCONNECTED);
    
    // 结束会话计时
    endSessionTimer(true);
}

void Asr::AsrManager::onError(AsrClient* client, const std::string& error) {
    (void)client;
    logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ ASR连接错误: " + error, true);
    updateStatus(AsrStatus::ERROR);
    
    // 结束会话计时（标记为未完成）
    endSessionTimer(false);
}

void Asr::AsrManager::onMessage(AsrClient* client, const std::string& message) {
    (void)client;
    
    // 记录接收到的消息
    if (m_config.enableProtocolLog) {
        logMessage(m_config.logLevel, ASR_LOG_DEBUG, "📨 收到ASR消息: " + message);
    }
    
    // 如果有回调函数，转发消息
    if (m_callback) {
        m_callback->onMessage(client, message);
    }
    
    // 解析消息并更新状态
    try {
        json j = json::parse(message);
        
        // 检查是否为最终响应
        if (j.contains("result")) {
            json result = j["result"];
            if (result.contains("utterances") && result["utterances"].is_array()) {
                json utterances = result["utterances"];
                for (const auto& utterance : utterances) {
                    if (utterance.contains("definite") && utterance["definite"].get<bool>()) {
                        logMessage(m_config.logLevel, ASR_LOG_INFO, "✅ 收到最终识别结果");
                        updateStatus(AsrStatus::RECOGNIZING);
                        break;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        logMessage(m_config.logLevel, ASR_LOG_ERROR, "❌ 解析ASR消息失败: " + std::string(e.what()), true);
    }
}

} // namespace Asr 