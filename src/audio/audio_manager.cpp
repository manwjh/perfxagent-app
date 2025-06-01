/**
 * @file audio_manager.cpp
 * @brief 音频管理器实现文件，负责音频设备、处理器和线程的管理
 */

#include "../../include/audio/audio_manager.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <opus/opus.h>
#include <ogg/ogg.h>

namespace perfx {
namespace audio {

/**
 * @brief AudioManager的PIMPL实现类
 * 封装了AudioManager的具体实现细节
 */
class AudioManager::Impl {
public:
    /**
     * @brief 构造函数
     * 初始化成员变量
     */
    Impl() : initialized_(false), opusFrameLength_(20) {}

    /**
     * @brief 析构函数
     * 清理资源
     */
    ~Impl() {
        cleanup();
    }

    /**
     * @brief 初始化音频管理器
     * @return 初始化是否成功
     */
    bool initialize() {
        if (initialized_) return true;

        // 初始化音频设备
        device_ = std::make_unique<AudioDevice>();
        if (!device_->initialize()) {
            return false;
        }

        // 初始化音频处理器，使用默认配置
        processor_ = std::make_shared<AudioProcessor>();
        AudioConfig defaultConfig;
        defaultConfig.sampleRate = SampleRate::RATE_48000;
        defaultConfig.channels = ChannelCount::MONO;
        defaultConfig.format = SampleFormat::FLOAT32;
        defaultConfig.framesPerBuffer = 256;
        defaultConfig.encodingFormat = EncodingFormat::WAV;
        defaultConfig.opusFrameLength = opusFrameLength_;
        
        if (!processor_->initialize(defaultConfig)) {
            return false;
        }

        config_ = defaultConfig;
        initialized_ = true;
        return true;
    }

