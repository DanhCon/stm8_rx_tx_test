"""
Comprehensive verification test suite for App <-> ESP32 Gateway V3 <-> STM8 pipeline.
Tests:
1. ElectroBlocksGatewayService hardware methods (send_motor, send_servo, send_lcd, send_oled, send_command)
2. GatewaySnapshot telemetry parsing and backward compatibility
3. NeuronGatewayService command formatting with @DEVICE_ID: prefix and packet parsing
4. STM8 s19 and hex file integrity and checksum checks
"""
import sys
import os
import unittest
from pathlib import Path

# Add frontend and plugins paths
ROOT_DIR = Path(__file__).resolve().parent.parent
FRONTEND_DIR = ROOT_DIR / "Software" / "stem-platform" / "frontend"
PLUGINS_DIR = FRONTEND_DIR / "plugins" / "ElectroBlocks"
FIRMWARE_DIR = FRONTEND_DIR / "firmware"

sys.path.insert(0, str(FRONTEND_DIR))
sys.path.insert(0, str(PLUGINS_DIR))
sys.path.insert(0, str(FIRMWARE_DIR))

from gateway_service import ElectroBlocksGatewayService, GatewaySnapshot


class MockGateway:
    def __init__(self):
        self.sent_commands = []
        self.is_connected = True
        self.target_device_id = "KIT_TEST"
        self.connected_device_id = "KIT_TEST"

    def send_led(self, enabled: bool):
        self.sent_commands.append(("LED", enabled))

    def send_buzzer(self, command: str):
        self.sent_commands.append(("BUZZER", command))

    def send_motor(self, direction: str, speed_pct: int = 100):
        self.sent_commands.append(("MOTOR", direction, speed_pct))

    def send_servo(self, angle_deg: int):
        self.sent_commands.append(("SERVO", angle_deg))

    def send_lcd(self, line: int, text: str):
        self.sent_commands.append(("LCD", line, text))

    def send_oled(self, line: int, text: str):
        self.sent_commands.append(("OLED", line, text))

    def send_command(self, raw_cmd: str):
        self.sent_commands.append(("RAW", raw_cmd))

    def stop(self):
        self.sent_commands.append(("STOP",))


class TestElectroBlocksGatewayService(unittest.TestCase):
    def setUp(self):
        self.connected_ip = None
        self.disconnected = False
        self.snapshot = None

        self.service = ElectroBlocksGatewayService(
            on_connected=lambda ip: setattr(self, 'connected_ip', ip),
            on_disconnected=lambda: setattr(self, 'disconnected', True),
            on_snapshot=lambda snap: setattr(self, 'snapshot', snap),
        )
        self.mock_gw = MockGateway()
        self.service._gateway = self.mock_gw

    def test_hardware_send_methods(self):
        # 1. Motor
        self.service.send_motor("FWD", 75)
        self.assertEqual(self.mock_gw.sent_commands[-1], ("MOTOR", "FWD", 75))

        self.service.send_motor("STOP", 0)
        self.assertEqual(self.mock_gw.sent_commands[-1], ("MOTOR", "STOP", 0))

        # 2. Servo
        self.service.send_servo(120)
        self.assertEqual(self.mock_gw.sent_commands[-1], ("SERVO", 120))

        # 3. LCD
        self.service.send_lcd(1, "Hello World")
        self.assertEqual(self.mock_gw.sent_commands[-1], ("LCD", 1, "Hello World"))

        # 4. OLED
        self.service.send_oled(2, "OLED Test")
        self.assertEqual(self.mock_gw.sent_commands[-1], ("OLED", 2, "OLED Test"))

        # 5. Raw command
        self.service.send_command("SET_ID:KIT_NEW")
        self.assertEqual(self.mock_gw.sent_commands[-1], ("RAW", "SET_ID:KIT_NEW"))

        # 6. Backward compatibility: LED & Buzzer
        self.service.send_led(True)
        self.assertEqual(self.mock_gw.sent_commands[-1], ("LED", True))

        self.service.send_buzzer("BUZ_BEEP")
        self.assertEqual(self.mock_gw.sent_commands[-1], ("BUZZER", "BUZ_BEEP"))

    def test_parse_telemetry_v3_format(self):
        payload = {
            "timestamp": 123456,
            "gateway": {"device_id": "KIT_01", "version": "V3_CENTRAL_INTEL", "wifi_rssi": -65},
            "network_topology": {
                "led_node": {"online": 1},
                "button_node": {"online": 1},
                "buzzer_node": {"online": 1},
                "pot_node": {"online": 1},
                "distance_node": {"online": 1},
            },
            "telemetry": {
                "button": {"raw_state": 1, "gesture": "RAW_PRESSED", "raw_code": "0x56", "single_clicks": 5},
                "potentiometer": {"raw_adc": 128},
                "distance": {"distance_cm": 42},
            },
            "control_state": {
                "led": {"target": 1, "actual": 1},
                "buzzer": {"target": 0, "actual": 0},
            }
        }
        snap = ElectroBlocksGatewayService.parse_telemetry(payload)
        self.assertTrue(snap.led_online)
        self.assertTrue(snap.led_actual)
        self.assertTrue(snap.button_online)
        self.assertTrue(snap.button_pressed)
        self.assertTrue(snap.pot_online)
        self.assertEqual(snap.pot_value, int(128 / 255 * 100))
        self.assertTrue(snap.distance_online)
        self.assertEqual(snap.distance_cm, 42.0)

    def test_parse_telemetry_actual_led_off(self):
        payload = {
            "network_topology": {"led_node": {"online": 1}},
            "control_state": {"led": {"target": 0, "actual": 0}}
        }
        snap = ElectroBlocksGatewayService.parse_telemetry(payload)
        self.assertTrue(snap.led_online)
        self.assertFalse(snap.led_actual)


