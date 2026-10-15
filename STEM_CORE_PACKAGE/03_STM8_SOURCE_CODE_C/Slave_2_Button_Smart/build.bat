@echo off
set COSMIC_DIR=C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8
set INC_DIR=%COSMIC_DIR%\Hstm8
set LIB_DIR=%COSMIC_DIR%\Lib

echo [1/3] Bien dich C source files sang object...
"%COSMIC_DIR%\cxstm8.exe" +mods0 -pp -i"%INC_DIR%" -clDebug\ -coDebug\ main.c
"%COSMIC_DIR%\cxstm8.exe" +mods0 -pp -i"%INC_DIR%" -clDebug\ -coDebug\ stm8_interrupt_vector.c

echo [2/3] Lien ket object files sang .sm8...
"%COSMIC_DIR%\clnk.exe" -l"%LIB_DIR%" -m Debug\slave_2_smart.map -o Debug\slave_2_smart.sm8 Debug\slave_2_smart.lkf

echo [3/3] Xuat file nap .s19 va .hex...
"%COSMIC_DIR%\chex.exe" -o Debug\slave_2_smart.s19 Debug\slave_2_smart.sm8
"%COSMIC_DIR%\chex.exe" -fi -o Debug\slave_2_smart.hex Debug\slave_2_smart.sm8

echo [OK] Bien dich Slave 2 Smart thanh cong 100%!