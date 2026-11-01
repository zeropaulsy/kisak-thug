#!/usr/bin/env python3
"""Summarise port/build-linux.log: first real error of each failed source, grouped by message."""
import re,collections,sys
R=__import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__)))+'/'
txt=open('port/build-linux.log',errors='replace').read().replace(R,'')
blocks=re.split(r'(?m)^FAILED: ',txt)[1:]
c=collections.Counter();ex={}
for b in blocks:
    m=re.search(r'^(\S+?):(\d+):\d+: (?:fatal )?error: (.*)$',b,re.M)
    if not m: c['(no compile error) '+b[:80]]+=1;continue
    k=f'{m[1]}:{m[2]}: {m[3]}'
    c[k]+=1
print(len(blocks),'failed')
for k,v in c.most_common(int(sys.argv[1]) if len(sys.argv)>1 else 60): print(v,k[:160])
