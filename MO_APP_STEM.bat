@echo off
title FabLab STEM Platform Launcher
chcp 65001 >nul
echo ===================================================
echo       KHỞI ĐỘNG PHẦN MỀM FABLAB STEM PLATFORM      
echo ===================================================
echo.

echo [1/2] Đang kiểm tra Backend Server...
powershell -Command "try { $r = Invoke-WebRequest -Uri 'http://localhost:8000/health' -UseBasicParsing -TimeoutSec 1; if ($r.StatusCode -eq 200) { exit 0 } } catch { exit 1 }"
if %ERRORLEVEL% EQU 0 (
    echo [OK] Backend Server đã đang chạy sẵn!
) else (
    echo [>] Đang mở Backend Server trong cửa sổ riêng...
    start "FabLab STEM - Backend Server" powershell -ExecutionPolicy Bypass -NoExit -File "F:\stm8_rx_tx_test\Software\stem-platform\run_backend.ps1"
    echo [>] Chờ Backend khởi động 3 giây...
    timeout /t 3 /nobreak >nul
)

echo.
echo [2/2] Đang khởi động Giao diện App FabLab STEM...
powershell -ExecutionPolicy Bypass -File "F:\stm8_rx_tx_test\Software\stem-platform\run-frontend.ps1"
