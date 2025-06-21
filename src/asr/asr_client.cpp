//
// 基于 IXWebSocket 的 ASR 客户端实现
// 参考 asr_ixwebsocket_example.cpp 和 asr_qt_client 的工作流程
// 完全兼容火山引擎 ASR WebSocket 协议，支持流式音频识别
//

#include "asr/asr_client.h"
#include "asr/asr_log_utils.h"
#include "asr/asr_manager.h"
#include "asr/asr_debug_config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <zlib.h>
#include <mutex>
#include <condition_variable>

using json = nlohmann::json;

namespace Asr {

// ============================================================================
// 日志工具函数
// ============================================================================

// 使用公共头文件中的函数，这里不再重复定义

// ============================================================================
// AsrClient 类实现
// ============================================================================

AsrClient::AsrClient() 
    : m_connected(false)
    , m_authType(AuthType::TOKEN)
    , m_format("raw")
    , m_sampleRate(16000)
    , m_bits(16)
    , m_channels(1)
    , m_codec("raw")
    , m_seq(1)
    , m_uid("test")
    , m_language("zh-CN")
    , m_resultType("full")
    , m_streaming(true)
    , m_segDuration(100)
    , m_callback(nullptr)
    , m_readyForAudio(false)
    , m_finalResponseReceived(false)
{
    // 生成唯一的请求ID
    m_reqId = generateUuid();
    
    // 设置 WebSocket 回调
    m_webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        this->handleMessage(msg);
    });
    
    // 设置连接参数
    m_webSocket.setUrl("wss://openspeech.bytedance.com/api/v3/sauc/bigmodel");
    m_webSocket.setHandshakeTimeout(30); // 30秒握手超时
    m_webSocket.setMaxWaitBetweenReconnectionRetries(1000); // 1秒重连间隔
    
    // 注意：凭据将通过 setAppId, setToken, setSecretKey 方法设置
    // 不在这里自动从环境变量加载，避免与外部配置冲突
}

AsrClient::~AsrClient() {
    disconnect();
}

// ============================================================================
// 配置方法
// ============================================================================

void AsrClient::setAppId(const std::string& appId) {
    m_appId = appId;
    updateHeaders();
}

void AsrClient::setToken(const std::string& token) {
    m_accessToken = token;
    updateHeaders();
}

void AsrClient::setSecretKey(const std::string& secretKey) {
    m_secretKey = secretKey;
    updateHeaders();
}

void AsrClient::setAuthType(AuthType authType) {
    m_authType = authType;
}

void AsrClient::setAudioFormat(const std::string& format, int channels, int sampleRate, int bits) {
    m_format = format;
    m_channels = channels;
    m_sampleRate = sampleRate;
    m_bits = bits;
}

void AsrClient::setCluster(const std::string& cluster) {
    m_cluster = cluster;
}

void AsrClient::setCallback(AsrCallback* callback) {
    m_callback = callback;
}

void AsrClient::setUid(const std::string& uid) {
    m_uid = uid;
}

void AsrClient::setLanguage(const std::string& language) {
    m_language = language;
}

void AsrClient::setResultType(const std::string& resultType) {
    m_resultType = resultType;
}

void AsrClient::setStreaming(bool streaming) {
    m_streaming = streaming;
}

void AsrClient::setSegDuration(int duration) {
    m_segDuration = duration;
}

// ============================================================================
// 连接控制方法
// ============================================================================

bool AsrClient::connect() {
    if (m_connected) {
        logWithTimestamp("ASR client is already connected");
        return true;
    }
    
    logWithTimestamp("🔗 正在连接 WebSocket...");
    logWithTimestamp("📡 目标URL: " + m_webSocket.getUrl());
    
    // 打印发送的 Header 信息
    logWithTimestamp("=== 发送的 HTTP Header ===");
    logWithTimestamp("X-Api-Resource-Id: volc.bigasr.sauc.duration");
    logWithTimestamp("X-Api-Access-Key: " + (m_accessToken.length() > 8 ? m_accessToken.substr(0, 4) + "****" + m_accessToken.substr(m_accessToken.length() - 4) : "****"));
    logWithTimestamp("X-Api-App-Key: " + m_appId);
    logWithTimestamp("X-Api-Request-Id: " + m_reqId);
    
    m_webSocket.start();
    
    // 等待连接建立
    int timeout = 0;
    while (!m_connected && timeout < 300) { // 30秒超时
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout++;
        
        // 每5秒打印一次等待信息
        if (timeout % 50 == 0) {
            logWithTimestamp("⏳ 等待连接建立... (" + std::to_string(timeout / 10) + "秒)");
        }
    }
    
    return m_connected;
}

void AsrClient::disconnect() {
    if (m_connected) {
        m_webSocket.stop();
        m_connected = false;
    }
}

