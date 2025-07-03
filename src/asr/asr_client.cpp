//
// 基于 IXWebSocket 的 ASR 客户端实现
// 完全兼容火山引擎 ASR WebSocket 协议，支持流式音频识别
//

#include "asr/asr_client.h"
#include "asr/asr_log_utils.h"
#include "asr/secure_key_manager.h"  // 使用ASR命名空间的SecureKeyManager
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
// AsrClient 类实现
// ============================================================================

AsrClient::AsrClient() : m_connected(false), m_seq(0), m_callback(nullptr), m_readyForAudio(false) {
    std::cout << "[ASR-CRED] AsrClient constructor called" << std::endl;
    
    // ix::WebSocket 只提供 setOnMessageCallback，所有事件（包括 open/close/error/message 等）
    // 都通过 WebSocketMessageType 区分，在 handleMessage 统一处理。
    // ⚠️ ASR服务器的API接口，决定了它能接收什么形式的AUDIO数据包。而不是Audio是什么数据，去配置ASR服务器
    m_webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        this->handleMessage(msg);
    });
    
    std::cout << "[ASR-CRED] AsrClient constructor completed" << std::endl;
    
    m_config = AsrApiConfig(); // 使用默认值
    m_finalResponseReceived = false;
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
    std::cout << "[ASR-THREAD] Destroying AsrClient..." << std::endl;
    
    try {
        // 断开WebSocket连接
        if (m_webSocket.getReadyState() == ix::ReadyState::Open) {
            std::cout << "[ASR-THREAD] Closing WebSocket connection..." << std::endl;
            m_webSocket.close();
            std::cout << "[ASR-THREAD] WebSocket connection closed" << std::endl;
        }
        
        std::cout << "[ASR-THREAD] AsrClient destroyed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ASR-THREAD][ERROR] Exception in AsrClient destructor: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[ASR-THREAD][ERROR] Unknown exception in AsrClient destructor" << std::endl;
    }
}

// ============================================================================
// 配置方法
// ============================================================================

void AsrClient::setAppId(const std::string& appId) {
    if (appId.empty()) {
        logErrorWithTimestamp("❌ AppId 不能为空");
        return;
    }
    m_config.appId = appId;
    updateHeaders();
    logWithTimestamp("✅ AppId 设置成功");
}

void AsrClient::setToken(const std::string& token) {
    if (token.empty()) {
        logErrorWithTimestamp("❌ Token 不能为空");
        return;
    }
    m_config.accessToken = token;
    updateHeaders();
    logWithTimestamp("✅ Token 设置成功");
}

void AsrClient::setSecretKey(const std::string& secretKey) {
    m_config.secretKey = secretKey;
    updateHeaders();
}

void AsrClient::setAudioFormat(const std::string& format, int channels, int sampleRate, int bits, const std::string& codec) {
    // 验证音频格式是否符合ASR API要求
    AudioFormatValidationResult validation = validateAudioFormat(format, channels, sampleRate, bits, codec);
    if (!validation.isValid) {
        logErrorWithTimestamp("❌ " + validation.errorMessage);
        return;
    }
    
    m_config.format = format;
    m_config.channels = channels;
    m_config.sampleRate = sampleRate;
    m_config.bits = bits;
    m_config.codec = codec;
    
    logWithTimestamp("✅ 音频格式设置成功: " + format + ", " + std::to_string(channels) + 
                    "ch, " + std::to_string(sampleRate) + "Hz, " + std::to_string(bits) + "bit, codec: " + codec);
}

void AsrClient::setCluster(const std::string& cluster) {
    m_config.cluster = cluster;
}

void AsrClient::setCallback(AsrCallback* callback) {
    m_callback = callback;
}

void AsrClient::setUid(const std::string& uid) {
    m_config.uid = uid;
}

