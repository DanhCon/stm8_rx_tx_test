"""
===================================================================
VÍ DỤ MẪU: KẾT NỐI VÀ ĐIỀU KHIỂN HỆ SINH THÁI STEM PHẦN CỨNG QUA UDP
===================================================================
Chạy script này độc lập để:
1. Lắng nghe luồng dữ liệu Telemetry (Port 9999) từ ESP32 Gateway (V2 hoặc V3).
2. Hiển thị trực quan dữ liệu cảm biến: Nút nhấn cử chỉ, Siêu âm, Biến trở, Độ ẩm đất, DHT11, DHT22.
3. Gửi lệnh điều khiển (Port 8888) bật/tắt đèn, còi, động cơ, servo.
Không cần cài đặt App FabLab STEM! Chỉ cần Python 3 tiêu chuẩn.
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

TELEMETRY_PORT = 9999
COMMAND_PORT   = 8888


def main():
    print("=" * 70)
    print("  STEM HARDWARE CORE CLIENT - PYTHON STANDALONE MONITOR & CONTROL")
    print("  Tương thích 100% với cả Gateway V2 (5 nodes) và Gateway V3 (12 nodes)")
    print("=" * 70)

    # 1. Khởi tạo Socket lắng nghe Telemetry UDP
    rx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    rx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    rx_sock.bind(("0.0.0.0", TELEMETRY_PORT))

    # 2. Khởi tạo Socket gửi lệnh điều khiển UDP
    tx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    print(f"[OK] Đang lắng nghe Telemetry tại 0.0.0.0:{TELEMETRY_PORT}...")
    print(f"[!] Nhấn Ctrl+C để dừng chương trình.\n")

    target_dev_id = sys.argv[1].strip() if len(sys.argv) > 1 else None
    if target_dev_id:
        print(f"[!] Đã cài đặt khóa mục tiêu: [{target_dev_id}]. Sẽ bỏ qua mọi bộ kit khác!")
    else:
        print("[*] Chế độ tự động: Sẽ tự động khóa vào bộ kit đầu tiên nhận diện được.")

    esp32_ip = None
    last_print = 0

    try:
        while True:
            raw_data, addr = rx_sock.recvfrom(4096)
            esp32_ip = addr[0]

            try:
                packet = json.loads(raw_data.decode("utf-8").strip().rstrip("\x00"))
            except Exception:
                continue

            # Lấy Device ID của gói tin
            dev_id = str(packet.get("gateway", {}).get("device_id") or packet.get("device_id") or "UNKNOWN").strip()

            # LỌC GÓI TIN ĐÍCH DANH (DEVICE FILTERING):
            if target_dev_id:
                if dev_id.upper() != target_dev_id.upper():
                    # Gói tin từ bộ kit khác -> BỎ QUA NGAY!
                    continue
            else:
                # Tự động khóa vào bộ kit đầu tiên
                target_dev_id = dev_id
                print(f"[OK] Đã tự động ghép nối độc quyền với bộ kit: [{target_dev_id}] tại {esp32_ip}")

            # Bỏ qua gói tin bắt tay HELLO ban đầu
            if packet.get("type") == "HELLO":
                print(f"[GATEWAY] Nhận diện ESP32 Gateway [{dev_id}] tại IP: {esp32_ip}")
                continue

            # ----------------------------------------------------
            # TRÍCH XUẤT DỮ LIỆU CẢM BIẾN (TƯƠNG THÍCH CẢ V2 VÀ V3)
            # ----------------------------------------------------
            telemetry = packet.get("telemetry", {})
            topology  = packet.get("network_topology", {})

            # 1. Khoảng cách (Slave 5)
            dist_cm = telemetry.get("distance", {}).get("distance_cm", 0)

            # 2. Biến trở (Slave 4)
            pot_val = telemetry.get("potentiometer", {}).get("raw_adc", 0)
            pot_pct = round(pot_val / 255.0 * 100)

            # 3. Nút bấm (Slave 2) - Hỗ trợ cả trạng thái raw và cử chỉ Gestures
            btn_data = telemetry.get("button", {})
            btn_state = btn_data.get("raw_state", 0)
            btn_gesture = btn_data.get("gesture", "NHẤN" if btn_state else "NHẢ")

            # 4. Độ ẩm đất (Slave 10 - nếu có)
            soil_pct = telemetry.get("soil_moisture", {}).get("moisture_pct", None)

            # 5. Nhiệt ẩm DHT11 & DHT22 (Slave 11 & 12 - nếu có)
            dht11_temp = telemetry.get("dht11", {}).get("temperature_c", None)
            dht22_temp = telemetry.get("dht22", {}).get("temperature_c", None)

            # Đếm số node đang Online
            online_count = 0
            for k, v in topology.items():
                if isinstance(v, dict) and v.get("online", 0) == 1:
                    online_count += 1
                elif isinstance(v, (int, bool)) and (v == 1 or v is True):
                    online_count += 1

            # In thông tin cập nhật
            now = time.time()
            if now - last_print >= 0.08:  # Giới hạn in console ~12Hz để chống giật chữ
                last_print = now
                extra_sensor_info = ""
                if soil_pct is not None:
                    extra_sensor_info += f" | 🌱 Đất: {soil_pct}%"
                if dht22_temp is not None:
                    extra_sensor_info += f" | 🌡️ DHT22: {dht22_temp:.1f}°C"
                elif dht11_temp is not None:
                    extra_sensor_info += f" | 🌡️ DHT11: {dht11_temp}°C"

                sys.stdout.write(
                    f"\r📡 [ESP: {esp32_ip} - {online_count} Nodes] "
                    f"📏 KC: {dist_cm:3d} cm | "
                    f"🎛️ Chiết áp: {pot_pct:3d}% | "
                    f"🔘 Nút: {btn_gesture:<12}"
                    f"{extra_sensor_info}    "
                )
                sys.stdout.flush()

            # --- VÍ DỤ TỰ ĐỘNG HÓA CỤC BỘ BẰNG PYTHON ---
            # Nếu vật cản quá gần (< 15cm) thì tự động bật còi cảnh báo
            # (Bạn có thể bỏ comment dưới đây để kích hoạt tính năng tự động)
            # if dist_cm > 0 and dist_cm < 15:
            #     tx_sock.sendto(b"BUZ_BEEP", (esp32_ip, COMMAND_PORT))

    except KeyboardInterrupt:
        print("\n\n[INFO] Đã dừng chương trình thành công.")
    finally:
        rx_sock.close()
        tx_sock.close()


if __name__ == "__main__":
    main()
