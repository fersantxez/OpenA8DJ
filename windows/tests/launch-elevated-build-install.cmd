@echo off
set SCRIPT=C:\Users\fersanchez\Documents\dev\OpenA8DJ\windows\tests\build-and-install-opena8dj-elevated.ps1
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath powershell.exe -ArgumentList '-NoProfile -ExecutionPolicy Bypass -NoExit -File \"%SCRIPT%\" -Configuration Release -Platform x64' -Verb RunAs"
