@echo off

setlocal

echo ==========================================
echo   SPIDER-MAN SNAKE - NINTENDO DS
echo ==========================================

docker build -t spiderman-snake-ds .

if errorlevel 1 (
    echo.
    echo Docker image build failed.
    pause
    exit /b 1
)

docker run --rm ^
    -v "%cd%:/spiderman-snake" ^
    spiderman-snake-ds

if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build complete:
echo   spiderman-snake.nds
echo.

pause
endlocal