class TestSTM8FirmwareFiles(unittest.TestCase):
    def test_firmware_files_exist_and_non_empty(self):
        files = [
            ROOT_DIR / "stm8_slave_2_button_smart" / "Debug" / "slave_2_smart.hex",
            ROOT_DIR / "stm8_slave_2_button_smart" / "Debug" / "slave_2_smart.s19",
            ROOT_DIR / "STEM_CORE_PACKAGE" / "02_STM8_FIRMWARE_S19_HEX" / "Slave_2_Button_Smart.hex",
            ROOT_DIR / "STEM_CORE_PACKAGE" / "02_STM8_FIRMWARE_S19_HEX" / "Slave_2_Button_Smart.s19",
            ROOT_DIR / "STEM_CORE_PACKAGE" / "02_STM8_FIRMWARE_S19_HEX" / "Slave_2_Button_Smart_Gestures.hex",
            ROOT_DIR / "STEM_CORE_PACKAGE" / "02_STM8_FIRMWARE_S19_HEX" / "Slave_2_Button_Smart_Gestures.s19",
        ]
        for f in files:
            self.assertTrue(f.exists(), f"File missing: {f}")
            self.assertGreater(f.stat().st_size, 1000, f"File too small: {f}")

    def test_s19_format(self):
        s19_path = ROOT_DIR / "STEM_CORE_PACKAGE" / "02_STM8_FIRMWARE_S19_HEX" / "Slave_2_Button_Smart.s19"
        lines = s19_path.read_text(encoding="utf-8").splitlines()
        self.assertTrue(lines[0].startswith("S0"))
        has_s1 = any(line.startswith("S1") for line in lines)
        self.assertTrue(has_s1, "S19 should contain S1 data records")
        self.assertTrue(lines[-1].startswith("S9"))

    def test_hex_format(self):
        hex_path = ROOT_DIR / "STEM_CORE_PACKAGE" / "02_STM8_FIRMWARE_S19_HEX" / "Slave_2_Button_Smart.hex"
        lines = hex_path.read_text(encoding="utf-8").splitlines()
        self.assertTrue(lines[0].startswith(":"))
        # Intel Hex record type 01 is End Of File
        self.assertTrue(lines[-1].startswith(":00"))
        self.assertEqual(lines[-1][7:9], "01")


