#!/usr/bin/env python3
"""Inserts `this->` where GCC reports a call through a dependent base ("there are no arguments to X that depend on a
template parameter"), which MSVC's lookup accepted. Valid on both compilers. Reads port/build-linux.log (built without -w)."""
import re,collections
R=__import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__)))+'/'
sites=collections.defaultdict(set)
SKIP={'Priority','new_pri','vHEAD_NODE'} # types/enums: need Base<_T>::, done by hand
for l in open('port/build-linux.log',errors='replace'):
    m=re.match(r"(\S+?):(\d+):(\d+): (?:warning|error): (?:there are no arguments to ‘(\w+)’|‘(\w+)’ was not declared in this scope.*-Wtemplate-body)",l)
    if m and m[1].startswith(R+'Code'): 
        name=m[4] or m[5]
        if name not in SKIP: sites[m[1]].add((int(m[2]),int(m[3]),name))
n=0
for f,ss in sites.items():
    lines=open(f,newline='',encoding='latin-1').read().split('\n')
    for ln,col,name in sorted(ss,reverse=True):
        s=lines[ln-1]
        # GCC columns count a tab as one byte here
        for m2 in reversed(list(re.finditer(r'\b'+name+r'\b',s))):
            i=m2.start()
            if s[:i].endswith(('this->','::','.','->')): continue
            s=s[:i]+'this->'+s[i:]; n+=1
        lines[ln-1]=s
    open(f,'w',newline='',encoding='latin-1').write('\n'.join(lines))
print(n,'sites in',len(sites),'files')
