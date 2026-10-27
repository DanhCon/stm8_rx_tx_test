with open('Software/stem-platform/frontend/plugins/ElectroBlocks/board_proxy.py', 'r', encoding='utf-8') as f:
    content = f.read()

target = """        self._ctrl._send_hardware_lcd(line, text)
        self._ctrl._send_hardware_oled(line, text)

    def lcd_clear(self) -> None:"""

replacement = """        self._ctrl._send_hardware_lcd(line, text)
        _time.sleep(0.015)
        self._ctrl._send_hardware_oled(line, text)

    def oled_print_line(self, line: int, text, color: str = "white") -> None:
        \"\"\"In văn bản lên dòng cụ thể (1-4) của màn hình OLED vật lý (Font 8x16, max 16 ký tự).\"\"\"
        self._check_stop()
        _time.sleep(0.005)
        text = str(text)
        hex_color = _OLED_COLOR_MAP.get(color, "#FFFFFF")
        try:
            line = max(1, min(4, int(line)))
        except (TypeError, ValueError):
            line = 1
        key = f"oled_line{line}"
        if self._hw_state.get(key) == text:
            return
        self._hw_state[key] = text
        if line <= 2:
            self._ctrl._lcd_lines[line - 1] = text
            self._ctrl._lcd_colors[line - 1] = hex_color
            self._emit(self._ctrl.lcdTextChanged.emit, text, line, hex_color)
        self._out(f"🖥️  OLED d{line}  ←  {text}", channel=key)
        self._ctrl._send_hardware_oled(line, text)

    def oled_clear(self) -> None:
        \"\"\"Xóa toàn bộ màn hình OLED vật lý & mô phỏng.\"\"\"
        self._check_stop()
        self._hw_state["oled_line1"] = ""
        self._hw_state["oled_line2"] = ""
        self._hw_state["oled_line3"] = ""
        self._hw_state["oled_line4"] = ""
        self._ctrl._lcd_lines = ["", ""]
        self._ctrl._lcd_colors = [_OLED_DEFAULT_COLOR, _OLED_DEFAULT_COLOR]
        self._emit(self._ctrl.lcdCleared.emit)
        self._out("🖥️  OLED xóa màn hình", channel="oled_clear")
        self._ctrl._send_hardware_oled(0, "CLEAR")

    def oled_invert(self) -> None:
        \"\"\"Đảo màu màn hình OLED vật lý (đen <-> trắng).\"\"\"
        self._check_stop()
        self._out("🖥️  OLED đảo màu", channel="oled_invert")
        self._ctrl._send_hardware_oled(0, "INVERT")

    def oled_splash(self) -> None:
        \"\"\"Hiện màn hình chào mừng FabLab STEM trên OLED.\"\"\"
        self._check_stop()
        self._out("🖥️  OLED hiện splash", channel="oled_splash")
        self._ctrl._send_hardware_oled(0, "SPLASH")

    def lcd_clear(self) -> None:"""

norm_content = content.replace('\r\n', '\n')
if target in norm_content:
    new_content = norm_content.replace(target, replacement, 1)
    with open('Software/stem-platform/frontend/plugins/ElectroBlocks/board_proxy.py', 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(new_content)
    print('SUCCESSFULLY UPDATED board_proxy.py')
else:
    print('TARGET NOT FOUND')
