"""
Script to fix, build and synchronize all STM8 slave projects.
Ensures:
1. Every slave has valid .stw workspace and .stp project files for ST Visual Develop (STVD).
2. Every slave has a working build.bat.
3. Every .s19 is a true Motorola S-Record (starts with S0/S1).
4. Every .hex is a true Intel HEX (starts with :).
5. All binaries and sources are synchronized to STEM_CORE_PACKAGE.
"""

import os
import shutil
import subprocess
import sys

COSMIC_DIR = r"C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8"
CXSTM8 = os.path.join(COSMIC_DIR, "cxstm8.exe")
CLNK = os.path.join(COSMIC_DIR, "clnk.exe")
CHEX = os.path.join(COSMIC_DIR, "chex.exe")
INC_DIR = os.path.join(COSMIC_DIR, "Hstm8")
LIB_DIR = os.path.join(COSMIC_DIR, "Lib")

STW_TEMPLATE = """;\tSTMicroelectronics Workspace file

[Version]
Keyword=ST7Workspace-V0.7

[Project0]
Filename={stp_name}
Dependencies=
[Options]
ActiveProject={project_name}
ActiveConfig=Debug
AddSortedElements=1
"""

def generate_stp_content(project_name, header_files=None):
    if header_files and len(header_files) > 0:
        first_h = header_files[0]
        header_child_entry = f"Child=Root.Include Files.{first_h}"
        header_file_entry = f"""
[Root.Include Files.{first_h}]
ElemType=File
PathName={first_h}
"""
    else:
        header_child_entry = ""
        header_file_entry = ""

    return f""";\tSTMicroelectronics Project file

[Version]
Keyword=ST7Project
Number=1.3

[Project]
Name={project_name}
Toolset=STM8 Cosmic

[Config]
0=Config.0
1=Config.1

[Config.0]
ConfigName=Debug
Target={project_name}.elf
OutputFolder=Debug
Debug=$(TargetFName)

[Config.1]
ConfigName=Release
Target={project_name}.elf
OutputFolder=Release
Debug=$(TargetFName)

[Root]
ElemType=Project
PathName={project_name}
Child=Root.Source Files
Config.0=Root.Config.0
Config.1=Root.Config.1

[Root.Config.0]
Settings.0.0=Root.Config.0.Settings.0
Settings.0.1=Root.Config.0.Settings.1
Settings.0.2=Root.Config.0.Settings.2
Settings.0.3=Root.Config.0.Settings.3
Settings.0.4=Root.Config.0.Settings.4
Settings.0.5=Root.Config.0.Settings.5
Settings.0.6=Root.Config.0.Settings.6
Settings.0.7=Root.Config.0.Settings.7
Settings.0.8=Root.Config.0.Settings.8

[Root.Config.1]
Settings.1.0=Root.Config.1.Settings.0
Settings.1.1=Root.Config.1.Settings.1
Settings.1.2=Root.Config.1.Settings.2
Settings.1.3=Root.Config.1.Settings.3
Settings.1.4=Root.Config.1.Settings.4
Settings.1.5=Root.Config.1.Settings.5
Settings.1.6=Root.Config.1.Settings.6
Settings.1.7=Root.Config.1.Settings.7
Settings.1.8=Root.Config.1.Settings.8

[Root.Config.0.Settings.0]
String.6.0=2026,10,20,12,0,0
String.100.0=ST Assembler Linker
String.100.1=ST7 Cosmic
String.100.2=STM8 Cosmic
String.100.3=ST7 Metrowerks V1.1
String.100.4=Raisonance
String.101.0=STM8 Cosmic
String.102.0=C:\\Program Files (x86)\\STMicroelectronics\\st_toolset\\stvd
String.103.0=
String.104.0=Hstm8
String.105.0=Lib
String.106.0=Debug
String.107.0={project_name}.elf
Int.108=0

[Root.Config.0.Settings.1]
String.6.0=2026,10,20,12,0,0
String.100.0=$(TargetFName)
String.101.0=
String.103.0=.\\;

[Root.Config.0.Settings.2]
String.2.0=
String.6.0=2026,10,20,12,0,0
String.100.0=STM8S103F3P

[Root.Config.0.Settings.3]
String.2.0=Compiling $(InputFile)...
String.3.0=cxstm8 -customDbg -customDebCompat -customOpt-no -customLst -l +mods0 -customC-pp $(ToolsetIncOpts) -cl$(IntermPath) -co$(IntermPath) $(InputFile) 
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Config.0.Settings.4]
String.2.0=Assembling $(InputFile)...
String.3.0=castm8 -xx -l $(ToolsetIncOpts) -o$(IntermPath)$(InputName).$(ObjectExt) $(InputFile)
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Config.0.Settings.5]
String.2.0=Running Pre-Link step
String.6.0=2026,10,20,12,0,0
String.8.0=

[Root.Config.0.Settings.6]
String.2.0=Running Linker
String.3.0=clnk -customMapFile -customMapFile-m $(OutputPath)$(TargetSName).map -fakeRunConv  -fakeInteger  -fakeSemiAutoGen  $(ToolsetLibOpts)  -o $(OutputPath)$(TargetSName).sm8 -fakeOutFile$(ProjectSFile).elf -customCfgFile $(OutputPath)$(TargetSName).lkf -fakeVectFilestm8_interrupt_vector.c    -fakeStartupcrtsi0.sm8 
String.3.1=cvdwarf $(OutputPath)$(TargetSName).sm8 -fakeVectAddr0x8000
String.4.0=$(OutputPath)$(TargetFName)
String.5.0=$(OutputPath)$(TargetSName).map $(OutputPath)$(TargetSName).st7 $(OutputPath)$(TargetSName).s19
String.6.0=2026,10,20,12,0,0
String.101.0=crtsi.st7
String.102.0=+seg .const -b 0x8080 -m 0x1f80  -n .const -it 
String.102.1=+seg .text -a .const  -n .text 
String.102.2=+seg .eeprom -b 0x4000 -m 0x280  -n .eeprom 
String.102.3=+seg .bsct -b 0x0 -m 0x100  -n .bsct 
String.102.4=+seg .ubsct -a .bsct  -n .ubsct 
String.102.5=+seg .bit -a .ubsct  -n .bit -id 
String.102.6=+seg .share -a .bit  -n .share -is 
String.102.7=+seg .data -b 0x100 -m 0x100  -n .data 
String.102.8=+seg .bss -a .data  -n .bss 
String.103.0=Code,Constants[0x8080-0x9fff]=.const,.text
String.103.1=Eeprom[0x4000-0x427f]=.eeprom
String.103.2=Zero Page[0x0-0xff]=.bsct,.ubsct,.bit,.share
String.103.3=Ram[0x100-0x1ff]=.data,.bss
String.104.0=0x3ff
Int.0=0
Int.1=0

[Root.Config.0.Settings.7]
String.2.0=Running Post-Build step
String.3.0=chex -o $(OutputPath)$(TargetSName).s19 $(OutputPath)$(TargetSName).sm8
String.3.1=chex -fi -o $(OutputPath)$(TargetSName).hex $(OutputPath)$(TargetSName).sm8
String.6.0=2026,10,20,12,0,0

[Root.Config.0.Settings.8]
String.2.0=Performing Custom Build on $(InputFile)
String.6.0=2026,10,20,12,0,0

[Root.Config.1.Settings.0]
String.6.0=2026,10,20,12,0,0
String.100.0=ST Assembler Linker
String.100.1=ST7 Cosmic
String.100.2=STM8 Cosmic
String.100.3=ST7 Metrowerks V1.1
String.100.4=Raisonance
String.101.0=STM8 Cosmic
String.102.0=C:\\Program Files (x86)\\STMicroelectronics\\st_toolset\\stvd
String.103.0=
String.104.0=Hstm8
String.105.0=Lib
String.106.0=Release
String.107.0={project_name}.elf
Int.108=0

[Root.Config.1.Settings.1]
String.6.0=2026,10,20,12,0,0
String.100.0=$(TargetFName)
String.101.0=
String.103.0=.\\;

[Root.Config.1.Settings.2]
String.2.0=
String.6.0=2026,10,20,12,0,0
String.100.0=STM8S103F3P

[Root.Config.1.Settings.3]
String.2.0=Compiling $(InputFile)...
String.3.0=cxstm8 +mods0 -customC-pp $(ToolsetIncOpts) -cl$(IntermPath) -co$(IntermPath) $(InputFile) 
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Config.1.Settings.4]
String.2.0=Assembling $(InputFile)...
String.3.0=castm8 $(ToolsetIncOpts) -o$(IntermPath)$(InputName).$(ObjectExt) $(InputFile)
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Config.1.Settings.5]
String.2.0=Running Pre-Link step
String.6.0=2026,10,20,12,0,0

[Root.Config.1.Settings.6]
String.2.0=Running Linker
String.3.0=clnk -fakeRunConv  -fakeInteger  -fakeSemiAutoGen  $(ToolsetLibOpts)  -o $(OutputPath)$(TargetSName).sm8 -fakeOutFile$(ProjectSFile).elf -customCfgFile $(OutputPath)$(TargetSName).lkf -fakeVectFilestm8_interrupt_vector.c    -fakeStartupcrtsi0.sm8 
String.3.1=cvdwarf $(OutputPath)$(TargetSName).sm8 -fakeVectAddr0x8000
String.4.0=$(OutputPath)$(TargetFName)
String.5.0=$(OutputPath)$(TargetSName).map $(OutputPath)$(TargetSName).st7 $(OutputPath)$(TargetSName).s19
String.6.0=2026,10,20,12,0,0
String.101.0=crtsi.st7
String.102.0=+seg .const -b 0x8080 -m 0x1f80  -n .const -it 
String.102.1=+seg .text -a .const  -n .text 
String.102.2=+seg .eeprom -b 0x4000 -m 0x280  -n .eeprom 
String.102.3=+seg .bsct -b 0x0 -m 0x100  -n .bsct 
String.102.4=+seg .ubsct -a .bsct  -n .ubsct 
String.102.5=+seg .bit -a .ubsct  -n .bit -id 
String.102.6=+seg .share -a .bit  -n .share -is 
String.102.7=+seg .data -b 0x100 -m 0x100  -n .data 
String.102.8=+seg .bss -a .data  -n .bss 
String.103.0=Code,Constants[0x8080-0x9fff]=.const,.text
String.103.1=Eeprom[0x4000-0x427f]=.eeprom
String.103.2=Zero Page[0x0-0xff]=.bsct,.ubsct,.bit,.share
String.103.3=Ram[0x100-0x1ff]=.data,.bss
String.104.0=0x3ff
Int.0=0
Int.1=0

[Root.Config.1.Settings.7]
String.2.0=Running Post-Build step
String.3.0=chex -o $(OutputPath)$(TargetSName).s19 $(OutputPath)$(TargetSName).sm8
String.3.1=chex -fi -o $(OutputPath)$(TargetSName).hex $(OutputPath)$(TargetSName).sm8
String.6.0=2026,10,20,12,0,0

[Root.Config.1.Settings.8]
String.2.0=Performing Custom Build on $(InputFile)
String.6.0=2026,10,20,12,0,0

[Root.Source Files]
ElemType=Folder
PathName=Source Files
Child=Root.Source Files.main.c
Next=Root.Include Files
Config.0=Root.Source Files.Config.0
Config.1=Root.Source Files.Config.1

[Root.Source Files.Config.0]
Settings.0.0=Root.Source Files.Config.0.Settings.0
Settings.0.1=Root.Source Files.Config.0.Settings.1
Settings.0.2=Root.Source Files.Config.0.Settings.2
Settings.0.3=Root.Source Files.Config.0.Settings.3

[Root.Source Files.Config.1]
Settings.1.0=Root.Source Files.Config.1.Settings.0
Settings.1.1=Root.Source Files.Config.1.Settings.1
Settings.1.2=Root.Source Files.Config.1.Settings.2
Settings.1.3=Root.Source Files.Config.1.Settings.3

[Root.Source Files.Config.0.Settings.0]
String.6.0=2026,10,20,12,0,0
String.8.0=Debug
Int.0=0
Int.1=0

[Root.Source Files.Config.0.Settings.1]
String.2.0=Compiling $(InputFile)...
String.3.0=cxstm8 -customDbg -customDebCompat -customOpt-no -customLst -l +mods0 -customC-pp $(ToolsetIncOpts) -cl$(IntermPath) -co$(IntermPath) $(InputFile) 
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Source Files.Config.0.Settings.2]
String.2.0=Assembling $(InputFile)...
String.3.0=castm8 -xx -l $(ToolsetIncOpts) -o$(IntermPath)$(InputName).$(ObjectExt) $(InputFile)
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Source Files.Config.0.Settings.3]
String.2.0=Performing Custom Build on $(InputFile)
String.3.0=
String.4.0=
String.5.0=
String.6.0=2026,10,20,12,0,0

[Root.Source Files.Config.1.Settings.0]
String.6.0=2026,10,20,12,0,0
String.8.0=Release
Int.0=0
Int.1=0

[Root.Source Files.Config.1.Settings.1]
String.2.0=Compiling $(InputFile)...
String.3.0=cxstm8 +mods0 -customC-pp $(ToolsetIncOpts) -cl$(IntermPath) -co$(IntermPath) $(InputFile) 
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Source Files.Config.1.Settings.2]
String.2.0=Assembling $(InputFile)...
String.3.0=castm8 $(ToolsetIncOpts) -o$(IntermPath)$(InputName).$(ObjectExt) $(InputFile)
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Source Files.Config.1.Settings.3]
String.2.0=Performing Custom Build on $(InputFile)
String.3.0=
String.4.0=
String.5.0=
String.6.0=2026,10,20,12,0,0

[Root.Source Files.main.c]
ElemType=File
PathName=main.c
Next=Root.Source Files.stm8_interrupt_vector.c

[Root.Source Files.stm8_interrupt_vector.c]
ElemType=File
PathName=stm8_interrupt_vector.c

[Root.Include Files]
ElemType=Folder
PathName=Include Files
{header_child_entry}
Config.0=Root.Include Files.Config.0
Config.1=Root.Include Files.Config.1

[Root.Include Files.Config.0]
Settings.0.0=Root.Include Files.Config.0.Settings.0
Settings.0.1=Root.Include Files.Config.0.Settings.1
Settings.0.2=Root.Include Files.Config.0.Settings.2
Settings.0.3=Root.Include Files.Config.0.Settings.3

[Root.Include Files.Config.1]
Settings.1.0=Root.Include Files.Config.1.Settings.0
Settings.1.1=Root.Include Files.Config.1.Settings.1
Settings.1.2=Root.Include Files.Config.1.Settings.2
Settings.1.3=Root.Include Files.Config.1.Settings.3

[Root.Include Files.Config.0.Settings.0]
String.6.0=2026,10,20,12,0,0
String.8.0=Debug
Int.0=0
Int.1=0

[Root.Include Files.Config.0.Settings.1]
String.2.0=Compiling $(InputFile)...
String.3.0=cxstm8 -customDbg -customDebCompat -customOpt-no -customLst -l +mods0 -customC-pp $(ToolsetIncOpts) -cl$(IntermPath) -co$(IntermPath) $(InputFile) 
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Include Files.Config.0.Settings.2]
String.2.0=Assembling $(InputFile)...
String.3.0=castm8 -xx -l $(ToolsetIncOpts) -o$(IntermPath)$(InputName).$(ObjectExt) $(InputFile)
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Include Files.Config.0.Settings.3]
String.2.0=Performing Custom Build on $(InputFile)
String.3.0=
String.4.0=
String.5.0=
String.6.0=2026,10,20,12,0,0

[Root.Include Files.Config.1.Settings.0]
String.6.0=2026,10,20,12,0,0
String.8.0=Release
Int.0=0
Int.1=0

[Root.Include Files.Config.1.Settings.1]
String.2.0=Compiling $(InputFile)...
String.3.0=cxstm8 +mods0 -customC-pp $(ToolsetIncOpts) -cl$(IntermPath) -co$(IntermPath) $(InputFile) 
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Include Files.Config.1.Settings.2]
String.2.0=Assembling $(InputFile)...
String.3.0=castm8 $(ToolsetIncOpts) -o$(IntermPath)$(InputName).$(ObjectExt) $(InputFile)
String.4.0=$(IntermPath)$(InputName).$(ObjectExt)
String.5.0=$(IntermPath)$(InputName).ls
String.6.0=2026,10,20,12,0,0

[Root.Include Files.Config.1.Settings.3]
String.2.0=Performing Custom Build on $(InputFile)
String.3.0=
String.4.0=
String.5.0=
String.6.0=2026,10,20,12,0,0
{header_file_entry}
"""

