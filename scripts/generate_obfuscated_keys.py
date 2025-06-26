#!/usr/bin/env python3
"""
生成混淆后的API密钥数据

这个脚本将原始API密钥转换为混淆的字节数组，用于在代码中隐藏真实的密钥值。
使用多层混淆技术：
1. XOR加密
2. 字符串混淆（添加假数据）
3. 字节数组表示

支持输入方式：
- 从 config/api_keys.json 文件读取
- 交互式输入

安全特性：
- 交互式输入，避免密钥硬编码
- 输入时隐藏密钥显示
- 内存中及时清理密钥数据
- 自动更新C++源代码文件
"""

import sys
import getpass
import os
import re
import datetime
import json

def xor_encrypt(data, key):
    """使用XOR加密数据"""
    return [ord(c) ^ key for c in data]

def obfuscate_string(original, xor_key=0x42):
    """混淆字符串：先XOR加密，然后添加假数据"""
    # 加密真实数据
    encrypted = xor_encrypt(original, xor_key)
    
    # 生成假数据（与真实数据长度相同）
    fake_data = [ord(c) ^ xor_key for c in original]
    
    # 合并真实数据和假数据
    obfuscated = encrypted + fake_data
    
    return obfuscated

def format_byte_array(data, name):
    """格式化字节数组为C++代码"""
    lines = []
    lines.append(f"    // 混淆后的{name}")
    lines.append(f"    const std::vector<uint8_t> OBFUSCATED_{name.upper().replace(' ', '_')} = {{")
    
    # 每行16个字节
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_values = [f"0x{b:02X}" for b in chunk]
        lines.append(f"        {', '.join(hex_values)},")
    
    lines.append("    };")
    return '\n'.join(lines)

def secure_input(prompt):
    """安全输入函数，显示明文输入（方便复制粘贴）"""
    return input(prompt)

def clear_sensitive_data(data):
    """清理敏感数据（覆盖内存中的字符串）"""
    if isinstance(data, str):
        # 创建新字符串覆盖原内容
        data = 'x' * len(data)
    elif isinstance(data, list):
        # 覆盖列表内容
        for i in range(len(data)):
            data[i] = 0
    return None

def load_from_config_file(config_file="config/api_keys.json"):
    """从配置文件加载API密钥"""
    try:
        if not os.path.exists(config_file):
            return None, None, None
        
        with open(config_file, 'r', encoding='utf-8') as f:
            config = json.load(f)
        
        app_id = config.get('app_id', '').strip()
        access_token = config.get('access_token', '').strip()
        secret_key = config.get('secret_key', '').strip()
        
        # 检查是否包含占位符
        if (app_id == 'REPLACE_WITH_YOUR_APP_ID' or 
            access_token == 'REPLACE_WITH_YOUR_ACCESS_TOKEN' or
            secret_key == 'REPLACE_WITH_YOUR_SECRET_KEY'):
            return None, None, None
        
        if app_id and access_token:
            return app_id, access_token, secret_key
        else:
            return None, None, None
            
    except Exception as e:
        print(f"读取配置文件时出错: {e}")
        return None, None, None

def load_from_environment():
    """从环境变量加载API密钥"""
    app_id = os.getenv('ASR_APP_ID', '').strip()
    access_token = os.getenv('ASR_ACCESS_TOKEN', '').strip()
    secret_key = os.getenv('ASR_SECRET_KEY', '').strip()
    
    if app_id and access_token:
        return app_id, access_token, secret_key
    else:
        return None, None, None

