@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "VERSION=2.3.3"
set "INSTALLER_NAME=ti_cgt_pru_%VERSION%_windows_installer.exe"
set "DOWNLOAD_URL=https://software-dl.ti.com/codegen/esd/cgt_public_sw/PRU/%VERSION%/%INSTALLER_NAME%"

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
if not defined PRU_CGT_PREFIX set "PRU_CGT_PREFIX=%ROOT%\.tools"
if not defined PRU_CGT_INSTALLER set "PRU_CGT_INSTALLER=%ROOT%\.cache\%INSTALLER_NAME%"
set "TOOLCHAIN=%PRU_CGT_PREFIX%\ti-cgt-pru_%VERSION%"
set "CLPRU=%TOOLCHAIN%\bin\clpru.exe"

if exist "%CLPRU%" (
    echo TI PRU CGT is already installed at "%TOOLCHAIN%"
    "%CLPRU%" --compiler_revision
    exit /b %errorlevel%
)

if not exist "%ROOT%\.cache" mkdir "%ROOT%\.cache"
if errorlevel 1 (
    echo ERROR: cannot create "%ROOT%\.cache". 1>&2
    exit /b 1
)

if not exist "%PRU_CGT_INSTALLER%" (
    where curl.exe >nul 2>nul
    if errorlevel 1 (
        echo ERROR: curl.exe is required. Install a current Windows 10/11 curl package. 1>&2
        exit /b 1
    )
    echo Downloading TI PRU CGT %VERSION% from:
    echo %DOWNLOAD_URL%
    curl.exe --fail --location --retry 3 --output "%PRU_CGT_INSTALLER%" "%DOWNLOAD_URL%"
    if errorlevel 1 (
        echo ERROR: download failed. 1>&2
        exit /b 1
    )
)

if not exist "%PRU_CGT_PREFIX%" mkdir "%PRU_CGT_PREFIX%"
echo Installing into "%TOOLCHAIN%"
start "TI PRU CGT installer" /wait "%PRU_CGT_INSTALLER%" --mode unattended --prefix "%PRU_CGT_PREFIX%"
if errorlevel 1 (
    echo ERROR: TI installer failed with exit code %errorlevel%. 1>&2
    exit /b 1
)
if not exist "%CLPRU%" (
    echo ERROR: installation finished but clpru.exe was not found at "%CLPRU%". 1>&2
    exit /b 1
)

echo Installed TI PRU CGT %VERSION%
"%CLPRU%" --compiler_revision
exit /b %errorlevel%
