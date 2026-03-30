#!/usr/bin/env python3
"""
QR Code decoder using pyzbar library
This replaces OpenCV's broken QRCodeDetector with working pyzbar
"""
import sys
import json
from pathlib import Path

try:
    from pyzbar.pyzbar import decode
    from PIL import Image
except ImportError as e:
    print(f"ERROR: Missing required library: {e}", file=sys.stderr)
    print("Install with: pip install pyzbar pillow", file=sys.stderr)
    sys.exit(1)


def decode_qr_from_image(image_path):
    """
    Decode QR code from image file

    Args:
        image_path: Path to image file (PPM, PNG, etc)

    Returns:
        dict: {"success": bool, "data": bytes/hex, "error": str or None}
    """
    try:
        if not Path(image_path).exists():
            return {
                "success": False,
                "data": None,
                "error": f"File not found: {image_path}"
            }

        # Read image
        img = Image.open(image_path)

        # Try to decode
        results = decode(img)

        if not results:
            return {
                "success": False,
                "data": None,
                "error": "No QR code found in image"
            }

        if len(results) > 1:
            return {
                "success": False,
                "data": None,
                "error": f"Multiple QR codes found ({len(results)}), expected 1"
            }

        # Extract data from first (and only) result
        qr_data = results[0].data

        return {
            "success": True,
            "data": qr_data.hex(),  # Return as hex string for universal compatibility
            "error": None
        }

    except Exception as e:
        return {
            "success": False,
            "data": None,
            "error": str(e)
        }


def main():
    """Command line interface"""
    if len(sys.argv) != 2:
        print("Usage: python decode_qr_pyzbar.py <image_path>", file=sys.stderr)
        print("\nExample: python decode_qr_pyzbar.py frame_0.ppm", file=sys.stderr)
        sys.exit(1)

    image_path = sys.argv[1]
    result = decode_qr_from_image(image_path)

    # Output as JSON for easy parsing by C++
    print(json.dumps(result))

    # Exit with appropriate code
    sys.exit(0 if result["success"] else 1)


if __name__ == "__main__":
    main()
