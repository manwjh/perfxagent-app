# 🔒 Token 安全保护指南

## 概述
本文档说明如何安全地管理和使用 ASR 服务的访问令牌，避免凭据泄露。

## 🚨 安全风险
- **硬编码风险**: 将 token 直接写在代码中
- **版本控制泄露**: 将包含 token 的文件提交到 Git
- **日志泄露**: 在日志中打印完整的 token
- **内存泄露**: 程序崩溃时 token 可能被写入 core dump

## ✅ 推荐的安全做法

### 1. 使用环境变量（推荐）

#### 设置环境变量
```bash
# 临时设置（仅当前会话有效）
export ASR_APP_ID="your_app_id"
export ASR_ACCESS_TOKEN="your_access_token"
export ASR_SECRET_KEY="your_secret_key"

# 永久设置（添加到 ~/.bashrc 或 ~/.zshrc）
echo 'export ASR_APP_ID="your_app_id"' >> ~/.bashrc
echo 'export ASR_ACCESS_TOKEN="your_access_token"' >> ~/.bashrc
echo 'export ASR_SECRET_KEY="your_secret_key"' >> ~/.bashrc
source ~/.bashrc
```

#### 使用配置文件
```bash
# 创建 .env 文件
cp asr_config_example.env .env
# 编辑 .env 文件，填入真实凭据

# 加载环境变量
source .env
```

### 2. 使用配置文件（生产环境）

#### 创建配置文件
```bash
# 创建配置文件（确保权限正确）
touch ~/.asr_config
chmod 600 ~/.asr_config  # 只有所有者可读写
```

#### 配置文件内容
```ini
[ASR]
app_id=your_app_id
access_token=your_access_token
secret_key=your_secret_key
```

### 3. 使用密钥管理服务（企业级）

#### AWS Secrets Manager
```bash
# 存储密钥
aws secretsmanager create-secret \
    --name "asr-credentials" \
    --description "ASR service credentials" \
    --secret-string '{"app_id":"xxx","access_token":"xxx","secret_key":"xxx"}'

# 获取密钥
aws secretsmanager get-secret-value --secret-id "asr-credentials"
```

#### HashiCorp Vault
```bash
# 存储密钥
vault kv put secret/asr app_id=xxx access_token=xxx secret_key=xxx

# 获取密钥
vault kv get secret/asr
```

## 🛡️ 代码安全实践

### 1. 避免硬编码
```cpp
// ❌ 错误做法
#define ASR_ACCESS_TOKEN "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3"

// ✅ 正确做法
QString token = qgetenv("ASR_ACCESS_TOKEN");
if (token.isEmpty()) {
    // 处理错误或使用默认值
}
```

### 2. 隐藏敏感信息
```cpp
// ✅ 在日志中隐藏敏感信息
QString maskedToken = token;
if (maskedToken.length() > 8) {
    maskedToken = maskedToken.left(4) + "****" + maskedToken.right(4);
}
qDebug() << "Token:" << maskedToken;
```

### 3. 内存安全
```cpp
// ✅ 使用后清理内存
QString token = getToken();
// 使用 token
token.fill('*');  // 覆盖内存中的敏感数据
```

## 📋 安全检查清单

- [ ] 没有在代码中硬编码 token
- [ ] 没有将包含 token 的文件提交到版本控制
- [ ] 环境变量或配置文件权限设置正确
- [ ] 日志中没有打印完整的 token
- [ ] 生产环境使用密钥管理服务
- [ ] 定期轮换 token
- [ ] 监控 token 使用情况

## 🚫 禁止的做法

1. **不要在代码中硬编码 token**
2. **不要将 token 提交到 Git**
3. **不要在日志中打印完整 token**
4. **不要将 token 发送到第三方服务**
5. **不要将 token 存储在客户端代码中**

## 🔄 Token 轮换

### 定期轮换
- 建议每 30-90 天轮换一次 token
- 轮换时确保新旧 token 有重叠期
- 轮换后立即更新所有相关配置

### 紧急轮换
如果怀疑 token 泄露：
1. 立即在服务端禁用旧 token
2. 生成新 token
3. 更新所有使用该 token 的系统
4. 检查是否有异常使用记录

## 📞 紧急联系

如果发现 token 泄露：
1. 立即禁用相关 token
2. 检查系统日志
3. 联系服务提供商
4. 更新所有相关凭据

## 📚 相关资源

- [OWASP 安全指南](https://owasp.org/)
- [GitHub 安全最佳实践](https://docs.github.com/en/actions/security-guides/encrypted-secrets)
- [Docker 安全指南](https://docs.docker.com/engine/security/) 