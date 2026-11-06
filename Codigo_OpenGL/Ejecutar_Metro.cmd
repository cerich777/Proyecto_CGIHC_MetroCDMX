@echo off
pushd "%~dp0bin" || exit /b 1
ProyectoMetro.exe %*
set "metro_exit=%errorlevel%"
popd
exit /b %metro_exit%