    /**
     * @brief 从配置文件加载配置
     * @param configPath 配置文件路径
     * @return 加载是否成功
     */
    bool loadConfig(const std::string& configPath) {
        try {
            std::ifstream file(configPath);
            if (!file.is_open()) {
                return false;
            }

            nlohmann::json j;
            file >> j;

            AudioConfig config;
            config.sampleRate = static_cast<SampleRate>(j["sampleRate"].get<int>());
            config.channels = static_cast<ChannelCount>(j["channels"].get<int>());
            config.format = static_cast<SampleFormat>(j["format"].get<int>());
            config.framesPerBuffer = j["framesPerBuffer"].get<int>();
            config.encodingFormat = static_cast<EncodingFormat>(j["encodingFormat"].get<int>());
            config.opusFrameLength = j["opusFrameLength"].get<int>();

            return updateConfig(config);
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief 保存配置到文件
     * @param configPath 配置文件路径
     * @return 保存是否成功
     */
    bool saveConfig(const std::string& configPath) {
        try {
            nlohmann::json j;
            j["sampleRate"] = static_cast<int>(config_.sampleRate);
            j["channels"] = static_cast<int>(config_.channels);
            j["format"] = static_cast<int>(config_.format);
            j["framesPerBuffer"] = config_.framesPerBuffer;
            j["encodingFormat"] = static_cast<int>(config_.encodingFormat);
            j["opusFrameLength"] = config_.opusFrameLength;

            std::ofstream file(configPath);
            if (!file.is_open()) {
                return false;
            }

            file << j.dump(4);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief 获取可用的音频设备列表
     * @return 设备信息列表
     */
    std::vector<DeviceInfo> getAvailableDevices() {
        return device_->getAvailableDevices();
    }

    /**
     * @brief 创建音频处理线程
     * @param config 音频配置
     * @return 音频线程指针
     */
    std::shared_ptr<AudioThread> createAudioThread(const AudioConfig& config) {
        auto thread = std::make_shared<AudioThread>();
        if (!thread->initialize(config)) {
            return nullptr;
        }
        return thread;
    }

    /**
     * @brief 获取音频处理器
     * @return 音频处理器指针
     */
    std::shared_ptr<AudioProcessor> getProcessor() {
        return processor_;
    }

    /**
     * @brief 更新音频配置
     * @param config 新的音频配置
     * @return 更新是否成功
     */
    bool updateConfig(const AudioConfig& config) {
        if (!initialized_) {
            return false;
        }

        config_ = config;
        opusFrameLength_ = config.opusFrameLength;
        return processor_->initialize(config);
    }

    /**
     * @brief 获取当前音频配置
     * @return 当前音频配置
     */
    AudioConfig getCurrentConfig() const {
        return config_;
    }

    /**
     * @brief 清理资源
     */
    void cleanup() {
        processor_.reset();
        device_.reset();
        initialized_ = false;
    }

    /**
     * @brief 将音频数据写入WAV文件
     * @param input 输入音频数据
     * @param frames 帧数
     * @param filename 输出文件名
     * @return 写入是否成功
     */
    bool writeWavFile(const void* input, size_t frames, const std::string& filename) {
        if (!input || frames == 0) return false;

        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        // 准备WAV文件头
        WavHeader header = {};
        memcpy(header.riff, "RIFF", 4);
        memcpy(header.wave, "WAVE", 4);
        memcpy(header.fmt, "fmt ", 4);
        memcpy(header.data, "data", 4);

        header.fmtSize = 16;
        header.format = 1; // PCM
        header.channels = static_cast<uint16_t>(config_.channels);
        header.sampleRate = static_cast<uint32_t>(config_.sampleRate);
        header.bitsPerSample = 16; // 16-bit PCM
        header.blockAlign = header.channels * header.bitsPerSample / 8;
        header.byteRate = header.sampleRate * header.blockAlign;
        
        // 计算数据大小
        size_t bytesPerFrame = header.channels * header.bitsPerSample / 8;
        header.dataSize = frames * bytesPerFrame;
        header.size = header.dataSize + sizeof(WavHeader) - 8;

        // 写入文件头
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // 转换为16-bit PCM并写入
        std::vector<int16_t> pcmData(frames * static_cast<int>(config_.channels));
        const float* floatInput = static_cast<const float*>(input);
        for (size_t i = 0; i < frames * static_cast<int>(config_.channels); ++i) {
            float sample = floatInput[i];
            // 限制在[-1.0, 1.0]范围内
            sample = std::max(-1.0f, std::min(1.0f, sample));
            // 转换为16-bit
            pcmData[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        
        file.write(reinterpret_cast<const char*>(pcmData.data()), pcmData.size() * sizeof(int16_t));

        return true;
    }

    /**
     * @brief 将音频数据写入Opus文件
     * @param input 输入音频数据
     * @param frames 帧数
     * @param filename 输出文件名
     * @return 写入是否成功
     */
    bool writeOpusFile(const void* input, size_t frames, const std::string& filename) {
        if (!input || frames == 0 || !processor_) return false;

        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        // 初始化OGG流
        ogg_stream_state os;
        ogg_page og;
        ogg_packet op;
        int serialno = rand();
        ogg_stream_init(&os, serialno);

        // 准备OpusHead包
        OggHeader header = {};
        memcpy(header.opus_head.magic, "OpusHead", 8);
        header.opus_head.version = 1;
        header.opus_head.channels = static_cast<uint8_t>(config_.channels);
        header.opus_head.pre_skip = 0;
        header.opus_head.sample_rate = static_cast<uint32_t>(config_.sampleRate);
        header.opus_head.output_gain = 0;
        header.opus_head.channel_mapping = 0;

        // 准备OpusTags包
        memcpy(header.opus_tags.magic, "OpusTags", 8);
        const char* vendor = "perfxagent";
        header.opus_tags.vendor_length = strlen(vendor);
        strncpy(header.opus_tags.vendor_string, vendor, sizeof(header.opus_tags.vendor_string));
        header.opus_tags.user_comment_list_length = 0;

        // 写入OpusHead包
        op.packet = reinterpret_cast<unsigned char*>(&header.opus_head);
        op.bytes = sizeof(header.opus_head);
        op.b_o_s = 1;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 0;

        ogg_stream_packetin(&os, &op);
        while (ogg_stream_pageout(&os, &og)) {
            file.write((char*)og.header, og.header_len);
            file.write((char*)og.body, og.body_len);
        }

        // 写入OpusTags包
        op.packet = reinterpret_cast<unsigned char*>(&header.opus_tags);
        op.bytes = sizeof(header.opus_tags);
        op.b_o_s = 0;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 1;

        ogg_stream_packetin(&os, &op);
        while (ogg_stream_pageout(&os, &og)) {
            file.write((char*)og.header, og.header_len);
            file.write((char*)og.body, og.body_len);
        }

        // 计算每帧的采样数
        int samplesPerFrame = (static_cast<int>(config_.sampleRate) * opusFrameLength_) / 1000;
        int totalFrames = frames / samplesPerFrame;

        // 逐帧编码并写入
        for (int i = 0; i < totalFrames; i++) {
            const float* frameStart = static_cast<const float*>(input) + (i * samplesPerFrame * static_cast<int>(config_.channels));
            
            // 转换为16-bit PCM
            std::vector<int16_t> pcmData(samplesPerFrame * static_cast<int>(config_.channels));
            for (int j = 0; j < samplesPerFrame * static_cast<int>(config_.channels); j++) {
                pcmData[j] = static_cast<int16_t>(frameStart[j] * 32767.0f);
            }

            // 编码
            std::vector<uint8_t> encoded_data;
            if (!processor_->encodeOpus(pcmData.data(), samplesPerFrame, encoded_data)) {
                ogg_stream_clear(&os);
                return false;
            }

            // 创建OGG包
            op.packet = encoded_data.data();
            op.bytes = encoded_data.size();
            op.b_o_s = 0;
            op.e_o_s = (i == totalFrames - 1);
            op.granulepos = (i + 1) * samplesPerFrame;
            op.packetno = i + 2;

            ogg_stream_packetin(&os, &op);
            while (ogg_stream_pageout(&os, &og)) {
                file.write((char*)og.header, og.header_len);
                file.write((char*)og.body, og.body_len);
            }
        }

        // 清理
        ogg_stream_clear(&os);
        return true;
    }

    /**
     * @brief 从WAV文件读取音频数据
     * @param filename 输入文件名
     * @param output 输出音频数据
     * @param frames 帧数
     * @return 读取是否成功
     */
    bool readWavFile(const std::string& filename, std::vector<float>& output, size_t& frames) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        // 读取WAV文件头
        WavHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // 验证WAV格式
        if (memcmp(header.riff, "RIFF", 4) != 0 ||
            memcmp(header.wave, "WAVE", 4) != 0 ||
            memcmp(header.fmt, "fmt ", 4) != 0 ||
            memcmp(header.data, "data", 4) != 0 ||
            header.format != 1) {
            return false;
        }

        // 读取数据
        std::vector<int16_t> pcmData(header.dataSize / sizeof(int16_t));
        file.read(reinterpret_cast<char*>(pcmData.data()), header.dataSize);

        // 转换为float格式
        frames = pcmData.size() / header.channels;
        output.resize(pcmData.size());
        for (size_t i = 0; i < pcmData.size(); ++i) {
            output[i] = pcmData[i] / 32768.0f;
        }

        return true;
    }

    /**
     * @brief 生成输出文件名
     * @param format 文件格式
     * @param sampleRate 采样率
     * @param channels 声道数
     * @return 生成的文件名
     */
    std::string generateOutputFilename(const std::string& format, int sampleRate, ChannelCount channels) {
        std::string channelStr = (channels == ChannelCount::MONO) ? "mono" : "stereo";
        std::string extension = (format == "WAV") ? "wav" : "ogg";
        return std::string("recording_") + std::to_string(sampleRate) + "hz_" + channelStr + "." + extension;
    }

private:
    bool initialized_;                    // 初始化标志
    AudioConfig config_;                  // 音频配置
    std::unique_ptr<AudioDevice> device_; // 音频设备
    std::shared_ptr<AudioProcessor> processor_; // 音频处理器
    int opusFrameLength_;                 // Opus帧长度
};

// AudioManager单例实现
AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

// AudioManager构造函数和析构函数
AudioManager::AudioManager() : impl_(std::make_unique<Impl>()) {}
AudioManager::~AudioManager() = default;

// AudioManager公共接口实现
bool AudioManager::initialize() { return impl_->initialize(); }
bool AudioManager::loadConfig(const std::string& configPath) { return impl_->loadConfig(configPath); }
bool AudioManager::saveConfig(const std::string& configPath) { return impl_->saveConfig(configPath); }
std::vector<DeviceInfo> AudioManager::getAvailableDevices() { return impl_->getAvailableDevices(); }
std::shared_ptr<AudioThread> AudioManager::createAudioThread(const AudioConfig& config) { return impl_->createAudioThread(config); }
std::shared_ptr<AudioProcessor> AudioManager::getProcessor() { return impl_->getProcessor(); }
bool AudioManager::updateConfig(const AudioConfig& config) { return impl_->updateConfig(config); }
AudioConfig AudioManager::getCurrentConfig() const { return impl_->getCurrentConfig(); }
void AudioManager::cleanup() { impl_->cleanup(); }

// 文件操作函数实现
bool AudioManager::writeWavFile(const void* input, size_t frames, const std::string& filename) {
    return impl_->writeWavFile(input, frames, filename);
}

bool AudioManager::writeOpusFile(const void* input, size_t frames, const std::string& filename) {
    return impl_->writeOpusFile(input, frames, filename);
}

bool AudioManager::readWavFile(const std::string& filename, std::vector<float>& output, size_t& frames) {
    return impl_->readWavFile(filename, output, frames);
}

std::string AudioManager::generateOutputFilename(const std::string& format, int sampleRate, ChannelCount channels) {
    return impl_->generateOutputFilename(format, sampleRate, channels);
}

//==============================================================================
// AudioConfig类实现
//==============================================================================

/**
 * @brief 获取默认输入配置
 * @return 默认输入配置
 */
AudioConfig AudioConfig::getDefaultInputConfig() {
    AudioConfig config;
    config.inputDevice = DeviceInfo{0, "Default Input", DeviceType::INPUT, 2, 0, 48000.0};
    return config;
}

/**
 * @brief 获取默认输出配置
 * @return 默认输出配置
 */
AudioConfig AudioConfig::getDefaultOutputConfig() {
    AudioConfig config;
    config.outputDevice = DeviceInfo{0, "Default Output", DeviceType::OUTPUT, 0, 2, 48000.0};
    return config;
}

/**
 * @brief 从JSON字符串加载配置
 * @param jsonStr JSON字符串
 */
void AudioConfig::fromJson(const std::string& jsonStr) {
    using nlohmann::json;
    auto j = json::parse(jsonStr);
    sampleRate = static_cast<SampleRate>(j.value("sampleRate", 48000));
    channels = static_cast<ChannelCount>(j.value("channels", 2));
    format = static_cast<SampleFormat>(j.value("format", 0));
    framesPerBuffer = j.value("framesPerBuffer", 256);
    recordingPath = j.value("recordingPath", "recordings");
    autoStartRecording = j.value("autoStartRecording", false);
    maxRecordingDuration = j.value("maxRecordingDuration", 3600);
}

/**
 * @brief 将配置转换为JSON字符串
 * @return JSON字符串
 */
std::string AudioConfig::toJson() const {
    using nlohmann::json;
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