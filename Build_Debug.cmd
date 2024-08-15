@echo off

set VS_VERSION=2022
set VS_EDITION=Community
set VS_PROMPT="C:\Program Files\Microsoft Visual Studio\%VS_VERSION%\%VS_EDITION%\VC\Auxiliary\Build\vcvars32.bat"

set DLL_EDITION=Debug

if exist %DLL_EDITION% (
    rmdir /s /q %DLL_EDITION%
)

if not exist %VS_PROMPT% (
    echo Visual Studio %VS_VERSION% %VS_EDITION% not found.
    echo Please install Visual Studio or check the VS_VERSION and VS_EDITION in this file.
    echo.
    pause
    exit /b 1
)

where premake5 >nul 2>&1

if %errorlevel% neq 0 (
    echo premake5 command is not found. Please install premake5.
    echo.
    pause
    exit /b 1
)

call %VS_PROMPT%

echo.
cd /d %~dp0\krkrsteam\steam

rem Check if STEAMWORKS_SDK is set
if "%STEAMWORKS_SDK%" == "" (
    echo STEAMWORKS_SDK is not set. Please set it to the path of the Steamworks SDK version 1.4x.
    echo.
    pause
    exit /b 1
)
echo STEAMWORKS_SDK = %STEAMWORKS_SDK%

rem Check if STEAMWORKS_SDK version is 1.4x or 1.45
set STEAMWORKS_VERSION_FILE=%STEAMWORKS_SDK%/Readme.txt

rem Read the version from the file
setlocal enabledelayedexpansion
set version=
for /f "tokens=*" %%A in (%STEAMWORKS_VERSION_FILE%) do (
    set line=%%A
    if "!line:~0,2!"=="v1" (
        for /f "tokens=1 delims= " %%B in ("!line!") do (
            set version=%%B
            goto :check_version
        )
    )
)

:check_version
if "%version%"=="" (
    echo.
    echo Version not found in %STEAMWORKS_VERSION_FILE%
    echo.
    pause
    exit /b 1
)

rem Remove 'v1.' from the version
set version=%version:~2%

rem Extract the minor and patch versions without using tokens
set minor=
set patch=
for /l %%i in (0,1,9) do (
    set char=!version:~%%i,1!
    if "!char!"=="" goto :done
    if "!char!" geq "0" if "!char!" leq "9" (
        set minor=!minor!!char!
    ) else (
        set patch=!patch!!char!
    )
)

:done

rem Output the version without 'v'
if "%patch%"=="" (
    echo STEAMWORKS_SDK version is 1.%minor%
) else (
    echo STEAMWORKS_SDK version is 1.%minor%%patch%
)
echo.

rem Check if the version is between 1.45 and less than 1.50
if %minor% LSS 45 (
    echo Error: STEAMWORKS_SDK version must be between 1.45 and less than 1.50.
    echo.
    pause
    exit /b 1
) else if %minor% GEQ 50 (
    echo Error: STEAMWORKS_SDK version must be between 1.45 and less than 1.50.
    echo.
    pause
    exit /b 1
)

premake5 vs%VS_VERSION%
if %errorlevel% neq 0 (
    echo Error: premake5 command failed.
    echo.
    pause
    exit /b 1
)
echo.

cd vs%VS_VERSION%

msbuild /p:Configuration=%DLL_EDITION% /p:Platform=Win32 /m /t:Rebuild
if %errorlevel% neq 0 (
    echo Error: msbuild command failed.
    echo.
    pause
    exit /b 1
)
echo.

echo Move folder %~dp0\krkrsteam\steam\bin to %~dp0%DLL_EDITION%
move "%~dp0\krkrsteam\steam\bin" "%~dp0\%DLL_EDITION%"
echo.

echo Copy steam_api.dll to %~dp0%DLL_EDITION%
copy "%STEAMWORKS_SDK%\redistributable_bin\steam_api.dll" "%~dp0\%DLL_EDITION%\steam_api.dll"
echo.

echo BUILD SUCCESSFUL!!
echo.
pause
exit /b 0