bool AsrClient::isConnected() const {
    return m_connected;
}

// ============================================================================
// 音频发送方法
// ============================================================================

bool AsrClient::sendAudio(const std::vector<uint8_t>& audioData, int32_t sequence) {
    if (!m_connected) {
        logErrorWithTimestamp("❌ 未连接");
        return false;
    }
    bool isLast = (sequence < 0);
    
    // 1. Header (4字节) - 不包含序列号
    std::vector<uint8_t> packet = generateHeader(
        AUDIO_ONLY_REQUEST,
        isLast ? NEG_WITH_SEQUENCE : POS_SEQUENCE,
        RAW_BYTES,
        GZIP_COMPRESSION,
        0x00
    );
    
    // 2. 序列号 (4字节，大端序，有符号)
    for (int i = 3; i >= 0; --i) {
        packet.push_back((sequence >> (i * 8)) & 0xFF);
    }
    
    // 3. Gzip 压缩音频数据
    std::vector<uint8_t> payload = gzipCompress(audioData);
    
    // 4. Payload Size (4字节，大端序)
    uint32_t payloadSize = payload.size();
    for (int i = 3; i >= 0; --i)
        packet.push_back((payloadSize >> (i * 8)) & 0xFF);
    
    // 5. Payload
    packet.insert(packet.end(), payload.begin(), payload.end());

    // ========== 协议包详细打印 ==========
#if ASR_ENABLE_PROTOCOL_LOG
    std::stringstream debugInfo;
    debugInfo << "==== 发送音频包 seq=" << sequence << " ====" << std::endl;
    debugInfo << "HEADER: " << hexString(packet) << std::endl;
    debugInfo << "PAYLOAD_LEN: " << hexString(std::vector<uint8_t>(packet.begin() + 8, packet.begin() + 12)) << std::endl;
    debugInfo << "PAYLOAD_HEAD: ";
    for (size_t i = 12; i < 32 && i < packet.size(); ++i) {
        debugInfo << hexString(std::vector<uint8_t>(packet.begin() + i, packet.begin() + i + 1)) << " ";
    }
    debugInfo << std::endl << "================";
    logWithTimestamp(debugInfo.str());
#endif

    // 实际发送
    auto sendInfo = m_webSocket.sendBinary(packet);
    return sendInfo.success;
}

bool AsrClient::sendAudioFile(const std::vector<uint8_t>& audioData, bool isLast, int32_t sequence) {
    if (audioData.empty()) {
        logErrorWithTimestamp("❌ 发送音频数据为空");
        return false;
    }
    return sendAudio(audioData, isLast ? -sequence : sequence);
}

// 保留原有接口用于兼容（可直接返回false或加警告）
bool AsrClient::sendAudioFile(const std::string& filePath) {
    (void)filePath; // 消除未使用参数警告
    logErrorWithTimestamp("❌ sendAudioFile(const std::string&) 已废弃，请使用分包后逐包发送");
    return false;
}

// ============================================================================
// 请求发送方法
// ============================================================================

bool AsrClient::sendFullClientRequestAndWaitResponse(int timeoutMs, std::string* response) {
    if (!m_connected) {
        logErrorWithTimestamp("❌ 未连接");
        return false;
    }
    
    // 构造请求 JSON
    json requestParams = constructRequest();
    std::string jsonStr = requestParams.dump();
#if ASR_ENABLE_PROTOCOL_LOG
    logWithTimestamp("📤 JSON_STRING: " + jsonStr);
    logWithTimestamp("📤 JSON原始长度: " + std::to_string(jsonStr.length()) + " bytes");
#endif
    
    // Gzip 压缩
    std::vector<uint8_t> payload = gzipCompress(jsonStr);
    if (payload.empty()) {
        logErrorWithTimestamp("❌ GZIP 压缩失败");
        return false;
    }
#if ASR_ENABLE_PROTOCOL_LOG
    logWithTimestamp("📤 gzip压缩后长度: " + std::to_string(payload.size()) + " bytes");
#endif
    
    // 1. Header (4字节) - 描述消息类型、序列化方式、压缩格式等
    std::vector<uint8_t> packet = generateHeader(
        FULL_CLIENT_REQUEST,
        POS_SEQUENCE,
        JSON_SERIALIZATION,
        GZIP_COMPRESSION,
        0x00
    );
    
    // 2. 序列号 (4字节，大端序，有符号)
    for (int i = 3; i >= 0; --i) {
        packet.push_back((m_seq >> (i * 8)) & 0xFF);
    }
    
    // 3. Payload Size (4字节，大端序) - payload的长度
    uint32_t payloadSize = payload.size();
    for (int i = 3; i >= 0; --i)
        packet.push_back((payloadSize >> (i * 8)) & 0xFF);
    
    // 4. Payload - 具体负载内容（压缩后的JSON数据）
    packet.insert(packet.end(), payload.begin(), payload.end());
    
    // 调试输出
#if ASR_ENABLE_PROTOCOL_LOG
    logWithTimestamp("📤 HEADER: " + hexString(packet));
    logWithTimestamp("📤 PAYLOAD_LEN: " + hexString(std::vector<uint8_t>(packet.begin() + 8, packet.begin() + 12)));
#endif

    // 4. 发送
    std::string binaryData(reinterpret_cast<const char*>(packet.data()), packet.size());
    m_webSocket.sendBinary(binaryData);
    
    return waitForResponse(timeoutMs, response);
}

