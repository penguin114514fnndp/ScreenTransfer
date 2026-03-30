#!/usr/bin/env python3
"""
Optimized QR Code decoder using pyzbar
Simple, fast, with image preprocessing for better detection
"""
import sys
import json
from pathlib import Path

try:
    from pyzbar.pyzbar import decode
    from PIL import Image, ImageEnhance, ImageOps
except ImportError as e:
    print(json.dumps({"success": False, "data": None, "error": str(e)}))
    sys.exit(1)


def enhance_for_detection(img):
    """预处理图像以改善识别准确率"""
    # 转换为RGB（如果需要）
    if img.mode != 'RGB':
        img = img.convert('RGB')

    # 增强对比度
    enhancer = ImageEnhance.Contrast(img)
    img = enhancer.enhance(1.5)

    # 增强亮度
    enhancer = ImageEnhance.Brightness(img)
    img = enhancer.enhance(1.1)

    # 转灰度
    img = ImageOps.grayscale(img)

    return img


def detect_qr(image_path):
    """Detect and decode QR code from image"""
    try:
        if not Path(image_path).exists():
            return {"success": False, "data": None}

        img = Image.open(image_path)

        # Try raw detection first
        results = decode(img)

        if not results:
            # Try with preprocessing
            img_enhanced = enhance_for_detection(img)
            results = decode(img_enhanced)

        if results:
            # Return hex data of first QR code found
            return {
                "success": True,
                "data": results[0].data.hex()
            }

        return {"success": False, "data": None}

    except Exception as e:
        return {"success": False, "data": None}


def main():
    if len(sys.argv) != 2:
        sys.exit(1)

    result = detect_qr(sys.argv[1])
    print(json.dumps(result))
    sys.exit(0 if result["success"] else 1)


if __name__ == "__main__":
    main()
