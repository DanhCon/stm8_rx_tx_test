@echo off
set COSMIC_DIR=C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8
set INC_DIR=%COSMIC_DIR%\Hstm8
set LIB_DIR=%COSMIC_DIR%\Lib

if not exist Debug mkdir Debug

echo [1/3] Bien dich C source files sang object...
"%COSMIC_DIR%\cxstm8.exe" +mods0 -pp -i"%INC_DIR%" -clDebug\ -coDebug\ main.c
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Loi bien dich main.c!
    exit /b %ERRORLEVEL%
)
"%COSMIC_DIR%\cxstm8.exe" +mods0 -pp -i"%INC_DIR%" -clDebug\ -coDebug\ stm8_interrupt_vector.c
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Loi bien dich stm8_interrupt_vector.c!
    exit /b %ERRORLEVEL%
)

echo [2/3] Lien ket object files sang .sm8...
"%COSMIC_DIR%\clnk.exe" -l"%LIB_DIR%" -m Debug\stm8_slave_khoang_cach.map -o Debug\stm8_slave_khoang_cach.sm8 Debug\stm8_slave_khoang_cach.lkf
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Loi lien ket!
    exit /b %ERRORLEVEL%
)

echo [3/3] Xuat file nap .s19 va .hex...
rem -o tao file Motorola S-Record (.s19)
"%COSMIC_DIR%\chex.exe" -o Debug\stm8_slave_khoang_cach.s19 Debug\stm8_slave_khoang_cach.sm8
rem -fi -o tao file Intel HEX (.hex)
"%COSMIC_DIR%\chex.exe" -fi -o Debug\stm8_slave_khoang_cach.hex Debug\stm8_slave_khoang_cach.sm8

echo [OK] Bien dich Slave 5 Ultrasonic Distance thanh cong 100%!
