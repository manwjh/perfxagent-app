# 配置文件说明

## API密钥配置

### 设置步骤

1. **复制模板文件**：
   ```bash
   cp config/api_keys_template.json config/api_keys.json
   ```

2. **编辑配置文件**：
   打开 `config/api_keys.json` 并替换为你的真实API密钥：
   ```json
   {
     "app_id": "your_real_app_id",
     "access_token": "your_real_access_token", 
     "secret_key": "your_real_secret_key"
   }
   ```

3. **生成混淆数据**：
   ```bash
   python3 scripts/generate_obfuscated_keys.py
   ```

### 安全注意事项

- ✅ `api_keys_template.json` - 可以提交到Git（模板文件）
- ❌ `api_keys.json` - 不要提交到Git（包含真实密钥）
- ✅ 使用环境变量 `ASR_*` 作为替代方案
- ✅ 使用UI界面配置作为替代方案

### 配置优先级

1. **环境变量**（最高优先级）
   - `ASR_APP_ID`
   - `ASR_ACCESS_TOKEN`
   - `ASR_SECRET_KEY`

2. **UI配置文件**
   - `~/.perfxagent/asr_config.json`

3. **混淆代码**（体验模式）
   - 从 `src/asr/secure_key_manager.cpp` 中提取

### 故障排除

如果遇到配置问题：
1. 检查 `config/api_keys.json` 是否存在
2. 验证JSON格式是否正确
3. 确认API密钥是否有效
4. 查看应用程序日志获取详细错误信息 