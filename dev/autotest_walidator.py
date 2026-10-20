#!/usr/bin/env python3
"""gothic-diablo: walidator scenariuszy bota fabuły (faza 10) - bez gry, na źródłach skryptów Gothic II NK.

Sprawdza nazwy w scenariuszu (postacie, informacje dialogów, wybory, przedmioty, zmienne, tematy dziennika),
czy informacja należy do postaci z kroku i czy wybór (Info_AddChoice) należy do bieżącej rozmowy.
Punktów drogi nie zna (są w plikach .ZEN) - nieznane nazwy NW_... tylko jako ostrzeżenie, gdy nie ma ich w skryptach.

Źródła skryptów: https://github.com/VaanaCZ/gothic-2-addon-scripts (gałąź 2.6-PL = wersja polska ze Steam),
folder _work/Data/Scripts/Content. Pliki w cp1250.

Użycie:
  python3 dev/autotest_walidator.py <scenariusz.txt> <Content> [--warunki]   sprawdzenie (kod 1 = błędy)
  python3 dev/autotest_walidator.py --drzewo <DIA_info> <Content>           wybory informacji (drzewo Info_AddChoice)
"""
import os
import re
import sys


def read(path):
    raw = open(path, 'rb').read()
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError:
        return raw.decode('cp1250', errors='replace')


def strip_comments(s):
    # line comments first (a "/*" inside them opens nothing), strings kept as they are
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c == '"':
            j = s.find('"', i + 1)
            j = n if j < 0 else j + 1
            out.append(s[i:j])
            i = j
        elif s.startswith('//', i):
            j = s.find('\n', i)
            i = n if j < 0 else j
        elif s.startswith('/*', i):
            j = s.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


class Scripts:
    def __init__(self, root):
        self.classes = {}      # prototype / class name -> parent
        self.instances = {}    # NAME -> parent (class or prototype)
        self.infos = {}        # INFO -> dict(npc, cond, info, desc, important, permanent, file)
        self.funcs = {}        # NAME -> body
        self.ints = set()
        self.strings = {}      # const string NAME -> value
        self.choices = {}      # function -> set(owner infos)
        self.adds = {}         # function (info fn or choice fn) -> [choice fns]
        self.text = []         # all string literals (waypoints)
        for dp, _, fs in os.walk(root):
            for f in fs:
                if f.lower().endswith('.d'):
                    self.parse(os.path.join(dp, f))

    def parse(self, path):
        s = strip_comments(read(path))
        for m in re.finditer(r'(?i)\b(class|prototype)\s+(\w+)\s*\(\s*(\w*)\s*\)', s):
            self.classes[m.group(2).upper()] = (m.group(3) or m.group(2)).upper() if m.group(1).lower() == 'prototype' else None
        for m in re.finditer(r'(?is)\binstance\s+(\w+)\s*\(\s*(\w+)\s*\)\s*\{(.*?)\};', s):
            name, parent, body = m.group(1).upper(), m.group(2).upper(), m.group(3)
            self.instances[name] = parent
            if parent == 'C_INFO':
                g = lambda k: (re.search(r'(?i)\b' + k + r'\s*=\s*([^;]+);', body) or [None, ''])[1].strip()
                self.infos[name] = dict(npc=g('npc').upper(), cond=g('condition').upper(), info=g('information').upper(),
                                        desc=g('description').strip('"'), important=g('important').upper() == 'TRUE',
                                        permanent=g('permanent').upper() == 'TRUE', file=os.path.basename(path))
        for m in re.finditer(r'(?i)\binstance\s+([\w\s,]+?)\s*\(\s*(\w+)\s*\)\s*;', s):
            for n in m.group(1).split(','):
                self.instances[n.strip().upper()] = m.group(2).upper()
        for m in re.finditer(r'(?is)\bfunc\s+\w+\s+(\w+)\s*\([^)]*\)\s*\{', s):
            start, depth, i = m.end(), 1, m.end()
            while i < len(s) and depth:
                depth += {'{': 1, '}': -1}.get(s[i], 0)
                i += 1
            body = s[start:i]
            fn = m.group(1).upper()
            self.funcs[fn] = body
            for c in re.finditer(r'(?i)Info_AddChoice\s*\(\s*(\w+)\s*,[^;]*?,\s*(\w+)\s*\)', body):
                self.choices.setdefault(c.group(2).upper(), set()).add(c.group(1).upper())
                self.adds.setdefault(fn, []).append(c.group(2).upper())
        for m in re.finditer(r'(?i)\b(?:var|const)\s+int\s+(\w+)', s):
            self.ints.add(m.group(1).upper())
        for m in re.finditer(r'(?i)\bconst\s+int\s+([\w\s,]+?)\s*=', s):
            for n in m.group(1).split(','):
                self.ints.add(n.strip().upper())
        for m in re.finditer(r'(?i)\bvar\s+int\s+([\w\s,]+?);', s):
            for n in m.group(1).split(','):
                self.ints.add(n.strip().upper())
        for m in re.finditer(r'(?i)\bconst\s+string\s+(\w+)\s*=\s*"([^"]*)"', s):
            self.strings[m.group(1).upper()] = m.group(2)
        self.text.extend(x.upper() for x in re.findall(r'"([^"\n]*)"', s))

    def cls(self, name):
        n, seen = name.upper(), 0
        while n in self.instances or n in self.classes:
            if n in self.instances:
                n = self.instances[n]
            elif self.classes.get(n):
                n = self.classes[n]
            else:
                break
            seen += 1
            if seen > 6:
                break
        return n

    def tree(self, info, depth=0, seen=None):
        seen = seen or set()
        out = []
        fn = self.infos[info]['info'] if info in self.infos else info
        for c in self.adds.get(fn, []):
            if c in seen:
                continue
            seen.add(c)
            out.append('  ' * (depth + 1) + c)
            out.extend(self.tree(c, depth + 1, seen))
        return out


