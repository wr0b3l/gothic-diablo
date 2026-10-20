# Lokalny build (Windows)

Szybka pętla na własnym PC: commit na GitHubie → build przyrostowy u siebie (~1 min) → gra.
Każda zmiana dalej idzie na GitHuba, CI i launcher (`build-<N>`) działają jak dotąd.

## Instalacja (raz)

`dev_setup.bat` w folderze launchera (`G:\projekty\gothic-diablo`):

1. MSYS2 do `G:\msys64` (albo istniejący `C:\msys64`) + gcc, cmake, ninja, glslang, Vulkan, ccache, lld,
2. klon repo do `src\` (z submodułami),
3. pierwszy pełny build (kilka minut).

## Na co dzień

| plik | działanie |
|---|---|
| `graj_lokalnie.bat` | pobiera najnowszy commit (gałąź z najnowszym commitem, jak launcher), buduje przyrostowo, uruchamia grę |
| `buduj_w_tle.bat` | zostaw otwarte: co 15 s sprawdza GitHuba i od razu buduje nowy commit (dźwięk = gotowe) |
| `autotest.bat` | bot testowy (faza 10): jak `graj_lokalnie.bat`, potem gra z `-autotest kamera` — objazd kamery po punktach drogi wszystkich światów, sama się zamyka; raport `gra\dane\autotest\<data_godzina>\raport.html`; opcje `-Swiaty newworld,oldworld`, `-Co N`, `-Limit N` |
| `bot_fabuly.bat` | bot fabuły (faza 10): jak `graj_lokalnie.bat`, potem gra z `-autotest rozdzial1` — sama przechodzi rozdział 1 (wątek główny, straż) jak gracz: klik, rozmowy, walka; raport `gra\dane\autotest\<data_godzina>_rozdzial1\raport.html`; `-Od <etykieta>` start od miejsca scenariusza (z zapisu bota), `-Zapis bot_blad1` zapis z chwili błędu; scenariusz do poprawek: `gra\dane\autotest\rozdzial1.txt`; sprawdzenie nazw bez gry: `python3 dev/autotest_walidator.py <scenariusz> <Content>` |
| `Diathic.bat` (wcześniej `Gothic Diablo.bat`) | launcher bez zmian – buildy z CI |

Gra startuje z `gra\dane` – te same zapisy i `Gothic.ini` co w launcherze.
Zbudowane wersje: `gra\lokalne\<commit>\` (2 ostatnie). Logi: `gra\lokalne\build.log`, `setup.log`.

Opcje (`graj_lokalnie.bat ...`): `-Classic`, `-Branch master`, `-NoPull`, `-NoStart`, `-GothicPath "D:\Gothic II"`;
pozostałe parametry trafiają do `Gothic2Notr.exe`.

Build lokalny: RelWithDebInfo z `-g1` (lżejsze info debug), tylko `Gothic2Notr` (bez Spacera), ccache, linker lld.
`dev.ps1` i `.bat` w folderze launchera aktualizują się same z `src\dev`.

## Piaskownica Claude (Linux)
`dev/piaskownica.sh` — build i sprawdzenie przed pushem, wypisuje tylko błędy:
- `setup` (raz na sesję: apt, nagłówki Vulkan, submoduły, cmake), `build` (przyrostowo; pierwszy build w tle), `clang` (clang `-Wconversion -Werror` jak macOS w CI, na zmienionych `.cpp` albo podanych plikach), `all`
- pełne logi w `/tmp/diathic` (`DIATHIC_TMP`)
