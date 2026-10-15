"""
===================================================================
VÍ DỤ MẪU: KẾT NỐI VÀ ĐIỀU KHIỂN HỆ SINH THÁI STEM PHẦN CỨNG QUA UDP
===================================================================
Chạy script này độc lập để:
1. Lắng nghe luồng dữ liệu cảm biến (Port 9999) từ ESP32 Gateway.
2. Gửi lệnh điều khiển (Port 8888) bật/tắt đèn, còi, ...
Không cần cài đặt App FabLab STEM! Chỉ cần Python 3 tiêu chuẩn.
"""

import json
import socket
import sys

TELEMETRY_PORT = 9999
COMMAND_PORT   = 8888

def main():
    print("=" * 60)
    print("  STEM HARDWARE CORE CLIENT - PYTHON STANDALONE DEMO")
    print("=" * 60)

    # 1. Khởi tạo Socket lắng nghe Telemetry
    rx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    rx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    rx_sock.bind(("0.0.0.0", TELEMETRY_PORT))

    # 2. Khởi tạo Socket gửi lệnh điều khiển
    tx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    print(f"[OK] Đang lắng nghe Telemetry tại 0.0.0.0:{TELEMETRY_PORT}...")
    print(f"[!] Nhấn Ctrl+C để dừng chương trình.\n")

    esp32_ip = None

    try:
        while True:
            raw_data, addr = rx_sock.recvfrom(4096)
            esp32_ip = addr[0]

            try:
                packet = json.loads(raw_data.decode("utf-8").strip().rstrip("\x00"))
            except Exception:
                continue

            # Bỏ qua gói tin bắt tay HELLO ban đầu
            if packet.get("type") == "HELLO":
                print(f"[GATEWAY] Nhận diện ESP32 Gateway tại IP: {esp32_ip}")
                continue

            # Trích xuất dữ liệu cảm biến
            telemetry = packet.get("telemetry", {})
            topology  = packet.get("network_topology", {})

            # 1. Cảm biến khoảng cách (Node 5)
            dist_cm = telemetry.get("distance", {}).get("distance_cm", 0)

            # 2. Biến trở (Node 4)
            pot_val = telemetry.get("potentiometer", {}).get("raw_adc", 0)
            pot_pct = round(pot_val / 255.0 * 100)

            # 3. Nút bấm (Node 2)
            btn_state = telemetry.get("button", {}).get("raw_state", 0)

            # 4. Trạng thái online của các node
            led_onl  = "ON " if topology.get("led_node", {}).get("online", 0) else "OFF"
            btn_onl  = "ON " if topology.get("button_node", {}).get("online", 0) else "OFF"
            pot_onl  = "ON " if topology.get("pot_node", {}).get("online", 0) else "OFF"
            dist_onl = "ON " if topology.get("distance_node", {}).get("online", 0) else "OFF"

            # In thông tin thời gian thực ra màn hình console
            sys.stdout.write(
                f"\r📡 [ESP: {esp32_ip}] "
                f"📏 KC: {dist_cm:3d} cm | "
                f"🎛️ Biến trở: {pot_pct:3d}% | "
                f"🔘 Nút: {'NHẤN' if btn_state else 'NHẢ '} | "
                f"Node: [L:{led_onl} B:{btn_onl} P:{pot_onl} D:{dist_onl}]"
            )
            sys.stdout.flush()

            # --- VÍ DỤ TỰ ĐỘNG ĐIỀU KHIỂN (AUTOMATION LOGIC) ---
            # Ví dụ: Nếu khoảng cách < 15cm thì tự động gửi lệnh bật đèn LED
            if dist_cm > 0 and dist_cm < 15:
                tx_sock.sendto(b"ON", (esp32_ip, COMMAND_PORT))
            else:
                # Nếu nút bấm đang được nhấn thì bật còi
                if btn_state == 1:
                    tx_sock.sendto(b"BUZ_ON", (esp32_ip, COMMAND_PORT))
                else:
                    tx_sock.sendto(b"BUZ_OFF", (esp32_ip, COMMAND_PORT))
                    tx_sock.sendto(b"OFF", (esp32_ip, COMMAND_PORT))

    except KeyboardInterrupt:
        print("\n\n[INFO] Đã dừng chương trình thành công.")
    finally:
        rx_sock.close()
        tx_sock.close()

if __name__ == "__main__":
    main()