def main():
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] == '--drzewo':
        sc = Scripts(a[2])
        i = a[1].upper()
        d = sc.infos.get(i)
        if d:
            print(f"{i}  npc={d['npc']}  {'WAŻNA ' if d['important'] else ''}{'STAŁA ' if d['permanent'] else ''}\"{d['desc']}\"")
            print('  warunek:', ' '.join(sc.funcs.get(d['cond'], '').split())[:400])
        print('\n'.join(sc.tree(i)) or '  (bez wyborów)')
        return 0
    if len(a) < 2:
        print(__doc__)
        return 2
    scen, root, conds = a[0], a[1], '--warunki' in a
    sc = Scripts(root)
    errors, warnings = [], []
    npc, info = None, None

    def need(kind, n, line):
        u = n.upper()
        ok = True
        if kind == 'npc':
            ok = sc.cls(u) == 'C_NPC'
        elif kind == 'info':
            ok = u in sc.infos
        elif kind == 'item':
            ok = sc.cls(u) == 'C_ITEM'
        elif kind == 'int':
            ok = re.fullmatch(r'[-+]?\d+', n) is not None or u in sc.ints
        elif kind == 'topic':
            ok = u in sc.strings
        elif kind == 'place':
            if sc.cls(u) == 'C_NPC':
                return True
            if u not in sc.text:
                warnings.append(f'linia {line}: punkt drogi {n} nie występuje w skryptach (sprawdzi gra)')
            return True
        if not ok:
            errors.append(f'linia {line}: nieznane {kind} {n}')
        return ok

    def cond(args, line):
        if not args:
            errors.append(f'linia {line}: brak warunku')
            return
        if args[0] == 'nie':
            return cond(args[1:], line)
        k, r = args[0], args[1:]
        if len(r) >= 2 and r[-2] == 'na':
            r = r[:-2]
        g = lambda i: r[i] if i < len(r) else ''
        if k == 'info':
            need('info', g(0), line)
        elif k == 'zmienna':
            need('int', g(0), line)
            need('int', g(2), line)
            if g(1) not in ('==', '!=', '>=', '<=', '>', '<'):
                errors.append(f'linia {line}: zły operator {g(1)}')
        elif k == 'przedmiot':
            need('item', g(0), line)
        elif k == 'dziennik':
            need('topic', g(0), line)
        elif k in ('martwy', 'obok'):
            need('npc', g(0), line)
        elif k == 'gildia':
            need('int', g(0), line)
        elif k == 'blisko':
            need('npc', g(0), line)
            need('place', g(1), line)
        elif k in ('rozdzial', 'godzina'):
            pass
        else:
            errors.append(f'linia {line}: nieznany warunek {k}')

    for no, raw in enumerate(read(scen).splitlines(), 1):
        t = raw.split('#', 1)[0].split()
        if not t:
            continue
        if t[0] == '?':
            t = t[1:]
        if t and t[0] == 'jesli':
            if 'to' not in t or t.index('to') == len(t) - 1:
                errors.append(f'linia {no}: jesli <warunek> to <krok>')
                continue
            k = t.index('to')
            cond(t[1:k], no)
            t = t[k + 1:]
        if not t:
            continue
        v, r = t[0], t[1:]
        if v == 'gdy':
            for x in r:
                if x.upper() not in sc.choices and x.upper() not in sc.infos:
                    errors.append(f'linia {no}: gdy {x} - nie ma takiego wyboru')
        elif v in ('po_bledzie', 'po_smierci'):
            pass
        elif v == 'rozmowa':
            if need('npc', r[0], no):
                npc = r[0].upper()
            info = None
            if len(r) > 1 and need('info', r[1], no):
                info = r[1].upper()
                d = sc.infos[info]
                if d['npc'] != npc:
                    errors.append(f'linia {no}: {r[1]} należy do {d["npc"]}, nie do {r[0]}')
                if conds:
                    print(f'{no:4d} {r[1]}: {"WAŻNA " if d["important"] else ""}{" ".join(sc.funcs.get(d["cond"], "").split())[:300]}')
        elif v == 'wybor':
            u = r[0].upper()
            if u in sc.infos:
                if sc.infos[u]['npc'] != npc:
                    errors.append(f'linia {no}: {r[0]} należy do {sc.infos[u]["npc"]}, rozmowa z {npc}')
            elif u in sc.choices:
                owners = sc.choices[u]
                if not any(sc.infos.get(o, {}).get('npc') == npc for o in owners):
                    errors.append(f'linia {no}: wybór {r[0]} jest z {", ".join(sorted(owners))}, rozmowa z {npc}')
            else:
                errors.append(f'linia {no}: nieznany wybór {r[0]}')
        elif v == 'zabij':
            for x in r:
                need('npc', x, no)
        elif v in ('zabierz', 'zaloz', 'uzyj'):
            need('item', r[0], no)
        elif v in ('idz', 'przenies'):
            need('place', r[0], no)
        elif v in ('czekaj', 'sprawdz'):
            cond(r, no)
        elif v == 'spij':
            if not re.fullmatch(r'\d\d:\d\d', r[0] if r else ''):
                errors.append(f'linia {no}: spij HH:MM')
        elif v in ('koniec', 'etykieta', 'zapisz'):
            pass
        else:
            errors.append(f'linia {no}: nieznany krok {v}')

    for w in warnings:
        print('uwaga:', w)
    for e in errors:
        print('BŁĄD:', e)
    print(f'{scen}: błędy {len(errors)}, uwagi {len(warnings)}')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