std::string AsrClient::sendFullClientRequestAndGetResponse(int timeoutMs) {
    std::string response;
    sendFullClientRequestAndWaitResponse(timeoutMs, &response);
    return response;
}

// ============================================================================
// 获取状态信息
// ============================================================================

std::string AsrClient::getLogId() const {
    return m_logId;
}

std::map<std::string, std::string> AsrClient::getResponseHeaders() const {
    return m_responseHeaders;
}

std::string AsrClient::getFullClientRequestJson() const {
    json req = constructRequest();
    return req.dump(2); // 格式化输出
}

// ============================================================================
// 静态方法
// ============================================================================

AsrClient::Credentials AsrClient::getCredentialsFromEnv() {
    Credentials creds;
    
    // 从环境变量获取凭据
    const char* appId = std::getenv("VOLC_APP_ID");
    const char* accessToken = std::getenv("VOLC_ACCESS_TOKEN");
    const char* secretKey = std::getenv("VOLC_SECRET_KEY");
    
    if (appId && accessToken) {
        creds.appId = appId;
        creds.accessToken = accessToken;
        creds.secretKey = secretKey ? secretKey : "";
        creds.isValid = true;
        
        logWithTimestamp("✅ 使用环境变量中的凭据");
    } else {
        // 使用默认凭据（仅用于测试）
        creds.appId = "8388344882";
        creds.accessToken = "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3";
        creds.secretKey = "";
        creds.isValid = true;
        
        logWithTimestamp("⚠️  使用默认凭据（仅用于测试）");
        logWithTimestamp("💡 建议设置环境变量：VOLC_APP_ID, VOLC_ACCESS_TOKEN, VOLC_SECRET_KEY");
    }
    
    return creds;
}

// ============================================================================
// 私有方法
// ============================================================================

