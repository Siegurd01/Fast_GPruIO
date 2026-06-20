@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
if not defined PRU_CGT set "PRU_CGT=%ROOT%\.tools\ti-cgt-pru_2.3.3"
set "CLPRU=%PRU_CGT%\bin\clpru.exe"
set "BUILD=%ROOT%\build"
set "OBJECT=%BUILD%\fast_gpruio_fw.obj"
set "OUTPUT=%BUILD%\fast_gpruio_pru1_fw.out"

if /I "%~1"=="--clean" if exist "%BUILD%" rmdir /S /Q "%BUILD%"
if not exist "%CLPRU%" (
    echo ERROR: clpru.exe not found at "%CLPRU%". 1>&2
    echo Run scripts\install_pru_cgt.cmd or set PRU_CGT. 1>&2
    exit /b 1
)
if not exist "%BUILD%" mkdir "%BUILD%"
if errorlevel 1 exit /b 1

echo Using "%CLPRU%"
"%CLPRU%" --compiler_revision
if errorlevel 1 exit /b 1

"%CLPRU%" ^
    -v3 ^
    -O2 ^
    --endian=little ^
    -I"%PRU_CGT%\include" ^
    -I"%ROOT%\include" ^
    -c "%ROOT%\firmware\fast_gpruio_fw.c" ^
    --output_file="%OBJECT%"
if errorlevel 1 (
    echo ERROR: PRU firmware compilation failed. 1>&2
    exit /b 1
)

"%CLPRU%" ^
    -v3 ^
    -z "%ROOT%\firmware\AM335x_PRU.cmd" ^
    "%OBJECT%" ^
    -o "%OUTPUT%" ^
    --entry_point=main
if errorlevel 1 (
    echo ERROR: PRU firmware link failed. 1>&2
    exit /b 1
)

echo Built: "%OUTPUT%"
exit /b 0
