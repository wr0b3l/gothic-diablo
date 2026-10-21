@echo off
rem gothic-diablo: bot fabuly (faza 10) - rozdzial 1 sciezka najemnika: farma Lobarta (ubranie), Greg i skarby,
rem czeladnik u Harada, najemnik bez pomocy Laresa i Martina, Lord Hagen; jak bot_fabuly.bat (build, gra sama sie zamyka)
rem wyniki: gra\dane\autotest\<data i godzina>_rozdzial1_najemnik\raport.html
rem opcje:  -Od <etykieta>   start od miejsca scenariusza (xardas, lester, cavalorn, farma, greg, brama, vatras, lares,
rem                          mistrzowie, ork, karczma, skarby, czeladnik, onar, bengar, glosy, najemnik, hagen)
rem         -Zapis <nazwa>   wczytaj zapis, np. -Zapis bot_blad1
rem scenariusz do poprawek bez nowego buildu: gra\dane\autotest\rozdzial1_najemnik.txt (kopia z src\assets\diathic\autotest_rozdzial1_najemnik.txt)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" -Autotest rozdzial1_najemnik %*
if errorlevel 1 pause