def generate_build_bat(target_name, lkf_file, desc=""):
    return f"""@echo off
set COSMIC_DIR=C:\\Program Files (x86)\\COSMIC\\FSE_Compilers\\CXSTM8
set INC_DIR=%COSMIC_DIR%\\Hstm8
set LIB_DIR=%COSMIC_DIR%\\Lib

if not exist Debug mkdir Debug

echo [1/3] Bien dich C source files sang object...
"%COSMIC_DIR%\\cxstm8.exe" +mods0 -pp -i"%INC_DIR%" -clDebug\\ -coDebug\\ main.c
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Loi bien dich main.c!
    exit /b %ERRORLEVEL%
)
"%COSMIC_DIR%\\cxstm8.exe" +mods0 -pp -i"%INC_DIR%" -clDebug\\ -coDebug\\ stm8_interrupt_vector.c
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Loi bien dich stm8_interrupt_vector.c!
    exit /b %ERRORLEVEL%
)

echo [2/3] Lien ket object files sang .sm8...
"%COSMIC_DIR%\\clnk.exe" -l"%LIB_DIR%" -m Debug\\{target_name}.map -o Debug\\{target_name}.sm8 Debug\\{lkf_file}
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Loi lien ket!
    exit /b %ERRORLEVEL%
)

echo [3/3] Xuat file nap .s19 va .hex...
rem -o tao file Motorola S-Record (.s19)
"%COSMIC_DIR%\\chex.exe" -o Debug\\{target_name}.s19 Debug\\{target_name}.sm8
rem -fi -o tao file Intel HEX (.hex)
"%COSMIC_DIR%\\chex.exe" -fi -o Debug\\{target_name}.hex Debug\\{target_name}.sm8

echo [OK] Bien dich {desc} thanh cong 100%!
"""

