#include <iostream>
#include <iomanip>
#include <QByteArray>
#include <QDataStream>
#include <QJsonObject>
#include <QJsonDocument>
#include <zlib.h>

// 协议常量定义
const uint8_t PROTOCOL_VERSION = 0x01;
const uint8_t FULL_CLIENT_REQUEST = 0x01;
const uint8_t POS_SEQUENCE = 0x01;
const uint8_t JSON_SERIALIZATION = 0x01;
const uint8_t GZIP_COMPRESSION = 0x01;

QByteArray generateHeader(uint8_t messageType, uint8_t messageTypeSpecificFlags, 
                         uint8_t serialMethod, uint8_t compressionType, uint8_t reservedData) {
    QByteArray header;
    uint8_t headerSize = 1;
    
    // protocol_version(4 bits), header_size(4 bits)
    header.append((PROTOCOL_VERSION << 4) | headerSize);
    
    // message_type(4 bits), message_type_specific_flags(4 bits)
    header.append((messageType << 4) | messageTypeSpecificFlags);
    
    // serialization_method(4 bits), message_compression(4 bits)
    header.append((serialMethod << 4) | compressionType);
    
    // reserved (8 bits)
    header.append(reservedData);
    
    return header;
}

QByteArray generateBeforePayload(int32_t sequence) {
    QByteArray beforePayload;
    QDataStream stream(&beforePayload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << sequence; // 4字节，大端序，有符号
    return beforePayload;
}

QByteArray gzipCompress(const QByteArray& data) {
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        std::cerr << "Failed to initialize zlib for compression" << std::endl;
        return data;
    }
    
    strm.avail_in = data.size();
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    
    QByteArray compressed;
    char outBuffer[4096];
    
    do {
        strm.avail_out = sizeof(outBuffer);
        strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
        
        int ret = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            std::cerr << "zlib compression error" << std::endl;
            deflateEnd(&strm);
            return data;
        }
        
        int have = sizeof(outBuffer) - strm.avail_out;
        compressed.append(outBuffer, have);
    } while (strm.avail_out == 0);
    
    deflateEnd(&strm);
    return compressed;
}

int main() {
    std::cout << "=== C++ Header Test ===" << std::endl;
    
    // 构建JSON请求（与Python代码一致）
    QJsonObject reqObj;
    
    // user section
    QJsonObject userObj;
    userObj["uid"] = "test";
    reqObj["user"] = userObj;
    
    // audio section
    QJsonObject audioObj;
    audioObj["format"] = "wav";
    audioObj["sample_rate"] = 16000;
    audioObj["bits"] = 16;
    audioObj["channel"] = 1;
    audioObj["codec"] = "raw";
    reqObj["audio"] = audioObj;
    
    // request section
    QJsonObject requestObj;
    requestObj["model_name"] = "bigmodel";
    requestObj["enable_punc"] = true;
    reqObj["request"] = requestObj;
    
    QJsonDocument doc(reqObj);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);
    QByteArray jsonData = jsonStr.toUtf8();
    
    std::cout << "JSON Request: " << jsonStr.toStdString() << std::endl;
    std::cout << "JSON Size: " << jsonData.size() << " bytes" << std::endl;
    
    // 压缩JSON数据
    QByteArray compressedData = gzipCompress(jsonData);
    std::cout << "Compressed Size: " << compressedData.size() << " bytes" << std::endl;
    
    // 生成协议头部
    QByteArray header = generateHeader(FULL_CLIENT_REQUEST, POS_SEQUENCE, JSON_SERIALIZATION, GZIP_COMPRESSION, 0x00);
    
    // 生成序列号前的内容
    int32_t seq = 1;
    QByteArray beforePayload = generateBeforePayload(seq);
    
    // 生成payload大小
    QByteArray payloadSize;
    QDataStream sizeStream(&payloadSize, QIODevice::WriteOnly);
    sizeStream.setByteOrder(QDataStream::BigEndian);
    sizeStream << static_cast<uint32_t>(compressedData.size());
    
    // 组装完整消息
    QByteArray fullRequest = header + beforePayload + payloadSize + compressedData;
    
    // 打印协议头部的十六进制值
    std::cout << "Protocol header (hex): ";
    for (int i = 0; i < header.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned char>(header[i]) << " ";
    }
    std::cout << std::endl;
    
    // 打印序列号前的内容
    std::cout << "Before payload (hex): ";
    for (int i = 0; i < beforePayload.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned char>(beforePayload[i]) << " ";
    }
    std::cout << std::endl;
    
    // 打印payload大小
    std::cout << "Payload size (hex): ";
    for (int i = 0; i < payloadSize.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned char>(payloadSize[i]) << " ";
    }
    std::cout << std::endl;
    
    // 打印完整请求的前100字节
    std::cout << "Full request (first 100 bytes): ";
    int dumpSize = std::min(100, fullRequest.size());
    for (int i = 0; i < dumpSize; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned char>(fullRequest[i]) << " ";
    }
    if (fullRequest.size() > dumpSize) {
        std::cout << "...";
    }
    std::cout << std::endl;
    
    std::cout << "Total size: " << fullRequest.size() << " bytes" << std::endl;
    
    return 0;
} 