"""
Comprehensive Edge-Case & Stress Test Suite for STEM Platform Pipeline:
- Category 1: Malformed Packets & Extreme JSON Robustness
- Category 2: Multi-Kit Classroom Isolation & Anti-Cross-talk
- Category 3: Rapid Button Gestures & Monotonic Event Counters
- Category 4: Actuator Boundary Clamping & Invalid Input Resilience
- Category 5: Simulation Fallback & Execution Cancellation (StopRequested)
- Category 6: Daisy-Chain Ring UART Checksum & Frame Integrity
"""
import sys
import os
import json
import time
import unittest
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
FRONTEND_DIR = ROOT_DIR / "Software" / "stem-platform" / "frontend"
PLUGINS_DIR = FRONTEND_DIR / "plugins" / "ElectroBlocks"
FIRMWARE_DIR = FRONTEND_DIR / "firmware"

sys.path.insert(0, str(FRONTEND_DIR))
sys.path.insert(0, str(PLUGINS_DIR))
sys.path.insert(0, str(FIRMWARE_DIR))

from gateway_service import ElectroBlocksGatewayService, GatewaySnapshot
from neuron_gateway_service import NeuronGatewayService, _DISCONNECT_TIMEOUT


# ─── CATEGORY 1: MALFORMED PACKETS & EXTREME JSON ROBUSTNESS ──────────────────

class TestMalformedPacketsAndRobustness(unittest.TestCase):
    def setUp(self):
        self.gw = NeuronGatewayService(target_device_id="KIT_TEST")
        self.telemetry_received = []
        self.gw.on_telemetry = lambda d: self.telemetry_received.append(d)

    def test_empty_udp_packet(self):
        """0-byte packet should be silently discarded without exception."""
        self.gw._handle_packet(b"", ("192.168.1.50", 9999))
        self.assertEqual(len(self.telemetry_received), 0)

    def test_truncated_json(self):
        """Incomplete JSON packet should be dropped cleanly."""
        truncated = b'{"timestamp": 123456, "gateway": {"device_id": "KIT_TEST"'
        self.gw._handle_packet(truncated, ("192.168.1.50", 9999))
        self.assertEqual(len(self.telemetry_received), 0)

    def test_binary_garbage_packet(self):
        """Binary/corrupted packet should not crash the receiver."""
        garbage = bytes([0xAA, 0x55, 0xFF, 0x00, 0xDE, 0xAD, 0xBE, 0xEF])
        self.gw._handle_packet(garbage, ("192.168.1.50", 9999))
        self.assertEqual(len(self.telemetry_received), 0)

    def test_huge_packet(self):
        """Large packet buffer should parse or drop safely without overflow."""
        huge_text = "A" * 10000
        payload = json.dumps({"gateway": {"device_id": "KIT_TEST"}, "notes": huge_text}).encode("utf-8")
        self.gw._handle_packet(payload, ("192.168.1.50", 9999))
        self.assertEqual(len(self.telemetry_received), 1)

    def test_missing_top_level_keys(self):
        """Payload completely missing standard sections should parse gracefully."""
        snap = ElectroBlocksGatewayService.parse_telemetry({})
        self.assertFalse(snap.led_online)
        self.assertFalse(snap.button_online)
        self.assertFalse(snap.pot_online)
        self.assertFalse(snap.distance_online)
        self.assertFalse(snap.soil_online)
        self.assertFalse(snap.dht11_online)
        self.assertFalse(snap.dht22_online)

    def test_stringified_and_out_of_bounds_numbers(self):
        """String numbers in telemetry (e.g. from custom JSON encoders) are parsed safely."""
        payload = {
            "network_topology": {
                "button_node": {"online": "1"},
                "pot_node": {"online": 1},
                "distance_node": {"online": 1},
                "soil_node": {"online": 1},
                "dht11_node": {"online": 1},
                "dht22_node": {"online": 1},
            },
            "telemetry": {
                "button": {"raw_state": "1", "press_count": "42"},
                "potentiometer": {"raw_adc": "255"},
                "distance": {"distance_cm": "150"},
                "soil_moisture": {"moisture_pct": "80"},
                "dht11": {"temperature_c": "25", "humidity_pct": "60"},
                "dht22": {"temperature_c": "25.5", "humidity_pct": "60.2"},
            }
        }
        snap = ElectroBlocksGatewayService.parse_telemetry(payload)
        self.assertTrue(snap.button_online)
        self.assertTrue(snap.button_pressed)
        self.assertEqual(snap.button_event_count, 42)
        self.assertEqual(snap.pot_value, 100)
        self.assertEqual(snap.distance_cm, 150.0)
        self.assertEqual(snap.soil_moisture_pct, 80)
        self.assertEqual(snap.dht11_temp, 25)
        self.assertEqual(snap.dht11_humidity, 60)
        self.assertAlmostEqual(snap.dht22_temp, 25.5)
        self.assertAlmostEqual(snap.dht22_humidity, 60.2)