void AsrClient::updateHeaders() {
    ix::WebSocketHttpHeaders headers;
    headers["User-Agent"] = "ASR-Client/1.0";
    headers["Accept"] = "*/*";
    headers["Accept-Encoding"] = "gzip, deflate";
    headers["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8";
    headers["Host"] = "openspeech.bytedance.com";
    
    // 火山引擎 ASR 认证头部
    headers["X-Api-Resource-Id"] = "volc.bigasr.sauc.duration";
    headers["X-Api-Access-Key"] = m_accessToken;
    headers["X-Api-App-Key"] = m_appId;
    headers["X-Api-Request-Id"] = m_reqId;
    
    m_webSocket.setExtraHeaders(headers);
}

void AsrClient::handleMessage(const ix::WebSocketMessagePtr& msg) {
    switch (msg->type) {
        case ix::WebSocketMessageType::Message: {
            if (msg->binary) {
                // 处理二进制消息
                if (msg->str.length() >= 2) {
                    uint8_t messageType = (msg->str[1] & 0xF0) >> 4;
                    uint8_t flags = msg->str[1] & 0x0F;

                    // message type: b1001 (9), flags: b0011 (3)
                    if (messageType == 0x09 && flags == 0x03) {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_finalResponseReceived = true;
                        logWithTimestamp("🎯 收到最终结果响应 (Full Server Response)");
                        m_cv.notify_one(); // 通知等待的线程
                    }
                }
#if ASR_ENABLE_PROTOCOL_LOG
                logWithTimestamp("📨 收到二进制消息，大小: " + std::to_string(msg->wireSize) + " 字节");
                
                // 打印前20字节的十六进制
                logWithTimestamp("🔍 原始数据(前20字节): " + hexString(std::vector<uint8_t>(msg->str.begin(), msg->str.begin() + std::min(size_t(20), msg->str.size()))));
#endif
                
                // 解析二进制协议
                std::string jsonResponse = parseBinaryResponse(msg->str);
                if (!jsonResponse.empty()) {
#if ASR_ENABLE_PROTOCOL_LOG
                    logWithTimestamp("🧹 解析后的响应: " + jsonResponse);
#endif
                    
                    // 解析错误信息
                    if (hasError(jsonResponse)) {
                        m_lastError = parseErrorResponse(jsonResponse);
                        logErrorWithTimestamp("❌ 检测到错误: " + m_lastError.getErrorDescription());
#if ASR_ENABLE_PROTOCOL_LOG
                        logWithTimestamp("🔍 错误码: " + std::to_string(m_lastError.code));
                        logWithTimestamp("📝 错误详情: " + m_lastError.message);
#endif
                        
                        if (m_callback) {
                            m_callback->onError(this, m_lastError.message);
                        }
                        return;
                    }
                    
                    // 尝试解析 JSON 获取 log_id 和检查最终响应
                    try {
                        json j = json::parse(jsonResponse);
                        if (j.contains("result") && j["result"].contains("additions") && 
                            j["result"]["additions"].contains("log_id")) {
                            m_logId = j["result"]["additions"]["log_id"];
#if ASR_ENABLE_PROTOCOL_LOG
                            logWithTimestamp("🔍 提取到 log_id: " + m_logId);
#endif
                        }
                        
                        // 检查是否为最终响应
                        bool isFinalResponse = false;
                        if (j.contains("result")) {
                            json result = j["result"];
                            
                            // 检查utterances中的definite字段
                            if (result.contains("utterances") && result["utterances"].is_array()) {
                                for (const auto& utterance : result["utterances"]) {
                                    if (utterance.contains("definite") && utterance["definite"].get<bool>()) {
                                        isFinalResponse = true;
                                        break;
                                    }
                                }
                            }
                            
                            // 检查是否有is_final字段
                            if (result.contains("is_final") && result["is_final"].get<bool>()) {
                                isFinalResponse = true;
                            }
                        }
                        
                        // 如果检测到最终响应，设置标志
                        if (isFinalResponse) {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            m_finalResponseReceived = true;
                            logWithTimestamp("🎯 检测到最终识别结果");
                            m_cv.notify_one(); // 通知等待的线程
                        }
                        
                    } catch (const std::exception& e) {
                        // 不是 JSON 格式，忽略
                    }
                    
                    if (m_callback) {
                        m_callback->onMessage(this, jsonResponse);
                    }
                    
                    // 检查是否为Full Server Response或ACK
                    if (!jsonResponse.empty()) {
                        if (jsonResponse.find("\"result\"") != std::string::npos ||
                            jsonResponse.find("\"code\":0") != std::string::npos ||
                            jsonResponse.find("\"status\":0") != std::string::npos) {
                            m_readyForAudio = true;
#if ASR_ENABLE_PROTOCOL_LOG
                            logWithTimestamp("✅ 识别会话已开始");
#endif
                        }
                    }
                }
            } else {
                // 处理文本消息
#if ASR_ENABLE_PROTOCOL_LOG
                logWithTimestamp("📨 收到文本消息: " + msg->str);
#endif
                
                // 解析错误信息
                if (hasError(msg->str)) {
                    m_lastError = parseErrorResponse(msg->str);
                    logErrorWithTimestamp("❌ 检测到错误: " + m_lastError.getErrorDescription());
#if ASR_ENABLE_PROTOCOL_LOG
                    logWithTimestamp("🔍 错误码: " + std::to_string(m_lastError.code));
                    logWithTimestamp("📝 错误详情: " + m_lastError.message);
#endif
                    
                    if (m_callback) {
                        m_callback->onError(this, m_lastError.message);
                    }
                    return;
                }
                
                // 尝试解析 JSON 获取 log_id 和检查最终响应
                try {
                    json j = json::parse(msg->str);
                    if (j.contains("result") && j["result"].contains("additions") && 
                        j["result"]["additions"].contains("log_id")) {
                        m_logId = j["result"]["additions"]["log_id"];
#if ASR_ENABLE_PROTOCOL_LOG
                        logWithTimestamp("🔍 提取到 log_id: " + m_logId);
#endif
                    }
                    
                    // 检查是否为最终响应
                    bool isFinalResponse = false;
                    if (j.contains("result")) {
                        json result = j["result"];
                        
                        // 检查utterances中的definite字段
                        if (result.contains("utterances") && result["utterances"].is_array()) {
                            for (const auto& utterance : result["utterances"]) {
                                if (utterance.contains("definite") && utterance["definite"].get<bool>()) {
                                    isFinalResponse = true;
                                    break;
                                }
                            }
                        }
                        
                        // 检查是否有is_final字段
                        if (result.contains("is_final") && result["is_final"].get<bool>()) {
                            isFinalResponse = true;
                        }
                    }
                    
                    // 如果检测到最终响应，设置标志
                    if (isFinalResponse) {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_finalResponseReceived = true;
                        logWithTimestamp("🎯 检测到最终识别结果");
                        m_cv.notify_one(); // 通知等待的线程
                    }
                    
                } catch (const std::exception& e) {
                    // 不是 JSON 格式，忽略
                }
                
                if (m_callback) {
                    m_callback->onMessage(this, msg->str);
                }
                
                // 检查是否为Full Server Response或ACK
                if (!msg->str.empty()) {
                    if (msg->str.find("\"result\"") != std::string::npos ||
                        msg->str.find("\"code\":0") != std::string::npos ||
                        msg->str.find("\"status\":0") != std::string::npos) {
                        m_readyForAudio = true;
#if ASR_ENABLE_PROTOCOL_LOG
                        logWithTimestamp("✅ 识别会话已开始");
#endif
                    }
                }
            }
            break;
        }
        case ix::WebSocketMessageType::Open: {
            logWithTimestamp("✅ WebSocket 连接已建立");
            m_connected = true;
            
            // 获取响应头
            const auto& headers = msg->openInfo.headers;
            for (const auto& header : headers) {
                m_responseHeaders[header.first] = header.second;
                logWithTimestamp("📋 响应头: " + header.first + ": " + header.second);
            }
            
            // 特别关注 X-Tt-Logid
            if (headers.find("X-Tt-Logid") != headers.end()) {
                logWithTimestamp("🎯 成功获取 X-Tt-Logid: " + headers.at("X-Tt-Logid"));
            } else {
                logWithTimestamp("⚠️  未找到 X-Tt-Logid");
            }
            
            if (m_callback) {
                m_callback->onOpen(this);
            }
            break;
        }
        case ix::WebSocketMessageType::Close: {
            logWithTimestamp("🔌 WebSocket 连接已关闭 (code: " + std::to_string(msg->closeInfo.code) + ", reason: " + msg->closeInfo.reason + ")");
            m_connected = false;
            
            if (m_callback) {
                m_callback->onClose(this);
            }
            break;
        }
        case ix::WebSocketMessageType::Error: {
            logErrorWithTimestamp("❌ WebSocket 错误: " + msg->errorInfo.reason);
            logErrorWithTimestamp("🔍 错误详情: HTTP状态=" + std::to_string(msg->errorInfo.http_status) 
                     + ", 重试次数=" + std::to_string(msg->errorInfo.retries) 
                     + ", 等待时间=" + std::to_string(msg->errorInfo.wait_time) + "ms");
            
            if (m_callback) {
                m_callback->onError(this, msg->errorInfo.reason);
            }
            break;
        }
        case ix::WebSocketMessageType::Fragment: {
            logWithTimestamp("📦 收到消息片段");
            break;
        }
        case ix::WebSocketMessageType::Ping: {
            logWithTimestamp("🏓 收到 Ping");
            break;
        }
        case ix::WebSocketMessageType::Pong: {
            logWithTimestamp("🏓 收到 Pong");
            break;
        }
    }
}

json AsrClient::constructRequest() const {
    json req = {
        {"user", {
            {"uid", m_uid}
        }},
        {"audio", {
            {"format", m_format},
            {"sample_rate", m_sampleRate},
            {"bits", m_bits},
            {"channel", m_channels},
            {"codec", m_codec}
        }},
        {"request", {
            {"model_name", "bigmodel"},
            {"enable_punc", true}
        }}
    };
    return req;
}

std::vector<uint8_t> AsrClient::generateHeader(
    uint8_t messageType,
    uint8_t messageTypeSpecificFlags,
    uint8_t serialMethod,
    uint8_t compressionType,
    uint8_t reservedData
) {
    std::vector<uint8_t> header;
    uint8_t headerSize = 2; // 8字节 = 2个4字节块
    header.push_back((PROTOCOL_VERSION << 4) | headerSize);
    header.push_back((messageType << 4) | messageTypeSpecificFlags);
    header.push_back((serialMethod << 4) | compressionType);
    header.push_back(reservedData);
    
    return header;
}

std::vector<uint8_t> AsrClient::generateBeforePayload(int32_t sequence) {
    std::vector<uint8_t> beforePayload;
    for (int i = 3; i >= 0; --i) {
        beforePayload.push_back((sequence >> (i * 8)) & 0xFF);
    }
    return beforePayload;
}

std::vector<uint8_t> AsrClient::gzipCompress(const std::string& data) {
    std::vector<uint8_t> compressed;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        logErrorWithTimestamp("❌ GZIP 压缩初始化失败");
        return compressed;
    }
    
    strm.avail_in = data.size();
    strm.next_in = (Bytef*)data.data();
    
    compressed.resize(data.size() + 1024); // 预留空间
    strm.avail_out = compressed.size();
    strm.next_out = compressed.data();
    
    int ret = deflate(&strm, Z_FINISH);
    if (ret == Z_STREAM_END) {
        compressed.resize(compressed.size() - strm.avail_out);
    } else {
        logErrorWithTimestamp("❌ GZIP 压缩失败");
        compressed.clear();
    }
    
    deflateEnd(&strm);
    return compressed;
}

