//
// ASR 管理模块实现文件
// 
// 实现 ASR 管理模块，提供统一的 ASR 接口
// 支持多种 ASR 客户端实现（IXWebSocket、Qt、WebSocketpp 等）
//

#include "asr/asr_manager.h"
#include "asr/asr_client.h"
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

using json = nlohmann::json;

namespace Asr {

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

AsrManager::AsrManager() 
    : m_status(AsrStatus::DISCONNECTED)
    , m_callback(nullptr)
{
    // 从环境变量加载配置
    loadConfigFromEnv(m_config);
}

AsrManager::~AsrManager() {
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

// ============================================================================
// 连接控制方法
// ============================================================================

bool AsrManager::connect() {
    if (m_status == AsrStatus::CONNECTED || m_status == AsrStatus::CONNECTING) {
        std::cout << "ASR 管理器已经连接或正在连接中" << std::endl;
        return true;
    }
    
    updateStatus(AsrStatus::CONNECTING);
    
    std::cout << "🔗 正在连接 ASR 服务器..." << std::endl;
    std::cout << "📡 使用客户端类型: " << getClientTypeName(m_config.clientType) << std::endl;
    
    // 创建并初始化客户端
    if (!initializeClient()) {
        updateStatus(AsrStatus::ERROR);
        return false;
    }
    
    // 连接客户端
    if (!m_client->connect()) {
        updateStatus(AsrStatus::ERROR);
        return false;
    }
    
    updateStatus(AsrStatus::CONNECTED);
    std::cout << "✅ ASR 连接成功" << std::endl;
    
    return true;
}

void AsrManager::disconnect() {
    if (m_client) {
        m_client->disconnect();
    }
    updateStatus(AsrStatus::DISCONNECTED);
    std::cout << "🔌 ASR 连接已断开" << std::endl;
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
        std::cerr << "❌ ASR 未连接，无法发送音频" << std::endl;
        return false;
    }
    
    updateStatus(AsrStatus::RECOGNIZING);
    
    if (!m_client->sendAudio(audioData, isLast)) {
        std::cerr << "❌ 发送音频数据失败" << std::endl;
        return false;
    }
    
    std::cout << "📤 音频数据发送成功 (" << audioData.size() << " bytes)" << std::endl;
    
    if (isLast) {
        updateStatus(AsrStatus::CONNECTED);
    }
    
    return true;
}

bool AsrManager::sendAudioFile(const std::string& filePath) {
    if (!isConnected()) {
        std::cerr << "❌ ASR 未连接，无法发送音频文件" << std::endl;
        return false;
    }
    
    updateStatus(AsrStatus::RECOGNIZING);
    
    if (!m_client->sendAudioFile(filePath)) {
        std::cerr << "❌ 发送音频文件失败: " << filePath << std::endl;
        return false;
    }
    
    std::cout << "📤 音频文件发送成功: " << filePath << std::endl;
    updateStatus(AsrStatus::CONNECTED);
    
    return true;
}

bool AsrManager::startRecognition() {
    if (!isConnected()) {
        std::cerr << "❌ ASR 未连接，无法开始识别" << std::endl;
        return false;
    }
    
    // 发送完整客户端请求
    std::string response = m_client->sendFullClientRequestAndGetResponse(10000);
    
    if (!response.empty()) {
        std::cout << "✅ 识别会话已开始" << std::endl;
        updateStatus(AsrStatus::RECOGNIZING);
        return true;
    } else {
        std::cerr << "❌ 开始识别失败" << std::endl;
        return false;
    }
}

void AsrManager::stopRecognition() {
    updateStatus(AsrStatus::CONNECTED);
    std::cout << "⏹️  识别已停止" << std::endl;
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
        
        std::cout << "🔐 使用环境变量中的凭据" << std::endl;
    } else {
        std::cout << "⚠️  环境变量未设置，使用默认凭据（仅用于测试）" << std::endl;
        std::cout << "   建议设置环境变量：" << std::endl;
        std::cout << "   export ASR_APP_ID=your_app_id" << std::endl;
        std::cout << "   export ASR_ACCESS_TOKEN=your_access_token" << std::endl;
        std::cout << "   export ASR_SECRET_KEY=your_secret_key" << std::endl;
        std::cout << "   或者使用 VOLC_ 前缀：" << std::endl;
        std::cout << "   export VOLC_APP_ID=your_app_id" << std::endl;
        std::cout << "   export VOLC_ACCESS_TOKEN=your_access_token" << std::endl;
        std::cout << "   export VOLC_SECRET_KEY=your_secret_key" << std::endl;
        
        // 使用默认凭据（仅用于测试）
        config.appId = "8388344882";
        config.accessToken = "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3";
        config.secretKey = "oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e";
        config.isValid = true;
    }
    
