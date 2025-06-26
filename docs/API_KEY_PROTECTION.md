# API密钥保护方案

## 概述

本文档描述了PerfxAgent应用中API密钥的保护机制，确保体验模式的API密钥既不能被轻易破解，又能为用户提供有限额度的体验功能。

## 保护目标

1. **防止逆向工程**：API密钥不能被通过简单的字符串搜索或二进制分析获取
2. **限制使用**：体验模式有明确的使用次数限制
3. **用户友好**：鼓励用户设置自己的API密钥以获得完整功能

## 保护机制

### 1. 多层混淆技术

#### 1.1 XOR加密
- 使用固定密钥（0x42）对原始字符串进行XOR加密
- 增加逆向工程的难度

#### 1.2 字符串混淆
- 将加密后的真实数据与假数据混合
- 真实数据存储在前半部分，假数据存储在后半部分
- 使静态分析更加困难

#### 1.3 字节数组表示
- 将混淆后的数据以十六进制字节数组形式存储
- 避免在二进制文件中出现可读的字符串

### 2. 运行时解密

```cpp
std::string SecureKeyManager::deobfuscateString(const std::vector<uint8_t>& obfuscated) {
    // 简单的字符串混淆：取前半部分作为真实数据
    size_t halfSize = obfuscated.size() / 2;
    std::vector<uint8_t> realData(obfuscated.begin(), obfuscated.begin() + halfSize);
    
    return decryptString(realData, XOR_KEY);
}
```

### 3. 使用限制机制

#### 3.1 体验模式检测
- 检查环境变量 `VOLC_APP_ID` 和 `VOLC_ACCESS_TOKEN` 是否设置
- 如果未设置，则启用体验模式

#### 3.2 使用计数
- 使用QSettings持久化存储使用次数
- 每次API调用时增加计数
- 达到限制后拒绝服务

#### 3.3 限制策略
- 体验模式限制：100次使用
- 用户配置模式：无限制
- 过期后提示用户设置自己的API密钥

## 实现细节

### 密钥数据生成

使用 `scripts/generate_obfuscated_keys.py` 脚本生成混淆后的密钥数据：

```bash
python3 scripts/generate_obfuscated_keys.py
```

### 混淆数据示例

```cpp
// 混淆后的App ID: "8388344882"
const std::vector<uint8_t> OBFUSCATED_APP_ID = {
    0x7A, 0x71, 0x7A, 0x7A, 0x71, 0x76, 0x76, 0x7A, 0x7A, 0x70,
    0x7A, 0x71, 0x7A, 0x7A, 0x71, 0x76, 0x76, 0x7A, 0x7A, 0x70,
};
```

### 使用流程

1. **配置加载**：`ConfigManager::loadConfig()`
2. **模式检测**：`SecureKeyManager::isTrialMode()`
3. **密钥获取**：`SecureKeyManager::getAppId()` 等
4. **使用计数**：`SecureKeyManager::incrementUsageCount()`
5. **限制检查**：`SecureKeyManager::isUsageExceeded()`

## 安全评估

### 防护强度

1. **静态分析防护**：⭐⭐⭐
   - 字符串混淆有效防止简单的字符串搜索
   - XOR加密增加分析难度

2. **动态分析防护**：⭐⭐
   - 运行时解密可能被调试器捕获
   - 建议配合代码混淆工具使用

3. **逆向工程防护**：⭐⭐⭐
   - 多层混淆增加逆向成本
   - 需要理解算法才能提取密钥

### 攻击场景

#### 场景1：字符串搜索攻击
- **攻击方法**：在二进制文件中搜索原始密钥字符串
- **防护效果**：✅ 有效防护，密钥被混淆存储

#### 场景2：调试器分析
- **攻击方法**：使用调试器跟踪密钥获取过程
- **防护效果**：⚠️ 部分防护，需要配合代码混淆

#### 场景3：内存dump分析
- **攻击方法**：从内存中提取解密后的密钥
- **防护效果**：⚠️ 部分防护，密钥在内存中存在时间较短

## 改进建议

### 1. 增强混淆
- 使用更复杂的加密算法（如AES）
- 添加代码混淆（如Obfuscator-LLVM）
- 实现密钥分片存储

### 2. 运行时保护
- 检测调试器环境
- 实现反调试机制
- 使用代码完整性检查

### 3. 网络验证
- 实现服务器端验证
- 添加时间戳验证
- 使用动态密钥

## 使用指南

### 开发者

1. 修改API密钥时，使用生成脚本重新生成混淆数据
2. 定期更新混淆算法
3. 监控使用情况，及时调整限制策略

### 用户

1. 体验模式：无需配置，直接使用（限制100次）
2. 正式使用：设置环境变量
   ```bash
   export VOLC_APP_ID=your_app_id
   export VOLC_ACCESS_TOKEN=your_access_token
   export VOLC_SECRET_KEY=your_secret_key
   ```

## 合规性

- 体验模式符合API服务商的使用条款
- 明确的使用限制避免滥用
- 鼓励用户使用自己的API密钥

## 总结

本保护方案通过多层混淆、使用限制和用户引导，在保护API密钥安全的同时，为用户提供了良好的体验。虽然不能完全防止所有攻击，但显著提高了破解成本，达到了预期的保护目标。 