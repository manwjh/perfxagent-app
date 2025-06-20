//
// 基于 IXWebSocket 的 ASR 客户端实现
// 参考 asr_ixwebsocket_example.cpp 和 asr_qt_client 的工作流程
// 完全兼容火山引擎 ASR WebSocket 协议，支持流式音频识别
//

#include "asr/asr_client.h"
#include "asr/asr_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace Asr {

// ============================================================================
// AsrClient 类实现
// ============================================================================

AsrClient::AsrClient() 
    : m_connected(false)
    , m_authType(AuthType::TOKEN)
    , m_format("wav")
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
    , m_firstAudioSent(false)
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
        std::cout << "ASR client is already connected" << std::endl;
        return true;
    }
    
    std::cout << "🔗 正在连接 WebSocket..." << std::endl;
    std::cout << "📡 目标URL: " << m_webSocket.getUrl() << std::endl;
    
    // 打印发送的 Header 信息
    std::cout << "=== 发送的 HTTP Header ===" << std::endl;
    std::cout << "X-Api-Resource-Id: volc.bigasr.sauc.duration" << std::endl;
    std::cout << "X-Api-Access-Key: " << (m_accessToken.length() > 8 ? m_accessToken.substr(0, 4) + "****" + m_accessToken.substr(m_accessToken.length() - 4) : "****") << std::endl;
    std::cout << "X-Api-App-Key: " << m_appId << std::endl;
    std::cout << "X-Api-Request-Id: " << m_reqId << std::endl;
    
    m_webSocket.start();
    
    // 等待连接建立
    int timeout = 0;
    while (!m_connected && timeout < 300) { // 30秒超时
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout++;
        
        // 每5秒打印一次等待信息
        if (timeout % 50 == 0) {
            std::cout << "⏳ 等待连接建立... (" << (timeout / 10) << "秒)" << std::endl;
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
        std::cerr << "❌ 未连接" << std::endl;
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
    printf("==== 发送音频包 seq=%d ====" "\n", sequence);
    printf("HEADER: ");
    for (size_t i = 0; i < 4 && i < packet.size(); ++i) printf("%02x ", packet[i]);
    printf("\nPAYLOAD_LEN: ");
    for (size_t i = 8; i < 12 && i < packet.size(); ++i) printf("%02x ", packet[i]);
    printf("\nPAYLOAD_HEAD: ");
    for (size_t i = 12; i < 32 && i < packet.size(); ++i) printf("%02x ", packet[i]);
    printf("\nAUDIO_PACKET_HEX: ");
    for (auto b : packet) printf("%02x", b);
    printf("\npayload gzip len: %zu\n", payload.size());
    printf("payload gzip head: ");
    for (size_t i = 0; i < 10 && i < payload.size(); ++i) printf("%02x", payload[i]);
    printf("\npayload gzip tail: ");
    for (size_t i = (payload.size() > 10 ? payload.size() - 10 : 0); i < payload.size(); ++i) printf("%02x", payload[i]);
    printf("\n================\n");

    // 实际发送
    auto sendInfo = m_webSocket.sendBinary(packet);
    return sendInfo.success;
}

bool AsrClient::sendAudioFile(const std::string& filePath) {
    // 首先解析音频文件，获取音频数据信息
    AsrManager manager;
    auto audioInfo = manager.parseAudioFile(filePath);
    
    if (!audioInfo.isValid) {
        std::cerr << "❌ 无法解析音频文件: " << filePath << std::endl;
        return false;
    }
    
    // 读取音频数据部分（不包括文件头）
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开音频文件: " << filePath << std::endl;
        return false;
    }
    
    // 跳过文件头，直接读取音频数据
    file.seekg(audioInfo.dataOffset);
    std::vector<uint8_t> audioData(audioInfo.dataSize);
    file.read(reinterpret_cast<char*>(audioData.data()), audioInfo.dataSize);
    file.close();
    
    if (audioData.empty()) {
        std::cerr << "❌ 音频数据为空: " << filePath << std::endl;
        return false;
    }
    
    std::cout << "📁 读取音频数据: " << filePath << " (" << audioData.size() << " bytes)" << std::endl;
    std::cout << "🎵 音频信息:" << std::endl;
    std::cout << "  - 格式: " << audioInfo.format << std::endl;
    std::cout << "  - 采样率: " << audioInfo.sampleRate << " Hz" << std::endl;
    std::cout << "  - 位深度: " << audioInfo.bitsPerSample << " bits" << std::endl;
    std::cout << "  - 声道数: " << audioInfo.channels << std::endl;
    std::cout << "  - 编解码器: " << audioInfo.codec << std::endl;
    std::cout << "  - 音频数据大小: " << audioInfo.dataSize << " bytes" << std::endl;
    std::cout << "  - 音频时长: " << audioInfo.duration << " 秒" << std::endl;
    
    // 分段发送音频数据
    size_t segmentSize = m_segDuration * m_sampleRate * m_channels * m_bits / 8 / 1000; // 转换为字节
    size_t offset = 0;
    
    while (offset < audioData.size()) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        size_t chunkSize = std::min(segmentSize, audioData.size() - offset);
        std::vector<uint8_t> chunk(audioData.begin() + offset, audioData.begin() + offset + chunkSize);
        
        bool isLast = (offset + chunkSize >= audioData.size());
        
        if (!sendAudio(chunk, isLast ? -m_seq : m_seq)) {
            std::cerr << "❌ 发送音频数据失败" << std::endl;
            return false;
        }
        
        offset += chunkSize;
        
        // 计算延迟时间，与 Python 版本保持一致
        if (m_streaming && !isLast) {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            long long sleepTime = std::max(0LL, static_cast<long long>(m_segDuration) - elapsed.count());
            
            if (sleepTime > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            }
        }
    }
    
    return true;
}

