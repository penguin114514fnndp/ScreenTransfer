# ScreenTransfer（新编码/解码分支）

本文件夹是一个“分支副本”，包含新的编码器与解码器实现。
目标：用可见光（屏幕显示）传输二进制文件，并在接收端还原。

---

## 目录结构
- Encoder：二维码视频编码器（新格式）
- Decoder：二维码视频解码器（OpenCV + ffmpeg）
- tools/ffmpeg.exe：抽帧/合成视频
- .vscode/tasks.json：构建任务

---

## 构建（Windows, g++）

### 编码器
```
g++ -std=c++17 -O2 -Wall -Wextra -IEncoder\include Encoder\src\main.cpp -o output\qr_video_encoder.exe
```

### 解码器（OpenCV）
VSCode 任务 **Build Video Decoder (OpenCV)** 依赖以下环境变量：
- OPENCV_INCLUDE：OpenCV 头文件路径
- OPENCV_LIB：OpenCV 库路径
- OPENCV_LIBS：链接库列表（例如 -lopencv_world480）

手动构建示例：
```
g++ -std=c++17 -O2 -Wall -Wextra -IDecoder\include -I%OPENCV_INCLUDE% Decoder\src\main.cpp -L%OPENCV_LIB% %OPENCV_LIBS% -o output\qr_video_decoder.exe
```

---

## 使用方法

### 编码
```
qr_video_encoder.exe <input_bin> <output_video> <duration_ms>
```
示例：
```
qr_video_encoder.exe input.bin output\qr_video.mp4 1000
```

### 解码
```
qr_video_decoder.exe <input_video> <output_bin> <output_valid>
```
示例：
```
qr_video_decoder.exe output\qr_video.mp4 output\out.bin output\wout.bin
```

---

## 新格式说明（v1）
- QR 内容为 Base64，保证二进制安全。
- 每帧包含固定帧头与 payload。
- 帧头包含：文件大小、文件 CRC32、分块大小、总帧数、帧序号、payload CRC32。

---

## 输出有效性文件（wout.bin）
- wout.bin 与 out.bin 等长
- 0xFF：该字节已正确解码并通过 CRC
- 0x00：缺帧或校验失败

---

## 兼容性
- 新解码器只兼容本文件夹里的新编码器。
- 旧编码器生成的视频需要旧解码逻辑。
