@echo off
title FabLab STEM - Backend Server
chcp 65001 >nul
echo ===================================================
echo           KHỞI ĐỘNG BACKEND SERVER (PORT 8000)      
echo ===================================================
echo.
echo [>] Đang khởi chạy Uvicorn Backend FastAPI...
echo [>] Swagger API Docs: http://localhost:8000/docs
echo [>] Health check:    http://localhost:8000/health
echo.
powershell -ExecutionPolicy Bypass -NoExit -File "F:\stm8_rx_tx_test\Software\stem-platform\run_backend.ps1"
