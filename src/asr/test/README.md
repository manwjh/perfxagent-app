# WebSocket测试程序

这个目录包含了用于测试Qt WebSocket连接的测试程序。

## 文件说明

- `test_qt_websocket.cpp` - 主要的测试程序源码
- `CMakeLists.txt` - CMake构建配置文件
- `build_test.sh` - 自动化构建脚本
- `Makefile` - 简化的Makefile构建配置
- `README.md` - 本说明文件

## 功能特性

测试程序具有以下功能：

1. **WebSocket连接测试** - 测试与ASR服务器的WebSocket连接
2. **SSL/TLS支持** - 支持安全的WebSocket连接
3. **代理配置** - 可配置网络代理设置
4. **超时处理** - 自动处理连接超时
5. **状态监控** - 实时监控连接状态
6. **错误处理** - 完善的错误处理机制

## 构建要求

### 系统要求
- macOS (推荐) 或 Linux
- CMake 3.10 或更高版本
- Qt6 开发环境

### 依赖安装

在macOS上安装依赖：

```bash
# 安装Qt6
brew install qt@6

# 安装CMake (如果未安装)
brew install cmake
```

## 构建方法

### 方法1: 使用自动化脚本 (推荐)

```bash
# 进入测试目录
cd src/asr/test

# 构建Debug版本
./build_test.sh Debug

# 或构建Release版本
./build_test.sh Release
```

### 方法2: 使用Makefile

```bash
# 进入测试目录
cd src/asr/test

# 检查依赖
make check-deps

# 构建Debug版本
make debug

# 或构建Release版本
make release

# 构建并运行
make run

# 查看帮助
make help
```

### 方法3: 手动构建

```bash
# 进入测试目录
cd src/asr/test

# 创建构建目录
mkdir build && cd build

# 配置CMake
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 编译
make -j4
```

## 运行测试

### 基本运行

```bash
# 使用默认URL运行
./bin/test_qt_websocket

# 使用自定义URL运行
./bin/test_qt_websocket "wss://your-websocket-server.com/path"
```

### 设置认证令牌

如果需要认证，可以设置环境变量：

```bash
# 设置ASR_TOKEN环境变量
export ASR_TOKEN="your_authentication_token_here"

# 运行测试
./bin/test_qt_websocket
```

## 测试输出说明

程序运行时会输出以下信息：

- **连接状态** - WebSocket连接的状态变化
- **错误信息** - 连接过程中的错误详情
- **状态检查** - 每秒输出一次连接状态信息
- **超时处理** - 连接超时的处理信息

### 示例输出

```
Using WebSocket URL: wss://openspeech.bytedance.com/api/v2/asr
Testing WebSocket connection to: wss://openspeech.bytedance.com/api/v2/asr
[WS] Authorization header set from ASR_TOKEN env.
Opening WebSocket...
WebSocket state changed to: ConnectingState
WebSocket connected successfully!
WebSocket state changed to: ConnectedState
WebSocket disconnected
```

## 故障排除

### 常见问题

1. **Qt6未找到**
   ```
   错误: Qt6 not found. Please install Qt6.
   ```
   解决方案：运行 `brew install qt@6`

2. **编译错误**
   - 确保CMake版本 >= 3.10
   - 确保Qt6正确安装
   - 检查编译器是否支持C++17

3. **连接失败**
   - 检查网络连接
   - 验证WebSocket URL是否正确
   - 确认是否需要认证令牌

4. **SSL证书问题**
   - 程序已配置为忽略SSL证书验证
   - 如果仍有问题，检查系统时间是否正确

### 调试模式

使用Debug模式构建可以获得更详细的调试信息：

```bash
./build_test.sh Debug
# 或
make debug
```

## 自定义配置

### 修改默认URL

编辑 `test_qt_websocket.cpp` 文件中的 `DEFAULT_ASR_WS_URL` 宏：

```cpp
#define DEFAULT_ASR_WS_URL "wss://your-custom-server.com/api/v2/asr"
```

### 修改超时时间

在 `test_qt_websocket.cpp` 中修改超时设置：

```cpp
// 启动超时定时器（修改为20秒）
m_timeoutTimer->start(20000);
```

### 修改状态检查间隔

```cpp
// 设置状态检查定时器（修改为2秒）
m_statusTimer->setInterval(2000);
```

## 构建工具对比

| 工具 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| `build_test.sh` | 自动化程度高，错误处理完善 | 依赖bash环境 | 推荐使用 |
| `Makefile` | 简单直接，编译快速 | 功能相对简单 | 快速构建 |
| `CMakeLists.txt` | 功能强大，跨平台 | 配置复杂 | 复杂项目 |

## 许可证

本测试程序遵循项目的整体许可证。 