# ─── CATEGORY 2: MULTI-KIT CLASSROOM ISOLATION & ANTI-CROSS-TALK ──────────────

class TestMultiKitClassroomIsolation(unittest.TestCase):
    def test_strict_isolation_under_multi_kit_flood(self):
        """When locked to NHOM_01, packets from other kits are 100% ignored."""
        gw = NeuronGatewayService(target_device_id="NHOM_01")
        accepted = []
        gw.on_telemetry = lambda d: accepted.append(d)

        # 30 packets from other kits
        for i in range(10):
            gw._handle_packet(
                json.dumps({"gateway": {"device_id": "NHOM_02"}, "val": i}).encode("utf-8"),
                ("192.168.1.102", 9999)
            )
            gw._handle_packet(
                json.dumps({"gateway": {"device_id": "NHOM_03"}, "val": i}).encode("utf-8"),
                ("192.168.1.103", 9999)
            )
            gw._handle_packet(
                json.dumps({"gateway": {"device_id": "KIT_RANDOM"}, "val": i}).encode("utf-8"),
                ("192.168.1.104", 9999)
            )

        self.assertEqual(len(accepted), 0)
        self.assertFalse(gw.is_connected)
        self.assertIsNone(gw.esp32_ip)

        # 1 valid packet from NHOM_01
        gw._handle_packet(
            json.dumps({"gateway": {"device_id": "NHOM_01"}, "val": 99}).encode("utf-8"),
            ("192.168.1.101", 9999)
        )
        self.assertEqual(len(accepted), 1)
        self.assertTrue(gw.is_connected)
        self.assertEqual(gw.esp32_ip, "192.168.1.101")
        self.assertEqual(gw.connected_device_id, "NHOM_01")

    def test_command_targeting_and_broadcast_integrity(self):
        """Commands sent by client carry target device prefix without duplication."""
        gw = NeuronGatewayService(target_device_id="ROBOT_LAB_01")
        gw._connected = True
        gw._esp32_ip = "192.168.1.200"

        sent = []
        class MockSock:
            def sendto(self, data, addr):
                sent.append((data.decode("utf-8"), addr))

        gw._cmd_sock = MockSock()

        # Regular command
        gw.send_motor("FWD", 80)
        self.assertEqual(sent[-1][0], "@ROBOT_LAB_01:MOTOR_FWD:80")
        self.assertEqual(sent[-1][1], ("192.168.1.200", 8888))

        # Stop command
        gw.send_motor("STOP", 0)
        self.assertEqual(sent[-1][0], "@ROBOT_LAB_01:MOTOR_STOP")

        # Global broadcast command (starts with @ALL:)
        gw.send_command("@ALL:BUZ_BEEP")
        self.assertEqual(sent[-1][0], "@ALL:BUZ_BEEP")
        self.assertFalse(sent[-1][0].startswith("@ROBOT_LAB_01:@ALL"))


# ─── CATEGORY 3: RAPID BUTTON GESTURES & MONOTONIC EVENT COUNTERS ────────────

class TestRapidButtonGesturesAndCounters(unittest.TestCase):
    def test_rapid_button_tap_counter_progression(self):
        """Verify rapid tap counter increments correctly and is parsed monotonically."""
        last_count = 0
        for tap in range(1, 10):
            payload = {
                "network_topology": {"button_node": {"online": 1}},
                "telemetry": {
                    "button": {
                        "raw_state": 0,
                        "press_count": tap,
                        "gesture": "SINGLE_CLICK",
                        "total": tap
                    }
                }
            }
            snap = ElectroBlocksGatewayService.parse_telemetry(payload)
            self.assertTrue(snap.button_event_count > last_count)
            last_count = snap.button_event_count

        self.assertEqual(last_count, 9)

    def test_button_gesture_distinction(self):
        """Verify each distinct gesture state is correctly interpreted."""
        gestures = [
            (0x50, "IDLE", 0, False),
            (0x56, "RAW_PRESSED", 1, True),
            (0x51, "SINGLE_CLICK", 0, False),
            (0x52, "DOUBLE_CLICK", 0, False),
            (0x53, "LONG_PRESS", 1, True),
        ]
        for code, name, raw, exp_pressed in gestures:
            payload = {
                "network_topology": {"button_node": {"online": 1}},
                "telemetry": {
                    "button": {
                        "raw_state": raw,
                        "gesture": name,
                        "raw_code": f"0x{code:02X}",
                    }
                }
            }
            snap = ElectroBlocksGatewayService.parse_telemetry(payload)
            self.assertEqual(snap.button_pressed, exp_pressed, f"Failed for gesture {name}")


