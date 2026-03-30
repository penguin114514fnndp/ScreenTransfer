# ScreenTransfer

一个基于自定义符号编码的视频传输实验项目：
将二进制图片数据编码为“类二维码”视频帧，再从视频中逐帧识别、重组并恢复原始文件。

## 项目简介

ScreenTransfer 的目标是验证一条完整的数据传输链路：

1. 输入文件（如 PNG）
2. 编码器将数据切片并写入自定义符号帧
3. ffmpeg 合成为视频
4. 解码器从视频中识别符号并恢复字节流
5. 与原始文件对比完整性（字节/位准确率）

当前版本在本仓库示例数据上可实现端到端 100% 还原。

## 核心特性

- 自定义帧协议
- 帧头包含 `magic / frameIndex / totalFrames / totalSize / payloadOffset / payloadLen / payloadCRC32`
- 解码端支持重复帧择优（优先 CRC 正确、负载更完整）
- 解码结果提供覆盖率与 CRC 统计
- 文件对比支持 Byte Accuracy 和 Bit Accuracy
- Windows + MSVC 构建脚本开箱可用

## 项目结构

- `Encoder/`：编码器源码（生成符号帧并合成视频）
- `Decoder/`：解码器源码（视频识别、帧解析、重组、校验）
- `output/`：示例输入输出与中间产物
- `.vscode/`：构建与运行任务、批处理脚本

## 快速开始（Windows）

### 1) 构建编码器

```bat
.\.vscode\build-encoder.cmd
```

### 2) 运行编码 Demo

```bat
output\qr_video_encoder.exe output\test.png output\qr_video.mp4 1000
```

### 3) 构建解码器

```bat
.\.vscode\build-decoder.cmd
```

### 4) 运行解码并对比

```bat
output\qr_video_decoder.exe output\qr_video.mp4 output\decoded.bin output\test.png
```

## 开发环境

- Windows
- Visual Studio 2022 C++ 工具链（MSVC）
- C++17
- OpenCV（Decoder 链接 `opencv_world4120.lib`）
- ffmpeg（用于视频合成）

## 适用场景

- 低层编码协议实验
- 视频通道下的数据承载与恢复验证
- 符号识别鲁棒性测试
- 端到端正确率评估与回归测试

## 说明

本项目偏向工程实验与链路验证，不是标准 QR 通讯协议实现。
欢迎在此基础上继续扩展纠错策略、吞吐优化和跨平台支持。
