@echo off
title Stop CompanionOS Background Controller
echo Stopping background CompanionOS Python Controller...
powershell -Command "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*companion_controller.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force; Write-Host 'Stopped PID:' $_.ProcessId }"
echo Done.
