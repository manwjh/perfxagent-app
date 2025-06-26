#!/bin/bash

# 推送前安全检查脚本
# 确保敏感文件不会被意外提交

echo "🔍 执行推送前安全检查..."

# 检查是否有敏感文件被跟踪
SENSITIVE_FILES=(
    "config/api_keys.json"
    "config/credentials.json"
    "config/secrets.json"
    ".env"
    "*.key"
    "*.pem"
)

echo "检查敏感文件..."

for file in "${SENSITIVE_FILES[@]}"; do
    if git ls-files | grep -q "$file"; then
        echo "❌ 警告：敏感文件 $file 正在被Git跟踪！"
        echo "请从Git中移除该文件："
        echo "  git rm --cached $file"
        echo "  git commit -m 'Remove sensitive file: $file'"
        exit 1
    fi
done

# 检查是否有API密钥相关的提交
echo "检查提交历史中的敏感信息..."

if git log --oneline | grep -i "api.*key\|secret\|password\|token" > /dev/null; then
    echo "⚠️  警告：提交历史中可能包含敏感信息"
    echo "建议检查最近的提交："
    git log --oneline -5
fi

# 检查工作目录状态
echo "检查工作目录状态..."

if [ -n "$(git status --porcelain)" ]; then
    echo "📝 当前有未提交的更改："
    git status --short
    echo ""
    echo "请确认这些更改是否应该被提交"
else
    echo "✅ 工作目录干净"
fi

# 检查远程仓库
echo "检查远程仓库配置..."

if ! git remote -v | grep -q "origin"; then
    echo "⚠️  警告：未配置远程仓库"
    echo "请添加远程仓库："
    echo "  git remote add origin <your-github-repo-url>"
fi

echo "✅ 安全检查完成"
echo ""
echo "如果所有检查都通过，可以安全推送："
echo "  git push origin main" 