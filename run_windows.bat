@echo off
rem ===== Windows launcher (needs only Docker Desktop in Linux-containers mode) =====
rem   run_windows.bat            full calculation (~30-40 min)
rem   run_windows.bat quick      short check (~3-5 min)
rem   run_windows.bat validate ^| stage2 ^| poster ^| unit ^| shell
rem   run_windows.bat calc --config config/runs/objective1_full_paraxial.json
rem Results are copied to the "output" folder next to this file.
chcp 65001 >nul
setlocal
cd /d "%~dp0"
set IMAGE=refraction-task01:1.0
set NAME=refraction_task01_%RANDOM%

docker info >nul 2>&1
if errorlevel 1 (
  echo ОШИБКА: Docker не запущен. Запустите Docker Desktop, дождитесь "Engine running" и повторите.
  pause
  exit /b 1
)

echo ^>^>^> Сборка образа %IMAGE% (первый раз 10-40 минут: скачивается около 2 ГБ, дальше из кэша)
docker build -t %IMAGE% .
if errorlevel 1 (
  echo ОШИБКА сборки образа. См. раздел "Если что-то пошло не так" в README.md
  pause
  exit /b 1
)

if /i "%~1"=="shell" (
  docker run --rm -it %IMAGE% shell
  exit /b %ERRORLEVEL%
)

docker run --name %NAME% %IMAGE% %*
set RC=%ERRORLEVEL%
if not exist output mkdir output
docker cp %NAME%:/work/out/. output\
docker rm %NAME% >nul
echo.
if "%RC%"=="0" (echo ^>^>^> ГОТОВО. Результаты в папке: %CD%\output) else (echo ^>^>^> Завершено с ошибкой, код %RC%. Журнал выше; результаты, что успели посчитаться, в %CD%\output)
pause
exit /b %RC%
