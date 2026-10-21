@echo off
rem gothic-diablo: bot fabuly (faza 10, czesc 4) - gra sama przechodzi rozdzial 1 (watek glowny, sciezka strazy)
rem chodzi na klik, rozmawia, walczy jak gracz; buduje najnowszy commit jak graj_lokalnie.bat; gra sama sie zamyka
rem wyniki: gra\dane\autotest\<data i godzina>_rozdzial1\raport.html
rem opcje:  -Od <etykieta>   start od miejsca scenariusza (xardas, lester, cavalorn, canthar, brama, vatras, lares, martin, andre, hagen)
rem                          z zapisu, ktory bot zrobil tam wczesniej
rem         -Zapis <nazwa>   wczytaj zapis, np. -Zapis bot_blad1 (zapis z chwili bledu, nazwa w raporcie)
rem scenariusz do poprawek bez nowego buildu: gra\dane\autotest\rozdzial1.txt (kopia z src\assets\diathic\autotest_rozdzial1.txt)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" -Autotest rozdzial1 %*
if errorlevel 1 pause
