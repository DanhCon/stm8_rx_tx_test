"""
Unit tests for ElectroBlocks BoardProxy and Controller hardware bridges.
"""
import sys
import unittest
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
FRONTEND_DIR = ROOT_DIR / "Software" / "stem-platform" / "frontend"
PLUGINS_DIR = FRONTEND_DIR / "plugins" / "ElectroBlocks"

sys.path.insert(0, str(FRONTEND_DIR))
sys.path.insert(0, str(PLUGINS_DIR))

from board_proxy import BoardProxy
from gateway_service import GatewaySnapshot


class DummySignal:
    def __init__(self):
        self.emitted = []

    def emit(self, *args):
        self.emitted.append(args)


class DummyRunner:
    def __init__(self):
        self.should_stop = False


class MockController:
    def __init__(self):
        self._runner = DummyRunner()
        self._destroyed = False
        self.sent_commands = []
        self.hw_buzzer = []
        self.hw_motor = []
        self.hw_lcd = []
        self.hw_oled = []

        self.buzzerStateChanged = DummySignal()
        self.motorStateChanged = DummySignal()
        self.lcdTextChanged = DummySignal()
        self.lcdCleared = DummySignal()
        self.ledStateChanged = DummySignal()
        self.pumpStateChanged = DummySignal()
        self.dht11DataChanged = DummySignal()
        self.soilMoistureChanged = DummySignal()
        self.airQualityChanged = DummySignal()
        self.servoAngleChanged = DummySignal()
        self.lcdScrollChanged = DummySignal()

        self._lcd_lines = ["", ""]
        self._lcd_colors = ["#00FF88", "#00FF88"]
        self._motor_speed = 0
        self._motor_direction = "stop"

    def sendCommand(self, cmd: str, args: dict):
        self.sent_commands.append((cmd, args))

    def _send_hardware_buzzer(self, command: str):
        self.hw_buzzer.append(command)

    def _send_hardware_motor(self, direction: str, speed_pct: int = 100):
        self.hw_motor.append((direction, speed_pct))

    def _send_hardware_lcd(self, line: int, text: str):
        self.hw_lcd.append((line, text))

    def _send_hardware_oled(self, line: int, text: str):
        self.hw_oled.append((line, text))


class TestBoardProxyHardwareBridges(unittest.TestCase):
    def setUp(self):
        self.ctrl = MockController()
        self.board = BoardProxy(self.ctrl, bridge=None)

    def test_buzzer_tone_bridges_hardware_on_and_off(self):
        self.board.buzzer_tone(1000, 20)
        self.assertIn("BUZ_ON", self.ctrl.hw_buzzer)
        self.assertIn("BUZ_OFF", self.ctrl.hw_buzzer)
        # BUZ_OFF must be after BUZ_ON
        idx_on = self.ctrl.hw_buzzer.index("BUZ_ON")
        idx_off = self.ctrl.hw_buzzer.index("BUZ_OFF")
        self.assertGreater(idx_off, idx_on)

    def test_motor_run_speed_zero_routes_to_motor_stop(self):
        self.board.motor_run("forward", 0)
        self.assertEqual(self.board._hw_state.get("motor"), ("stop", 0))
        self.assertEqual(self.ctrl.hw_motor[-1], ("STOP", 0))

    def test_motor_run_positive_speed(self):
        self.board.motor_run("forward", 128)
        self.assertEqual(self.ctrl.hw_motor[-1], ("forward", 50))

    def test_lcd_print_bridges_hardware_lcd_and_oled(self):
        self.board.lcd_print("Hello STEM")
        self.assertEqual(self.ctrl.hw_lcd[-1], (1, "Hello STEM"))
        self.assertEqual(self.ctrl.hw_oled[-1], (1, "Hello STEM"))


class TestControllerSnapshotSync(unittest.TestCase):
    def test_controller_unblocks_on_button_event_count_increment(self):
        from plugins.ElectroBlocks.controller import ElectroBlocksController
        from PySide6.QtCore import QCoreApplication

        app = QCoreApplication.instance() or QCoreApplication([])
        ctrl = ElectroBlocksController()

        # Initial state: button not pressed, event cleared
        self.assertFalse(ctrl._btn_event.is_set())
        self.assertEqual(ctrl._last_btn_total_count, 0)

        # First snapshot: button physically released, but press_count = 1 (fast tap occurred)
        snap1 = GatewaySnapshot(
            button_online=True,
            button_pressed=False,
            button_event_count=1
        )
        ctrl._on_hw_snapshot(snap1)

        # _btn_event MUST be set because button_event_count incremented from 0 to 1!
        self.assertTrue(ctrl._btn_event.is_set())
        self.assertEqual(ctrl._last_btn_total_count, 1)

    def test_controller_syncs_climate_sensors(self):
        from plugins.ElectroBlocks.controller import ElectroBlocksController
        from PySide6.QtCore import QCoreApplication

        app = QCoreApplication.instance() or QCoreApplication([])
        ctrl = ElectroBlocksController()

        snap = GatewaySnapshot(
            soil_online=True,
            soil_moisture_pct=78,
            dht11_online=True,
            dht11_temp=29,
            dht11_humidity=85
        )
        ctrl._on_hw_snapshot(snap)

        self.assertEqual(ctrl._soil_moisture, 78.0)
        self.assertEqual(ctrl._dht11_temp, 29.0)
        self.assertEqual(ctrl._dht11_humidity, 85.0)


if __name__ == "__main__":
    unittest.main()