    // 脱敏显示凭据信息
    std::string maskedToken = config.accessToken;
    if (maskedToken.length() > 8) {
        maskedToken = maskedToken.substr(0, 4) + "****" + maskedToken.substr(maskedToken.length() - 4);
    }
    
    std::string maskedSecret = config.secretKey;
    if (maskedSecret.length() > 8) {
        maskedSecret = maskedSecret.substr(0, 4) + "****" + maskedSecret.substr(maskedSecret.length() - 4);
    } else {
        maskedSecret = "****";
    }
    
    std::cout << "📋 凭据信息:" << std::endl;
    std::cout << "   - App ID: " << config.appId << std::endl;
    std::cout << "   - Access Token: " << maskedToken << std::endl;
    std::cout << "   - Secret Key: " << maskedSecret << std::endl;
    
    return config.isValid;
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
    // 创建客户端实例
    m_client = createClient(m_config.clientType);
    if (!m_client) {
        std::cerr << "❌ 创建客户端实例失败" << std::endl;
        return false;
    }
    
    // 设置客户端配置
    m_client->setAppId(m_config.appId);
    m_client->setToken(m_config.accessToken);
    m_client->setSecretKey(m_config.secretKey);
    m_client->setAudioFormat(m_config.format, m_config.channels, m_config.sampleRate, m_config.bits);
    m_client->setUid(m_config.uid);
    m_client->setLanguage(m_config.language);
    m_client->setResultType(m_config.resultType);
    m_client->setStreaming(m_config.streaming);
    m_client->setSegDuration(m_config.segDuration);
    
    // 设置回调
    if (m_callback) {
        m_client->setCallback(m_callback);
    }
    
    std::cout << "✅ 客户端初始化成功" << std::endl;
    return true;
}

void AsrManager::updateStatus(AsrStatus status) {
    m_status = status;
    std::cout << "📊 ASR 状态更新: " << getStatusName(status) << std::endl;
}

// ============================================================================
// 音频文件解析和识别方法
// ============================================================================