std::vector<uint8_t> AsrClient::gzipCompress(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> compressed;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        logErrorWithTimestamp("❌ GZIP 压缩初始化失败");
        return compressed;
    }
    
    strm.avail_in = data.size();
    strm.next_in = (Bytef*)data.data();
    
    compressed.resize(data.size() + 1024); // 预留空间
    strm.avail_out = compressed.size();
    strm.next_out = compressed.data();
    
    int ret = deflate(&strm, Z_FINISH);
    if (ret == Z_STREAM_END) {
        compressed.resize(compressed.size() - strm.avail_out);
    } else {
        logErrorWithTimestamp("❌ GZIP 压缩失败");
        compressed.clear();
    }
    
    deflateEnd(&strm);
    return compressed;
}

bool AsrClient::waitForResponse(int timeoutMs, std::string* response) {
    // 等待WebSocket消息响应
    auto startTime = std::chrono::high_resolution_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);
    
    // 等待直到收到响应或超时
    while (std::chrono::high_resolution_clock::now() - startTime < timeout) {
        // 检查是否已经收到响应
        if (m_readyForAudio) {
            if (response) {
                *response = "Session started"; // 表示会话已开始
            }
            return true;
        }
        
        // 短暂等待，避免过度占用CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 超时
    if (response) {
        *response = "";
    }
    return false;
}