SLAVE_CONFIGS = [
    {
        "folder": "Stm8_slave_1_den",
        "workspace_name": "Stm8_slave_1_den.stw",
        "project_name": "slave_1_den",
        "target_name": "slave_1_den",
        "lkf_file": "slave_1_den.lkf",
        "headers": [],
        "desc": "Slave 1 LED Basic",
        "firmware_aliases": ["Slave_1_LED_Basic"]
    },
    {
        "folder": "Stm8_slave_1_den_pwm",
        "workspace_name": "Stm8_slave_1_den.stw",
        "project_name": "slave_1_den",
        "target_name": "slave_1_den",
        "lkf_file": "slave_1_den.lkf",
        "headers": [],
        "desc": "Slave 1 LED PWM",
        "firmware_aliases": ["Slave_1_LED_PWM"]
    },
    {
        "folder": "stm8_slave_1_den_smart",
        "workspace_name": "stm8_slave_1_den_smart.stw",
        "project_name": "slave_1_smart",
        "target_name": "slave_1_smart",
        "lkf_file": "slave_1_smart.lkf",
        "headers": [],
        "desc": "Slave 1 LED Smart",
        "firmware_aliases": ["Slave_1_LED_Smart", "Slave_1_LED_Smart_Effects"]
    },
    {
        "folder": "stm8_slave_2_button",
        "workspace_name": "stm8_slave_2_button.stw",
        "project_name": "slave_2_button",
        "target_name": "slave_2_button",
        "lkf_file": "slave_2_button.lkf",
        "headers": [],
        "desc": "Slave 2 Button",
        "firmware_aliases": ["Slave_2_Button"]
    },
    {
        "folder": "stm8_slave_2_button_smart",
        "workspace_name": "stm8_slave_2_button_smart.stw",
        "project_name": "slave_2_smart",
        "target_name": "slave_2_smart",
        "lkf_file": "slave_2_smart.lkf",
        "headers": [],
        "desc": "Slave 2 Button Smart",
        "firmware_aliases": ["Slave_2_Button_Smart", "Slave_2_Button_Smart_Gestures"]
    },
    {
        "folder": "stm8_slave_3_coi",
        "workspace_name": "stm8_slave_3_coi.stw",
        "project_name": "slave_3_coi",
        "target_name": "slave_3_coi",
        "lkf_file": "slave_3_coi.lkf",
        "headers": [],
        "desc": "Slave 3 Buzzer",
        "firmware_aliases": ["Slave_3_Buzzer"]
    },
    {
        "folder": "stm8_slave_4_bien_tro",
        "workspace_name": "stm8_slave_4_bien_tro.stw",
        "project_name": "slave_4_bien_tro",
        "target_name": "slave_4_bien_tro",
        "lkf_file": "slave_4_bien_tro.lkf",
        "headers": [],
        "desc": "Slave 4 Potentiometer",
        "firmware_aliases": ["Slave_4_Potentiometer"]
    },
    {
        "folder": "stm8_slave_5_khoang_cach",
        "workspace_name": "stm8_slave_5_khoang_cach.stw",
        "project_name": "stm8_slave_khoang_cach",
        "target_name": "stm8_slave_khoang_cach",
        "lkf_file": "stm8_slave_khoang_cach.lkf",
        "headers": [],
        "desc": "Slave 5 Ultrasonic Distance",
        "firmware_aliases": ["Slave_5_Distance_HCSR04"]
    },
    {
        "folder": "stm8_slave_6_lcd",
        "workspace_name": "stm8_slave_6_lcd.stw",
        "project_name": "stm8_slave_khoang_cach",
        "target_name": "stm8_slave_khoang_cach",
        "lkf_file": "stm8_slave_khoang_cach.lkf",
        "headers": [],
        "desc": "Slave 6 LCD 1602",
        "firmware_aliases": ["Slave_6_LCD_1602"]
    },
    {
        "folder": "stm8_slave_7_dong_co_dc",
        "workspace_name": "stm8_slave_7_dong_co_dc.stw",
        "project_name": "slave_7_dc",
        "target_name": "slave_7_dc",
        "lkf_file": "slave_7_dc.lkf",
        "headers": [],
        "desc": "Slave 7 DC Motor",
        "firmware_aliases": ["Slave_7_DC_Motor", "slave_7_dc"]
    },
    {
        "folder": "stm8_slave_8_servo",
        "workspace_name": "stm8_slave_8_servo.stw",
        "project_name": "slave_8_servo",
        "target_name": "slave_8_servo",
        "lkf_file": "slave_8_servo.lkf",
        "headers": [],
        "desc": "Slave 8 Servo SG90",
        "firmware_aliases": ["Slave_8_Servo_SG90", "slave_8_servo"]
    },
    {
        "folder": "stm8_slave_9_oled",
        "workspace_name": "stm8_slave_9_oled.stw",
        "project_name": "slave_9_oled",
        "target_name": "slave_9_oled",
        "lkf_file": "slave_9_oled.lkf",
        "headers": ["ssd1306_fonts.h"],
        "desc": "Slave 9 OLED SSD1306",
        "firmware_aliases": ["Slave_9_OLED_SSD1306"]
    },
    {
        "folder": "stm8_slave_10_do_am_dat",
        "workspace_name": "stm8_slave_10_do_am_dat.stw",
        "project_name": "slave_10_soil",
        "target_name": "slave_10_soil",
        "lkf_file": "slave_10_soil.lkf",
        "headers": [],
        "desc": "Slave 10 Soil Moisture",
        "firmware_aliases": ["Slave_10_Soil_Moisture_LM393"]
    },
    {
        "folder": "stm8_slave_11_dht11",
        "workspace_name": "stm8_slave_11_dht11.stw",
        "project_name": "slave_11_dht11",
        "target_name": "slave_11_dht11",
        "lkf_file": "slave_11_dht11.lkf",
        "headers": [],
        "desc": "Slave 11 DHT11",
        "firmware_aliases": ["Slave_11_DHT11_Temp_Humidity", "slave_11_dht11"]
    },
    {
        "folder": "stm8_slave_12_dht22",
        "workspace_name": "stm8_slave_12_dht22.stw",
        "project_name": "slave_12_dht22",
        "target_name": "slave_12_dht22",
        "lkf_file": "slave_12_dht22.lkf",
        "headers": [],
        "desc": "Slave 12 DHT22",
        "firmware_aliases": ["Slave_12_DHT22_AM2302", "slave_12_dht22"]
    }
]

