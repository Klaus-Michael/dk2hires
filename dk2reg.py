#!/usr/bin/env python3
"""Kleiner Editor fuer Wine-Registry-Textdateien (user.reg) ohne Wine zu starten.

Wird von install.sh/uninstall.sh benutzt. Aendert nur die genannten Werte, alles andere
bleibt Zeichen fuer Zeichen erhalten. Wine darf fuer das Prefix nicht laufen, sonst
ueberschreibt der wineserver die Datei beim Beenden wieder.

  dk2reg.py get    <user.reg> <key> <name>
  dk2reg.py set    <user.reg> <key> <name> <rohwert>     z. B. dword:00000f00 oder '"native,builtin"'
  dk2reg.py delete <user.reg> <key> <name>               (leere Schluessel werden mit entfernt)
Schluessel ohne fuehrende Wurzel und mit einfachen Backslashes, z. B.
  'Software\\Wine\\AppDefaults\\DKII-DX.exe\\DllOverrides'
"""
import os
import sys
import time


def esc_key(key):
    return key.replace('\\', '\\\\')


def load(path):
    with open(path, 'r', encoding='utf-8', newline='') as f:
        return f.read().split('\n')


def save(path, lines):
    tmp = path + '.dk2hires-tmp'
    with open(tmp, 'w', encoding='utf-8', newline='') as f:
        f.write('\n'.join(lines))
    os.replace(tmp, path)


def find_section(lines, key):
    head = '[' + esc_key(key) + ']'
    for i, l in enumerate(lines):
        if l.startswith('[') and (l == head or l.startswith(head + ' ')):
            j = i + 1
            while j < len(lines) and lines[j] != '' and not lines[j].startswith('['):
                j += 1
            return i, j
    return None


def find_value(lines, sec, name):
    i, j = sec
    prefix = '"' + name.replace('\\', '\\\\').replace('"', '\\"') + '"='
    for k in range(i + 1, j):
        if lines[k].lower().startswith(prefix.lower()):
            return k
    return None


def cmd_get(path, key, name):
    lines = load(path)
    sec = find_section(lines, key)
    if not sec:
        return None
    k = find_value(lines, sec, name)
    if k is None:
        return None
    return lines[k].split('=', 1)[1]


def cmd_set(path, key, name, raw):
    lines = load(path)
    sec = find_section(lines, key)
    line = '"' + name + '"=' + raw
    if not sec:
        now = int(time.time())
        filetime = (now + 11644473600) * 10000000
        block = ['[' + esc_key(key) + '] ' + str(now), '#time=%x' % filetime, line, '']
        while lines and lines[-1] == '':
            lines.pop()
        lines += [''] + block
    else:
        k = find_value(lines, sec, name)
        if k is None:
            lines.insert(sec[1], line)
        else:
            lines[k] = line
    save(path, lines)


def cmd_delete(path, key, name):
    lines = load(path)
    sec = find_section(lines, key)
    if not sec:
        return
    k = find_value(lines, sec, name)
    if k is not None:
        del lines[k]
    sec = find_section(lines, key)
    i, j = sec
    if all(l.startswith('#') for l in lines[i + 1:j]):
        end = j + 1 if j < len(lines) and lines[j] == '' else j
        del lines[i:end]
    save(path, lines)


def main(argv):
    if len(argv) < 5:
        print(__doc__)
        return 2
    cmd, path, key, name = argv[1:5]
    if cmd == 'get':
        v = cmd_get(path, key, name)
        if v is None:
            return 1
        print(v)
        return 0
    if cmd == 'set' and len(argv) == 6:
        cmd_set(path, key, name, argv[5])
        return 0
    if cmd == 'delete':
        cmd_delete(path, key, name)
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