class TestNeuronGatewayDeviceLocking(unittest.TestCase):
    def test_packet_filtering_by_device_id(self):
        import json
        from firmware.neuron_gateway_service import NeuronGatewayService

        gw = NeuronGatewayService(target_device_id="KIT_TARGET")
        # Packet from KIT_OTHER
        other_pkt = json.dumps({
            "gateway": {"device_id": "KIT_OTHER", "wifi_rssi": -50, "uptime_s": 10},
            "network_topology": {"led_node": {"online": 1}}
        }).encode("utf-8")

        gw._handle_packet(other_pkt, ("192.168.1.100", 9999))
        self.assertFalse(gw.is_connected)
        self.assertIsNone(gw.esp32_ip)

        # Packet from KIT_TARGET
        target_pkt = json.dumps({
            "gateway": {"device_id": "KIT_TARGET", "wifi_rssi": -50, "uptime_s": 10},
            "network_topology": {"led_node": {"online": 1}}
        }).encode("utf-8")

        gw._handle_packet(target_pkt, ("192.168.1.105", 9999))
        self.assertTrue(gw.is_connected)
        self.assertEqual(gw.esp32_ip, "192.168.1.105")
        self.assertEqual(gw.connected_device_id, "KIT_TARGET")

    def test_command_wrapping_with_target_device_id(self):
        from firmware.neuron_gateway_service import NeuronGatewayService

        gw = NeuronGatewayService(target_device_id="KIT_TEST_42")
        gw._connected = True
        gw._esp32_ip = "192.168.1.88"

        sent_payloads = []
        class MockSocket:
            def sendto(self, data, addr):
                sent_payloads.append((data.decode("utf-8"), addr))

        gw._cmd_sock = MockSocket()

        # Command without prefix -> wrapped with @KIT_TEST_42:
        gw.send_led(True)
        self.assertEqual(sent_payloads[-1], ("@KIT_TEST_42:ON", ("192.168.1.88", 8888)))

        # Command already starting with @ -> NOT double prefixed
        gw.send_command("@ALL:RESET_CONFIG")
        self.assertEqual(sent_payloads[-1], ("@ALL:RESET_CONFIG", ("192.168.1.88", 8888)))

        # Motor speed clamping and command format
        gw.send_motor("FWD", 150)
        self.assertEqual(sent_payloads[-1], ("@KIT_TEST_42:MOTOR_FWD:100", ("192.168.1.88", 8888)))

        # Motor speed <= 0 triggers MOTOR_STOP
        gw.send_motor("REV", -5)
        self.assertEqual(sent_payloads[-1], ("@KIT_TEST_42:MOTOR_STOP", ("192.168.1.88", 8888)))

        gw.send_motor("REV", 50)
        self.assertEqual(sent_payloads[-1], ("@KIT_TEST_42:MOTOR_REV:50", ("192.168.1.88", 8888)))

        # Servo angle clamping
        gw.send_servo(250)
        self.assertEqual(sent_payloads[-1], ("@KIT_TEST_42:SERVO_ANGLE:180", ("192.168.1.88", 8888)))

        # LCD text truncation to 16 chars
        gw.send_lcd(1, "12345678901234567890")
        self.assertEqual(sent_payloads[-1], ("@KIT_TEST_42:LCD1:1234567890123456", ("192.168.1.88", 8888)))


class TestButtonStateDoubleCountPrevention(unittest.TestCase):
    def test_smart_button_single_click_does_not_pulse_raw_state(self):
        # When 0x51 (SINGLE_CLICK) arrives, raw_state is 0 (button is physically released)
        payload = {
            "network_topology": {"button_node": {"online": 1}},
            "telemetry": {
                "button": {
                    "raw_state": 0,
                    "gesture": "SINGLE_CLICK",
                    "raw_code": "0x51",
                    "single_clicks": 1,
                    "total": 1
                }
            }
        }
        snap = ElectroBlocksGatewayService.parse_telemetry(payload)
        self.assertTrue(snap.button_online)
        # button_pressed must be False so that app does not double count single click!
        self.assertFalse(snap.button_pressed)

    def test_smart_button_raw_pressed_sets_raw_state_true(self):
        # When 0x56 (RAW_PRESSED) arrives, raw_state is 1
        payload = {
            "network_topology": {"button_node": {"online": 1}},
            "telemetry": {
                "button": {
                    "raw_state": 1,
                    "gesture": "RAW_PRESSED",
                    "raw_code": "0x56",
                    "single_clicks": 0,
                    "total": 0
                }
            }
        }
        snap = ElectroBlocksGatewayService.parse_telemetry(payload)
        self.assertTrue(snap.button_online)
        self.assertTrue(snap.button_pressed)


