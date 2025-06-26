//
// ASR 计时器使用示例
// 
// 本示例展示如何使用 ASR 管理器的计时器功能
// 包括连接时间统计、数据查询和导出等功能
// 
// 作者: PerfXAgent Team
// 版本: 1.0.0
// 日期: 2024
//

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include "asr/asr_manager.h"

using namespace Asr;

int main() {
    std::cout << "=== ASR 计时器功能示例 ===" << std::endl;
    
    // 创建 ASR 管理器实例
    AsrManager asrManager;
    
    // 配置 ASR 管理器
    AsrConfig config;
    config.enableUsageTracking = true;  // 启用使用统计
    config.statsDataDir = "./data";     // 设置数据存储目录
    config.logLevel = ASR_LOG_INFO;     // 设置日志级别
    
    asrManager.setConfig(config);
    
    // 加载历史统计数据
    if (asrManager.loadStats()) {
        std::cout << "✅ 历史统计数据加载成功" << std::endl;
    } else {
        std::cout << "ℹ️ 没有找到历史统计数据，将创建新的统计记录" << std::endl;
    }
    
    // 显示统计摘要
    std::cout << "\n" << asrManager.getStatsSummary() << std::endl;
    
    // 模拟多次连接和断开
    for (int i = 1; i <= 3; ++i) {
        std::cout << "\n--- 第 " << i << " 次连接测试 ---" << std::endl;
        
        // 连接 ASR 服务
        std::cout << "🔗 正在连接 ASR 服务..." << std::endl;
        if (asrManager.connect()) {
            std::cout << "✅ ASR 连接成功" << std::endl;
            
            // 模拟使用时间
            std::cout << "⏱️ 模拟使用中..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2 + i));  // 2-4秒的使用时间
            
            // 断开连接
            std::cout << "🔌 断开 ASR 连接..." << std::endl;
            asrManager.disconnect();
            std::cout << "✅ ASR 连接已断开" << std::endl;
            
        } else {
            std::cout << "❌ ASR 连接失败" << std::endl;
        }
        
        // 显示当前会话信息
        auto currentSession = asrManager.getCurrentSession();
        if (currentSession.connectTime.time_since_epoch().count() > 0) {
            std::cout << "📊 当前会话ID: " << currentSession.sessionId << std::endl;
        }
        
        // 显示今日统计
        auto todayStats = asrManager.getTodayStats();
        std::cout << "📈 今日使用时长: " << todayStats.getFormattedTotalDuration() << std::endl;
        std::cout << "📈 今日会话次数: " << todayStats.sessionCount << std::endl;
    }
    
    // 显示最终统计摘要
    std::cout << "\n=== 最终统计摘要 ===" << std::endl;
    std::cout << asrManager.getStatsSummary() << std::endl;
    
    // 显示最近7天的统计
    std::cout << "\n=== 最近7天统计 ===" << std::endl;
    auto recentStats = asrManager.getRecentStats(7);
    for (const auto& dailyStats : recentStats) {
        if (dailyStats.sessionCount > 0) {
            std::cout << dailyStats.date << ": " 
                      << dailyStats.getFormattedTotalDuration() 
                      << " (" << dailyStats.sessionCount << " 次会话)" << std::endl;
        }
    }
    
    // 导出统计数据为CSV
    std::string csvPath = "./data/asr_usage_export.csv";
    if (asrManager.exportToCsv(csvPath)) {
        std::cout << "\n✅ 统计数据已导出到: " << csvPath << std::endl;
    } else {
        std::cout << "\n❌ 统计数据导出失败" << std::endl;
    }
    
    // 保存统计数据
    if (asrManager.saveStats()) {
        std::cout << "✅ 统计数据已保存" << std::endl;
    } else {
        std::cout << "❌ 统计数据保存失败" << std::endl;
    }
    
    std::cout << "\n=== 示例完成 ===" << std::endl;
    return 0;
} 