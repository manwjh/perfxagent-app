# ASR 计时器功能说明

## 概述

ASR计时器功能已经集成到`AsrManager`类中，用于跟踪用户与ASR云连接的有效时间。该功能可以统计从连接成功到连接断开的时间，支持按天统计和持久化存储。

## 主要功能

### 1. 连接时间统计
- **自动计时**: 在`connect()`方法中自动开始计时
- **自动结束**: 在`disconnect()`方法中自动结束计时
- **会话管理**: 每个连接会话都有唯一的会话ID
- **异常处理**: 支持连接异常情况的统计

### 2. 每日使用统计
- **按日分组**: 统计数据按日期（YYYY-MM-DD）分组
- **累计时长**: 计算每日总使用时长
- **会话次数**: 统计每日连接会话次数
- **详细记录**: 保存每个会话的详细信息

### 3. 总体统计信息
- **总使用时长**: 所有日期的累计使用时长
- **总会话次数**: 所有连接会话的总数
- **活跃天数**: 有使用记录的天数
- **使用时间范围**: 首次使用和最后使用时间

### 4. 数据持久化
- **JSON格式存储**: 统计数据以JSON格式保存
- **自动备份**: 保存时自动创建备份文件
- **数据恢复**: 支持从文件加载历史统计数据
- **CSV导出**: 支持导出统计数据为CSV格式

## 配置选项

在`AsrConfig`结构体中添加了以下配置选项：

```cpp
struct AsrConfig {
    // ... 其他配置 ...
    
    // 计时器配置
    bool enableUsageTracking = true;                       // 是否启用使用统计
    std::string statsDataDir = "";                         // 统计数据存储目录
};
```

### 配置说明

- **enableUsageTracking**: 控制是否启用计时器功能，默认为true
- **statsDataDir**: 指定统计数据存储目录，默认为空（使用当前目录下的data文件夹）

## 使用方法

### 1. 基本使用

```cpp
#include "asr/asr_manager.h"

// 创建ASR管理器
AsrManager asrManager;

// 配置计时器功能
AsrConfig config;
config.enableUsageTracking = true;  // 启用使用统计
config.statsDataDir = "./data";     // 设置数据存储目录
asrManager.setConfig(config);

// 加载历史统计数据
asrManager.loadStats();

// 连接ASR服务（自动开始计时）
if (asrManager.connect()) {
    // 使用ASR服务...
    
    // 断开连接（自动结束计时）
    asrManager.disconnect();
}

// 保存统计数据
asrManager.saveStats();
```

### 2. 查询统计信息

```cpp
// 获取今日统计
auto todayStats = asrManager.getTodayStats();
std::cout << "今日使用时长: " << todayStats.getFormattedTotalDuration() << std::endl;
std::cout << "今日会话次数: " << todayStats.sessionCount << std::endl;

// 获取指定日期统计
auto dateStats = asrManager.getDateStats("2024-01-15");

// 获取总体统计
auto overallStats = asrManager.getOverallStats();
std::cout << "总使用时长: " << overallStats.getFormattedTotalDuration() << std::endl;
std::cout << "总会话次数: " << overallStats.totalSessionCount << std::endl;

// 获取最近7天统计
auto recentStats = asrManager.getRecentStats(7);

// 获取所有历史统计
auto allStats = asrManager.getAllStats();
```

### 3. 数据导出

```cpp
// 导出为CSV格式
asrManager.exportToCsv("./data/asr_usage_export.csv");

// 获取统计摘要
std::string summary = asrManager.getStatsSummary();
std::cout << summary << std::endl;
```

### 4. 数据管理

```cpp
// 清除所有统计数据
asrManager.clearStats();

// 手动保存数据
asrManager.saveStats();

// 手动加载数据
asrManager.loadStats();
```

## 数据结构

### ConnectionSession（连接会话）

```cpp
struct ConnectionSession {
    std::chrono::system_clock::time_point connectTime;    // 连接开始时间
    std::chrono::system_clock::time_point disconnectTime; // 连接结束时间
    std::chrono::milliseconds duration;                   // 连接持续时间
    std::string sessionId;                                // 会话ID
    bool isCompleted;                                     // 是否正常完成
};
```

### DailyUsageStats（每日统计）

```cpp
struct DailyUsageStats {
    std::string date;                                     // 日期 (YYYY-MM-DD)
    std::chrono::milliseconds totalDuration;              // 当日总使用时长
    int sessionCount;                                     // 会话次数
    std::vector<ConnectionSession> sessions;              // 当日所有会话
};
```

### OverallStats（总体统计）

```cpp
struct OverallStats {
    std::chrono::milliseconds totalDuration;              // 总使用时长
    int totalSessionCount;                                 // 总会话次数
    std::chrono::system_clock::time_point firstUsage;     // 首次使用时间
    std::chrono::system_clock::time_point lastUsage;      // 最后使用时间
    int activeDays;                                        // 活跃天数
};
```

## 文件存储

### 数据文件位置

- **主文件**: `{statsDataDir}/asr_usage_stats.json`
- **备份文件**: `{statsDataDir}/asr_usage_stats_backup.json`

### JSON文件格式

```json
{
  "version": "1.0",
  "last_updated": "2024-01-15",
  "overall_stats": {
    "total_duration_ms": 3600000,
    "total_session_count": 10,
    "active_days": 5,
    "first_usage": "2024-01-10 09:30:00",
    "last_usage": "2024-01-15 18:45:00"
  },
  "daily_stats": [
    {
      "date": "2024-01-15",
      "total_duration_ms": 720000,
      "session_count": 2,
      "sessions": [
        {
          "session_id": "session-1705312800000-12345",
          "connect_time": "2024-01-15 09:30:00",
          "disconnect_time": "2024-01-15 09:42:00",
          "duration_ms": 720000,
          "is_completed": true
        }
      ]
    }
  ]
}
```

## 注意事项

1. **线程安全**: 所有统计操作都是线程安全的，使用互斥锁保护
2. **自动保存**: 每次会话结束后会自动保存统计数据
3. **错误处理**: 文件操作失败时会记录错误日志，不会影响ASR功能
4. **性能影响**: 计时器功能对ASR性能影响极小，主要开销在文件I/O操作
5. **数据完整性**: 保存时会自动创建备份文件，确保数据安全

## 示例输出

```
=== ASR 使用统计摘要 ===
总使用时长: 2h 15m 30s
总会话次数: 25
活跃天数: 7
首次使用: 2024-01-10 09:30:00
最后使用: 2024-01-15 18:45:00
今日使用时长: 45m 20s
今日会话次数: 3
```

## 集成建议

1. **在应用启动时**: 调用`loadStats()`加载历史数据
2. **在应用关闭时**: 调用`saveStats()`保存当前数据
3. **定期导出**: 可以定期调用`exportToCsv()`导出数据用于分析
4. **UI显示**: 可以在UI中显示`getStatsSummary()`的内容
5. **配置管理**: 可以通过配置文件控制是否启用计时器功能 