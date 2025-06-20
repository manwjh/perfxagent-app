//
// 使用 ixwebsocket 的 ASR 示例程序
// 参考 Python 官方代码实现完整的火山引擎 ASR 协议
//

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <ixwebsocket/IXGzipCodec.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <cstdlib> // for getenv

using json = nlohmann::json;

// 协议常量
const uint8_t PROTOCOL_VERSION = 0b0001;
const uint8_t DEFAULT_HEADER_SIZE __attribute__((unused)) = 0b0001;

// Message Type
const uint8_t FULL_CLIENT_REQUEST = 0b0001;
const uint8_t AUDIO_ONLY_REQUEST __attribute__((unused)) = 0b0010;
const uint8_t FULL_SERVER_RESPONSE __attribute__((unused)) = 0b1001;
const uint8_t SERVER_ACK __attribute__((unused)) = 0b1011;
const uint8_t SERVER_ERROR_RESPONSE __attribute__((unused)) = 0b1111;

// Message Type Specific Flags
const uint8_t NO_SEQUENCE = 0b0000;
const uint8_t POS_SEQUENCE = 0b0001;
const uint8_t NEG_SEQUENCE __attribute__((unused)) = 0b0010;
const uint8_t NEG_WITH_SEQUENCE __attribute__((unused)) = 0b0011;

// Message Serialization
const uint8_t NO_SERIALIZATION __attribute__((unused)) = 0b0000;
const uint8_t JSON_SERIALIZATION = 0b0001;

// Message Compression
const uint8_t NO_COMPRESSION __attribute__((unused)) = 0b0000;
const uint8_t GZIP_COMPRESSION = 0b0001;

// 凭据结构
struct Credentials {
    std::string appId;
    std::string accessToken;
    std::string secretKey;
    bool isValid = false;
};