// ============================================================================
// 请求发送方法
// ============================================================================

bool AsrClient::sendFullClientRequestAndWaitResponse(int timeoutMs, std::string* response) {
    if (!m_connected) {
        std::cerr << "❌ 未连接" << std::endl;
        return false;
    }
    
    // 构造请求 JSON
    json requestParams = constructRequest();
    std::string jsonStr = requestParams.dump();
    std::cout << "📤 JSON_STRING: " << jsonStr << std::endl;
    std::cout << "📤 JSON原始长度: " << jsonStr.length() << " bytes" << std::endl;
    
    // Gzip 压缩
    std::vector<uint8_t> payload = gzipCompress(jsonStr);
    if (payload.empty()) {
        std::cerr << "❌ GZIP 压缩失败" << std::endl;
        return false;
    }
    std::cout << "📤 gzip压缩后长度: " << payload.size() << " bytes" << std::endl;
    
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
    std::cout << "📤 HEADER: ";
    for (size_t i = 0; i < 4 && i < packet.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)packet[i] << " ";
    }
    std::cout << std::dec << std::endl;
    std::cout << "📤 PAYLOAD_LEN: ";
    for (size_t i = 8; i < 12 && i < packet.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)packet[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
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
        
        std::cout << "✅ 使用环境变量中的凭据" << std::endl;
    } else {
        // 使用默认凭据（仅用于测试）
        creds.appId = "8388344882";
        creds.accessToken = "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3";
        creds.secretKey = "";
        creds.isValid = true;
        
        std::cout << "⚠️  使用默认凭据（仅用于测试）" << std::endl;
        std::cout << "💡 建议设置环境变量：VOLC_APP_ID, VOLC_ACCESS_TOKEN, VOLC_SECRET_KEY" << std::endl;
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
                std::cout << "📨 收到二进制消息，大小: " << msg->wireSize << " 字节" << std::endl;
                
                // 打印前20字节的十六进制
                std::cout << "🔍 原始数据(前20字节): ";
                for (size_t i = 0; i < std::min(size_t(20), msg->str.size()); ++i) {
                    printf("%02x ", static_cast<unsigned char>(msg->str[i]));
                }
                std::cout << std::endl;
                
                // 解析二进制协议
                std::string jsonResponse = parseBinaryResponse(msg->str);
                if (!jsonResponse.empty()) {
                    std::cout << "🧹 解析后的响应: " << jsonResponse << std::endl;
                    
                    // 解析错误信息
                    if (hasError(jsonResponse)) {
                        m_lastError = parseErrorResponse(jsonResponse);
                        std::cout << "❌ 检测到错误: " << m_lastError.getErrorDescription() << std::endl;
                        std::cout << "🔍 错误码: " << m_lastError.code << std::endl;
                        std::cout << "📝 错误详情: " << m_lastError.message << std::endl;
                        
                        if (m_callback) {
                            m_callback->onError(this, m_lastError.message);
                        }
                        return;
                    }
                    
                    // 尝试解析 JSON 获取 log_id
                    try {
                        json j = json::parse(jsonResponse);
                        if (j.contains("result") && j["result"].contains("additions") && 
                            j["result"]["additions"].contains("log_id")) {
                            m_logId = j["result"]["additions"]["log_id"];
                            std::cout << "🔍 提取到 log_id: " << m_logId << std::endl;
                        }
                    } catch (const std::exception& e) {
                        // 不是 JSON 格式，忽略
                    }
                    
                    if (m_callback) {
                        m_callback->onMessage(this, jsonResponse);
                        // 音频包流式发送：收到Full server response后自动发下一个包
                        auto* manager = dynamic_cast<AsrManager*>(m_callback);
                        if (manager) {
                            // 检查是否还有音频包需要发送
                            if (manager->hasMoreAudioPackets()) {
                                std::cout << "🔄 收到Full server response，发送下一个音频包" << std::endl;
                                if (!manager->sendNextAudioPacket()) {
                                    std::cerr << "❌ 发送下一个音频包失败" << std::endl;
                                }
                            } else {
                                std::cout << "🎉 收到最终识别结果" << std::endl;
                            }
                        }
                    }
                    
                    // 检查是否为Full Server Response或ACK
                    if (!jsonResponse.empty()) {
                        if (jsonResponse.find("\"result\"") != std::string::npos ||
                            jsonResponse.find("\"code\":0") != std::string::npos ||
                            jsonResponse.find("\"status\":0") != std::string::npos) {
                            m_readyForAudio = true;
                            std::cout << "✅ 识别会话已开始" << std::endl;
                            // 推荐最佳实践：收到Full server response立即发第一个音频包
                            if (!m_firstAudioSent && m_callback) {
                                auto* manager = dynamic_cast<AsrManager*>(m_callback);
                                if (manager) {
                                    if (manager->sendNextAudioPacket()) {
                                        m_firstAudioSent = true;
                                        std::cout << "✅ 第一个音频包发送成功" << std::endl;
                                    } else {
                                        std::cerr << "❌ 第一个音频包发送失败" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                // 处理文本消息
                std::cout << "📨 收到文本消息: " << msg->str << std::endl;
                
                // 解析错误信息
                if (hasError(msg->str)) {
                    m_lastError = parseErrorResponse(msg->str);
                    std::cout << "❌ 检测到错误: " << m_lastError.getErrorDescription() << std::endl;
                    std::cout << "🔍 错误码: " << m_lastError.code << std::endl;
                    std::cout << "📝 错误详情: " << m_lastError.message << std::endl;
                    
                    if (m_callback) {
                        m_callback->onError(this, m_lastError.message);
                    }
                    return;
                }
                
                // 尝试解析 JSON 获取 log_id
                try {
                    json j = json::parse(msg->str);
                    if (j.contains("result") && j["result"].contains("additions") && 
                        j["result"]["additions"].contains("log_id")) {
                        m_logId = j["result"]["additions"]["log_id"];
                        std::cout << "🔍 提取到 log_id: " << m_logId << std::endl;
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
                        std::cout << "✅ 识别会话已开始" << std::endl;
                        // 推荐最佳实践：收到Full server response立即发第一个音频包
                        if (!m_firstAudioSent && m_callback) {
                            auto* manager = dynamic_cast<AsrManager*>(m_callback);
                            if (manager) {
                                if (manager->sendNextAudioPacket()) {
                                    m_firstAudioSent = true;
                                    std::cout << "✅ 第一个音频包发送成功" << std::endl;
                                } else {
                                    std::cerr << "❌ 第一个音频包发送失败" << std::endl;
                                }
                            }
                        }
                    }
                }
            }
            break;
        }
        case ix::WebSocketMessageType::Open: {
            std::cout << "✅ WebSocket 连接已建立" << std::endl;
            m_connected = true;
            
            // 获取响应头
            const auto& headers = msg->openInfo.headers;
            for (const auto& header : headers) {
                m_responseHeaders[header.first] = header.second;
                std::cout << "📋 响应头: " << header.first << ": " << header.second << std::endl;
            }
            
            // 特别关注 X-Tt-Logid
            if (headers.find("X-Tt-Logid") != headers.end()) {
                std::cout << "🎯 成功获取 X-Tt-Logid: " << headers.at("X-Tt-Logid") << std::endl;
            } else {
                std::cout << "⚠️  未找到 X-Tt-Logid" << std::endl;
            }
            
            if (m_callback) {
                m_callback->onOpen(this);
            }
            break;
        }
        case ix::WebSocketMessageType::Close: {
            std::cout << "🔌 WebSocket 连接已关闭 (code: " << msg->closeInfo.code << ", reason: " << msg->closeInfo.reason << ")" << std::endl;
            m_connected = false;
            
            if (m_callback) {
                m_callback->onClose(this);
            }
            break;
        }
        case ix::WebSocketMessageType::Error: {
            std::cout << "❌ WebSocket 错误: " << msg->errorInfo.reason << std::endl;
            std::cout << "🔍 错误详情: HTTP状态=" << msg->errorInfo.http_status 
                     << ", 重试次数=" << msg->errorInfo.retries 
                     << ", 等待时间=" << msg->errorInfo.wait_time << "ms" << std::endl;
            
            if (m_callback) {
                m_callback->onError(this, msg->errorInfo.reason);
            }
            break;
        }
        case ix::WebSocketMessageType::Fragment: {
            std::cout << "📦 收到消息片段" << std::endl;
            break;
        }
        case ix::WebSocketMessageType::Ping: {
            std::cout << "🏓 收到 Ping" << std::endl;
            // 兜底：如果主流程没发第一个音频包，收到Ping时再发
            static bool firstAudioSent = false;
            if (!firstAudioSent && m_callback) {
                firstAudioSent = true;
                auto* manager = dynamic_cast<AsrManager*>(m_callback);
                if (manager) {
                    if (!manager->sendNextAudioPacket()) {
                        std::cerr << "❌ 第一个音频包发送失败，终止识别" << std::endl;
                        disconnect();
                    }
                }
            }
            break;
        }
        case ix::WebSocketMessageType::Pong: {
            std::cout << "🏓 收到 Pong" << std::endl;
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
        std::cerr << "❌ GZIP 压缩初始化失败" << std::endl;
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
        std::cerr << "❌ GZIP 压缩失败" << std::endl;
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
        std::cerr << "❌ GZIP 压缩初始化失败" << std::endl;
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
        std::cerr << "❌ GZIP 压缩失败" << std::endl;
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
        std::cerr << "❌ 二进制数据太小，无法解析协议头" << std::endl;
        return "";
    }
    
    // 解析协议头
    uint8_t protocolVersion = (binaryData[0] >> 4) & 0x0F;
    uint8_t headerSize = binaryData[0] & 0x0F;
    uint8_t messageType = (binaryData[1] >> 4) & 0x0F;
    uint8_t messageTypeSpecificFlags = binaryData[1] & 0x0F;
    uint8_t serializationMethod = (binaryData[2] >> 4) & 0x0F;
    uint8_t compressionType = (binaryData[2] & 0x0F);
    uint8_t reserved = binaryData[3];
    
    std::cout << "🔍 协议解析:" << std::endl;
    std::cout << "  - 协议版本: " << (int)protocolVersion << std::endl;
    std::cout << "  - 头部大小: " << (int)headerSize << " (4字节块)" << std::endl;
    std::cout << "  - 消息类型: " << (int)messageType << std::endl;
    std::cout << "  - 消息标志: " << (int)messageTypeSpecificFlags << std::endl;
    std::cout << "  - 序列化方法: " << (int)serializationMethod << std::endl;
    std::cout << "  - 压缩类型: " << (int)compressionType << std::endl;
    std::cout << "  - 保留字段: " << (int)reserved << std::endl;
    
    // 计算头部总大小
    size_t totalHeaderSize = headerSize * 4;
    if (binaryData.size() < totalHeaderSize) {
        std::cerr << "❌ 二进制数据太小，无法包含完整头部" << std::endl;
        return "";
    }
    
    // 提取 payload
    std::string payload = binaryData.substr(totalHeaderSize);
    
    // 根据消息类型处理 payload
    if (messageType == FULL_SERVER_RESPONSE) {
        // 完整服务器响应: Header(4字节) + 序列号(4字节) + Payload Size(4字节) + Payload
        if (payload.size() < 8) {
            std::cerr << "❌ payload 太小，无法解析序列号和payload size" << std::endl;
            return "";
        }
        
        // 解析序列号 (4字节，大端序，有符号)
        int32_t sequence = 0;
        for (int i = 0; i < 4; ++i) {
            sequence = (sequence << 8) | static_cast<unsigned char>(payload[i]);
        }
        std::cout << "  - 序列号: " << sequence << std::endl;
        
        // 解析 payload size (4字节，大端序)
        uint32_t payloadSize = 0;
        for (int i = 4; i < 8; ++i) {
            payloadSize = (payloadSize << 8) | static_cast<unsigned char>(payload[i]);
        }
        
        std::cout << "  - payload size: " << payloadSize << std::endl;
        
        if (payload.size() < 8 + payloadSize) {
            std::cerr << "❌ payload 数据不完整" << std::endl;
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
            std::cerr << "❌ 错误响应 payload 太小" << std::endl;
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
        
        std::cout << "  - 错误码: " << errorCode << std::endl;
        std::cout << "  - payload size: " << payloadSize << std::endl;
        
        if (payload.size() < 8 + payloadSize) {
            std::cerr << "❌ 错误响应数据不完整" << std::endl;
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
    
    std::cerr << "❌ 不支持的消息类型: " << (int)messageType << std::endl;
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
        std::cerr << "❌ GZIP 解压缩初始化失败" << std::endl;
        return "";
    }
    
    std::string decompressed;
    char buffer[4096];
    
    do {
        strm.avail_out = sizeof(buffer);
        strm.next_out = reinterpret_cast<Bytef*>(buffer);
        
        int ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            std::cerr << "❌ GZIP 解压缩错误" << std::endl;
            inflateEnd(&strm);
            return "";
        }
        
        int have = sizeof(buffer) - strm.avail_out;
        decompressed.append(buffer, have);
    } while (strm.avail_out == 0);
    
    inflateEnd(&strm);
    return decompressed;
}

} // namespace Asr 