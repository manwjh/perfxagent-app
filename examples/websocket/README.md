
## 服务器地址
https://aiot.llmworld.net/#/home

## 客户端连接Websocket服务器时携带以下headers:
Authorization: Bearer <access_token>
Protocol-Version: 1
Device-Id: <本机MAC地址>
Client-Id: <设备UUID>


## 测试程序流程
- 第一步：建立WebSocket连接

- 第二步：握手
本程序发送到服务器
消息类型：hello
{
  "type": "hello",
  "version": 1,
  "transport": "websocket",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}

服务器回应端侧
{
  "type": "hello",
  "version": 1,
  "transport": "websocket",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
如果在超时时间（默认 10 秒）内未收到正确回复，认为连接失败并触发网络错误回调。

第三步：结束
关闭 WebSocket 连接，websocket线程进入空闲状态。