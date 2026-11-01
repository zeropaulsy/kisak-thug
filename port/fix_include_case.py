#!/usr/bin/env python3
"""Rewrite #include paths to the on-disk case with forward slashes (from hit-and-run-linux).
Windows resolves includes case-insensitively and accepts these too, so this is safe for both builds.
Search order mimics MSVC: the including file's directory, then Code/, then Code/Sk/.
System headers (no match under Code/) and console-only sources are left alone. Keeps line endings. Re-run after
merging code that adds includes."""
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
SKIP = re.compile(r'/(ngc|ngps)/', re.I)
INC = re.compile(rb'^([ \t]*#[ \t]*include[ \t]*[<"])([^>"\r\n]+)([>"])', re.M)
idx = {}
for dp, dn, fn in os.walk('Code'):
    for f in fn:
        p = os.path.join(dp, f)
        idx[p.lower()] = p
changed = fixed = 0
for p in sorted(idx.values()):
    if not p.lower().endswith(('.cpp', '.c', '.h', '.hpp', '.inl')) or SKIP.search(p.replace(os.sep, '/') + '/'):
        continue
    data = open(p, 'rb').read()
    def fix(m):
        global fixed
        inc = m.group(2).decode('latin-1').replace('\\', '/')
        for base in (os.path.dirname(p), 'Code', 'Code/Sk'):
            cand = os.path.normpath(os.path.join(base, inc))
            real = idx.get(cand.lower())
            if not real:
                continue
            # keep the path's shape (relative or rooted); correct only each component's case
            parts = real.split(os.sep)[-len(os.path.normpath(inc).split(os.sep)):] if not inc.startswith('.') else None
            new = '/'.join(parts) if parts else '/'.join(
                os.path.relpath(real, os.path.dirname(p)).split(os.sep))
            if new.encode() != m.group(2):
                fixed += 1
            return m.group(1) + new.encode('latin-1') + m.group(3)
        return m.group(0)
    out = INC.sub(fix, data)
    if out != data:
        open(p, 'wb').write(out)
        changed += 1
print(f'{fixed} includes fixed in {changed} files')
