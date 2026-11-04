# Diathic — decyzje użytkownika i historia faz

Archiwum z dokumentów projektu claude.ai (02-plan, 03-faza6, 04-faza9), przeniesione 2026-10-16. Działanie funkcji opisuje GOTHIC-DIABLO.md, wiedzę o silniku jego sekcja „Ustalenia techniczne”. Aktualny stan i następne kroki: dokument projektu claude/02-plan.md.

## Ogólne
- 2026-10-11: baza = fork OpenGothic (C++, MIT) + oryginalne pliki Gothic II Noc Kruka ze Steam (wersja polska); standalone z assetami bez zgody THQ Nordic — nie (analiza: analiza-wykonalnosci.md)
- 2026-10-15: oficjalna nazwa **Diathic** (gothic-diablo = nazwa kodowa)
- Zasada raportowania: na koniec każdego buildu napisać, co następne
- 2026-10-13: lokalny build na PC (MSYS2 w G:\msys64, klon w G:\projekty\gothic-diablo\src, graj_lokalnie.bat, buduj_w_tle.bat; opis w dev/README.md); push na GitHuba zostaje, test lokalnym buildem bez czekania na CI
- 2026-10-15: CI buduje tylko Windows (fbac690); Linux, deb, Android, macOS wracają ze zmienną repo ALL_PLATFORMS = true; commity tylko z narzędziami / CI z `[skip ci]`
- W PowerShell znaki „ ” działają jak cudzysłów — w napisach w cudzysłowie podwójnym ich nie używać

## Wersja dla testerów (2026-10-14)
- Steam Workshop odpada (Workshop Gothic II kopiuje mody do folderu gry i uruchamia oryginalny silnik, a my mamy osobny program); testerzy dostają paczkę launchera; tylko wersje „testowe” — użytkownik sam wybiera build; zgłaszanie błędów z gry „nie teraz” (opis + log.txt ręcznie)
- Wymagania testera: Windows 10/11 64-bit, karta z Vulkan, własny Gothic II Gold (z Nocą Kruka; zalecana polska wersja); instalacja Gothic II nie jest zmieniana
- Później do rozważenia: samoaktualizacja launchera testerów, zgłaszanie błędów z gry