def main():
    print("=" * 70)
    print("  STEM STM8 SLAVE AUDIT, PROJECT CREATION, BUILD & SYNC")
    print("=" * 70)
    
    root_dir = os.path.abspath(".")
    core_fw_dir = os.path.join(root_dir, "STEM_CORE_PACKAGE", "02_STM8_FIRMWARE_S19_HEX")
    core_src_dir = os.path.join(root_dir, "STEM_CORE_PACKAGE", "03_STM8_SOURCE_CODE_C")
    
    os.makedirs(core_fw_dir, exist_ok=True)
    os.makedirs(core_src_dir, exist_ok=True)
    
    all_success = True
    
    for cfg in SLAVE_CONFIGS:
        folder = cfg["folder"]
        folder_path = os.path.join(root_dir, folder)
        debug_path = os.path.join(folder_path, "Debug")
        os.makedirs(debug_path, exist_ok=True)
        
        print(f"\n---> Processing: {folder} ({cfg['desc']})")
        
        # 1. Create or update .stw
        stw_path = os.path.join(folder_path, cfg["workspace_name"])
        stp_file = cfg["project_name"] + ".stp"
        stw_content = STW_TEMPLATE.format(stp_name=stp_file, project_name=cfg["project_name"])
        with open(stw_path, "w", encoding="utf-8") as f:
            f.write(stw_content)
        print(f"  [+] STW Workspace: {cfg['workspace_name']}")
        
        # 2. Create or update .stp
        stp_path = os.path.join(folder_path, stp_file)
        stp_content = generate_stp_content(cfg["project_name"], cfg["headers"])
        with open(stp_path, "w", encoding="utf-8") as f:
            f.write(stp_content)
        print(f"  [+] STP Project: {stp_file}")
        
        # 3. Create or update build.bat
        bat_path = os.path.join(folder_path, "build.bat")
        bat_content = generate_build_bat(cfg["target_name"], cfg["lkf_file"], cfg["desc"])
        with open(bat_path, "w", encoding="utf-8") as f:
            f.write(bat_content)
        print(f"  [+] Build Script: build.bat")
        
        # 4. Compile with Cosmic C
        cmd_c1 = [CXSTM8, "+mods0", "-pp", f"-i{INC_DIR}", "-clDebug\\", "-coDebug\\", "main.c"]
        cmd_c2 = [CXSTM8, "+mods0", "-pp", f"-i{INC_DIR}", "-clDebug\\", "-coDebug\\", "stm8_interrupt_vector.c"]
        cmd_link = [CLNK, f"-l{LIB_DIR}", "-m", f"Debug\\{cfg['target_name']}.map", "-o", f"Debug\\{cfg['target_name']}.sm8", f"Debug\\{cfg['lkf_file']}"]
        
        s19_out = os.path.join(debug_path, f"{cfg['target_name']}.s19")
        hex_out = os.path.join(debug_path, f"{cfg['target_name']}.hex")
        
        cmd_s19 = [CHEX, "-o", f"Debug\\{cfg['target_name']}.s19", f"Debug\\{cfg['target_name']}.sm8"]
        cmd_hex = [CHEX, "-fi", "-o", f"Debug\\{cfg['target_name']}.hex", f"Debug\\{cfg['target_name']}.sm8"]
        
        try:
            r1 = subprocess.run(cmd_c1, cwd=folder_path, capture_output=True, text=True, check=True)
            r2 = subprocess.run(cmd_c2, cwd=folder_path, capture_output=True, text=True, check=True)
            r3 = subprocess.run(cmd_link, cwd=folder_path, capture_output=True, text=True, check=True)
            r4 = subprocess.run(cmd_s19, cwd=folder_path, capture_output=True, text=True, check=True)
            r5 = subprocess.run(cmd_hex, cwd=folder_path, capture_output=True, text=True, check=True)
            print("  [OK] Compiled & Linked successfully!")
        except subprocess.CalledProcessError as e:
            print(f"  [ERROR] Build failed! {e.cmd}")
            print(f"  Output: {e.stdout}\n  Error: {e.stderr}")
            all_success = False
            continue
            
        # 5. Verify S19 and HEX format
        with open(s19_out, "r", encoding="latin-1") as f:
            s19_line1 = f.readline().strip()
        with open(hex_out, "r", encoding="latin-1") as f:
            hex_line1 = f.readline().strip()
            
        if not s19_line1.startswith("S0") and not s19_line1.startswith("S1"):
            print(f"  [FATAL] S19 format invalid: {s19_line1[:20]}")
            all_success = False
        else:
            print(f"  [VERIFIED] .s19 is valid Motorola S-Record ({s19_line1[:6]}...)")
            
        if not hex_line1.startswith(":"):
            print(f"  [FATAL] HEX format invalid: {hex_line1[:20]}")
            all_success = False
        else:
            print(f"  [VERIFIED] .hex is valid Intel HEX ({hex_line1[:8]}...)")
            
        # 6. Copy to STEM_CORE_PACKAGE/02_STM8_FIRMWARE_S19_HEX
        for alias in cfg["firmware_aliases"]:
            dst_s19 = os.path.join(core_fw_dir, f"{alias}.s19")
            dst_hex = os.path.join(core_fw_dir, f"{alias}.hex")
            shutil.copyfile(s19_out, dst_s19)
            shutil.copyfile(hex_out, dst_hex)
            print(f"  [SYNC FW] -> {alias}.s19 & .hex")
            
    print("\n" + "=" * 70)
    print("  SYNCHRONIZING SOURCE PACKAGES TO STEM_CORE_PACKAGE/03_STM8_SOURCE_CODE_C")
    print("=" * 70)
    
    # List of mappings for source code packages:
    src_mappings = [
        ("Stm8_slave_1_den", "Slave_1_LED_Basic"),
        ("Stm8_slave_1_den_pwm", "Slave_1_LED_PWM"),
        ("stm8_slave_1_den_smart", "Slave_1_LED_Smart"),
        ("stm8_slave_1_den_smart", "stm8_slave_1_den_smart"),
        ("stm8_slave_2_button", "Slave_2_Button"),
        ("stm8_slave_2_button_smart", "Slave_2_Button_Smart"),
        ("stm8_slave_2_button_smart", "stm8_slave_2_button_smart"),
        ("stm8_slave_3_coi", "Slave_3_Buzzer"),
        ("stm8_slave_4_bien_tro", "Slave_4_Potentiometer"),
        ("stm8_slave_5_khoang_cach", "Slave_5_Distance_HCSR04"),
        ("stm8_slave_6_lcd", "Slave_6_LCD_1602"),
        ("stm8_slave_7_dong_co_dc", "Slave_7_DC_Motor"),
        ("stm8_slave_7_dong_co_dc", "stm8_slave_7_dong_co_dc"),
        ("stm8_slave_8_servo", "Slave_8_Servo_SG90"),
        ("stm8_slave_8_servo", "stm8_slave_8_servo"),
        ("stm8_slave_9_oled", "Slave_9_OLED_SSD1306"),
        ("stm8_slave_9_oled", "stm8_slave_9_oled"),
        ("stm8_slave_10_do_am_dat", "Slave_10_Soil_Moisture"),
        ("stm8_slave_10_do_am_dat", "stm8_slave_10_do_am_dat"),
        ("stm8_slave_11_dht11", "Slave_11_DHT11"),
        ("stm8_slave_11_dht11", "stm8_slave_11_dht11"),
        ("stm8_slave_12_dht22", "Slave_12_DHT22"),
        ("stm8_slave_12_dht22", "stm8_slave_12_dht22")
    ]
    
    for src_folder, dst_folder in src_mappings:
        s_path = os.path.join(root_dir, src_folder)
        d_path = os.path.join(core_src_dir, dst_folder)
        os.makedirs(d_path, exist_ok=True)
        
        # Copy root files (.c, .h, .stw, .stp, .bat)
        for f in os.listdir(s_path):
            sp = os.path.join(s_path, f)
            if os.path.isfile(sp):
                shutil.copyfile(sp, os.path.join(d_path, f))
                
        # Copy Debug folder
        s_debug = os.path.join(s_path, "Debug")
        d_debug = os.path.join(d_path, "Debug")
        if os.path.exists(s_debug):
            os.makedirs(d_debug, exist_ok=True)
            for f in os.listdir(s_debug):
                sp = os.path.join(s_debug, f)
                if os.path.isfile(sp):
                    shutil.copyfile(sp, os.path.join(d_debug, f))
        print(f"  [SYNC SRC] {src_folder} -> {dst_folder}")
        
    print("\n" + "=" * 70)
    if all_success:
        print("  ALL 15 STM8 SLAVE CONFIGURATIONS BUILT, VERIFIED & SYNCHRONIZED 100%!")
    else:
        print("  SOME TASKS ENCOUNTERED ERRORS, PLEASE CHECK LOG ABOVE!")
    print("=" * 70)

if __name__ == "__main__":
    main()