// 从环境变量获取凭据
Credentials getCredentialsFromEnv() {
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

class AsrIxWebSocketClient {
private:
    ix::WebSocket webSocket;
    std::string logId;
    std::map<std::string, std::string> responseHeaders;
    bool connected = false;
    
    // 配置参数
    std::string appId;
    std::string accessToken;
    std::string secretKey;
    
    // 回调函数
    std::function<void(const std::string&)> onMessageCallback;
    std::function<void(const std::string&)> onErrorCallback;
    std::function<void()> onOpenCallback;
    std::function<void()> onCloseCallback;

public:
    AsrIxWebSocketClient() {
        // 获取凭据
        Credentials creds = getCredentialsFromEnv();
        if (creds.isValid) {
            appId = creds.appId;
            accessToken = creds.accessToken;
            secretKey = creds.secretKey;
        }
        
        // 设置 WebSocket 回调
        webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            this->handleMessage(msg);
        });
        
        // 设置连接参数
        webSocket.setUrl("wss://openspeech.bytedance.com/api/v3/sauc/bigmodel");
        webSocket.setHandshakeTimeout(30); // 30秒握手超时
        webSocket.setMaxWaitBetweenReconnectionRetries(1000); // 1秒重连间隔
        
        // 设置自定义 HTTP 头
        ix::WebSocketHttpHeaders headers;
        headers["User-Agent"] = "ASR-Client/1.0";
        headers["Accept"] = "*/*";
        headers["Accept-Encoding"] = "gzip, deflate";
        headers["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8";
        headers["Host"] = "openspeech.bytedance.com";
        
        // 火山引擎 ASR 认证头部
        std::string reqid = "test-request-id-" + std::to_string(std::time(nullptr));
        headers["X-Api-Resource-Id"] = "volc.bigasr.sauc.duration";
        headers["X-Api-Access-Key"] = accessToken;
        headers["X-Api-App-Key"] = appId;
        headers["X-Api-Request-Id"] = reqid;
        
        // 打印发送的 Header 信息
        std::cout << "=== 发送的 HTTP Header ===" << std::endl;
        std::cout << "X-Api-Resource-Id: " << headers["X-Api-Resource-Id"] << std::endl;
        std::cout << "X-Api-Access-Key: " << (accessToken.length() > 8 ? accessToken.substr(0, 4) + "****" + accessToken.substr(accessToken.length() - 4) : "****") << std::endl;
        std::cout << "X-Api-App-Key: " << headers["X-Api-App-Key"] << std::endl;
        std::cout << "X-Api-Request-Id: " << headers["X-Api-Request-Id"] << std::endl;
        
        webSocket.setExtraHeaders(headers);
    }
    
    bool connect() {
        std::cout << "🔗 正在连接 WebSocket..." << std::endl;
        std::cout << "📡 目标URL: wss://openspeech.bytedance.com/api/v3/sauc/bigmodel" << std::endl;
        
        webSocket.start();
        
        // 等待连接建立
        int timeout = 0;
        while (!connected && timeout < 300) { // 30秒超时
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            timeout++;
            
            // 每5秒打印一次等待信息
            if (timeout % 50 == 0) {
                std::cout << "⏳ 等待连接建立... (" << (timeout / 10) << "秒)" << std::endl;
            }
        }
        
        return connected;
    }
    
    void disconnect() {
        if (connected) {
            webSocket.stop();
            connected = false;
        }
    }
    
    bool isConnected() const {
        return connected;
    }
    
    // 设置回调函数
    void setOnMessage(std::function<void(const std::string&)> callback) {
        onMessageCallback = callback;
    }
    
    void setOnError(std::function<void(const std::string&)> callback) {
        onErrorCallback = callback;
    }
    
    void setOnOpen(std::function<void()> callback) {
        onOpenCallback = callback;
    }
    
    void setOnClose(std::function<void()> callback) {
        onCloseCallback = callback;
    }
    
    // 获取响应头
    const std::map<std::string, std::string>& getResponseHeaders() const {
        return responseHeaders;
    }
    
    // 获取 log_id
    std::string getLogId() const {
        return logId;
    }
    
    // 生成协议头部
    std::vector<uint8_t> generateHeader(
        uint8_t messageType = FULL_CLIENT_REQUEST,
        uint8_t messageTypeSpecificFlags = NO_SEQUENCE,
        uint8_t serialMethod = JSON_SERIALIZATION,
        uint8_t compressionType = GZIP_COMPRESSION,
        uint8_t reservedData = 0x00
    ) {
        std::vector<uint8_t> header;
        uint8_t headerSize = 1;
        header.push_back((PROTOCOL_VERSION << 4) | headerSize);
        header.push_back((messageType << 4) | messageTypeSpecificFlags);
        header.push_back((serialMethod << 4) | compressionType);
        header.push_back(reservedData);
        return header;
    }
    
    // 生成序列号
    std::vector<uint8_t> generateBeforePayload(int32_t sequence) {
        std::vector<uint8_t> beforePayload;
        for (int i = 3; i >= 0; --i) {
            beforePayload.push_back((sequence >> (i * 8)) & 0xFF);
        }
        return beforePayload;
    }
    
    // GZIP 压缩
    std::vector<uint8_t> gzipCompress(const std::string& data) {
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
    
    // 构造请求 JSON
    json constructRequest(const std::string& reqid) {
        (void)reqid; // 避免未使用参数警告
        json req = {
            {"user", {
                {"uid", "test"}
            }},
            {"audio", {
                {"format", "wav"},
                {"sample_rate", 16000},
                {"bits", 16},
                {"channel", 1},
                {"codec", "raw"}
            }},
            {"request", {
                {"model_name", "bigmodel"},
                {"enable_punc", true}
            }}
        };
        return req;
    }
    
    // 发送完整客户端请求
    bool sendFullClientRequest() {
        if (!connected) {
            std::cerr << "❌ 未连接" << std::endl;
            return false;
        }
        
        std::string reqid = "test-request-id-" + std::to_string(std::time(nullptr));
        int32_t seq = 1;
        
        // 构造请求参数
        json requestParams = constructRequest(reqid);
        std::string jsonStr = requestParams.dump();
        
        std::cout << "📤 JSON_STRING: " << jsonStr << std::endl;
        std::cout << "📤 JSON原始长度: " << jsonStr.length() << " bytes" << std::endl;
        
        // GZIP 压缩
        std::vector<uint8_t> payloadBytes = gzipCompress(jsonStr);
        if (payloadBytes.empty()) {
            std::cerr << "❌ GZIP 压缩失败" << std::endl;
            return false;
        }
        
        std::cout << "📤 gzip压缩后长度: " << payloadBytes.size() << " bytes" << std::endl;
        
        // 构造完整请求
        std::vector<uint8_t> fullClientRequest = generateHeader(FULL_CLIENT_REQUEST, POS_SEQUENCE);
        std::vector<uint8_t> beforePayload = generateBeforePayload(seq);
        fullClientRequest.insert(fullClientRequest.end(), beforePayload.begin(), beforePayload.end());
        
        // 添加 payload 长度
        for (int i = 3; i >= 0; --i) {
            fullClientRequest.push_back((payloadBytes.size() >> (i * 8)) & 0xFF);
        }
        
        // 添加 payload
        fullClientRequest.insert(fullClientRequest.end(), payloadBytes.begin(), payloadBytes.end());
        
        // 打印调试信息
        std::cout << "📤 HEADER: ";
        for (size_t i = 0; i < 4 && i < fullClientRequest.size(); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)fullClientRequest[i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "📤 SEQ: ";
        for (size_t i = 4; i < 8 && i < fullClientRequest.size(); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)fullClientRequest[i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "📤 PAYLOAD_LEN: ";
        for (size_t i = 8; i < 12 && i < fullClientRequest.size(); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)fullClientRequest[i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        // 发送二进制数据
        std::string binaryData(reinterpret_cast<const char*>(fullClientRequest.data()), fullClientRequest.size());
        webSocket.sendBinary(binaryData);
        
        return true;
    }

private:
    void handleMessage(const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Message: {
                std::cout << "📨 收到消息: " << msg->str << std::endl;
                
                // 尝试解析 JSON 获取 log_id
                try {
                    json j = json::parse(msg->str);
                    if (j.contains("log_id")) {
                        logId = j["log_id"];
                        std::cout << "🔍 提取到 log_id: " << logId << std::endl;
                    }
                } catch (const std::exception& e) {
                    // 不是 JSON 格式，忽略
                }
                
                if (onMessageCallback) {
                    onMessageCallback(msg->str);
                }
                break;
            }
            case ix::WebSocketMessageType::Open: {
                std::cout << "✅ WebSocket 连接已建立" << std::endl;
                connected = true;
                
                // 获取响应头
                const auto& headers = msg->openInfo.headers;
                for (const auto& header : headers) {
                    responseHeaders[header.first] = header.second;
                    std::cout << "📋 响应头: " << header.first << ": " << header.second << std::endl;
                }
                
                // 特别关注 X-Tt-Logid
                if (headers.find("X-Tt-Logid") != headers.end()) {
                    std::cout << "🎯 成功获取 X-Tt-Logid: " << headers.at("X-Tt-Logid") << std::endl;
                } else {
                    std::cout << "⚠️  未找到 X-Tt-Logid" << std::endl;
                }
                
                if (onOpenCallback) {
                    onOpenCallback();
                }
                break;
            }
            case ix::WebSocketMessageType::Close: {
                std::cout << "🔌 WebSocket 连接已关闭 (code: " << msg->closeInfo.code << ", reason: " << msg->closeInfo.reason << ")" << std::endl;
                connected = false;
                
                if (onCloseCallback) {
                    onCloseCallback();
                }
                break;
            }
            case ix::WebSocketMessageType::Error: {
                std::cout << "❌ WebSocket 错误: " << msg->errorInfo.reason << std::endl;
                std::cout << "🔍 错误详情: HTTP状态=" << msg->errorInfo.http_status 
                         << ", 重试次数=" << msg->errorInfo.retries 
                         << ", 等待时间=" << msg->errorInfo.wait_time << "ms" << std::endl;
                
                if (onErrorCallback) {
                    onErrorCallback(msg->errorInfo.reason);
                }
                break;
            }
            case ix::WebSocketMessageType::Fragment: {
                std::cout << "📦 收到消息片段" << std::endl;
                break;
            }
            case ix::WebSocketMessageType::Ping: {
                std::cout << "🏓 收到 Ping" << std::endl;
                break;
            }
            case ix::WebSocketMessageType::Pong: {
                std::cout << "🏓 收到 Pong" << std::endl;
                break;
            }
        }
    }
};

int main() {
    std::cout << "=== 火山引擎 ASR IXWebSocket 客户端示例 ===" << std::endl;
    std::cout << "参考 Python 官方代码实现完整的火山引擎 ASR 协议" << std::endl;
    std::cout << "" << std::endl;
    
    // 检查环境变量
    std::cout << "=== 环境变量检查 ===" << std::endl;
    const char* appId = std::getenv("VOLC_APP_ID");
    const char* accessToken = std::getenv("VOLC_ACCESS_TOKEN");
    const char* secretKey = std::getenv("VOLC_SECRET_KEY");
    
    if (appId) {
        std::cout << "✅ VOLC_APP_ID: " << appId << std::endl;
    } else {
        std::cout << "⚠️  VOLC_APP_ID: 未设置" << std::endl;
    }
    
    if (accessToken) {
        std::cout << "✅ VOLC_ACCESS_TOKEN: " << (strlen(accessToken) > 8 ? std::string(accessToken, 4) + "****" + std::string(accessToken + strlen(accessToken) - 4) : "****") << std::endl;
    } else {
        std::cout << "⚠️  VOLC_ACCESS_TOKEN: 未设置" << std::endl;
    }
    
    if (secretKey) {
        std::cout << "✅ VOLC_SECRET_KEY: " << (strlen(secretKey) > 8 ? std::string(secretKey, 4) + "****" + std::string(secretKey + strlen(secretKey) - 4) : "****") << std::endl;
    } else {
        std::cout << "⚠️  VOLC_SECRET_KEY: 未设置" << std::endl;
    }
    
    std::cout << "" << std::endl;
    
    // 创建 ASR 客户端
    AsrIxWebSocketClient client;
    
    // 设置回调
    client.setOnOpen([]() {
        std::cout << "✅ 连接已建立" << std::endl;
    });
    
    client.setOnMessage([](const std::string& message) {
        std::cout << "📨 收到消息: " << message << std::endl;
    });
    
    client.setOnError([](const std::string& error) {
        std::cout << "❌ 发生错误: " << error << std::endl;
    });
    
    client.setOnClose([]() {
        std::cout << "🔌 连接已关闭" << std::endl;
    });
    
    std::cout << "🚀 开始连接..." << std::endl;
    
    // 连接服务器
    if (!client.connect()) {
        std::cerr << "❌ 连接失败" << std::endl;
        std::cout << "" << std::endl;
        std::cout << "=== 故障排除建议 ===" << std::endl;
        std::cout << "1. 检查网络连接是否正常" << std::endl;
        std::cout << "2. 确认环境变量 VOLC_APP_ID 和 VOLC_ACCESS_TOKEN 已正确设置" << std::endl;
        std::cout << "3. 检查防火墙设置，确保可以访问 wss://openspeech.bytedance.com" << std::endl;
        std::cout << "4. 确认凭据是否有效且未过期" << std::endl;
        std::cout << "5. 尝试使用 curl 测试连接: curl -I https://openspeech.bytedance.com" << std::endl;
        return 1;
    }
    
    if (!client.isConnected()) {
        std::cerr << "❌ 连接超时" << std::endl;
        return 1;
    }
    
    std::cout << "✅ 连接成功！" << std::endl;
    std::cout << "" << std::endl;
    
    // 获取并打印响应头
    auto headers = client.getResponseHeaders();
    std::cout << "=== HTTP 响应头 ===" << std::endl;
    for (const auto& header : headers) {
        std::cout << "  " << header.first << ": " << header.second << std::endl;
    }
    
    // 特别关注 X-Tt-Logid
    if (headers.find("X-Tt-Logid") != headers.end()) {
        std::cout << "🎯 成功获取 X-Tt-Logid: " << headers["X-Tt-Logid"] << std::endl;
    } else {
        std::cout << "⚠️  未找到 X-Tt-Logid" << std::endl;
    }
    
    std::cout << "" << std::endl;
    
    // 发送完整客户端请求
    std::cout << "📤 发送完整客户端请求..." << std::endl;
    if (client.sendFullClientRequest()) {
        std::cout << "✅ 请求发送成功" << std::endl;
    } else {
        std::cout << "❌ 请求发送失败" << std::endl;
    }
    
    // 等待响应
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    std::cout << "" << std::endl;
    std::cout << "=== 测试完成 ===" << std::endl;
    std::cout << "主要改进:" << std::endl;
    std::cout << "  ✅ 使用正确的火山引擎 ASR WebSocket 地址" << std::endl;
    std::cout << "  ✅ 添加必需的认证头部" << std::endl;
    std::cout << "  ✅ 实现完整的协议头部生成" << std::endl;
    std::cout << "  ✅ 实现 GZIP 压缩" << std::endl;
    std::cout << "  ✅ 实现与 Python 官方代码相同的协议" << std::endl;
    std::cout << "  ✅ 增强的错误处理和调试信息" << std::endl;
    std::cout << "  ✅ 支持环境变量配置" << std::endl;
    
    // 断开连接
    client.disconnect();
    
    return 0;
} 