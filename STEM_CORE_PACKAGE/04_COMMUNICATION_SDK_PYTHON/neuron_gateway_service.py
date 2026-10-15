"""
NeuronGatewayService — background UDP bridge giữa ESP32 và ElectroBlocks.

Chạy trong daemon thread riêng. Controller đăng ký callbacks:
    on_connected(ip)      → ESP32 vừa online
    on_disconnected()     → ESP32 offline (timeout 3s)
    on_telemetry(data)    → nhận packet telemetry mới
"""
import json
import logging
import socket
import threading
import time

logger = logging.getLogger(__name__)

_TELEMETRY_PORT    = 9999
_COMMAND_PORT      = 8888
_DISCONNECT_TIMEOUT = 6.0


class NeuronGatewayService:

    def __init__(self):
        self._sock:     socket.socket | None = None
        self._cmd_sock: socket.socket | None = None
        self._thread:   threading.Thread | None = None

        self._connected        = False
        self._last_packet_time = 0.0
        self._esp32_ip:  str | None = None

        # Node status — track từng slave trên UART ring
        self._node_online: dict[str, bool] = {
            "led_node":      False,
            "button_node":   False,
            "buzzer_node":   False,
            "pot_node":      False,
            "distance_node": False,
        }

        # Callbacks — controller gán trực tiếp
        self.on_connected:    callable | None = None   # fn(ip: str)
        self.on_disconnected: callable | None = None   # fn()
        self.on_telemetry:    callable | None = None   # fn(data: dict)

    # ── Public API ────────────────────────────────────────────────────

    @property
    def is_connected(self) -> bool:
        return self._connected

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
        return self._connected and self._node_online.get(node, False)

    def send_led(self, is_on: bool) -> None:
        logger.info("[NeuronGW] send_led: %s (node_online=%s)", is_on, self.node_online("led_node"))
        self._send("ON" if is_on else "OFF")

    def send_buzzer(self, cmd: str) -> None:
        logger.info("[NeuronGW] send_buzzer: %s (node_online=%s)", cmd, self.node_online("buzzer_node"))
        self._send(cmd)

    # ── Internal ──────────────────────────────────────────────────────

    def _send(self, cmd: str) -> None:
        if not self._connected or not self._esp32_ip:
            logger.warning("[NeuronGW] Cannot send '%s': not connected or no ESP32 IP", cmd)
            return
        try:
            if self._cmd_sock is None:
                self._cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self._cmd_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self._cmd_sock.sendto(cmd.encode(), (self._esp32_ip, _COMMAND_PORT))
            logger.info("[NeuronGW] TX UDP: '%s' -> %s:%d", cmd, self._esp32_ip, _COMMAND_PORT)
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
        now          = time.monotonic()
        was_connected = self._connected
        ip           = addr[0]

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
            logger.info("[NeuronGW] HELLO from %s", data.get("device_id"))
            return

        # Cập nhật node status từ network_topology
        topology = data.get("network_topology", {})
        for node in self._node_online:
            self._node_online[node] = topology.get(node, {}).get("online", 0) == 1

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