# ─── CATEGORY 4: ACTUATOR BOUNDARY CLAMPING & RESILIENCE ─────────────────────

class TestActuatorBoundaryClamping(unittest.TestCase):
    def test_sdk_servo_angle_clamping(self):
        gw = NeuronGatewayService(target_device_id="KIT_TEST")
        gw._connected = True
        gw._esp32_ip = "192.168.1.10"
        sent = []
        class MockSock:
            def sendto(self, d, a): sent.append(d.decode("utf-8"))
        gw._cmd_sock = MockSock()

        # Underflow
        gw.send_servo(-50)
        self.assertEqual(sent[-1], "@KIT_TEST:SERVO_ANGLE:0")

        # Zero
        gw.send_servo(0)
        self.assertEqual(sent[-1], "@KIT_TEST:SERVO_ANGLE:0")

        # Standard
        gw.send_servo(90)
        self.assertEqual(sent[-1], "@KIT_TEST:SERVO_ANGLE:90")

        # Boundary
        gw.send_servo(180)
        self.assertEqual(sent[-1], "@KIT_TEST:SERVO_ANGLE:180")

        # Overflow
        gw.send_servo(360)
        self.assertEqual(sent[-1], "@KIT_TEST:SERVO_ANGLE:180")

    def test_sdk_motor_speed_clamping_and_stop(self):
        gw = NeuronGatewayService(target_device_id="KIT_TEST")
        gw._connected = True
        gw._esp32_ip = "192.168.1.10"
        sent = []
        class MockSock:
            def sendto(self, d, a): sent.append(d.decode("utf-8"))
        gw._cmd_sock = MockSock()

        # Speed <= 0 must send STOP
        gw.send_motor("FWD", 0)
        self.assertEqual(sent[-1], "@KIT_TEST:MOTOR_STOP")

        gw.send_motor("FWD", -20)
        self.assertEqual(sent[-1], "@KIT_TEST:MOTOR_STOP")

        # Normal forward
        gw.send_motor("FWD", 50)
        self.assertEqual(sent[-1], "@KIT_TEST:MOTOR_FWD:50")

        # Overflow forward
        gw.send_motor("FWD", 200)
        self.assertEqual(sent[-1], "@KIT_TEST:MOTOR_FWD:100")

        # Normal reverse
        gw.send_motor("REV", 75)
        self.assertEqual(sent[-1], "@KIT_TEST:MOTOR_REV:75")

        # Brake
        gw.send_motor("BRAKE", 100)
        self.assertEqual(sent[-1], "@KIT_TEST:MOTOR_BRAKE")


# ─── CATEGORY 5: SIMULATION FALLBACK & EXECUTION CANCELLATION ────────────────

class DummySignal:
    def __init__(self): self.calls = []
    def emit(self, *args): self.calls.append(args)

class DummyRunner:
    def __init__(self): self.should_stop = False

class DummyController:
    def __init__(self):
        self._runner = DummyRunner()
        self._destroyed = False
        self.buzzer_hw = []
        self.motor_hw = []
        self.servo_hw = []
        self.lcd_hw = []
        self.oled_hw = []

        self.buzzerStateChanged = DummySignal()
        self.motorStateChanged = DummySignal()
        self.lcdTextChanged = DummySignal()
        self.lcdCleared = DummySignal()
        self.servoAngleChanged = DummySignal()
        self.pumpStateChanged = DummySignal()
        self.dht11DataChanged = DummySignal()
        self.soilMoistureChanged = DummySignal()
        self.airQualityChanged = DummySignal()
        self.lcdScrollChanged = DummySignal()
        self.ledStateChanged = DummySignal()

        self._lcd_lines = ["", ""]
        self._lcd_colors = ["#00FF88", "#00FF88"]
        self._motor_speed = 0
        self._motor_direction = "stop"

    def sendCommand(self, cmd, args): pass
    def _send_hardware_buzzer(self, c): self.buzzer_hw.append(c)
    def _send_hardware_motor(self, d, s): self.motor_hw.append((d, s))
    def _send_hardware_servo(self, a): self.servo_hw.append(a)
    def _send_hardware_lcd(self, l, t): self.lcd_hw.append((l, t))
    def _send_hardware_oled(self, l, t): self.oled_hw.append((l, t))

