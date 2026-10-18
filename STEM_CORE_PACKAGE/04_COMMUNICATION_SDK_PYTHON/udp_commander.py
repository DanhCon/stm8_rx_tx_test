"""
===================================================================
NEURON GATEWAY - UDP COMMANDER (CONSOLE MULTI-KIT TESTING TOOL)
===================================================================
Công cụ gửi lệnh nhanh qua mạng UDP (Cổng 8888) hỗ trợ Multi-Kit:
- Tự động dò tìm tất cả các bộ kit ESP32 đang hoạt động trong cùng mạng WiFi.
- Cho phép chọn đích danh bộ kit cần điều khiển (Ví dụ: NHOM_01, NHOM_02).
- Khóa gửi lệnh Unicast + Prefix (@NHOM_01:CMD) chống bấm nhầm mạch nhóm khác.
- Hỗ trợ đổi tên bộ kit từ xa (SET_ID:NHOM_xx).
- Kiểm tra toàn bộ 12 module Slaves: Đèn thông minh, Còi, Động cơ DC, Servo, OLED, LCD...
===================================================================
"""
import json
import socket
import sys
import time

if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

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

    # ── 6. HỆ THỐNG & ĐỊNH DANH ──
    ("24", "GET_ID",            "Đọc mã Device ID hiện tại của ESP32"),
    ("25", "RESET_CONFIG",      "Xóa WiFi -> ESP32 vào lại AP Mode 192.168.4.1"),
]

def scan_online_kits(timeout_sec: float = 2.0) -> dict[str, dict]:
    """
    Quét cổng Telemetry 9999 trong timeout_sec giây để tìm tất cả các bộ kit đang online.
    Trả về dict: { "device_id": {"ip": str, "rssi": int, "uptime_s": int} }
    """
    kits = {}
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.3)
    try:
        sock.bind(("0.0.0.0", TELEMETRY_PORT))
        start = time.time()
        while time.time() - start < timeout_sec:
            try:
                raw, addr = sock.recvfrom(2048)
                data = json.loads(raw.decode("utf-8").strip().rstrip("\x00"))
                gw = data.get("gateway", {})
                dev_id = str(gw.get("device_id") or data.get("device_id") or "UNKNOWN").strip()
                if dev_id != "UNKNOWN" and dev_id not in kits:
                    kits[dev_id] = {
                        "ip": addr[0],
                        "device_id": dev_id,
                        "rssi": gw.get("wifi_rssi", 0),
                        "uptime_s": gw.get("uptime_s", 0)
                    }
            except (socket.timeout, json.JSONDecodeError, UnicodeDecodeError):
                continue
    except Exception:
        pass
    finally:
        sock.close()
    return kits

