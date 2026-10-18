"""
===================================================================
NEURON GATEWAY - UDP COMMANDER (CONSOLE TESTING TOOL)
===================================================================
Công cụ gửi lệnh nhanh qua mạng UDP (Cổng 8888) để kiểm tra các module:
- Đèn LED thông minh (Bật/Tắt/Đèn thở/SOS/Nháy đôi/Heartbeat)
- Còi Buzzer (Bật/Tắt/Beep)
- Động cơ DC (Tiến/Lùi/Dừng/Phanh)
- Động cơ Servo RC (Đặt góc 0 - 180 độ)
- Màn hình OLED & LCD 1602
- Xóa cấu hình WiFi (Reset Config)
===================================================================
"""
import socket
import sys
import time

ESP32_PORT     = 8888
TELEMETRY_PORT = 9999

COMMAND_MENU = [
    # ── 1. ĐÈN LED (SLAVE 1) ──
    ("1",  "ON",                "Bật Đèn LED 100%"),
    ("2",  "OFF",               "Tắt Đèn LED"),
    ("3",  "LED_BLINK",         "LED Nhấp nháy 1Hz"),
    ("4",  "LED_BREATHE",       "LED Đèn thở êm dịu (Breathing)"),
    ("5",  "LED_DOUBLE_FLASH",  "LED Nháy đôi tín hiệu (Double Flash)"),
    ("6",  "LED_SOS",           "LED Mã cứu hộ SOS Morse"),
    ("7",  "LED_HEARTBEAT",     "LED Nhịp đập trái tim (Heartbeat)"),

    # ── 2. CÒI BUZZER (SLAVE 3) ──
    ("8",  "BUZ_BEEP",          "Còi kêu 1 tiếng Beep"),
    ("9",  "BUZ_ON",            "Bật còi liên tục"),
    ("10", "BUZ_OFF",           "Tắt còi"),

    # ── 3. ĐỘNG CƠ DC (SLAVE 7) ──
    ("11", "MOTOR_FWD:50",      "Động cơ DC: TIẾN 50%"),
    ("12", "MOTOR_FWD:100",     "Động cơ DC: TIẾN 100%"),
    ("13", "MOTOR_REV:50",      "Động cơ DC: LÙI 50%"),
    ("14", "MOTOR_BRAKE",       "Động cơ DC: PHANH ĐIỆN TỪ"),
    ("15", "MOTOR_STOP",        "Động cơ DC: DỪNG (Thả trôi)"),

    # ── 4. ĐỘNG CƠ SERVO (SLAVE 8) ──
    ("16", "SERVO_LEFT",        "Servo quay GÓC 0° (Trái)"),
    ("17", "SERVO_MID",         "Servo quay GÓC 90° (Giữa)"),
    ("18", "SERVO_RIGHT",       "Servo quay GÓC 180° (Phải)"),

    # ── 5. MÀN HÌNH OLED & LCD (SLAVE 6 & 9) ──
    ("19", "OLED1:XIN CHAO STEM", "OLED: Ghi dòng 1"),
    ("20", "OLED_SPLASH",       "OLED: Hiện Splash Logo"),
    ("21", "OLED_CLEAR",        "OLED: Xóa màn hình"),
    ("22", "LCD1:FABLAB VIETNAM", "LCD: Ghi dòng 1"),
    ("23", "LCD_CLEAR",         "LCD: Xóa màn hình"),

    # ── 6. HỆ THỐNG ──
    ("24", "RESET_CONFIG",      "Xóa WiFi -> ESP32 vào lại AP Mode 192.168.4.1"),
]

def find_esp32_ip(timeout_sec: float = 2.0) -> str:
    """Tự động dò IP của ESP32 Gateway qua cổng Telemetry 9999"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout_sec)
    try:
        sock.bind(("0.0.0.0", TELEMETRY_PORT))
        raw, addr = sock.recvfrom(2048)
        sock.close()
        return addr[0]
    except Exception:
        sock.close()
        return "255.255.255.255"

def send(cmd: str, target_ip: str) -> None:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.sendto(cmd.encode("utf-8"), (target_ip, ESP32_PORT))
    s.close()
    print(f"\n[SENT] '{cmd}' -> {target_ip}:{ESP32_PORT}")

def main() -> None:
    print("=" * 60)
    print("  NEURON IOT GATEWAY - CONSOLE UDP COMMANDER")
    print("=" * 60)

    # 1. Truyền lệnh trực tiếp qua tham số: python udp_commander.py LED_BREATHE
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        target_ip = sys.argv[2] if len(sys.argv) > 2 else "255.255.255.255"
        send(cmd, target_ip)
        return

    # 2. Dò IP của ESP32
    print("[...] Đang tự động dò tìm IP của ESP32 Gateway qua mạng WiFi...")
    target_ip = find_esp32_ip()
    if target_ip != "255.255.255.255":
        print(f"[OK] Đã tìm thấy ESP32 Gateway tại IP: {target_ip}")
    else:
        print("[!] Không thấy gói tin trực tiếp, sử dụng địa chỉ Broadcast (255.255.255.255)")

    # 3. Vòng lặp Menu
    menu_dict = {key: (cmd, desc) for key, cmd, desc in COMMAND_MENU}

    while True:
        print("\n" + "-" * 60)
        print(f" DANH MỤC LỆNH ĐIỀU KHIỂN (Mục tiêu: {target_ip}:{ESP32_PORT})")
        print("-" * 60)
        for key, cmd, desc in COMMAND_MENU:
            print(f"  [{key:>2}] {cmd:<20} : {desc}")
        print("  [ t] Nhập lệnh text tùy ý (Ví dụ: MOTOR_FWD:70, SERVO_ANGLE:45)")
        print("  [ r] Quét lại IP của ESP32")
        print("  [ q] Thoát chương trình")
        print("-" * 60)

        choice = input("Nhập số chọn lệnh: ").strip().lower()
        if choice == "q":
            print("\nĐã thoát.")
            break
        elif choice == "r":
            target_ip = find_esp32_ip()
            print(f"[OK] Cập nhật Target IP: {target_ip}")
        elif choice == "t":
            custom_cmd = input("Nhập lệnh tùy ý: ").strip()
            if custom_cmd:
                send(custom_cmd, target_ip)
        elif choice in menu_dict:
            send(menu_dict[choice][0], target_ip)
        else:
            print("[!] Lựa chọn không hợp lệ, vui lòng thử lại.")

if __name__ == "__main__":
    main()
