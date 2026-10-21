@echo off
rem gothic-diablo: bot testowy (faza 10, czesc 1) - objazd kamery po punktach drogi wszystkich swiatow
rem buduje najnowszy commit jak graj_lokalnie.bat i uruchamia gre z -autotest kamera; gra sama sie zamyka
rem wyniki: gra\dane\autotest\<data i godzina>\raport.html
rem opcje:  -Swiaty oldworld,addonworld   -Co 5 (co 5. punkt)   -Limit 50 (najwyzej 50 punktow na swiat)
rem przyklad szybkiej proby:  autotest.bat -Swiaty newworld -Limit 30
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" -Autotest kamera %*
if errorlevel 1 pause