AsrManager::AudioFileInfo AsrManager::parseAudioFile(const std::string& filePath) {
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
    std::cout << "=== 火山引擎 ASR 自动化识别流程 ===" << std::endl;
    std::cout << "🎯 目标文件: " << filePath << std::endl;
    
    // 检查音频文件是否存在
    std::ifstream testFile(filePath);
    if (!testFile.good()) {
        std::cerr << "❌ 音频文件不存在: " << filePath << std::endl;
        return false;
    }
    testFile.close();
    
    // 步骤1: 自动解析音频文件头
    std::cout << "=== 步骤1: 音频文件头解析 ===" << std::endl;
    AudioFileInfo audioInfo = parseAudioFile(filePath);
    
    if (!audioInfo.isValid) {
        std::cerr << "❌ 音频文件解析失败" << std::endl;
        return false;
    }
    
    std::cout << "" << std::endl;
    
    // 步骤2: 自动配置ASR参数
    std::cout << "=== 步骤2: 自动配置ASR参数 ===" << std::endl;
    m_config.sampleRate = audioInfo.sampleRate;
    m_config.channels = audioInfo.channels;
    m_config.format = audioInfo.format;
    m_config.segDuration = 100; // 强制100ms分包
    
    // 步骤3: 连接ASR服务
    std::cout << "=== 步骤3: 连接ASR服务 ===" << std::endl;
    if (!connect()) {
        std::cerr << "❌ 连接ASR服务失败" << std::endl;
        return false;
    }
    
    // 步骤4: 启动识别
    std::cout << "=== 步骤4: 启动识别 ===" << std::endl;
    if (!startRecognition()) {
        std::cerr << "❌ 启动识别失败" << std::endl;
        return false;
    }
    
    // 步骤5: 读取音频数据并分包
    std::cout << "=== 步骤5: 读取音频数据并分包 ===" << std::endl;
    
    // 读取完整的音频文件（与Python版本保持一致）
    std::ifstream audioFile(filePath, std::ios::binary);
    if (!audioFile.is_open()) {
        std::cerr << "❌ 无法打开音频文件" << std::endl;
        return false;
    }
    
    // 读取完整文件内容
    std::vector<uint8_t> audioData((std::istreambuf_iterator<char>(audioFile)),
                                   std::istreambuf_iterator<char>());
    audioFile.close();
    
    if (audioData.empty()) {
        std::cerr << "❌ 音频文件为空" << std::endl;
        return false;
    }
    
    std::cout << "✅ 成功读取音频文件: " << audioData.size() << " bytes" << std::endl;
    
    // 计算分段大小 - 按照100ms帧长计算
    size_t bytesPerSecond = audioInfo.channels * (audioInfo.bitsPerSample / 8) * audioInfo.sampleRate;
    size_t segmentSize = bytesPerSecond * m_config.segDuration / 1000;
    
    std::cout << "📊 音频分段信息:" << std::endl;
    std::cout << "  - 每秒字节数: " << bytesPerSecond << " bytes/s" << std::endl;
    std::cout << "  - 分段大小: " << segmentSize << " bytes (100ms)" << std::endl;
    std::cout << "  - 预计分段数: " << (audioData.size() + segmentSize - 1) / segmentSize << std::endl;
    
    // 创建音频分段器并分包
    AudioSegmenter segmenter(audioData, segmentSize);
    m_audioPackets.clear();
    std::vector<uint8_t> chunk;
    bool isLast;
    while (segmenter.getNextChunk(chunk, isLast)) {
        m_audioPackets.push_back(chunk);
    }
    m_audioSendIndex = 0;
    std::cout << "✅ 音频分包完成: " << m_audioPackets.size() << " 个包" << std::endl;
    
    // 步骤6: 开始流式发送音频包
    std::cout << "=== 步骤6: 开始流式发送音频包 ===" << std::endl;
    std::cout << "🚀 开始流式发送，总共 " << m_audioPackets.size() << " 个音频包" << std::endl;
    
    // 发送第一个音频包
    if (!m_audioPackets.empty()) {
        std::cout << "📤 发送第一个音频包..." << std::endl;
        if (!sendNextAudioPacket()) {
            std::cerr << "❌ 第一个音频包发送失败" << std::endl;
            return false;
        }
        std::cout << "✅ 第一个音频包发送成功" << std::endl;
    }
    
    // 流式发送循环：等待响应->发送下一包
    while (m_audioSendIndex < m_audioPackets.size()) {
        // 等待当前包的Full server response
        auto startTime = std::chrono::high_resolution_clock::now();
        auto timeout = std::chrono::seconds(10); // 10秒超时
        
        while (std::chrono::high_resolution_clock::now() - startTime < timeout) {
            // 检查是否收到服务器响应（通过m_audioSendIndex判断）
            if (m_audioSendIndex > 0) {
                std::cout << "✅ 收到服务器响应" << std::endl;
                break;
            }
            
            // 检查连接状态
            if (!m_client->isConnected()) {
                std::cerr << "❌ 连接已断开，终止流式发送" << std::endl;
                return false;
            }
            
            // 检查是否收到错误响应
            auto lastError = m_client->getLastError();
            if (!lastError.isSuccess()) {
                std::cerr << "❌ 服务器返回错误: " << lastError.message << std::endl;
                std::cerr << "🔍 错误码: " << lastError.code << std::endl;
                std::cerr << "📝 错误详情: " << lastError.details << std::endl;
                return false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 如果超时没有收到响应
        if (m_audioSendIndex == 0) {
            std::cerr << "❌ 等待服务器响应超时，终止流式发送" << std::endl;
            return false;
        }
        
        // 发送下一个音频包
        if (m_audioSendIndex < m_audioPackets.size()) {
            std::cout << "📤 发送下一个音频包..." << std::endl;
            if (!sendNextAudioPacket()) {
                std::cerr << "❌ 发送音频包失败，终止流式发送" << std::endl;
                return false;
            }
            std::cout << "✅ 音频包发送成功，剩余 " << (m_audioPackets.size() - m_audioSendIndex) << " 个包" << std::endl;
        }
    }
    
    std::cout << "✅ 所有音频包发送完成" << std::endl;
    
    // 等待最终识别结果（可选）
    if (waitForFinal) {
        std::cout << "=== 步骤7: 等待最终识别结果 ===" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : 2000));
    }
    
    std::cout << "=== 识别流程结束 ===" << std::endl;
    return true;
}

