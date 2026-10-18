"""
NeuronGatewayService — Background UDP Bridge giữa ESP32 và Ứng dụng Python.
Tương thích 100% hai chiều với cả Gateway V2 (5 nodes) và Gateway V3 (12 nodes).

Chạy trong daemon thread riêng. Controller đăng ký callbacks:
    on_connected(ip)      → ESP32 vừa online
    on_disconnected()     → ESP32 offline (timeout 6s)
    on_telemetry(data)    → Nhận packet telemetry mới (10Hz)
"""
import json
import logging
import socket
import threading
import time

logger = logging.getLogger(__name__)

_TELEMETRY_PORT     = 9999
_COMMAND_PORT       = 8888
_DISCONNECT_TIMEOUT = 6.0


class NeuronGatewayService:

    def __init__(self):
        self._sock:     socket.socket | None = None
        self._cmd_sock: socket.socket | None = None
        self._thread:   threading.Thread | None = None

        self._connected        = False
        self._last_packet_time = 0.0
        self._esp32_ip:  str | None = None

        # Trạng thái Online của toàn bộ 12 Nodes trên mạng vòng Ring
        self._node_online: dict[str, bool] = {
            "led_node":      False,  # Slave 1: LED (Bật/Tắt/PWM/Smart Effects)
            "button_node":   False,  # Slave 2: Nút nhấn (Single/Double/Long Click)
            "buzzer_node":   False,  # Slave 3: Còi Buzzer
            "pot_node":      False,  # Slave 4: Biến trở B10K
            "distance_node": False,  # Slave 5: Siêu âm HC-SR04 (hoặc dist_node)
            "lcd_node":      False,  # Slave 6: Màn hình LCD 1602 I2C
            "motor_node":    False,  # Slave 7: Động cơ DC cầu H
            "servo_node":    False,  # Slave 8: Động cơ Servo RC SG90
            "oled_node":     False,  # Slave 9: Màn hình OLED 0.96 inch SSD1306
            "soil_node":     False,  # Slave 10: Cảm biến độ ẩm đất LM393
            "dht11_node":    False,  # Slave 11: Cảm biến nhiệt ẩm DHT11
            "dht22_node":    False,  # Slave 12: Cảm biến nhiệt ẩm DHT22/AM2302
        }

        # Callbacks sự kiện
        self.on_connected:    callable | None = None   # fn(ip: str)
        self.on_disconnected: callable | None = None   # fn()
        self.on_telemetry:    callable | None = None   # fn(data: dict)

    # ── Public API ────────────────────────────────────────────────────

    @property
    def is_connected(self) -> bool:
        return self._connected

    @property
    def esp32_ip(self) -> str | None:
        return self._esp32_ip

    def start(self) -> None:
        self._thread = threading.Thread(
            target=self._run, daemon=True, name="NeuronGateway")
        self._thread.start()
        logger.info("[NeuronGW] Service started — listening on 0.0.0.0:%d", _TELEMETRY_PORT)

    def stop(self) -> None:
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass

    def node_online(self, node: str) -> bool:
        """Kiểm tra trạng thái online của một node cụ thể"""
        # Hỗ trợ cả tên 'dist_node' và 'distance_node'
        if node in ("dist_node", "distance_node"):
            return self._connected and (self._node_online.get("distance_node", False) or self._node_online.get("dist_node", False))
        return self._connected and self._node_online.get(node, False)

    is_node_online = node_online

    # ── 1. ĐIỀU KHIỂN ĐÈN LED (SLAVE 1) ──────────────────────────────

    def send_led(self, is_on: bool) -> None:
        """Bật / Tắt đèn LED cơ bản (100% tương thích ngược)"""
        logger.info("[NeuronGW] send_led: %s (node_online=%s)", is_on, self.node_online("led_node"))
        self._send("ON" if is_on else "OFF")

    def send_led_mode(self, mode: str) -> None:
        """
        Kích hoạt chế độ / hiệu ứng thông minh trên Slave 1:
        'ON', 'OFF', 'BLINK', 'BREATHE', 'DOUBLE_FLASH', 'SOS', 'HEARTBEAT'
        """
        cmd_map = {
            "ON": "LED_ON", "OFF": "LED_OFF", "BLINK": "LED_BLINK",
            "BREATHE": "LED_BREATHE", "DOUBLE_FLASH": "LED_DOUBLE_FLASH",
            "SOS": "LED_SOS", "HEARTBEAT": "LED_HEARTBEAT"
        }
        cmd = cmd_map.get(mode.upper(), mode.upper())
        self._send(cmd)

    def send_led_pwm(self, duty_pct: int) -> None:
        """Điều chỉnh độ sáng đèn LED (0% đến 100%)"""
        duty = max(0, min(100, int(duty_pct)))
        self._send(f"LED_PWM:{duty}")

    # ── 2. ĐIỀU KHIỂN CÒI BUZZER (SLAVE 3) ───────────────────────────

    def send_buzzer(self, cmd: str) -> None:
        """Điều khiển còi: 'BUZ_ON', 'BUZ_OFF', 'BUZ_BEEP'"""
        logger.info("[NeuronGW] send_buzzer: %s (node_online=%s)", cmd, self.node_online("buzzer_node"))
        self._send(cmd.upper())

    # ── 3. ĐIỀU KHIỂN ĐỘNG CƠ DC (SLAVE 7) ───────────────────────────

    def send_motor(self, direction: str, speed_pct: int = 100) -> None:
        """
        Điều khiển Động cơ DC:
        direction: 'FWD' (Tiến), 'REV' (Lùi), 'STOP' (Dừng), 'BRAKE' (Phanh)
        speed_pct: 1 đến 100 (%)
        """
        spd = max(1, min(100, int(speed_pct)))
        d = direction.upper()
        if d in ("FWD", "FORWARD", "TIEN"):
            self._send(f"MOTOR_FWD:{spd}")
        elif d in ("REV", "REVERSE", "LUI", "BACK"):
            self._send(f"MOTOR_REV:{spd}")
        elif d in ("BRAKE", "PHANH"):
            self._send("MOTOR_BRAKE")
        else:
            self._send("MOTOR_STOP")

    # ── 4. ĐIỀU KHIỂN ĐỘNG CƠ SERVO RC (SLAVE 8) ─────────────────────

    def send_servo(self, angle_deg: int) -> None:
        """Đặt góc quay Servo RC từ 0 đến 180 độ"""
        ang = max(0, min(180, int(angle_deg)))
        self._send(f"SERVO_ANGLE:{ang}")

    # ── 5. ĐIỀU KHIỂN MÀN HÌNH LCD 1602 (SLAVE 6) ────────────────────

    def send_lcd(self, line: int, text: str) -> None:
        """Ghi văn bản lên màn hình LCD 1602 (line: 1 hoặc 2)"""
        clean_text = str(text)[:16]
        if line == 1:
            self._send(f"LCD1:{clean_text}")
        elif line == 2:
            self._send(f"LCD2:{clean_text}")
        elif line == 0 or clean_text.upper() == "CLEAR":
            self._send("LCD_CLEAR")

    # ── 6. ĐIỀU KHIỂN MÀN HÌNH OLED 0.96" SSD1306 (SLAVE 9) ─────────

    def send_oled(self, line: int, text: str) -> None:
        """
        Ghi văn bản hoặc lệnh lên màn hình OLED:
        line: 1 đến 4 (in dòng chữ to 8x16)
        Hoặc text: 'CLEAR', 'INVERT', 'SPLASH'
        """
        t = str(text).upper()
        if t in ("CLEAR", "OLED_CLEAR"):
            self._send("OLED_CLEAR")
        elif t in ("INVERT", "OLED_INVERT"):
            self._send("OLED_INVERT")
        elif t in ("SPLASH", "OLED_SPLASH"):
            self._send("OLED_SPLASH")
        elif 1 <= line <= 4:
            self._send(f"OLED{line}:{str(text)[:16]}")

    # ── 7. LỆNH TÙY BIẾN HOẶC RESET HỆ THỐNG ─────────────────────────

    def send_command(self, raw_cmd: str) -> None:
        """Gửi chuỗi lệnh UDP tùy biến trực tiếp xuống ESP32"""
        self._send(raw_cmd)

    def reset_gateway_config(self) -> None:
        """Xóa WiFi trên ESP32 Gateway để chuyển về AP cấu hình Web"""
        self._send("RESET_CONFIG")

    # ── Internal Communication ────────────────────────────────────────

    def _send(self, cmd: str) -> None:
        if not self._connected or not self._esp32_ip:
            logger.warning("[NeuronGW] Cannot send '%s': not connected or no ESP32 IP", cmd)
            return
        try:
            if self._cmd_sock is None:
                self._cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self._cmd_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self._cmd_sock.sendto(cmd.encode("utf-8"), (self._esp32_ip, _COMMAND_PORT))
            logger.debug("[NeuronGW] TX UDP: '%s' -> %s:%d", cmd, self._esp32_ip, _COMMAND_PORT)
        except Exception as exc:
            logger.warning("[NeuronGW] Send error: %s", exc)

    def _run(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.settimeout(1.0)
        self._sock = sock
        try:
            sock.bind(("0.0.0.0", _TELEMETRY_PORT))
            while True:
                try:
                    raw, addr = sock.recvfrom(65535)
                    self._handle_packet(raw, addr)
                except socket.timeout:
                    self._check_timeout()
        except OSError as exc:
            logger.error("[NeuronGW] Socket error: %s", exc)
        finally:
            sock.close()

    def _handle_packet(self, raw: bytes, addr: tuple) -> None:
        now           = time.monotonic()
        was_connected = self._connected
        ip            = addr[0]

        self._esp32_ip         = ip
        self._last_packet_time = now
        self._connected        = True

        if not was_connected:
            logger.info("[NeuronGW] ESP32 CONNECTED: %s", ip)
            if self.on_connected:
                self.on_connected(ip)

        try:
            data = json.loads(raw.decode("utf-8").strip().rstrip("\x00"))
        except Exception:
            return

        if data.get("type") == "HELLO":
            logger.info("[NeuronGW] HELLO from %s (IP: %s)", data.get("device_id"), ip)
            return

        # Cập nhật trạng thái online của các node (Tương thích cả V2 dict và V3 int/bool)
        topology = data.get("network_topology", {})
        for node in self._node_online:
            node_val = topology.get(node)
            if node == "distance_node" and node_val is None:
                node_val = topology.get("dist_node")

            if isinstance(node_val, dict):
                # Chuẩn Gateway V2: {"online": 1, ...}
                self._node_online[node] = (node_val.get("online", 0) == 1)
            elif isinstance(node_val, (int, bool)):
                # Chuẩn Gateway V3: "led_node": 1
                self._node_online[node] = (node_val == 1) or (node_val is True)

        if self.on_telemetry:
            self.on_telemetry(data)

    def _check_timeout(self) -> None:
        if self._connected and self._last_packet_time > 0:
            if time.monotonic() - self._last_packet_time > _DISCONNECT_TIMEOUT:
                self._connected = False
                self._esp32_ip  = None
                logger.info("[NeuronGW] ESP32 DISCONNECTED (timeout)")
                if self.on_disconnected:
                    self.on_disconnected()
