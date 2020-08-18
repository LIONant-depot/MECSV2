@echo OFF
setlocal enabledelayedexpansion
set MECS_PATH=%cd%

rem --------------------------------------------------------------------------------------------------------
rem Set the color of the terminal to blue with yellow text
rem --------------------------------------------------------------------------------------------------------
COLOR 8E
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore Cyan Welcome I am your MEGS GRAPHICS dependency updater bot, let me get to work...
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
echo.

:DOWNLOAD_DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White MEGS - DOWNLOADING GRAPHICS DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------

powershell write-host -fore Cyan Downloading cmake...
rmdir "../dependencies/cmake" /S /Q
mkdir "..\dependencies\cmake"

rem bitsadmin /TRANSFER Downloading_CMAKE https://github.com/Kitware/CMake/releases/download/v3.16.4/cmake-3.16.4-win64-x64.zip %MECS_PATH%\..\dependencies\cmake\cmake.zip
cd "..\dependencies\cmake"
powershell -Command "(New-Object Net.WebClient).DownloadFile('https://github.com/Kitware/CMake/releases/download/v3.16.4/cmake-3.16.4-win64-x64.zip', 'cmake.zip')"
if %ERRORLEVEL% GEQ 1 goto :PAUSE
powershell -Command "Invoke-WebRequest https://github.com/Kitware/CMake/releases/download/v3.16.4/cmake-3.16.4-win64-x64.zip -OutFile cmake.zip"
if %ERRORLEVEL% GEQ 1 goto :PAUSE
cd /d %MECS_PATH%


rmdir "../dependencies/cmake-3.16.4-win64-x64" /S /Q
powershell write-host -fore Cyan Decompressing cmake...
powershell.exe -nologo -noprofile -command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('../dependencies/cmake/cmake.zip', '../dependencies'); }"
if %ERRORLEVEL% GEQ 1 goto :PAUSE

powershell write-host -fore Cyan Getting DiligentEngine and its dependencies...
rmdir "../dependencies/DiligentEngine" /S /Q
git clone --recursive https://github.com/DiligentGraphics/DiligentEngine.git "../dependencies/DiligentEngine"
if %ERRORLEVEL% GEQ 1 goto :PAUSE

:FIND_VISUAL_STUDIO
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White MEGS - FINDING VISUAL STUDIO / MSBuild
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
cd /d %MECS_PATH%

for /f "usebackq tokens=*" %%i in (`.\..\dependencies\xcore\bin\vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    SET MSBUILD=%%i
)

for /f "usebackq tokens=1* delims=: " %%i in (`.\..\dependencies\xcore\bin\vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop`) do (
    if /i "%%i"=="installationPath" set VSPATH=%%j
)

IF EXIST "%MSBUILD%" ( 
    echo OK 
    GOTO :COMPILATION
    )
echo Failed to find MSBuild!!! 
GOTO :PAUSE

:COMPILATION
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White MEGS - COMPILING DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
cd /d %MECS_PATH%


powershell write-host -fore Cyan Creating project files...
cd "../dependencies/DiligentEngine"
..\cmake-3.16.4-win64-x64\bin\cmake -H. -B./.build/Win64 -G "Visual Studio 16 2019"
cd /d %MECS_PATH%


powershell write-host -fore Cyan Compiling...
echo.
powershell write-host -fore Cyan DiligentEngine Release: Compiling...
"%MSBUILD%" "%CD%\..\dependencies\DiligentEngine\.build\Win64\DiligentEngine.sln" /p:CL_MPCount=3 /m /t:Asteroids /p:configuration=Release /p:Platform="x64" /verbosity:minimal /property:MultiProcessorCompilation=true
if %ERRORLEVEL% GEQ 1 goto :PAUSE

powershell write-host -fore Cyan DiligentEngine Release: Compiling...
"%MSBUILD%" "%CD%\..\dependencies\DiligentEngine\.build\Win64\DiligentTools\Imgui\Diligent-Imgui.sln" /p:CL_MPCount=3 /m /p:configuration=Release /p:Platform="x64" /verbosity:minimal /property:MultiProcessorCompilation=true
if %ERRORLEVEL% GEQ 1 goto :PAUSE


echo.
powershell write-host -fore Cyan DiligentEngine Debug: Compiling...
"%MSBUILD%" "%CD%\..\dependencies\DiligentEngine\.build\Win64\DiligentEngine.sln" /p:CL_MPCount=3 /m /t:Asteroids /p:configuration=Debug /p:Platform="x64" /verbosity:minimal /property:MultiProcessorCompilation=true
if %ERRORLEVEL% GEQ 1 goto :PAUSE

:COPY_DLLS
powershell write-host -fore Cyan Copying DLLs...
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Debug\GraphicsEngineD3D11_64d.dll ..\dependencies\DiligentEngine\libs\Debug\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Debug\GraphicsEngineD3D12_64d.dll ..\dependencies\DiligentEngine\libs\Debug\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Debug\GraphicsEngineOpenGL_64d.dll ..\dependencies\DiligentEngine\libs\Debug\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Debug\GraphicsEngineVk_64d.dll ..\dependencies\DiligentEngine\libs\Debug\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Debug\D3Dcompiler_47.dll ..\dependencies\DiligentEngine\libs\Debug\

xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Release\GraphicsEngineD3D11_64r.dll ..\dependencies\DiligentEngine\libs\Release\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Release\GraphicsEngineD3D12_64r.dll ..\dependencies\DiligentEngine\libs\Release\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Release\GraphicsEngineOpenGL_64r.dll ..\dependencies\DiligentEngine\libs\Release\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Release\GraphicsEngineVk_64r.dll ..\dependencies\DiligentEngine\libs\Release\
xcopy /y ..\dependencies\DiligentEngine\.build\Win64\Projects\Asteroids\Release\D3Dcompiler_47.dll ..\dependencies\DiligentEngine\libs\Release\


:DONE
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White MEGS GRAPHICS - DONE!!
powershell write-host -fore White ------------------------------------------------------------------------------------------------------

:PAUSE
cd /d %MECS_PATH%
rem if no one give us any parameters then we will pause it at the end, else we are assuming that another batch file called us
if %1.==. pause