## Fazy 0–3 (2026-10-11 – 13)
- Faza 0: stałej kamery ARPG nie da się zrobić skryptami (CAMERA.DAT)
- Faza 1 (PR #1): kamera 55°, 11 m, bez kolizji, Q/E obrót, ruch względem ekranu, ukrywanie sufitów, mgła wojny, kursor; 2026-10-15: zoom kółkiem tylko ±10% (10–12 m)
- Faza 2 (PR #2): sterowanie myszą jak w Diablo, wskazywanie według zasad klawisza Z, omijanie przeszkód
- Faza 3 (PR #3): czarne przekroje, linia wzroku we wnętrzach, prześwit, jaśniejsze postacie, minimapa
- Pętla przy kliknięciu (krążenie wokół stołów; poprawka 3637 nie pomogła) → 2026-10-16 siatka ArpgNav; skok / przejście przez niską przeszkodę użytkownik dopuścił, na razie tylko obejście
- Znane problemy: wiszące dekoracje ucięte na wysokości cięcia, jaskinie wielopoziomowe, wydajność promieni we wnętrzach

## Faza 4 — walka (PR #4, 2026-10-14)
- 1 = mikstura zdrowia, 2 = mana, 3–0 = czary; Spacja = fikołek z klatkami nieczułości (później umiejętność do nauczenia); LPM = atak, PPM = atak ładowany (2 s, okno 0,2 s, ×3, 180°, 5 s), SPM = blok / odepchnięcie łukiem; cios trafia wszystkich w łuku; mikstury od razu jak w Diablo; łuk: celność jak w Gothicu, obrażenia ×0,5, Ctrl = celowanie za kursorem; pasek umiejętności z ikonami z gry; podświetlenie wroga; błysk przy trafieniu niepotrzebny
- 2026-10-16 („walka dziwna”): szybkość ataku bazowo jak w Gothicu (było +20%), Płynne ruchy +7% za punkt (do +35%); walka dalej do poprawy większą liczbą animacji
- 2026-10-16 parowanie: blok zaczęty najwyżej 150 ms przed ciosem wroga na wprost → bez obrażeń, wróg dostaje Zachwianie; dźwięk: dzwoneczek z plików gry albo głuche uderzenie (teraz szczęk metalu z iskrami + dzwonek, jeśli jest)
- Zachwianie = pierwszy z wielu statusów negatywnych (potwory, NPC, bohater): przerwanie ciosu, brak ataku 250 ms, obrażenia ×4, niebieskie gwiazdki; 250 ms to mniej niż czas ciosu bohatera — sprawdzić w grze; Podpalenie i Krwawienie to też statusy

## Faza 5 — interfejs i dialogi (PR #5, 2026-10-14)
- Ekwipunek ze slotami jak w RPG; panel postaci; drzewka jak w Last Epoch (każda umiejętność ma własne); na start 3 drzewka: zwykły atak, atak ładowany, fikołek
- Interfejs z jak największą liczbą tekstur i modeli z gry; okno postaci i ekwipunek pokazują Bezimiennego animowanego w spoczynku z założonym ekwipunkiem
- Dialogi kinowe jak w Mass Effect; ryzyka: ciasne wnętrza, postacie przy ścianie, skrypty z odejściem postaci w trakcie rozmowy, sceny wieloosobowe (Lord Hagen), dużo napisów nad głowami w walce
- Do sprawdzenia w dużej rundzie testowej: trzecia osoba w rozmowie, kamera księgi, kamera w skale, walka po poprawkach

## Faza 6 — rozwój postaci (PR #6, 2026-10-14)
- Źródła punktów: poziom, nauczyciele (za punkty nauki), rzadkie znaleziska; bez punktów za używanie umiejętności; pula mieszana (z poziomu i od nauczycieli wolne, znaleziska do swojej gałęzi)
- Procenty broni (1H, 2H, łuk, kusza) w drzewku Biegłość; siła, zręczność, mana, skradanie, alchemia, kowalstwo, zamki itd. jak w Gothicu
- Bez nauczyciela do 30%, odblokowanie do maksimum nauczyciela za punkty nauki; u każdego nauczyciela 5 punktów nauki = 1 punkt umiejętności; kamienne tablice walki = punkty do gałęzi biegłości (siła, zręczność, życie, mana po staremu); reset drzewek za złoto, cena rośnie z poziomem; testy u Pomocnika („insert ch”)
- Wartości na start (do balansu): 1 punkt za poziom; odblokowanie 5 punktów nauki za 10% (30→60 = 15, 60→75 = 8, 75→90 = 8, 90→100 = 5); ranga biegłości +5% (Nowicjusz 4 rangi do 30%, Wojownik / Strzelec 6 do 60%, Mistrz 8 do 100%); tablica poziom 1/2/3 = 1/2/3 punkty; reset 100 złota × poziom; rangi razem: Cios 44, Atak ładowany 32, Fikołek 23, Biegłość 72
- 2026-10-15: bonus z broni (np. +3% noża) nie liczy się do limitu Biegłości
- Po testach: „balans jest ok, zostawmy to na dłuższą sesję”

## Fazy 7–8 (odłożone 2026-10-14)
- Faza 7 loot: rzadkość, afiksy na bazie przedmiotów z Gothic II, dropy z potworów
- Faza 8 świat i balans: gęstość potworów / spawnery, balans łuku względem broni białej, fikołek jako umiejętność do nauczenia (akrobatyka?), balans drzewek, kolejne statusy negatywne

## Faza 9 — interfejs: menu, kursor, paski, mapa (2026-10-14/15)
- Kule życia i many: NIE — „paski Gothica, tylko ładniejsze”
- Mapa: duża mapa + zapis odkrytego terenu; szkic Bezimiennego atramentem, kupiona mapa zamienia go na malowaną; handlarze i nauczyciele z dziennika na mapie (w aktualnym miejscu postaci); znaki z map misji + własne pinezki; obie formy: nakładka na grę (jak Diablo II) i pergamin na cały ekran
- Zamiast proponowanych dodatków: kursor = model miecza, czerwony nad wrogami; w menu nazwa Diathic i wersja gry; logo, muzyka i tło menu od użytkownika (assets/diathic)
- Launcher i paczka dla testerów pod nazwą Diathic
- 2026-10-15 wieczorem: menu „bardziej nowoczesne” z nowym tłem, muzyką, logo i assetami z gry; muzyka Nocy Kruka nie może nakładać się na naszą; skróty starego sterowania nakładały się na nowe (skradanie, chód / bieg) → nowe skróty i strona „Sterowanie”
- Test tester-3679: ekran ładowania, zapisu i dziennik w starym stylu → przerobić; „większy prześwit albo drzewa ukryte do 2× wzrostu bohatera” — wybrane oba; minimapa pokazywała potwory pod ziemią i na niższych piętrach

### Lista do testu (PR #10 i dalej)
- Menu: tylko nasza muzyka, tło z iskrami, wybór myszą i klawiaturą, menu w grze (Esc), podmenu (opcje, wczytywanie)
- Sterowanie: R chód / bieg, V skradanie (z talentem / komunikat bez), brak podwójnych działań klawiszy, strona „Sterowanie” (zmiana, zamiana, domyślne, Esc)
- Kursor: czubek przy myszy, rozmiar, czy nie „pływa”, czerwony nad wrogiem
- Paski: położenie, liczby, ślad ubytku, pulsowanie, doświadczenie (najechanie)
- Tab trzy razy: minimapa, linie na grze, nic; M: bez mapy krainy szkic odkrytego terenu, z mapą (np. od Brahima) malowana, zakładki map
- Zapis i wczytanie: odkryty teren i pinezki zostają; znaczniki nauczycieli i handlarzy po rozmowach
- PR #10: ekran ładowania (nowa gra, wczytanie, zmiana świata), zapis gry, opcje / zapis / wczytanie, dziennik (L), prześwit i korony drzew w lesie, minimapa przy jaskiniach i piętrach, ruch na klik przy meblach, parowanie i zachwianie, szybkość ataku

## Faza 10 — przejście gry (2026-10-15)
- Poprawki map pionowych (Górnicza Dolina, Jarkendar, wieże), jaskinie wielopoziomowe, kamera, ruch na klik
- Ręczny test zbyt czasochłonny → bot testowy, „zrobimy to później” (plan: bot-testowy.md)

## Publikacja (2026-10-16)
- Użytkownik: „dodajmy do planu dokładne kroki do zrobienia przed publikacją” → publikacja.md (etapy A–G, decyzje otwarte: język pierwszej wersji, launcher, fazy 7–8 przed betą, pad)
- Model: mod wymagający własnego Gothic II Gold (Steam / GOG), niekomercyjnie; publiczna beta (GitHub, ModDB, fora) → 1.0 → Steam za zgodą THQ Nordic

## Faza 11 — drzewka broni (2026-10-16)
- Użytkownik: „zróbmy brancha z osobnym drzewkiem i skillami do łuków, tarcz, broni 1H, 2H” — po burzy mózgów:
  - jedno drzewko na broń (umiejętności jako gałęzie, nie osobne drzewko na każdą umiejętność)
  - łuk i kusza we wspólnym drzewku
  - tarcza i blok bronią to samo: drzewko Blok działa dla bloku bronią i pięściami, do czasu dodania przedmiotu tarczy
  - Q / E / F dla umiejętności; obrót kamery przeniesiony na strzałki (przypisywanie umiejętności do PPM i 3–0 — możliwe później)
- Do testu: wszystkie umiejętności (Q / E z 1H, 2H, łukiem; F), przeniesione punkty łuku w starym zapisie, wygląd księgi z 8 drzewkami, pasek z Q / E / F, strona „Sterowanie”

## Faza 12 — grafika (2026-10-17)
- 2026-10-17: użytkownik wskazał poprawę jakości tekstur jako następne ulepszenie; pierwszy krok planu to audyt mipmap i próbnika tekstur świata.
- 2026-10-17: użytkownik dodał ocenę DLSS Super Resolution przez Streamline/Vulkan jako późniejszy, opcjonalny krok; nie zastępuje to poprawy samych tekstur.
- 2026-10-17: użytkownik chce panel „Grafika” w grze do zmiany i zapisu dostępnych opcji bez ręcznej edycji `Gothic.ini`; wdrożenie UI wymaga uzgodnienia z agentem rozgrywki.
- 2026-10-17: panel ustawień grafiki = własna strona „Grafika Diathic” w nowoczesnym menu po „Sterowanie”, nie w Opcjach Gothica; tabela ustawień u agenta grafiki (`ArpgGfx::options()` w arpggfx), stronę rysującą tabelę robi agent rozgrywki po scaleniu PR #10, razem z anizotropią.
- 2026-10-17: kierunek grafiki: mroczny Gothic — klimat Gothic II na pierwszym planie, subtelne akcenty Diablo.
- Użytkownik: drugi agent ulepsza tekstury i grafikę równolegle z pracą nad grą → gałąź faza12-grafika, zasady i podział plików w grafika.md
- Agent grafiki to ChatGPT (bez skilli, pamięci i dokumentów projektu) → grafika.md samowystarczalny, stan w jego sekcji „Stan”

## Poprawki walki i księga umiejętności (2026-10-19, gałąź poprawki-walka)
- Użytkownik: Taniec ostrzy = jedno uderzenie zamiast trzech; obrażenia zawsze 5 albo wielokrotność; po parowaniu potwory odepchnięte; potwory blokują w nieskończoność
  - Taniec ostrzy: każde cięcie ma swój czas (0,36 s), trafienie liczy umiejętność (0,17 s w cięciu), nie animacja; przed trafieniem krok do wroga z przodu
  - obrażenia bohatera (decyzja: „ochrona procentowo”): (siła albo zręczność + broń) × 100 / (100 + ochrona); cios bez pełnych obrażeń = 50% (Gothic: 10%); czary i ciosy innych bez zmian
  - parowanie: zachwiany wróg stoi w miejscu (bez odepchnięcia animacji)
  - blok ludzi i potworów jak bohatera: 3 ładunki, +1 na sekundę poza blokiem, przy 0 przepuszcza 25% za każdy ładunek poniżej; AI z zużytym blokiem zamiast parady: atak (50%), odskok (25%), krok w bok (25%); pasek ładunków nad głową, gdy niepełny
- Klawisze: F = broń (dobycie / schowanie), Shift przytrzymany + LPM = atak w miejscu (z lockiem: w cel); umiejętności drzewek na 3–0 zamiast G / F / B (przypisanie: najazd w księdze + klawisz; nowo poznana sama na pierwszy wolny klawisz; runa na klawisz z umiejętnością zdejmuje umiejętność i odwrotnie)
- Księga umiejętności (decyzja „4 + 4”): strona główna = spis — lewa: pasywne (Cios, Atak ładowany, Fikołek, Biegłość), prawa: umiejętności (Blok, Jednoręczna, Dwuręczna, Łuk i kusza, z ich umiejętnościami i klawiszami); klik = przewrócenie strony na drzewko: lewa drzewko, prawa opis węzła; „Wróć do spisu” / Backspace
- Wartości do balansu: `ArpgStatus::ArmorK` 100, `NormalHit` 0,5, `DanceCutTime` 0,36 s, `DanceHitAt` 0,17 s, `DanceMul` 1, `DanceArc` 150°
- Do testu: Taniec ostrzy (3 i 4 cięcia, trafienia w liczbach), obrażenia różnych broni i siły (bez stałych 5), parowanie bez odepchnięcia, blok bandytów / orków (pasek nad głową, po 3 blokach atak / odskok), F i Shift + LPM, przypisanie do 3–0 i runy, stary zapis (umiejętności same na wolne klawisze), strona „Sterowanie”, spis i przewracanie stron

## Poprawki potworów (2026-10-29, gałąź poprawki-potwory)
- Użytkownik: potwory w Jarkendarze i Górniczej Dolinie nadal mają nieskończony blok, a zmiany balansu ich nie obejmują
  - przyczyna: odskok (FA_ENEMY_PREHIT, większość potworów i orkowie) liczył się jako blok przez całą animację bez zużycia ładunków, więc cios bohatera nie dochodził do liczenia obrażeń; w Khorinis walki głównie z ludźmi (parada — działała), kretoszczurami i goblinami
  - decyzja „propozycja A”: odskok wrogów zużywa ładunki bloku jak parada; AI z zużytym blokiem zamiast odskoku i parady: atak (50%) albo krok w bok (50%) — odskok z 25% z 2026-10-19 usunięty, bo jest teraz blokiem
  - potwory, zwierzęta i orkowie (decyzja: „potwory + orkowie”, ludzie bez zmian): życie +20%, obrażenia +10%
- Wartości do balansu: `ArpgStatus::MonsterHpMul` 1,2, `MonsterDmgMul` 1,1
- Do testu: zębacz / cieniostwór / ork — po 3 blokach („Blok”) czwarty cios przechodzi częściowo, pasek ładunków nad głową; potem atak albo krok w bok zamiast odskoku; potwory wytrzymują więcej ciosów i biją mocniej; stary zapis w Jarkendarze / Górniczej Dolinie (życie podniesione raz, nie przy każdym wczytaniu)

## Bossowie — smoki i Kruk (2026-10-31, gałąź bossowie)
- Użytkownik: „zróbmy mechaniki bossów do smoków i Kruka”; zakres: wszystko naraz (część wspólna + 5 smoków + Kruk w jednym PR)
- Oryginał (skrypty 2.6 PL): smoki GIL_DRAGON (Dragon_Swamp Pandrodor, Dragon_Rock Pedrakhan, Dragon_Fire Feomathar, Dragon_Ice Finkregh, Dragon_Undead; Dragon_Fire_Island Feodaron nieśmiertelny — bez zmian), 1000 życia, ochrona 170, rozmowa przed walką (Oko Innosa), z daleka (> FIGHT_DIST_DRAGON_MAGIC) ItRu_InstantFireball, ożywieniec ItRu_Deathball (B_SelectSpell), regeneracja +1 życia co 2 przebiegi ZS_MM_Attack_Loop; Kruk BDT_1090_Addon_Raven: 500 życia, FAI_HUMAN_MASTER, ItMw_BeliarWeapon_Raven, świątynia Adanosa (ADW_ADANOSTEMPEL_RAVEN_11), po jego śmierci (RavenIsDead) kamienni strażnicy w walce giną
- Zasady: tylko tryb ARPG; rozmowy, fabuła, B_DragonKillCounter i nagrody bez zmian; rozpoznanie bossa po nazwie instancji skryptu; kod w arpgboss.cpp / .h, liczby w jednym miejscu
- Wspólne:
  - pasek bossa u góry ekranu: imię, życie, znaczniki faz
  - 3 fazy według życia, progi 70% i 35%; przejście = ryk i 1,5 s nietykalności
  - ataki z ostrzeżeniem: czerwony kształt na ziemi (koło, stożek, linia) 1–1,5 s, potem obrażenia; przewrót pozwala uciec
  - przyzwańcy z modeli gry, dobrani do bossa
- Smoki:
  - Pandrodor (bagienny): trujące kałuże (nowy status „Zatrucie”); faza 3: bagienne golemy (SwampGolem)
  - Pedrakhan (skalny): spadające głazy w kołach; faza 3: kamienne golemy (StoneGolem)
  - Feomathar (ognisty): zionięcie ogniem w stożek z Podpaleniem; faza 3: plamy ognia i ogniste jaszczurki (FireWaran)
  - Finkregh (lodowy): lodowy podmuch w linii (nowy status „Zmrożenie”, spowolnienie); faza 3: lodowe golemy (IceGolem)
  - Smok ożywieniec (Irdorath): kula śmierci jak w oryginale + fala ciemności (pierścień, przez który trzeba przeskoczyć przewrotem); faza 3: szkielety (Lesser_Skeleton)
- Kruk: faza 1 szermierz z Mieczem Beliara (jak w oryginale); faza 2 budzi kamiennych strażników (Stoneguardian) i rzuca piorun Beliara w linii; faza 3 „Gniew Beliara” — fala od Kruka co kilka sekund
- Test bez fabuły: konsola Marvina `insert Dragon_Fire` (itd.), `insert BDT_1090_Addon_Raven`
- Do ustalenia po teście: życie bossów (mnożnik ponad MonsterHpMul), obrażenia ataków z ostrzeżeniem, czasy ostrzeżeń, liczba przyzwańców
- Wartości do balansu: `ArpgBossCfg` w common/game/arpgboss.h (progi faz, czasy ostrzeżeń i ataków, trafienia jako część życia bohatera, Zatrucie, Podpalenie, Zmrożenie, liczba przyzwańców)
- Do testu (konsola Marvina w trybie ARPG: `insert Dragon_Fire`, `insert Dragon_Swamp`, `insert Dragon_Rock`, `insert Dragon_Ice`, `insert Dragon_Undead`, `insert BDT_1090_Addon_Raven`): pasek u góry w walce; czerwone kształty na ziemi i unik przewrotem; przy 70% i 35% ryk, złoty pasek, „Nietykalny”; przyzwańcy w fazie 3 (Kruk: strażnicy w fazie 2, nie atakują Kruka); Zatrucie w kałuży, Zmrożenie = tylko chód; czy ataki nie są za częste / za mocne
- Zgłoszone 2026-10-31 (test w świątyni Adanosa): przyciski na ścianie nie podświetlały się; z Krukiem nie dało się porozmawiać przed walką (był czerwony, LPM = atak). Poprawione na gałęzi bossowie:
  - przełączniki i dźwignie bez nazwy fokusu wskazywalne, etykieta „Przełącznik”; Z ich nie pokazuje (ukryte przełączniki zostają ukryte)
  - ludzie z AIV_EnemyOverride (Kruk, Dexter, rozbójnicy na drogach, Poszukiwacze, Alvares, Engardo) nie są wrogami, dopóki nie walczą z bohaterem: LPM = rozmowa, Ctrl + LPM = atak
  - Do testu: 5 przycisków w przedsionku świątyni (kursor, etykieta, LPM, pułapka / przejście); Kruk: LPM → rozmowa z wyborami, po niej walka i pasek bossa; dźwignie u Rhademesa; Dexter / rozbójnicy przy drodze — rozmowa przed walką

## Leczenie, łup elit, życie w rozdziałach (2026-11-04, gałąź smoki-lot)
- Użytkownik: pasywna regeneracja % zdrowia po zjedzeniu — 5% życia, mikstury 20%, 25% i 35%; więcej złota i losowe tabliczki z elitarnych potworów; mniejszy przyrost życia na akt
- Odpowiedzi (2026-11-04): leczenie przez czas oprócz natychmiastowego leczenia Gothica; łup 30–60 złota × rozdział, tablica w 25% (losowa, najczęściej poziom 1); życie ×1,05 na rozdział (jak obrażenia)
- Zmiany: jedzenie i rośliny 5% przez 10 s, esencja / ekstrakt / eliksir leczniczy 20 / 25 / 35% przez 5 s, dawki się sumują, „+N” nad bohaterem; dowódca elity ma przy sobie złoto i czasem kamienną tablicę budowniczych; życie potworów i bossów ×1,05 za rozdział (było ×1,15)
- Wartości do balansu: `ArpgStatus::FoodRegen`, `FoodRegenMs`, `PotionRegen1..3`, `PotionRegenMs`; `ArpgElite::LootGoldMin`, `LootGoldMax`, `LootTabletPct`, `ChapterMul`
- Do testu: zjeść jabłko / chleb / roślinę i wypić esencję w walce — „+N” co sekundę przez 10 s / 5 s, kilka dawek naraz; klawisze mikstur też; zabity dowódca „(elita)” — złoto, czasem tablica (konsola Marvina: `insert OrcElite_Roam` nie jest dowódcą — dowódców szukać w grupach w świecie); życie potworów w późnych rozdziałach

## Smoki — lot, golemy, odporności (2026-11-04, gałąź smoki-lot)
- Użytkownik: gdy są addy, smok lata nad areną, dopóki addy nie zginą albo 5 min; w locie bez melee, tylko broń dystansowa; dopracować animację lotu; addy zawsze golemami żywiołu smoka; addy zadają za dużo obrażeń; smok nadal ucięty — pokazać cały model; smoki atakują za szybko, potrafią dwa ataki naraz albo bardzo szybko ten sam; smoki: −25% obrażeń dystansowych, −15% wręcz, −50% z czarów; obrażenia rosnące w rozdziałach najwyżej 5% na rozdział
- Odpowiedzi (2026-11-04): w locie czary też trafiają (łuk, kusza, czary); smok ożywieniec zostaje przy szkieletach
- Zmiany:
  - lot w fazie 3 (wtedy przyzwańcy): start po ryku, krążenie nad areną, lądowanie w miejscu startu, gdy przyzwańcy zginą, po 5 min albo gdy bohater odejdzie; w locie ataki z ostrzeżeniem dalej, bez ciosów i czarów Gothica
  - animacja lotu: model smoka może nie mieć animacji lotu (plików gry nie ma w piaskownicy) — najpierw S_FLY, bez niej stojąca + ruch z kodu: wznoszenie z nosem w górę, kołysanie z każdym uderzeniem skrzydeł, przechył w skręcie, skrzydła (kości z WING w nazwie) w górę i w dół
  - golemy: bagienne, kamienne, ogniste (zamiast jaszczurek), lodowe; ożywieniec — szkielety; ich ciosy ×0,5
  - jeden atak naraz: wspólna przerwa dla ataku z ostrzeżeniem i własnego (cios, zionięcie, czar): 3 s po trafieniu ataku z ostrzeżeniem, 2,5 s po ciosie, 5 s po czarze; ataki z ostrzeżeniem co 16 s / 13 s (było 13 / 10)
  - smok cały: kula bez cięcia liczona z kości (wcześniej z pudełka Gothica — za mała), bez mgły wojny na smoku, kamera dalej w walce ze smokiem (×1,35, w locie ×1,6)
  - obrażenia potworów i bossów ×1,05 za rozdział (było ×1,15); życie bez zmian (×1,15)
- Wartości do balansu: `ArpgBossCfg` — `AttackMs1`, `AttackMs2`, `GapMs`, `RestMs`, `SpellRestMs`, `MinionDmg`, `Resist*`, `FlyMaxMs`, `FlyHeight`, `FlyRadius`, `FlySpeed`, `FlapMs`, `FlapDeg`, `FlyBob`, `BankDeg`, `CamZoom`, `CamFlyZoom`; `ArpgElite::ChapterDmgMul`
- Do testu (konsola Marvina w trybie ARPG: `insert Dragon_Fire` / `Dragon_Swamp` / `Dragon_Rock` / `Dragon_Ice` / `Dragon_Undead`; do fazy 3 = 35% życia):
  - lot: start po ryku, wysokość i okrąg (nie w skale), w jaskini smoka ożywieńca niżej / mniejszy okrąg; czy ruch skrzydeł wygląda dobrze (gdy skrzydła stoją albo kręcą się źle — kości skrzydeł nazywają się inaczej, potrzebna lista kości z modelu); w log.txt linia „Diathic: smok startuje” — czy jest S_FLY i lista kości modelu (do poprawki skrzydeł)
  - lądowanie po zabiciu golemów, ryk, potem znowu walka wręcz
  - w locie: miecz nie trafia, napis z podpowiedzią, klik nie wysyła pod smoka; łuk, kusza i czary trafiają
  - golemy żywiołu, ich ciosy słabsze
  - smok nie robi dwóch ataków naraz, ten sam atak nie powtarza się szybko; czy teraz za wolno
  - smok cały na ekranie: z bliska, gdy zasłania bohatera, w jaskini; kamera dalej w walce i w locie — czy nie za daleko
  - odporności: liczby obrażeń na smoku (strzała, miecz, czar)
  - potwory w późnych rozdziałach: obrażenia ×1,05 za rozdział

## Bossowie — poprawki (2026-11-03, gałąź bossowie-poprawki)
- Użytkownik: bossowie atakują za szybko; lepsze znaki mocnych umiejętności; dźwięk przy mocnych umiejętnościach (zionięcie zabijało bohatera jednym ciosem — nie powinno); przyzwane golemy bez odepchnięcia; rozmowy ze smokami — kamera na głowę, nie na brzuch; smoki ucięte — poprawić widoczność; hitbox większy i zgodny z modelem; dotyczy wszystkich smoków
- Przyczyny: zionięcie = zwykły atak smoka w Gothicu, obrażenia DAM_FIRE|DAM_FLY (siła 200 liczona dla każdego rodzaju) × mnożnik rozdziału — więcej niż życie bohatera; golemy kamienne i bagienne mają DAM_FLY (odrzut); smok bez kości „BIP01 HEAD” — głowa 1,8 m nad stopami; we wnętrzach cięcie sufitu i prześwit cięły też smoka; zasięg ciosu liczył tylko kości do 2,5 m w górę (ogon)
- Zmiany: ataki z ostrzeżeniem co 13 s / 10 s (było 10 / 7), ostrzeżenie 1,6 s (było 1,3); po własnym ataku smok stoi 1,5 s, Kruk 0,6 s; jeden cios bossa najwyżej 35% maksymalnego życia; wyraźniejsze znaki na ziemi + nazwa ataku pod paskiem + dźwięki; własny atak smoka ma słabszy znak (stożek) i ryk; przyzwańcy bez odrzutu; kamera i ustawienie w rozmowie ze smokiem; smok bez cięcia; hitbox i kursor na modelu
- Wartości do balansu: `ArpgBossCfg` — `AttackMs1`, `AttackMs2`, `WarnMs`, `RestMs`, `RavenRestMs`, `MaxHit`, `MeleeHalfDeg`, `MeleeSignPart`, `SoundRange`
- Do testu (konsola Marvina w trybie ARPG: `insert Dragon_Fire` / `Dragon_Swamp` / `Dragon_Rock` / `Dragon_Ice` / `Dragon_Undead`, `insert BDT_1090_Addon_Raven`):
  - czy ataki nie za częste / za rzadkie; czy smok nie stoi za długo po ciosie
  - znaki: widać je na lawie (Feomathar) i śniegu (Finkregh); czy błysk i strzałki pomagają; nazwa ataku pod paskiem; znak własnego ataku smoka (stożek) — czy pokrywa się z zionięciem
  - dźwięki: ryk na początku, dźwięk przy trafieniu — czy słychać, czy nie za głośno; czy nazwy dźwięków istnieją w polskiej wersji (cisza = brak)
  - zionięcie / ugryzienie smoka: najwyżej 35% życia
  - golemy (Pedrakhan, Pandrodor faza 3): cios bez odrzutu, tylko potknięcie
  - rozmowa ze smokiem (z Okiem Innosa): ujęcia na głowę, nie za blisko i nie w środku pyska; Bezimienny 5,5–8 m od smoka; patrzy w górę na głowę
  - smok w całości, także w jaskini (smok ożywieniec w Irdorath) i gdy zasłania bohatera
  - kursor na szyi, głowie, skrzydle i ogonie = smok; cios w nogę i bok trafia; bohater nie wchodzi pod głowę / w nogi

## Interakcje z postaciami (2026-11-01, gałąź poprawki-interakcje)
- Użytkownik: sprawdzić ukryte dźwignie i zły rodzaj interakcji bohatera z przedmiotami, wrogami i NPC
- Znalezione w kodzie: każdy potwór i ork był wrogiem pod kursorem (LPM = atak, automatyczny cel, lock, umiejętności) — także przyzwańcy bohatera, ognik, owce (Liesel, owca Akila), przyjazny ork Ur-Shak i orkowie dowódcy, którzy w grze najpierw mówią; ogłuszonego człowieka nie dało się dobić (LPM zawsze = przeszukanie)
- Poprawione: stwory przyjazne / w drużynie / z rozmową przed walką = LPM rozmowa, Ctrl + LPM atak; Ctrl + LPM na ogłuszonym = cios dobijający
- Dźwignie i przełączniki: w kodzie bez nowych braków (wskazywane: z nazwą, z funkcją stanu, z wymaganym przedmiotem, przełącznik / koło, każdy obiekt z triggerTarget; kursor widzi obiekt w ścianie do 1 m za trafieniem promienia); pełna lista obiektów świata tylko z plików gry — test w grze
- Do testu: przyzwany szkielet / golem / wilk — zwykły kursor, przytrzymany LPM po zabiciu wroga nie przechodzi na przyzwańca, Taniec ostrzy / lock go nie celują; ognik (Jarkendar, od Riordiana) — LPM = rozmowa; owca Liesel / owca Akila — LPM = rozmowa „chodź”; zwykła owca na farmie Onara — LPM nic nie robi (Ctrl + LPM = atak, przestępstwo jak w grze); Ur-Shak — LPM = rozmowa; orkowie dowódcy (rozdział 5) — LPM = rozmowa, po niej walka; ogłuszony człowiek (np. bójka z najemnikiem) — LPM = przeszukanie, Ctrl + LPM = dobicie; chrząszcze i uśpieni kamienni strażnicy w Jarkendarze — LPM nadal atakuje

## Przechył w biegu (2026-11-01, gałąź poprawki-przechyl)
- Użytkownik: postać przechyla się w jedną stronę przy biegu w dół ekranu (S) i na boki — sprawdzić
- Znalezione w kodzie: przechył Gothica liczony z azymutu kamery; w trybie ARPG to kąt bohater − kamera (W = 0°, S = 180°, A / D = ±90°), stąd stały przechył 14,5°
- Decyzja: przechył tylko w ostrym skręcie, w stronę skrętu; bieg w jednym kierunku bez przechyłu
- Wartości: opóźniony kierunek dogania bohatera 3/s, próg 25°, najwięcej 14,5° (`ArpgLean*` w playercontrol.h); skręt o 90° ≈ 11° przez ~0,6 s, o 45° ledwo widać, małe poprawki kierunku bez przechyłu
- Do testu: bieg S, A, D, W i po skosie — postać pionowo; skręt W → D i W → A w biegu — przechył w stronę skrętu i powrót; ruch na klik po ścieżce — bez kołysania; czy strona przechyłu dobra (jeśli odwrotnie — zmienić znak)

## Potwory: rozdziały, czerwona aura, elity (2026-11-01, gałąź poprawki-potwory-elity)
- Użytkownik: ataki z czerwoną aurą u potworów jak ork elita, jaszczuroludzie, trolle — nieparowalne, zablokowane zabierają od razu 3 ładunki bloku; życie i obrażenia potworów +15% na rozdział; elitarne potwory
- Decyzje (odpowiedzi 2026-11-01): elity = grupy z dowódcą; aura = te trzy + podobne (cieniostwory, golemy, demony); skalowanie składane (×1,15 za rozdział); bossowie też skalowani
- Wartości (`ArpgElite` w common/game/arpgelite.h): rozdział ×1,15 (rozdział 6 ≈ ×2,01; z ×1,2 życia potworów ≈ ×2,41); aura co 6–10 s, dowódca 4–7 s, ostrzeżenie ≥ 0,8 s, gaśnie po 4 s bez ataku, blok = 3 ładunki; grupa: łańcuch ≤ 15 m, 2–8 potworów, 25% grup z dowódcą; dowódca: życie ×3, obrażenia ×1,5, model ×1,15, doświadczenie ×3; reszta grupy ×1,25 obrażeń, dopóki dowódca żyje (40 m)
- Ataki bossów z ostrzeżeniem na ziemi bez zmian (część życia bohatera, rośnie razem z nim); skalowane są ich życie i zwykłe ciosy
- Do testu: konsola Marvina `insert Troll` / `OrcElite_Roam` / `Draconian` — aura, potem cios: blok zabiera 3 ładunki, parowanie nie działa, unik działa; dźwięk aury nie męczy; grupy w świecie (wilki, orkowie w Górniczej Dolinie; co czwarta grupa ma dowódcę) — dowódca większy, etykieta „(elita)”, więcej życia, po zabiciu „Doświadczenie +”; zmiana rozdziału — życie potworów rośnie (pasek życia u góry); stary zapis wczytuje się bez zmian życia w rozdziale 1; czy elity nie za częste / za mocne w rozdziale 1

## Pasek, runy, teleport (2026-11-01, gałąź poprawki-pasek-teleport)
- Użytkownik: większy pasek szybkiego wyboru i interfejs; dokładniej pokazać runy na pasku; runy teleportacji jako jedna runa z oknem wyboru celu; nieodblokowane cele niewidoczne, odblokowanie = podniesienie runy
- Decyzje (odpowiedzi 2026-11-01): suwak „Rozmiar interfejsu” w ustawieniach (domyślnie 130%); znak runy + nazwa po najechaniu; wybór celu na mapie ze znacznikami (inny świat jako lista obok); cele z innego świata wyszarzone z nazwą świata
- Wartości: rozmiar 80–160% co 10%, pasek najwyżej 70% szerokości ekranu
- Do testu: „Grafika Diathic” → „Rozmiar interfejsu” — pasek, paski życia i many, liczby i klawisze rosną od razu; mała rozdzielczość: pasek się mieści; runy i zwoje na 3–0 — znak zamiast kamienia (jeśli któraś bez znaku albo zły wycinek: nazwa runy); najechanie na slot — nazwa, mana, klawisz; plecak: kilka run teleportu = jedno pole „Runa teleportacji” (liczba znanych miejsc); klawisz z runą teleportu — mapa ze znacznikami, klik — teleport (mana 10), cele z Górniczej Doliny w Khorinis wyszarzone; konsola Marvina: `insert ItRu_TeleportSeaport`, `insert ItRu_TeleportMonastery`, `insert ItRu_TeleportOC`