std::string AsrClient::generateUuid() {
    // 简单的UUID生成（实际项目中建议使用更安全的实现）
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    std::stringstream ss;
    ss << "req-" << std::hex << millis << "-" << std::rand();
    return ss.str();
}

// ============================================================================
// 错误处理方法实现
// ============================================================================

AsrError AsrClient::getLastError() const {
    return m_lastError;
}

bool AsrClient::hasReceivedFinalResponse() const {
    return m_finalResponseReceived;
}

std::mutex& AsrClient::getMutex() {
    return m_mutex;
}

std::condition_variable& AsrClient::getConditionVariable() {
    return m_cv;
}

AsrError AsrClient::parseErrorResponse(const std::string& response) {
    AsrError error;
    
    try {
        json j = json::parse(response);
        
        // 检查是否有错误字段
        if (j.contains("error")) {
            error.message = j["error"];
            error.details = response;
            
            // 尝试从错误消息中提取错误码
            if (j.contains("code")) {
                error.code = j["code"];
            } else {
                // 如果没有明确的错误码，根据错误消息判断
                if (error.message.find("decode ws request failed") != std::string::npos) {
                    error.code = ERROR_INVALID_PARAMS;
                } else if (error.message.find("timeout") != std::string::npos) {
                    error.code = ERROR_TIMEOUT;
                } else if (error.message.find("unauthorized") != std::string::npos) {
                    error.code = ERROR_UNAUTHORIZED;
                } else {
                    error.code = ERROR_UNKNOWN;
                }
            }
        } else {
            // 没有错误字段，设置成功状态
            error.code = ERROR_SUCCESS;
            error.message = "Success";
            error.details = response;
        }
    } catch (const std::exception& e) {
        // JSON 解析失败，设置错误状态
        error.code = ERROR_INVALID_PARAMS;
        error.message = "响应格式错误";
        error.details = response;
    }
    
    return error;
}

bool AsrClient::hasError(const std::string& response) {
    try {
        json j = json::parse(response);
        return j.contains("error") || (j.contains("code") && j["code"] != ERROR_SUCCESS);
    } catch (const std::exception& e) {
        return true;
    }
}

std::string AsrClient::getErrorDescription(uint32_t errorCode) {
    switch (errorCode) {
        case ERROR_SUCCESS:
            return "成功";
        case ERROR_INVALID_PARAMS:
            return "请求参数无效，请求参数缺失必需字段 / 字段值无效 / 重复请求";
        case ERROR_UNAUTHORIZED:
            return "未授权，认证失败";
        case ERROR_FORBIDDEN:
            return "禁止访问，权限不足";
        case ERROR_NOT_FOUND:
            return "资源不存在";
        case ERROR_TIMEOUT:
            return "等包超时";
        case ERROR_SERVER_ERROR:
            return "服务器内部错误";
        case ERROR_SERVICE_UNAVAILABLE:
            return "服务不可用";
        case ERROR_QUOTA_EXCEEDED:
            return "配额超限";
        case ERROR_RATE_LIMITED:
            return "请求频率超限";
        default:
            return "未知错误";
    }
}

