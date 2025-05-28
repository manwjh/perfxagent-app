#include "websocket_message.h"
#include <stdexcept>

namespace perfxagent {
namespace connection {

namespace {
    std::string messageTypeToString(MessageType type) {
        switch (type) {
            case MessageType::HELLO: return "hello";
            case MessageType::LISTEN: return "listen";
            case MessageType::TTS: return "tts";
            case MessageType::ABORT: return "abort";
            case MessageType::IOT: return "iot";
            case MessageType::LLM: return "llm";
            default: throw std::runtime_error("Unknown message type");
        }
    }

    MessageType stringToMessageType(const std::string& str) {
        if (str == "hello") return MessageType::HELLO;
        if (str == "listen") return MessageType::LISTEN;
        if (str == "tts") return MessageType::TTS;
        if (str == "abort") return MessageType::ABORT;
        if (str == "iot") return MessageType::IOT;
        if (str == "llm") return MessageType::LLM;
        throw std::runtime_error("Unknown message type string: " + str);
    }

    std::string listenStateToString(ListenState state) {
        switch (state) {
            case ListenState::START: return "start";
            case ListenState::STOP: return "stop";
            case ListenState::DETECT: return "detect";
            default: throw std::runtime_error("Unknown listen state");
        }
    }

    ListenState stringToListenState(const std::string& str) {
        if (str == "start") return ListenState::START;
        if (str == "stop") return ListenState::STOP;
        if (str == "detect") return ListenState::DETECT;
        throw std::runtime_error("Unknown listen state string: " + str);
    }

    std::string listenModeToString(ListenMode mode) {
        switch (mode) {
            case ListenMode::AUTO: return "auto";
            case ListenMode::MANUAL: return "manual";
            case ListenMode::REALTIME: return "realtime";
            default: throw std::runtime_error("Unknown listen mode");
        }
    }

    ListenMode stringToListenMode(const std::string& str) {
        if (str == "auto") return ListenMode::AUTO;
        if (str == "manual") return ListenMode::MANUAL;
        if (str == "realtime") return ListenMode::REALTIME;
        throw std::runtime_error("Unknown listen mode string: " + str);
    }
}

nlohmann::json serializeMessage(const BaseMessage& msg) {
    nlohmann::json json;
    
    if (msg.session_id) {
        json["session_id"] = *msg.session_id;
    }
    
    json["type"] = messageTypeToString(msg.type);

    // 根据具体消息类型添加额外字段
    switch (msg.type) {
        case MessageType::HELLO: {
            const auto& hello = static_cast<const HelloMessage&>(msg);
            json["version"] = hello.version;
            json["transport"] = hello.transport;
            json["audio_params"] = {
                {"format", hello.audio_params.format},
                {"sample_rate", hello.audio_params.sample_rate},
                {"channels", hello.audio_params.channels},
                {"frame_duration", hello.audio_params.frame_duration}
            };
            break;
        }
        case MessageType::LISTEN: {
            const auto& listen = static_cast<const ListenMessage&>(msg);
            json["state"] = listenStateToString(listen.state);
            if (listen.mode) {
                json["mode"] = listenModeToString(*listen.mode);
            }
            if (listen.text) {
                json["text"] = *listen.text;
            }
            break;
        }
        case MessageType::TTS: {
            const auto& tts = static_cast<const TTSMessage&>(msg);
            json["state"] = tts.state;
            if (tts.text) {
                json["text"] = *tts.text;
            }
            break;
        }
        case MessageType::ABORT: {
            const auto& abort = static_cast<const AbortMessage&>(msg);
            if (abort.reason) {
                json["reason"] = *abort.reason;
            }
            break;
        }
        case MessageType::IOT: {
            const auto& iot = static_cast<const IoTMessage&>(msg);
            if (iot.descriptors) {
                json["descriptors"] = *iot.descriptors;
            }
            if (iot.states) {
                json["states"] = *iot.states;
            }
            break;
        }
        case MessageType::LLM: {
            const auto& llm = static_cast<const LLMMessage&>(msg);
            json["emotion"] = llm.emotion;
            break;
        }
    }

    return json;
}

std::shared_ptr<BaseMessage> deserializeMessage(const std::string& json_str) {
    auto json = nlohmann::json::parse(json_str);
    auto type = stringToMessageType(json["type"]);

    std::shared_ptr<BaseMessage> msg;
    
    switch (type) {
        case MessageType::HELLO: {
            auto hello = std::make_shared<HelloMessage>();
            hello->version = json["version"];
            hello->transport = json["transport"];
            hello->audio_params = {
                json["audio_params"]["format"],
                json["audio_params"]["sample_rate"],
                json["audio_params"]["channels"],
                json["audio_params"]["frame_duration"]
            };
            msg = hello;
            break;
        }
        case MessageType::LISTEN: {
            auto listen = std::make_shared<ListenMessage>();
            listen->state = stringToListenState(json["state"]);
            if (json.contains("mode")) {
                listen->mode = stringToListenMode(json["mode"]);
            }
            if (json.contains("text")) {
                listen->text = json["text"];
            }
            msg = listen;
            break;
        }
        case MessageType::TTS: {
            auto tts = std::make_shared<TTSMessage>();
            tts->state = json["state"];
            if (json.contains("text")) {
                tts->text = json["text"];
            }
            msg = tts;
            break;
        }
        case MessageType::ABORT: {
            auto abort = std::make_shared<AbortMessage>();
            if (json.contains("reason")) {
                abort->reason = json["reason"];
            }
            msg = abort;
            break;
        }
        case MessageType::IOT: {
            auto iot = std::make_shared<IoTMessage>();
            if (json.contains("descriptors")) {
                iot->descriptors = json["descriptors"];
            }
            if (json.contains("states")) {
                iot->states = json["states"];
            }
            msg = iot;
            break;
        }
        case MessageType::LLM: {
            auto llm = std::make_shared<LLMMessage>();
            llm->emotion = json["emotion"];
            msg = llm;
            break;
        }
    }

    if (json.contains("session_id")) {
        msg->session_id = json["session_id"];
    }
    msg->type = type;

    return msg;
}

} // namespace connection
} // namespace perfxagent 