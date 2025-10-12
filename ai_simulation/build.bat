@echo off
REM Build script for AI Simulation Game (Windows)

echo ================================================
echo   Building AI Simulation Game
echo ================================================
echo.

REM Navigate to build directory
cd /d "%~dp0build"
if not exist "build" mkdir build
cd build

REM Configure CMake
echo Configuring CMake...
cmake ..
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo.
echo Building project...
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ================================================
echo   Build Complete!
echo ================================================
echo.
echo Executables created:
echo   - AISimulationGame.exe (Main simulation)
echo   - test_main.exe (Unit tests)
echo.
echo Run the simulation with:
echo   .\AISimulationGame.exe
echo.
echo Run tests with:
echo   .\test_main.exe
echo.
pause