std::string AsrClient::parseBinaryResponse(const std::string& binaryData) {
    if (binaryData.size() < 4) {
        logErrorWithTimestamp("❌ 二进制数据太小，无法解析协议头");
        return "";
    }
    
    // 解析协议头
#if ASR_ENABLE_PROTOCOL_LOG
    uint8_t protocolVersion = (binaryData[0] >> 4) & 0x0F;
    uint8_t headerSize = binaryData[0] & 0x0F;
    uint8_t messageType = (binaryData[1] >> 4) & 0x0F;
    uint8_t messageTypeSpecificFlags = binaryData[1] & 0x0F;
    uint8_t serializationMethod = (binaryData[2] >> 4) & 0x0F;
    uint8_t compressionType = (binaryData[2] & 0x0F);
    uint8_t reserved = binaryData[3];
    
    logWithTimestamp("🔍 协议解析:");
    logWithTimestamp("  - 协议版本: " + std::to_string(protocolVersion));
    logWithTimestamp("  - 头部大小: " + std::to_string(headerSize) + " (4字节块)");
    logWithTimestamp("  - 消息类型: " + std::to_string(messageType));
    logWithTimestamp("  - 消息标志: " + std::to_string(messageTypeSpecificFlags));
    logWithTimestamp("  - 序列化方法: " + std::to_string(serializationMethod));
    logWithTimestamp("  - 压缩类型: " + std::to_string(compressionType));
    logWithTimestamp("  - 保留字段: " + std::to_string(reserved));
#else
    uint8_t headerSize = binaryData[0] & 0x0F;
    uint8_t messageType = (binaryData[1] >> 4) & 0x0F;
    uint8_t compressionType = (binaryData[2] & 0x0F);
#endif
    
    // 计算头部总大小
    size_t totalHeaderSize = headerSize * 4;
    if (binaryData.size() < totalHeaderSize) {
        logErrorWithTimestamp("❌ 二进制数据太小，无法包含完整头部");
        return "";
    }
    
    // 提取 payload
    std::string payload = binaryData.substr(totalHeaderSize);
    
    // 根据消息类型处理 payload
    if (messageType == FULL_SERVER_RESPONSE) {
        // 完整服务器响应: Header(4字节) + 序列号(4字节) + Payload Size(4字节) + Payload
        if (payload.size() < 8) {
            logErrorWithTimestamp("❌ payload 太小，无法解析序列号和payload size");
            return "";
        }
        
        // 解析序列号 (4字节，大端序，有符号)
        int32_t sequence = 0;
        for (int i = 0; i < 4; ++i) {
            sequence = (sequence << 8) | static_cast<unsigned char>(payload[i]);
        }
#if ASR_ENABLE_PROTOCOL_LOG
        logWithTimestamp("  - 序列号: " + std::to_string(sequence));
#endif
        
        // 解析 payload size (4字节，大端序)
        uint32_t payloadSize = 0;
        for (int i = 4; i < 8; ++i) {
            payloadSize = (payloadSize << 8) | static_cast<unsigned char>(payload[i]);
        }
        
#if ASR_ENABLE_PROTOCOL_LOG
        logWithTimestamp("  - payload size: " + std::to_string(payloadSize));
#endif
        
        if (payload.size() < 8 + payloadSize) {
            logErrorWithTimestamp("❌ payload 数据不完整");
            return "";
        }
        
        std::string payloadData = payload.substr(8, payloadSize);
        
        // 解压缩
        if (compressionType == GZIP_COMPRESSION) {
            std::string decompressed = gzipDecompress(payloadData);
            if (!decompressed.empty()) {
                return decompressed;
            }
        }
        
        return payloadData;
    } else if (messageType == ERROR_RESPONSE) {
        // 错误响应: Header(4字节) + 错误码(4字节) + Payload Size(4字节) + Payload
        if (payload.size() < 8) {
            logErrorWithTimestamp("❌ 错误响应 payload 太小");
            return "";
        }
        
        // 解析错误码 (4字节，大端序)
        uint32_t errorCode = 0;
        for (int i = 0; i < 4; ++i) {
            errorCode = (errorCode << 8) | static_cast<unsigned char>(payload[i]);
        }
        
        // 解析 payload size (4字节，大端序)
        uint32_t payloadSize = 0;
        for (int i = 4; i < 8; ++i) {
            payloadSize = (payloadSize << 8) | static_cast<unsigned char>(payload[i]);
        }
        
#if ASR_ENABLE_PROTOCOL_LOG
        logWithTimestamp("  - 错误码: " + std::to_string(errorCode));
        logWithTimestamp("  - payload size: " + std::to_string(payloadSize));
#endif
        
        if (payload.size() < 8 + payloadSize) {
            logErrorWithTimestamp("❌ 错误响应数据不完整");
            return "";
        }
        
        std::string payloadData = payload.substr(8, payloadSize);
        
        // 解压缩
        if (compressionType == GZIP_COMPRESSION) {
            std::string decompressed = gzipDecompress(payloadData);
            if (!decompressed.empty()) {
                return decompressed;
            }
        }
        
        return payloadData;
    }
    
    logErrorWithTimestamp("❌ 不支持的消息类型: " + std::to_string(messageType));
    return "";
}

