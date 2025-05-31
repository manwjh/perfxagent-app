#include "../../include/audio/audio_types.h"
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace perfx {
namespace audio {

AudioConfig AudioConfig::getDefaultInputConfig() {
    AudioConfig config;
    config.inputDevice = DeviceInfo{0, "Default Input", DeviceType::INPUT, 2, 0, 48000.0};
    return config;
}

AudioConfig AudioConfig::getDefaultOutputConfig() {
    AudioConfig config;
    config.outputDevice = DeviceInfo{0, "Default Output", DeviceType::OUTPUT, 0, 2, 48000.0};
    return config;
}

void AudioConfig::fromJson(const std::string& jsonStr) {
    auto j = json::parse(jsonStr);
    // 这里只做简单解析，实际可根据需要完善
    sampleRate = static_cast<SampleRate>(j.value("sampleRate", 48000));
    channels = static_cast<ChannelCount>(j.value("channels", 2));
    format = static_cast<SampleFormat>(j.value("format", 0));
    framesPerBuffer = j.value("framesPerBuffer", 256);
    recordingPath = j.value("recordingPath", "recordings");
    autoStartRecording = j.value("autoStartRecording", false);
    maxRecordingDuration = j.value("maxRecordingDuration", 3600);
}

std::string AudioConfig::toJson() const {
    json j;
    j["sampleRate"] = static_cast<int>(sampleRate);
    j["channels"] = static_cast<int>(channels);
    j["format"] = static_cast<int>(format);
    j["framesPerBuffer"] = framesPerBuffer;
    j["recordingPath"] = recordingPath;
    j["autoStartRecording"] = autoStartRecording;
    j["maxRecordingDuration"] = maxRecordingDuration;
    return j.dump();
}

} // namespace audio
} // namespace perfx 