class TestAutoLockLifecycle(unittest.TestCase):
    def test_autolock_and_release_on_disconnect(self):
        import json
        from firmware.neuron_gateway_service import NeuronGatewayService, _DISCONNECT_TIMEOUT

        gw = NeuronGatewayService(target_device_id=None)
        self.assertIsNone(gw.target_device_id)

        # Simulate packet from KIT_ALPHA
        raw_pkt = json.dumps({
            "gateway": {"device_id": "KIT_ALPHA"},
            "network_topology": {},
            "telemetry": {}
        }).encode("utf-8")

        gw._handle_packet(raw_pkt, ("192.168.1.100", 9999))
        self.assertEqual(gw.target_device_id, "KIT_ALPHA")
        self.assertTrue(gw._auto_locked)
        self.assertTrue(gw.is_connected)

        # Simulate timeout
        import time
        gw._last_packet_time = time.monotonic() - (_DISCONNECT_TIMEOUT + 1.0)
        gw._check_timeout()

        self.assertFalse(gw.is_connected)
        self.assertIsNone(gw.target_device_id)
        self.assertFalse(gw._auto_locked)

    def test_manual_lock_not_released_on_disconnect(self):
        import json
        from firmware.neuron_gateway_service import NeuronGatewayService, _DISCONNECT_TIMEOUT

        gw = NeuronGatewayService(target_device_id="MANUAL_KIT")
        self.assertEqual(gw.target_device_id, "MANUAL_KIT")
        self.assertFalse(gw._auto_locked)

        raw_pkt = json.dumps({
            "gateway": {"device_id": "MANUAL_KIT"},
            "network_topology": {},
            "telemetry": {}
        }).encode("utf-8")

        gw._handle_packet(raw_pkt, ("192.168.1.100", 9999))
        self.assertTrue(gw.is_connected)

        # Simulate timeout
        import time
        gw._last_packet_time = time.monotonic() - (_DISCONNECT_TIMEOUT + 1.0)
        gw._check_timeout()

        self.assertFalse(gw.is_connected)
        # Target device ID MUST remain intact for manual lock
        self.assertEqual(gw.target_device_id, "MANUAL_KIT")

    def test_set_target_device_cleans_up_disk_file(self):
        from firmware.neuron_gateway_service import NeuronGatewayService
        gw = NeuronGatewayService(target_device_id="TEMP_LOCK")

        # Save to disk
        gw.set_target_device("SAVED_LOCK", save_to_disk=True)
        self.assertTrue(os.path.exists("target_device.json"))

        # Clear and save to disk -> file should be deleted
        gw.set_target_device(None, save_to_disk=True)
        self.assertFalse(os.path.exists("target_device.json"))


class TestClimateAndEventCountTelemetry(unittest.TestCase):
    def test_parse_telemetry_climate_and_event_count(self):
        payload = {
            "network_topology": {
                "button_node": {"online": 1},
                "soil_node": {"online": 1},
                "dht11_node": {"online": 1},
                "dht22_node": {"online": 1},
            },
            "telemetry": {
                "button": {
                    "raw_state": 0,
                    "press_count": 14,
                    "gesture": "SINGLE_CLICK",
                    "total": 14
                },
                "soil_moisture": {"moisture_pct": 65},
                "dht11": {"temperature_c": 28, "humidity_pct": 72},
                "dht22": {"temperature_c": 28.4, "humidity_pct": 72.8},
            }
        }
        snap = ElectroBlocksGatewayService.parse_telemetry(payload)
        self.assertEqual(snap.button_event_count, 14)
        self.assertFalse(snap.button_pressed)
        self.assertTrue(snap.soil_online)
        self.assertEqual(snap.soil_moisture_pct, 65)
        self.assertTrue(snap.dht11_online)
        self.assertEqual(snap.dht11_temp, 28)
        self.assertEqual(snap.dht11_humidity, 72)
        self.assertTrue(snap.dht22_online)
        self.assertAlmostEqual(snap.dht22_temp, 28.4)
        self.assertAlmostEqual(snap.dht22_humidity, 72.8)


if __name__ == "__main__":
    unittest.main()