std::string AsrClient::gzipDecompress(const std::string& data) {
    if (data.empty()) {
        return "";
    }
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = data.size();
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    
    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        logErrorWithTimestamp("❌ GZIP 解压缩初始化失败");
        return "";
    }
    
    std::string decompressed;
    char buffer[4096];
    
    do {
        strm.avail_out = sizeof(buffer);
        strm.next_out = reinterpret_cast<Bytef*>(buffer);
        
        int ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            logErrorWithTimestamp("❌ GZIP 解压缩错误");
            inflateEnd(&strm);
            return "";
        }
        
        int have = sizeof(buffer) - strm.avail_out;
        decompressed.append(buffer, have);
    } while (strm.avail_out == 0);
    
    inflateEnd(&strm);
    return decompressed;
}

// ============================================================================
// 音频文件解析方法
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

AsrClient::AudioFileInfo AsrClient::parseAudioFile(const std::string& filePath) {
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

AsrClient::AudioFileInfo AsrClient::parseWavFile(const std::string& filePath, const std::vector<uint8_t>& header) {
    AudioFileInfo info;
    info.format = "wav";
    
    if (header.size() < sizeof(WavHeader)) {
        std::cerr << "❌ WAV文件头太小" << std::endl;
        return info;
    }
    
    const WavHeader* wavHeader = reinterpret_cast<const WavHeader*>(header.data());
    
    // 检查WAV文件标识
    if (std::string(wavHeader->riff, 4) != "RIFF" || std::string(wavHeader->wave, 4) != "WAVE") {
        std::cerr << "❌ 无效的WAV文件格式" << std::endl;
        return info;
    }
    
    info.sampleRate = wavHeader->sampleRate;
    info.bitsPerSample = wavHeader->bitsPerSample;
    info.channels = wavHeader->numChannels;
    info.codec = "PCM";
    
    // 查找data块
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开WAV文件" << std::endl;
        return info;
    }
    
    file.seekg(sizeof(WavHeader));
    char chunkId[4];
    uint32_t chunkSize;
    
    while (file.read(chunkId, 4) && file.read(reinterpret_cast<char*>(&chunkSize), 4)) {
        if (std::string(chunkId, 4) == "data") {
            info.dataOffset = file.tellg();
            info.dataSize = chunkSize;
            break;
        }
        file.seekg(chunkSize, std::ios::cur);
    }
    
    file.close();
    
    if (info.dataSize == 0) {
        std::cerr << "❌ 未找到WAV数据块" << std::endl;
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

AsrClient::AudioFileInfo AsrClient::parseMp3File(const std::string& filePath, const std::vector<uint8_t>& header) {
    (void)header; // 避免未使用参数警告
    AudioFileInfo info;
    info.format = "mp3";
    info.codec = "MP3";
    
    // 简单的MP3解析（实际项目中可能需要更复杂的解析）
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开MP3文件" << std::endl;
        return info;
    }
    
    file.seekg(0, std::ios::end);
    info.dataSize = file.tellg();
    file.close();
    
    // MP3默认参数（实际应该从文件头解析）
    info.sampleRate = 44100;
    info.bitsPerSample = 16;
    info.channels = 2;
    info.dataOffset = 0; // MP3文件整体作为数据
    info.duration = static_cast<double>(info.dataSize) / 
                   (info.channels * info.sampleRate * info.bitsPerSample / 8);
    
    info.isValid = true;
    
    std::cout << "📁 成功解析MP3文件: " << filePath << std::endl;
    std::cout << "🎵 音频信息:" << std::endl;
    std::cout << "  - 格式: " << info.format << std::endl;
    std::cout << "  - 采样率: " << info.sampleRate << " Hz (默认)" << std::endl;
    std::cout << "  - 位深度: " << info.bitsPerSample << " bits (默认)" << std::endl;
    std::cout << "  - 声道数: " << info.channels << " (默认)" << std::endl;
    std::cout << "  - 编解码器: " << info.codec << std::endl;
    std::cout << "  - 文件大小: " << info.dataSize << " bytes" << std::endl;
    std::cout << "  - 音频时长: " << info.duration << " 秒 (估算)" << std::endl;
    
    return info;
}

AsrClient::AudioFileInfo AsrClient::parsePcmFile(const std::string& filePath, const std::vector<uint8_t>& header) {
    (void)header; // 避免未使用参数警告
    AudioFileInfo info;
    info.format = "pcm";
    info.codec = "PCM";
    
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

} // namespace Asr 