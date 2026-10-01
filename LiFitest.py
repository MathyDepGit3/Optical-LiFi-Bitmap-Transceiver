import socket
import time
import sys
from PIL import Image

ARDUINO_IP = "192.168.4.1"
PORT = 4242

WIDTH = 320
HEIGHT = 240
BYTES_PER_LINE = WIDTH * 2  # 640 bytes

def rgb_to_rgb565(r, g, b):
    """Zet 24-bit RGB om naar 16-bit RGB565 (2 bytes, big-endian)."""
    val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return (val >> 8) & 0xFF, val & 0xFF

def load_image_rgb(image_path):
    img = Image.open(image_path).convert("RGB")
    img = img.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    
    # Optioneel: compenseer het koele TFT-scherm (10% minder fel blauw, 5% extra rood)
    r, g, b = img.split()
    r = r.point(lambda i: min(255, int(i * 1.10)))
    g = g.point(lambda i: min(255, int(i * 1.10)))
    b = b.point(lambda i: int(i * 0.80))
    img = Image.merge("RGB", (r, g, b))
    
    return img

def build_line_packet(line_idx, img_pixels):
    payload = bytearray(BYTES_PER_LINE)
    for x in range(WIDTH):
        r, g, b = img_pixels[x, line_idx]
        high_b, low_b = rgb_to_rgb565(r, g, b)
        payload[x * 2] = low_b
        payload[x * 2 + 1] = high_b

    len_high = (BYTES_PER_LINE >> 8) & 0xFF
    len_low = BYTES_PER_LINE & 0xFF

    checksum = line_idx ^ len_high ^ len_low
    for b in payload:
        checksum ^= b

    header = bytes([0x5A, 0xA5, 0xFF, line_idx, len_high, len_low])
    return header + bytes(payload) + bytes([checksum])

def main():
    if len(sys.argv) < 2:
        print("Gebruik: python send_image_fullcolor.py <afbeelding.jpg/png>")
        return

    image_path = sys.argv[1]
    print(f"[1/3] Afbeelding inlezen: {image_path}...")
    img = load_image_rgb(image_path)
    pixels = img.load()

    print(f"[2/3] Verbinden met {ARDUINO_IP}:{PORT}...")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((ARDUINO_IP, PORT))
        print("[3/3] Optische Full-Color transmissie gestart (19200 baud)...")

        start_time = time.time()
        for y in range(HEIGHT):
            packet = build_line_packet(y, pixels)
            s.sendall(packet)

            # 647 bytes / 1920 B/s = ~0.337s. 
            # 0.35s voorkomt dat de buffer van de Arduino overloopt
            time.sleep(0.7)

            percent = int(((y + 1) / HEIGHT) * 100)
            bar = '=' * (percent // 5)
            print(f"\rVerzenden: [{bar:20}] {percent}% (Regel {y + 1}/{HEIGHT})", end="")

        time.sleep(1.0)
        total_time = round(time.time() - start_time, 1)
        print(f"\nKlaar! Volledig kleurenscherm verzonden in {total_time} seconden.")

if __name__ == "__main__":
    main()