def send(cmd: str, target_ip: str, target_device_id: str = "") -> None:
    """Gửi lệnh UDP có bảo vệ 2 lớp (Unicast + Prefix @DEVICE_ID:CMD)"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    if target_device_id and not cmd.startswith("@"):
        wire_cmd = f"@{target_device_id}:{cmd}"
    else:
        wire_cmd = cmd

    s.sendto(wire_cmd.encode("utf-8"), (target_ip, ESP32_PORT))
    s.close()
    dest_str = f"[{target_device_id}] @ {target_ip}:{ESP32_PORT}" if target_device_id else f"{target_ip}:{ESP32_PORT}"
    print(f"\n[SENT] '{wire_cmd}' -> {dest_str}")

def select_kit_interactive(kits: dict[str, dict]) -> tuple[str, str]:
    """Hiển thị danh sách các bộ kit tìm thấy và cho người dùng chọn"""
    kit_list = list(kits.values())

    if len(kit_list) == 0:
        print("[!] Không tìm thấy bộ kit nào đang phát trực tiếp.")
        print("[!] Tự động chuyển sang chế độ Broadcast (255.255.255.255).")
        return "255.255.255.255", "ALL"

    if len(kit_list) == 1:
        chosen = kit_list[0]
        print(f"[OK] Tìm thấy duy nhất 1 bộ kit: [{chosen['device_id']}] tại IP {chosen['ip']} (RSSI: {chosen['rssi']} dBm)")
        print(f"[OK] Đã tự động ghép nối độc quyền với [{chosen['device_id']}]!")
        return chosen["ip"], chosen["device_id"]

    print("\n" + "=" * 65)
    print(f"  TÌM THẤY {len(kit_list)} BỘ KIT NEURON TRÊN MẠNG WIFI NÀY:")
    print("=" * 65)
    for idx, k in enumerate(kit_list, 1):
        rssi_str = "Rất khỏe" if k["rssi"] > -60 else ("Bình thường" if k["rssi"] > -75 else "Yếu")
        print(f"  [{idx}] {k['device_id']:<14} | IP: {k['ip']:<15} | Sóng: {k['rssi']} dBm ({rssi_str})")
    print("=" * 65)

    while True:
        choice = input(f"Chọn số bộ kit bạn muốn điều khiển [1-{len(kit_list)}] (mặc định 1): ").strip()
        if not choice:
            chosen = kit_list[0]
            break
        try:
            num = int(choice)
            if 1 <= num <= len(kit_list):
                chosen = kit_list[num - 1]
                break
        except ValueError:
            pass
        print("[!] Lựa chọn không hợp lệ, vui lòng nhập lại.")

    print(f"\n[OK] ĐÃ KHÓA KẾT NỐI VỚI BỘ KIT: [{chosen['device_id']}] (IP: {chosen['ip']})")
    print("    -> Các lệnh gửi đi sẽ chỉ điều khiển duy nhất bộ kit này!")
    return chosen["ip"], chosen["device_id"]

def main() -> None:
    print("=" * 65)
    print("   NEURON IOT GATEWAY - CONSOLE UDP MULTI-KIT COMMANDER")
    print("=" * 65)

    # 1. Truyền lệnh trực tiếp qua tham số: python udp_commander.py LED_BREATHE [TARGET_IP] [DEV_ID]
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        target_ip = sys.argv[2] if len(sys.argv) > 2 else "255.255.255.255"
        dev_id = sys.argv[3] if len(sys.argv) > 3 else ""
        send(cmd, target_ip, dev_id)
        return

    # 2. Dò tìm danh sách các bộ kit trên mạng
    print("[...] Đang tự động quét tìm các bộ kit ESP32 qua mạng WiFi (2s)...")
    found_kits = scan_online_kits(timeout_sec=2.0)
    target_ip, target_device_id = select_kit_interactive(found_kits)

    # 3. Vòng lặp Menu
    menu_dict = {key: (cmd, desc) for key, cmd, desc in COMMAND_MENU}

    while True:
        print("\n" + "-" * 65)
        print(f" DANH MỤC LỆNH [ĐÍCH: {target_device_id} | {target_ip}:{ESP32_PORT}]")
        print("-" * 65)
        for key, cmd, desc in COMMAND_MENU:
            print(f"  [{key:>2}] {cmd:<20} : {desc}")
        print("  [ t] Nhập lệnh text tùy ý (Ví dụ: MOTOR_FWD:70, SERVO_ANGLE:45)")
        print("  [ s] Đổi Device ID của bộ kit này (Ví dụ đặt: NHOM_01)")
        print("  [ k] Quét và đổi sang bộ kit khác trên mạng WiFi")
        print("  [ q] Thoát chương trình")
        print("-" * 65)

        choice = input("Nhập lựa chọn: ").strip().lower()
        if choice == "q":
            print("\nĐã thoát.")
            break
        elif choice == "k":
            print("\n[...] Đang quét lại mạng WiFi...")
            found_kits = scan_online_kits(timeout_sec=2.0)
            target_ip, target_device_id = select_kit_interactive(found_kits)
        elif choice == "s":
            new_id = input(f"Nhập ID mới cho bộ kit [{target_device_id}]: ").strip()
            if new_id:
                send(f"SET_ID:{new_id}", target_ip, target_device_id)
                target_device_id = new_id
                print(f"[OK] Đã cập nhật mục tiêu sang ID mới: [{target_device_id}]")
        elif choice == "t":
            custom_cmd = input("Nhập lệnh tùy ý: ").strip()
            if custom_cmd:
                send(custom_cmd, target_ip, target_device_id)
        elif choice in menu_dict:
            send(menu_dict[choice][0], target_ip, target_device_id)
        else:
            print("[!] Lựa chọn không hợp lệ, vui lòng thử lại.")


if __name__ == "__main__":
    main()