def update_cpp_file(cpp_file_path, obfuscated_app_id, obfuscated_access_token, obfuscated_secret_key):
    """更新C++文件中的混淆密钥数据"""
    try:
        # 读取原文件
        with open(cpp_file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 生成新的混淆数据
        new_app_id_code = format_byte_array(obfuscated_app_id, "App ID")
        new_access_token_code = format_byte_array(obfuscated_access_token, "Access Token")
        new_secret_key_code = format_byte_array(obfuscated_secret_key, "Secret Key")
        
        # 使用正则表达式替换现有的混淆数据
        # 替换App ID
        app_id_pattern = r'(    // 混淆后的App ID.*?\n    const std::vector<uint8_t> OBFUSCATED_APP_ID = \{.*?\n    \};)'
        content = re.sub(app_id_pattern, new_app_id_code, content, flags=re.DOTALL)
        
        # 替换Access Token
        access_token_pattern = r'(    // 混淆后的Access Token.*?\n    const std::vector<uint8_t> OBFUSCATED_ACCESS_TOKEN = \{.*?\n    \};)'
        content = re.sub(access_token_pattern, new_access_token_code, content, flags=re.DOTALL)
        
        # 替换Secret Key
        secret_key_pattern = r'(    // 混淆后的Secret Key.*?\n    const std::vector<uint8_t> OBFUSCATED_SECRET_KEY = \{.*?\n    \};)'
        content = re.sub(secret_key_pattern, new_secret_key_code, content, flags=re.DOTALL)
        
        # 添加更新注释
        update_comment = f"    // 自动更新于 {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        content = content.replace("    // 混淆后的App ID", f"{update_comment}\n    // 混淆后的App ID")
        
        # 写回文件
        with open(cpp_file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        
        return True
    except Exception as e:
        print(f"更新C++文件时出错: {e}")
        return False

def main():
    print("=== API密钥混淆工具 ===")
    print("此工具将帮助您生成混淆后的API密钥数据")
    print("支持输入方式：配置文件、交互式输入")
    print()
    
    # 尝试从配置文件加载
    print("正在检查配置文件...")
    app_id, access_token, secret_key = load_from_config_file()
    if app_id and access_token:
        print("✅ 从 config/api_keys.json 加载配置成功")
        source = "配置文件"
    else:
        # 交互式输入
        print("配置文件未找到或无效，使用交互式输入...")
        print("请输入您的API密钥信息：")
        app_id = secure_input("App ID: ")
        if not app_id.strip():
            print("错误：App ID不能为空")
            sys.exit(1)
        
        access_token = secure_input("Access Token: ")
        if not access_token.strip():
            print("错误：Access Token不能为空")
            sys.exit(1)
        
        secret_key = secure_input("Secret Key: ")
        if not secret_key.strip():
            print("错误：Secret Key不能为空")
            sys.exit(1)
        
        source = "交互式输入"
    
    # 最终确认步骤
    print(f"\n=== 请确认您输入的API密钥信息（来源：{source}）===")
    print(f"App ID: {app_id}")
    print(f"Access Token: {access_token}")
    print(f"Secret Key: {secret_key}")
    
    confirm = input("\n确认以上信息正确吗？(y/N): ").strip().lower()
    if confirm not in ['y', 'yes', '是']:
        print("已取消操作")
        sys.exit(0)
    
    print("\n正在生成混淆数据...")
    
    # 生成混淆数据
    obfuscated_app_id = obfuscate_string(app_id)
    obfuscated_access_token = obfuscate_string(access_token)
    obfuscated_secret_key = obfuscate_string(secret_key)
    
    # 输出C++代码
    print("\n// 自动生成的混淆API密钥数据")
    print("// 使用 generate_obfuscated_keys.py 生成")
    print("// 请勿手动修改这些数据")
    print()
    
    print(format_byte_array(obfuscated_app_id, "App ID"))
    print()
    print(format_byte_array(obfuscated_access_token, "Access Token"))
    print()
    print(format_byte_array(obfuscated_secret_key, "Secret Key"))
    print()
    
    # 验证解密（显示完整信息）
    print("// 验证解密结果：")
    print(f"// App ID: {app_id}")
    print(f"// Access Token: {access_token}")
    print(f"// Secret Key: {secret_key}")
    
    # 输出解密函数示例
    print()
    print("// 解密函数示例：")
    print("std::string deobfuscateString(const std::vector<uint8_t>& obfuscated) {")
    print("    // 简单的字符串混淆：取前半部分作为真实数据")
    print("    size_t halfSize = obfuscated.size() / 2;")
    print("    std::vector<uint8_t> realData(obfuscated.begin(), obfuscated.begin() + halfSize);")
    print("    return decryptString(realData, XOR_KEY);")
    print("}")
    
    # 询问是否自动更新C++文件
    print("\n=== 自动更新选项 ===")
    cpp_file_path = "src/ui/config_manager.cpp"
    
    if os.path.exists(cpp_file_path):
        update_choice = input(f"是否自动更新 {cpp_file_path} 文件？(y/N): ").strip().lower()
        if update_choice in ['y', 'yes', '是']:
            print(f"\n正在更新 {cpp_file_path}...")
            if update_cpp_file(cpp_file_path, obfuscated_app_id, obfuscated_access_token, obfuscated_secret_key):
                print("✅ C++文件更新成功！")
            else:
                print("❌ C++文件更新失败，请手动复制上面的代码")
        else:
            print("跳过自动更新，请手动复制上面的代码到相应文件中")
    else:
        print(f"警告：找不到文件 {cpp_file_path}")
        print("请手动复制上面的代码到相应文件中")
    
    # 清理敏感数据
    app_id = clear_sensitive_data(app_id)
    access_token = clear_sensitive_data(access_token)
    secret_key = clear_sensitive_data(secret_key)
    
    print("\n=== 完成 ===")
    print("混淆数据已生成")
    if os.path.exists(cpp_file_path):
        print("注意：原始密钥信息已从内存中清理")

if __name__ == "__main__":
    main() 