void AsrClient::setLanguage(const std::string& language) {
    // 验证语言代码
    if (language.empty()) {
        logErrorWithTimestamp("❌ 语言代码不能为空");
        return;
    }
    
    // 支持的语言代码列表（可根据需要扩展）
    std::vector<std::string> supportedLanguages = {
        "zh-CN", "zh-TW", "en-US", "en-GB", "ja-JP", "ko-KR"
    };
    
    bool isValid = false;
    for (const auto& lang : supportedLanguages) {
        if (language == lang) {
            isValid = true;
            break;
        }
    }
    
    if (!isValid) {
        logErrorWithTimestamp("❌ 不支持的语言代码: " + language + "，支持的语言: zh-CN, zh-TW, en-US, en-GB, ja-JP, ko-KR");
        return;
    }
    
    m_config.language = language;
    logWithTimestamp("✅ 语言设置成功: " + language);
}

void AsrClient::setResultType(const std::string& resultType) {
    m_config.resultType = resultType;
}

void AsrClient::setStreaming(bool streaming) {
    m_config.streaming = streaming;
}

void AsrClient::setSegDuration(int duration) {
    // 验证分段时长
    if (duration <= 0 || duration > 10000) {
        logErrorWithTimestamp("❌ 无效的分段时长: " + std::to_string(duration) + "，应在 1-10000ms 范围内");
        return;
    }
    
    m_config.segDuration = duration;
    logWithTimestamp("✅ 分段时长设置成功: " + std::to_string(duration) + "ms");
}

// ============================================================================
// 火山引擎 ASR 高级配置方法实现
// ============================================================================

void AsrClient::setModelName(const std::string& modelName) {
    if (modelName.empty()) {
        logErrorWithTimestamp("❌ 模型名称不能为空");
        return;
    }
    m_config.modelName = modelName;
    logWithTimestamp("✅ 模型名称设置成功: " + modelName);
}

