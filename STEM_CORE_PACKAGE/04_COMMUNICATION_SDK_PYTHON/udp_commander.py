import socket
import sys

ESP32_PORT = 8888
BROADCAST   = "192.168.10.7"

COMMANDS = {
    "1": ("ON",           "Bat LED"),
    "2": ("OFF",          "Tat LED"),
    "3": ("BUZ_ON",       "Bat Buzzer"),
    "4": ("BUZ_OFF",      "Tat Buzzer"),
    "5": ("BUZ_BEEP",     "Beep Buzzer"),
    "6": ("RESET_CONFIG", "Xoa WiFi -> ESP32 vao AP mode de cau hinh lai"),
}

def send(cmd: str) -> None:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.sendto(cmd.encode("utf-8"), (BROADCAST, ESP32_PORT))
    s.close()
    print(f"[SENT] {cmd} -> {BROADCAST}:{ESP32_PORT}")

def main() -> None:
    # Cho phép truyền lệnh trực tiếp qua argument: python udp_commander.py RESET_CONFIG
    if len(sys.argv) > 1:
        send(sys.argv[1].upper())
        return

    print("=" * 45)
    print("  NEURON GATEWAY - UDP COMMANDER")
    print(f"  Target: {BROADCAST}:{ESP32_PORT}")
    print("=" * 45)
    for key, (cmd, desc) in COMMANDS.items():
        print(f"  [{key}] {cmd:<15} - {desc}")
    print("  [q] Thoat")
    print("=" * 45)

    while True:
        choice = input("Chon lenh: ").strip().lower()
        if choice == "q":
            break
        if choice in COMMANDS:
            send(COMMANDS[choice][0])
        else:
            print("Lenh khong hop le.")

if __name__ == "__main__":
    main()
