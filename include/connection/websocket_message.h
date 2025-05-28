#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace perfxagent {
namespace connection {

// 消息类型枚举
enum class MessageType {
    HELLO,
    LISTEN,
    TTS,
    ABORT,
    IOT,
    LLM
};

// 监听状态枚举
enum class ListenState {
    START,
    STOP,
    DETECT
};

// 监听模式枚举
enum class ListenMode {
    AUTO,
    MANUAL,
    REALTIME
};

// 音频参数结构
struct AudioParams {
    std::string format;
    int sample_rate;
    int channels;
    int frame_duration;
};

// 基础消息结构
struct BaseMessage {
    std::optional<std::string> session_id;
    MessageType type;
};

// Hello消息
struct HelloMessage : public BaseMessage {
    int version;
    std::string transport;
    AudioParams audio_params;
};

// 监听消息
struct ListenMessage : public BaseMessage {
    ListenState state;
    std::optional<ListenMode> mode;
    std::optional<std::string> text;
};

// TTS消息
struct TTSMessage : public BaseMessage {
    std::string state;
    std::optional<std::string> text;
};

// 中止消息
struct AbortMessage : public BaseMessage {
    std::optional<std::string> reason;
};

// IoT消息
struct IoTMessage : public BaseMessage {
    std::optional<nlohmann::json> descriptors;
    std::optional<nlohmann::json> states;
};

// LLM消息
struct LLMMessage : public BaseMessage {
    std::string emotion;
};

// 消息序列化/反序列化函数
nlohmann::json serializeMessage(const BaseMessage& msg);
std::shared_ptr<BaseMessage> deserializeMessage(const std::string& json_str);

} // namespace connection
} // namespace perfxagent 