#include "ui/config_manager.h"
#include <QStringList>

namespace perfx {
namespace ui {

// ============================================================================
// ConfigValidator 实现
// ============================================================================

QStringList ConfigValidator::validationErrors_;

bool ConfigValidator::validateAsrConfig(const AsrConfig& config) {
    validationErrors_.clear();
    
    // 验证认证信息
    if (config.appId.empty()) {
        validationErrors_.append("应用ID不能为空");
    }
    
    if (config.accessToken.empty()) {
        validationErrors_.append("访问令牌不能为空");
    }
    
    // 验证音频配置
    if (!validateAudioConfig(config)) {
        return false;
    }
    
    // 验证连接配置
    if (config.handshakeTimeout < 1 || config.handshakeTimeout > 300) {
        validationErrors_.append("握手超时时间必须在1-300秒之间");
    }
    
    if (config.connectionTimeout < 1 || config.connectionTimeout > 300) {
        validationErrors_.append("连接超时时间必须在1-300秒之间");
    }
    
    return validationErrors_.isEmpty();
}

bool ConfigValidator::validateAudioConfig(const AsrConfig& config) {
    // 验证采样率
    if (config.sampleRate < 8000 || config.sampleRate > 48000) {
        validationErrors_.append("采样率必须在8000-48000之间");
    }
    
    // 验证位深度
    if (config.bits != 8 && config.bits != 16 && config.bits != 24 && config.bits != 32) {
        validationErrors_.append("位深度必须是8、16、24或32");
    }
    
    // 验证声道数
    if (config.channels < 1 || config.channels > 2) {
        validationErrors_.append("声道数必须是1或2");
    }
    
    // 验证分段时长
    if (config.segDuration < 50 || config.segDuration > 1000) {
        validationErrors_.append("分段时长必须在50-1000ms之间");
    }
    
    // 验证音频格式
    if (config.format.empty()) {
        validationErrors_.append("音频格式不能为空");
    }
    
    // 验证语言设置
    if (config.language.empty()) {
        validationErrors_.append("语言设置不能为空");
    }
    
    return validationErrors_.isEmpty();
}

QStringList ConfigValidator::getValidationErrors() {
    return validationErrors_;
}

} // namespace perfx
} // namespace ui 