bool AsrManager::sendNextAudioPacket() {
    if (m_audioSendIndex >= m_audioPackets.size()) {
        std::cerr << "❌ 没有更多音频包可发送" << std::endl;
        return false;
    }
    bool isLast = (m_audioSendIndex == m_audioPackets.size() - 1);
    int seq = 2 + m_audioSendIndex;
    int sendSeq = isLast ? -seq : seq;
    if (!m_client->sendAudio(m_audioPackets[m_audioSendIndex], sendSeq)) {
        std::cerr << "❌ 发送音频包失败 seq=" << sendSeq << std::endl;
        return false;
    }
    std::cout << "📤 已发送音频包 seq=" << sendSeq << std::endl;
    m_audioSendIndex++;
    return true;
}

void AsrManager::onAudioAck() {
    // 收到服务器响应/ACK后，立即发送下一个音频包
    std::cout << "📨 收到音频包ACK，准备发送下一个包..." << std::endl;
    
    // 检查是否还有更多音频包需要发送
    if (m_audioSendIndex < m_audioPackets.size()) {
        std::cout << "⏳ 还有 " << (m_audioPackets.size() - m_audioSendIndex) << " 个音频包待发送" << std::endl;
        
        // 立即发送下一个音频包
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
    return m_audioSendIndex < m_audioPackets.size();
}

// ============================================================================
// 私有音频文件解析方法
// ============================================================================

AsrManager::AudioFileInfo AsrManager::parseWavFile(const std::string& filePath, const std::vector<uint8_t>& header) {
    (void)header; // 避免未使用参数警告
    AudioFileInfo info;
    info.format = "wav";
    info.codec = "raw";
    
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
    std::cout << "  - 格式: " << info.format << std::endl;
    std::cout << "  - 采样率: " << info.sampleRate << " Hz" << std::endl;
    std::cout << "  - 位深度: " << info.bitsPerSample << " bits" << std::endl;
    std::cout << "  - 声道数: " << info.channels << std::endl;
    std::cout << "  - 编解码器: " << info.codec << std::endl;
    std::cout << "  - 音频数据大小: " << info.dataSize << " bytes" << std::endl;
    std::cout << "  - 音频时长: " << info.duration << " 秒" << std::endl;
    
    return info;
}

AsrManager::AudioFileInfo AsrManager::parseMp3File(const std::string& filePath, const std::vector<uint8_t>& header) {
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

AsrManager::AudioFileInfo AsrManager::parsePcmFile(const std::string& filePath, const std::vector<uint8_t>& header) {
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

} // namespace Asr 