class TestBoardProxyDefensiveAndCancellation(unittest.TestCase):
    def setUp(self):
        try:
            from board_proxy import BoardProxy
            self.BoardProxy = BoardProxy
        except ImportError:
            self.skipTest("BoardProxy requires PySide6 environment")

        self.ctrl = DummyController()
        self.board = self.BoardProxy(self.ctrl, bridge=None)

    def test_servo_invalid_types_resilience(self):
        """Passing None or invalid string to servo does not crash."""
        self.board.servo_set_angle(None)
        self.assertEqual(self.ctrl.servo_hw[-1], 90)

        self.board.servo_set_angle("invalid")
        self.assertEqual(self.ctrl.servo_hw[-1], 90)

        self.board.servo_set_angle("120")
        self.assertEqual(self.ctrl.servo_hw[-1], 120)

    def test_motor_invalid_types_resilience(self):
        """Passing None or invalid types to motor does not crash."""
        self.board.motor_run("forward", None)
        self.assertEqual(self.ctrl.motor_hw[-1], ("STOP", 0))

        self.board.motor_run("backward", "200")
        self.assertEqual(self.ctrl.motor_hw[-1], ("backward", int(round(200 * 100 / 255))))

    def test_buzzer_cancellation_shuts_down_hardware(self):
        """When code runner stops during buzzer_tone, hardware buzzer is shut down."""
        from core.code_runner import StopRequested
        self.ctrl._runner.should_stop = True

        with self.assertRaises(StopRequested):
            self.board.buzzer_tone(1000, 500)

        self.assertIn("BUZ_OFF", self.ctrl.buzzer_hw)


# ─── CATEGORY 6: DAISY-CHAIN RING CHECKSUM & PROTOCOL INTEGRITY ───────────────

class TestDaisyChainRingChecksum(unittest.TestCase):
    @staticmethod
    def calc_ring_checksum(slave_id: int, cmd_data: int) -> int:
        return (slave_id + cmd_data) & 0xFF

    def test_checksum_algorithm_for_all_slaves(self):
        """Verify ring UART checksum formula matches across all 12 Slaves."""
        # Slave 1 LED
        self.assertEqual(self.calc_ring_checksum(0x01, 0x20), 0x21) # ON
        self.assertEqual(self.calc_ring_checksum(0x01, 0x30), 0x31) # OFF
        self.assertEqual(self.calc_ring_checksum(0x01, 0x71), 0x72) # BREATHE

        # Slave 2 Button
        self.assertEqual(self.calc_ring_checksum(0x02, 0x50), 0x52) # Query
        self.assertEqual(self.calc_ring_checksum(0x02, 0x51), 0x53) # Single ACK
        self.assertEqual(self.calc_ring_checksum(0x02, 0x56), 0x58) # Raw pressed ACK

        # Slave 3 Buzzer
        self.assertEqual(self.calc_ring_checksum(0x03, 0x40), 0x43) # ON
        self.assertEqual(self.calc_ring_checksum(0x03, 0x41), 0x44) # OFF

        # Slave 4 Potentiometer
        self.assertEqual(self.calc_ring_checksum(0x04, 0x60), 0x64) # Query

        # Slave 5 HC-SR04
        self.assertEqual(self.calc_ring_checksum(0x05, 0x70), 0x75) # Query

        # Slave 6 LCD
        self.assertEqual(self.calc_ring_checksum(0x06, 0x01), 0x07) # Clear
        self.assertEqual(self.calc_ring_checksum(0x06, 0x82), 0x88) # Compat Clear

        # Slave 7 DC Motor
        self.assertEqual(self.calc_ring_checksum(0x07, 0x00), 0x07) # STOP
        self.assertEqual(self.calc_ring_checksum(0x07, 0x64), 0x6B) # 100%

        # Slave 8 Servo
        self.assertEqual(self.calc_ring_checksum(0x08, 0x5A), 0x62) # 90 deg

        # Slave 9 OLED
        self.assertEqual(self.calc_ring_checksum(0x09, 0x82), 0x8B) # Clear

        # Slave 10 Soil Moisture
        self.assertEqual(self.calc_ring_checksum(0x0A, 0x60), 0x6A) # Query

        # Slave 11 DHT11
        self.assertEqual(self.calc_ring_checksum(0x0B, 0x71), 0x7C) # Temp

        # Slave 12 DHT22
        self.assertEqual(self.calc_ring_checksum(0x0C, 0x71), 0x7D) # Temp


if __name__ == "__main__":
    unittest.main()