void AsrClient::setEnablePunc(bool enable) {
    m_config.enablePunc = enable;
    logWithTimestamp("✅ 标点符号设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setVadSegmentDuration(int duration) {
    if (duration <= 0 || duration > 10000) {
        logErrorWithTimestamp("❌ 无效的VAD分段时长: " + std::to_string(duration) + "，应在 1-10000ms 范围内");
        return;
    }
    m_config.vadSegmentDuration = duration;
    logWithTimestamp("✅ VAD分段时长设置成功: " + std::to_string(duration) + "ms");
}

void AsrClient::setEnableItn(bool enable) {
    m_config.enableItn = enable;
    logWithTimestamp("✅ 数字文本规范化设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableTimestamp(bool enable) {
    m_config.enableTimestamp = enable;
    logWithTimestamp("✅ 时间戳设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableVoiceDetection(bool enable) {
    m_config.enableVoiceDetection = enable;
    logWithTimestamp("✅ 语音检测设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableSemanticSentenceDetection(bool enable) {
    m_config.enableSemanticSentenceDetection = enable;
    logWithTimestamp("✅ 语义句子检测设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableInverseTextNormalization(bool enable) {
    m_config.enableInverseTextNormalization = enable;
    logWithTimestamp("✅ 逆文本规范化设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableWordTimeOffset(bool enable) {
    m_config.enableWordTimeOffset = enable;
    logWithTimestamp("✅ 词级别时间偏移设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnablePartialResult(bool enable) {
    m_config.enablePartialResult = enable;
    logWithTimestamp("✅ 部分结果设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableFinalResult(bool enable) {
    m_config.enableFinalResult = enable;
    logWithTimestamp("✅ 最终结果设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableInterimResult(bool enable) {
    m_config.enableInterimResult = enable;
    logWithTimestamp("✅ 中间结果设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setEnableSilenceDetection(bool enable) {
    m_config.enableSilenceDetection = enable;
    logWithTimestamp("✅ 静音检测设置: " + std::string(enable ? "启用" : "禁用"));
}

void AsrClient::setSilenceThreshold(int threshold) {
    if (threshold <= 0 || threshold > 10000) {
        logErrorWithTimestamp("❌ 无效的静音阈值: " + std::to_string(threshold) + "，应在 1-10000ms 范围内");
        return;
    }
    m_config.silenceThreshold = threshold;
    logWithTimestamp("✅ 静音阈值设置成功: " + std::to_string(threshold) + "ms");
}

// ============================================================================
// 连接控制方法
// ============================================================================

bool AsrClient::connect() {
    try {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            return false; // 避免死锁
        }
        
        if (m_connected) {
            return true;
        }

        // 检查URL是否有效
        std::string url = m_config.cluster;
        if (url.empty()) {
            // 使用默认URL
            url = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel";
            std::cout << "[ASR-CRED] 使用默认URL: " << url << std::endl;
        } else {
            std::cout << "[ASR-CRED] 使用配置URL: " << url << std::endl;
        }

        m_webSocket.setUrl(url);
        m_webSocket.disableAutomaticReconnection(); // 禁用自动重连，手动控制
        m_webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            handleMessage(msg);
        });

        // 启动WebSocket线程
        m_webSocket.start();
        
        // 等待连接建立，但设置超时
        auto startTime = std::chrono::steady_clock::now();
        while (m_webSocket.getReadyState() != ix::ReadyState::Open && 
               std::chrono::steady_clock::now() - startTime < std::chrono::seconds(5)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        m_connected = (m_webSocket.getReadyState() == ix::ReadyState::Open);
        if (m_connected) {
            std::cout << "[ASR-CRED] WebSocket连接成功" << std::endl;
        } else {
            std::cout << "[ASR-CRED] WebSocket连接失败" << std::endl;
        }
        return m_connected;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] ASR connection failed: " << e.what() << std::endl;
        return false;
    }
}

void AsrClient::disconnect() {
    try {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            // 如果无法获取锁，强制停止
            m_webSocket.stop();
            return;
        }
        
        if (!m_connected) {
            return;
        }

        m_connected = false;
        m_webSocket.stop();
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] ASR disconnect failed: " << e.what() << std::endl;
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
    //debugInfo << "HEADER: " << hexString(packet) << std::endl;
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

// ============================================================================
// 请求发送方法
// ============================================================================

bool AsrClient::sendFullClientRequestAndWaitResponse(int timeoutMs, std::string* response) {
    if (!m_connected) {
        logErrorWithTimestamp("❌ 未连接");
        return false;
    }
    
    // 连接建立后等待短暂时间，确保连接稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 构造请求 JSON
    json requestParams = constructRequest();
    std::string jsonStr = requestParams.dump();
    // 打印Full Client Request包的json字符串
    logWithTimestamp("📤 JSON_STRING: " + jsonStr);
    logWithTimestamp("📤 JSON原始长度: " + std::to_string(jsonStr.length()) + " bytes");
    
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
    
    // 递增序列号
    m_seq++;
    
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
// 音频格式验证方法实现
// ============================================================================

AsrClient::AudioFormatValidationResult AsrClient::validateAudioFormat(const std::string& format, int channels, 
                                                                      int sampleRate, int bits, const std::string& codec) const {
    // 验证音频格式
    if (format != "pcm" && format != "wav" && format != "ogg") {
        return AudioFormatValidationResult(false, "不支持的音频格式: " + format + "，仅支持 pcm/wav/ogg");
    }
    
    // 验证声道数
    if (channels != 1 && channels != 2) {
        return AudioFormatValidationResult(false, "不支持的声道数: " + std::to_string(channels) + "，仅支持 1(mono)/2(stereo)");
    }
    
    // 验证采样率
    if (sampleRate != 16000) {
        return AudioFormatValidationResult(false, "不支持的采样率: " + std::to_string(sampleRate) + "，仅支持 16000");
    }
    
    // 验证位深度
    if (bits != 16) {
        return AudioFormatValidationResult(false, "不支持的位深度: " + std::to_string(bits) + "，仅支持 16");
    }
    
    // 验证编解码器
    if (codec != "raw" && codec != "opus") {
        return AudioFormatValidationResult(false, "不支持的编解码器: " + codec + "，仅支持 raw/opus");
    }
    
    return AudioFormatValidationResult(true, "");
}

std::string AsrClient::getSupportedAudioFormats() const {
    return "支持的音频格式:\n"
           "- 格式: pcm, wav, ogg\n"
           "- 声道数: 1 (mono), 2 (stereo)\n"
           "- 采样率: 16000 Hz\n"
           "- 位深度: 16 bits\n"
           "- 编解码器: raw, opus";
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
    headers["X-Api-Access-Key"] = m_config.accessToken;
    headers["X-Api-App-Key"] = m_config.appId;
    headers["X-Api-Request-Id"] = m_reqId;
    
    m_webSocket.setExtraHeaders(headers);
}

void AsrClient::handleMessage(const ix::WebSocketMessagePtr& msg) {
    try {
        if (!msg) {
            logErrorWithTimestamp("❌ 收到空的WebSocket消息");
            return;
        }
        
        switch (msg->type) {
            case ix::WebSocketMessageType::Message: {
                if (msg->binary) {
                    handleBinaryMessage(msg);
                } else {
                    handleTextMessage(msg);
                }
                break;
            }
            case ix::WebSocketMessageType::Open: {
                handleConnectionOpen(msg);
                break;
            }
            case ix::WebSocketMessageType::Close: {
                handleConnectionClose(msg);
                break;
            }
            case ix::WebSocketMessageType::Error: {
                handleConnectionError(msg);
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
    } catch (const std::exception& e) {
        logErrorWithTimestamp("❌ handleMessage异常: " + std::string(e.what()));
    } catch (...) {
        logErrorWithTimestamp("❌ handleMessage发生未知异常");
    }
}

void AsrClient::handleBinaryMessage(const ix::WebSocketMessagePtr& msg) {
    if (msg->str.length() < 4) {
        logErrorWithTimestamp("❌ 二进制消息太短，无法解析协议头");
        return;
    }
    
    uint8_t messageType = (msg->str[1] & 0xF0) >> 4;
    uint8_t flags = msg->str[1] & 0x0F;
    
#if ASR_ENABLE_PROTOCOL_LOG
    logWithTimestamp("📨 收到二进制消息，大小: " + std::to_string(msg->wireSize) + " 字节");
    logWithTimestamp("🔍 消息类型: " + std::to_string(messageType) + ", 标志: " + std::to_string(flags));
    logWithTimestamp("🔍 原始数据(前20字节): " + hexString(std::vector<uint8_t>(msg->str.begin(), msg->str.begin() + std::min(size_t(20), msg->str.size()))));
#endif
    
    switch (messageType) {
        case FULL_SERVER_RESPONSE: // 0x09
            handleFullServerResponse(msg, flags);
            break;
        case SERVER_ACK: // 0x0B
            handleServerAck(msg);
            break;
        case ERROR_RESPONSE: // 0x0F
            handleErrorResponse(msg);
            break;
        default:
            logWithTimestamp("⚠️ 未知消息类型: " + std::to_string(messageType) + " (0x" + 
                           std::to_string(messageType) + ")");
            // 尝试解析为错误响应
            if (messageType == 0x0F) {
                logWithTimestamp("🔄 尝试解析为错误响应");
                handleErrorResponse(msg);
            }
    }
}

void AsrClient::handleFullServerResponse(const ix::WebSocketMessagePtr& msg, uint8_t flags) {
    // 基于协议标志判断是否为最终响应
    if (flags == 0x03) { // 最后一包音频结果
        setFinalResponseReceived();
        logWithTimestamp("🎯 收到最终结果响应 (Full Server Response)");
    }
    
    // 解析二进制协议获取JSON响应
    std::string jsonResponse = parseBinaryResponse(msg->str);
    if (!jsonResponse.empty()) {
#if ASR_ENABLE_PROTOCOL_LOG
        logWithTimestamp("🧹 解析后的响应: " + jsonResponse);
#endif
        processJsonResponse(jsonResponse);
    }
}

void AsrClient::handleServerAck(const ix::WebSocketMessagePtr& msg) {
    logWithTimestamp("✅ 收到服务器确认 (Server ACK)");
    
    // 解析ACK消息，可能包含额外信息
    std::string jsonResponse = parseBinaryResponse(msg->str);
    if (!jsonResponse.empty()) {
        processJsonResponse(jsonResponse);
    }
}

void AsrClient::handleErrorResponse(const ix::WebSocketMessagePtr& msg) {
    logErrorWithTimestamp("❌ 收到错误响应");
    
    std::string jsonResponse = parseBinaryResponse(msg->str);
    if (!jsonResponse.empty()) {
        m_lastError = parseErrorResponse(jsonResponse);
        logErrorWithTimestamp("❌ 错误详情: " + m_lastError.getErrorDescription());
        logErrorWithTimestamp("❌ 错误消息: " + m_lastError.message);
        logErrorWithTimestamp("❌ 错误代码: " + std::to_string(m_lastError.code));
        
        // 特别处理"payload unmarshal: no request object before data"错误
        if (m_lastError.message.find("payload unmarshal: no request object before data") != std::string::npos) {
            logErrorWithTimestamp("❌ ❌ ASR连接错误: payload unmarshal: no request object before data");
            logErrorWithTimestamp("🔍 可能原因: 1) 请求格式不正确 2) 发送时机过早 3) 协议版本不匹配");
        }
        
        if (m_callback) {
            m_callback->onError(this, m_lastError.message);
        }
    } else {
        logErrorWithTimestamp("❌ 无法解析错误响应");
        if (m_callback) {
            m_callback->onError(this, "Unknown error response");
        }
    }
}

void AsrClient::handleTextMessage(const ix::WebSocketMessagePtr& msg) {
#if ASR_ENABLE_PROTOCOL_LOG
    logWithTimestamp("📨 收到文本消息: " + msg->str);
#endif
    
    // 检查是否有错误
    if (hasError(msg->str)) {
        m_lastError = parseErrorResponse(msg->str);
        logErrorWithTimestamp("❌ 检测到错误: " + m_lastError.getErrorDescription());
        
        if (m_callback) {
            m_callback->onError(this, m_lastError.message);
        }
        return;
    }
    
    // 处理JSON响应
    processJsonResponse(msg->str);
}

void AsrClient::handleConnectionOpen(const ix::WebSocketMessagePtr& msg) {
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
        logWithTimestamp("⚠️ 未找到 X-Tt-Logid");
    }
    
    if (m_callback) {
        m_callback->onOpen(this);
    }
}

void AsrClient::handleConnectionClose(const ix::WebSocketMessagePtr& msg) {
    logWithTimestamp("🔌 WebSocket 连接已关闭 (code: " + std::to_string(msg->closeInfo.code) + ", reason: " + msg->closeInfo.reason + ")");
    m_connected = false;
    
    if (m_callback) {
        m_callback->onClose(this);
    }
}

void AsrClient::handleConnectionError(const ix::WebSocketMessagePtr& msg) {
    logErrorWithTimestamp("❌ WebSocket 错误: " + msg->errorInfo.reason);
    logErrorWithTimestamp("🔍 错误详情: HTTP状态=" + std::to_string(msg->errorInfo.http_status) 
             + ", 重试次数=" + std::to_string(msg->errorInfo.retries) 
             + ", 等待时间=" + std::to_string(msg->errorInfo.wait_time) + "ms");
    
    if (m_callback) {
        m_callback->onError(this, msg->errorInfo.reason);
    }
}

void AsrClient::processJsonResponse(const std::string& jsonStr) {
    if (jsonStr.empty()) {
        logWithTimestamp("⚠️ 收到空的JSON响应");
        return;
    }
    
    try {
        json j = json::parse(jsonStr);
        
        // 保存实际的响应内容
        m_lastResponse = jsonStr;
        
        // 提取 log_id
        extractLogId(j);
        
        // 检查是否为会话开始响应
        if (checkSessionStarted(j)) {
            setReadyForAudio();
        }
        
        // 调用回调函数
        if (m_callback) {
            m_callback->onMessage(this, jsonStr);
        }
        
    } catch (const std::exception& e) {
        logWithTimestamp("⚠️ JSON解析失败: " + std::string(e.what()));
    }
}

void AsrClient::extractLogId(const json& j) {
    if (j.contains("result") && j["result"].contains("additions") && 
        j["result"]["additions"].contains("log_id")) {
        m_logId = j["result"]["additions"]["log_id"];
#if ASR_ENABLE_PROTOCOL_LOG
        logWithTimestamp("🔍 提取到 log_id: " + m_logId);
#endif
    }
}

bool AsrClient::checkSessionStarted(const json& j) {
    // 检查是否包含result字段，表示会话已开始
    return j.contains("result") || 
           (j.contains("code") && j["code"] == 0) ||
           (j.contains("status") && j["status"] == 0);
}

void AsrClient::setFinalResponseReceived() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_finalResponseReceived = true;
    logWithTimestamp("🎯 设置最终响应标志");
    m_cv.notify_one();
}

void AsrClient::setReadyForAudio() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_readyForAudio = true;
    logWithTimestamp("✅ 识别会话已开始");
    m_cv.notify_one();
}

// 构造请求包
json AsrClient::constructRequest() const {
    json req = {
        {"user", {{"uid", m_config.uid}}},
        {"audio", {
            {"format", m_config.format},
            {"sample_rate", m_config.sampleRate},
            {"bits", m_config.bits},
            {"channel", m_config.channels},
            {"codec", m_config.codec}
        }},
        {"request", {
            {"model_name", m_config.modelName},
            {"enable_punc", m_config.enablePunc},
            {"vad_segment_duration", m_config.vadSegmentDuration},
            {"enable_final_result", true},
            {"enable_interim_result", true},
            {"language", m_config.language},
            {"result_type", m_config.resultType}
        }}
    };
    
    // 添加可选的高级配置参数
    if (m_config.enableItn) {
        req["request"]["enable_itn"] = m_config.enableItn;
    }
    if (m_config.enableTimestamp) {
        req["request"]["enable_timestamp"] = m_config.enableTimestamp;
    }
    if (m_config.enableVoiceDetection) {
        req["request"]["enable_voice_detection"] = m_config.enableVoiceDetection;
    }
    if (m_config.enableSemanticSentenceDetection) {
        req["request"]["enable_semantic_sentence_detection"] = m_config.enableSemanticSentenceDetection;
    }
    if (m_config.enableInverseTextNormalization) {
        req["request"]["enable_inverse_text_normalization"] = m_config.enableInverseTextNormalization;
    }
    if (m_config.enableWordTimeOffset) {
        req["request"]["enable_word_time_offset"] = m_config.enableWordTimeOffset;
    }
    if (m_config.enablePartialResult) {
        req["request"]["enable_partial_result"] = m_config.enablePartialResult;
    }
    if (m_config.enableFinalResult) {
        req["request"]["enable_final_result"] = m_config.enableFinalResult;
    }
    if (m_config.enableInterimResult) {
        req["request"]["enable_interim_result"] = m_config.enableInterimResult;
    }
    if (m_config.enableSilenceDetection) {
        req["request"]["enable_silence_detection"] = m_config.enableSilenceDetection;
        req["request"]["silence_threshold"] = m_config.silenceThreshold;
    }
    
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
    // 使用条件变量等待响应，避免忙等待
    std::unique_lock<std::mutex> lock(m_mutex);
    
    // 等待直到收到响应或超时
    bool received = m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
        return m_readyForAudio || m_finalResponseReceived;
    });
    
    if (received) {
        if (response) {
            // 返回实际的响应内容，如果没有则返回状态描述
            if (!m_lastResponse.empty()) {
                *response = m_lastResponse;
            } else if (m_readyForAudio) {
                *response = "Session started"; // 表示会话已开始
            } else {
                *response = "Final response received"; // 表示收到最终响应
            }
        }
        return true;
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

// 解析二进制协议，详见火山引擎 ASR WebSocket 协议文档：
// Header(4字节) + [序列号/错误码](4字节) + Payload Size(4字节) + Payload
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
    
    // 添加原始数据的十六进制打印，帮助调试
    std::string hexData;
    for (size_t i = 0; i < std::min(size_t(32), binaryData.size()); ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", static_cast<unsigned char>(binaryData[i]));
        hexData += hex;
        if ((i + 1) % 16 == 0) hexData += " ";
    }
    logWithTimestamp("  - 原始数据(前32字节): " + hexData);
#else
    uint8_t headerSize = binaryData[0] & 0x0F;
    uint8_t messageType = (binaryData[1] >> 4) & 0x0F;
    uint8_t serializationMethod = (binaryData[2] >> 4) & 0x0F;
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
        // 包含完整的识别结果，可能包含多个utterances和最终状态
        if (payload.size() < 8) {
            logErrorWithTimestamp("❌ payload 太小，无法解析序列号和payload size");
            return "";
        }
        
        // 解析序列号 (4字节，大端序，有符号)
        // sequence: 对应客户端发送的音频包序列号，用于匹配请求和响应
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
                // 根据序列化方法处理数据
                if (serializationMethod == JSON_SERIALIZATION) {
                    // 验证解压缩后的数据是否为有效的JSON
                    try {
                        json parsed = json::parse(decompressed);
                        (void)parsed; // 避免未使用变量警告
                        return decompressed;
                    } catch (const std::exception& e) {
                        logErrorWithTimestamp("❌ 解压缩后的数据不是有效JSON: " + std::string(e.what()));
                        logErrorWithTimestamp("❌ 解压缩数据内容: " + decompressed.substr(0, 100));
                        return "";
                    }
                } else {
                    // 非JSON序列化，直接返回解压缩后的数据
                    return decompressed;
                }
            }
        }
        
        // 验证未压缩的数据
        if (serializationMethod == JSON_SERIALIZATION) {
            // 验证未压缩的数据是否为有效的JSON
            try {
                json parsed = json::parse(payloadData);
                (void)parsed; // 避免未使用变量警告
                return payloadData;
            } catch (const std::exception& e) {
                logErrorWithTimestamp("❌ 未压缩的数据不是有效JSON: " + std::string(e.what()));
                logErrorWithTimestamp("❌ 数据内容: " + payloadData.substr(0, 100));
                return "";
            }
        } else {
            // 非JSON序列化，直接返回原始数据
            return payloadData;
        }
    }
    
    if (messageType == SERVER_ACK) {
        // 服务器确认响应: Header(4字节) + 序列号(4字节) + [Payload Size(4字节) + Payload]
        // ACK消息用于确认音频包已被服务器接收，可能包含额外的状态信息
        if (payload.size() < 4) {
            logErrorWithTimestamp("❌ ACK payload 太小，无法解析序列号");
            return "";
        }
        
        // 解析序列号 (4字节，大端序，有符号)
        // sequence: 确认的音频包序列号，表示服务器已成功接收该音频包
        int32_t sequence = 0;
        for (int i = 0; i < 4; ++i) {
            sequence = (sequence << 8) | static_cast<unsigned char>(payload[i]);
        }
        
#if ASR_ENABLE_PROTOCOL_LOG
        logWithTimestamp("  - ACK 序列号: " + std::to_string(sequence));
#endif
        
        // 检查是否有额外的 payload
        if (payload.size() >= 8) {
            // 解析 payload size (4字节，大端序)
            uint32_t payloadSize = 0;
            for (int i = 4; i < 8; ++i) {
                payloadSize = (payloadSize << 8) | static_cast<unsigned char>(payload[i]);
            }
            
#if ASR_ENABLE_PROTOCOL_LOG
            logWithTimestamp("  - ACK payload size: " + std::to_string(payloadSize));
#endif
            
            if (payload.size() >= 8 + payloadSize) {
                std::string payloadData = payload.substr(8, payloadSize);
                
                // 解压缩
                if (compressionType == GZIP_COMPRESSION) {
                    std::string decompressed = gzipDecompress(payloadData);
                    if (!decompressed.empty()) {
                        // 根据序列化方法处理数据
                        if (serializationMethod == JSON_SERIALIZATION) {
                            // 验证解压缩后的数据是否为有效的JSON
                            try {
                                json parsed = json::parse(decompressed);
                                (void)parsed; // 避免未使用变量警告
                                return decompressed;
                            } catch (const std::exception& e) {
                                logErrorWithTimestamp("❌ ACK解压缩后的数据不是有效JSON: " + std::string(e.what()));
                                return "";
                            }
                        } else {
                            // 非JSON序列化，直接返回解压缩后的数据
                            return decompressed;
                        }
                    }
                }
                
                // 验证未压缩的数据
                if (serializationMethod == JSON_SERIALIZATION) {
                    // 验证未压缩的数据是否为有效的JSON
                    try {
                        json parsed = json::parse(payloadData);
                        (void)parsed; // 避免未使用变量警告
                        return payloadData;
                    } catch (const std::exception& e) {
                        logErrorWithTimestamp("❌ ACK未压缩的数据不是有效JSON: " + std::string(e.what()));
                        return "";
                    }
                } else {
                    // 非JSON序列化，直接返回原始数据
                    return payloadData;
                }
            }
        }
        
        // 如果没有 payload，返回简单的 ACK 确认
        return "{\"type\":\"ack\",\"sequence\":" + std::to_string(sequence) + "}";
    }
    
    if (messageType == ERROR_RESPONSE) {
        // 错误响应: Header(4字节) + 错误码(4字节) + Payload Size(4字节) + Payload
        // 当服务器处理请求时发生错误时返回，包含详细的错误信息
        if (payload.size() < 8) {
            logErrorWithTimestamp("❌ 错误响应 payload 太小");
            return "";
        }
        
        // 解析错误码 (4字节，大端序)
        // errorCode: 火山引擎ASR官方错误码，用于标识具体的错误类型
        // 45xxxxxx: 客户端错误，55xxxxxx: 服务器错误
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
                // 根据序列化方法处理数据
                if (serializationMethod == JSON_SERIALIZATION) {
                    // 验证解压缩后的数据是否为有效的JSON
                    try {
                        json parsed = json::parse(decompressed);
                        (void)parsed; // 避免未使用变量警告
                        return decompressed;
                    } catch (const std::exception& e) {
                        logErrorWithTimestamp("❌ 错误响应解压缩后的数据不是有效JSON: " + std::string(e.what()));
                        return "";
                    }
                } else {
                    // 非JSON序列化，直接返回解压缩后的数据
                    return decompressed;
                }
            }
        }
        
        // 验证未压缩的数据
        if (serializationMethod == JSON_SERIALIZATION) {
            // 验证未压缩的数据是否为有效的JSON
            try {
                json parsed = json::parse(payloadData);
                (void)parsed; // 避免未使用变量警告
                return payloadData;
            } catch (const std::exception& e) {
                logErrorWithTimestamp("❌ 错误响应未压缩的数据不是有效JSON: " + std::string(e.what()));
                return "";
            }
        } else {
            // 非JSON序列化，直接返回原始数据
            return payloadData;
        }
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

bool AsrClient::testHandshake() {
    bool connected = this->connect();
    this->disconnect();
    return connected;
}

} // namespace Asr 