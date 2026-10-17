#!/usr/bin/env bash
# Raport błędu testera → funkcje i linie kodu.
# Użycie: dev/symbolizuj.sh <kontekst.txt | Diathic-raport-*.zip> [tag wydania, np. tester-3679 / build-3700]
# Tag bez podania = z linii „exe:” raportu (gra\wersje\<tag>\Gothic2Notr.exe). Lokalny build: tag „lokalny” + DIATHIC_EXE.
set -euo pipefail
src=$1
tmp=${DIATHIC_TMP:-/tmp/diathic}
mkdir -p "$tmp"

ctx=$src
if [[ $src == *.zip ]]; then
  rm -rf "$tmp/raport" && mkdir -p "$tmp/raport"
  unzip -o -q "$src" -d "$tmp/raport"
  ctx=$(find "$tmp/raport" -name kontekst.txt | sort | tail -1)
  [ -n "$ctx" ] || { echo "brak kontekst.txt w raporcie (gra nie zapisała crasha) — zobacz system.txt i log.txt w $tmp/raport"; exit 1; }
fi

tag=${2:-$(grep -m1 '^exe:' "$ctx" | tr '\\' '/' | sed -n 's#.*/wersje/\([^/]*\)/.*#\1#p')}
[ -n "$tag" ] || { echo "nie znam wersji — podaj tag jako drugi argument"; exit 1; }

if [ "$tag" = lokalny ]; then
  exe=${DIATHIC_EXE:?ustaw DIATHIC_EXE na Gothic2Notr.exe}
else
  exe=$tmp/sym-$tag/Gothic2Notr.exe
  if [ ! -f "$exe" ]; then
    mkdir -p "$tmp/sym-$tag"
    # REST (gh release download idzie przez GraphQL, niedostępny w piaskownicy); exe ma pełne info debug (~0,5 GB)
    url=$(gh api "repos/wr0b3l/gothic-diablo/releases/tags/$tag" --jq '.assets[] | select(.name=="gothic-diablo-win64.zip") | .url')
    gh api -H 'Accept: application/octet-stream' "$url" > "$tmp/sym-$tag/gothic-diablo-win64.zip"
    unzip -o -q "$tmp/sym-$tag/gothic-diablo-win64.zip" Gothic2Notr.exe -d "$tmp/sym-$tag"
  fi
fi
command -v x86_64-w64-mingw32-addr2line >/dev/null || sudo apt-get install -y -q binutils-mingw-w64-x86-64 >/dev/null

grep -E '^(sygnal|wyjatek|dostep|tryb|swiat|bohater):' "$ctx" || true
echo "--- stos ($tag)"
grep -o 'addr2line 0x[0-9a-f]*' "$ctx" | awk '{print $2}' | x86_64-w64-mingw32-addr2line -f -C -p -e "$exe" | sed "s#$PWD/##"
