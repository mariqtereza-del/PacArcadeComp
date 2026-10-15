# dis.py: prints a range of the linear disassembly of the player's ROM (analysis helper; needs build/rom/prog_raw.asm made from your own ROM).
import re,sys
lines=open('build/rom/prog_raw.asm').read().split('\n')
idx={}
for n,l in enumerate(lines):
    m=re.search(r';([0-9a-f]{4})\t',l)
    if m: idx.setdefault(int(m.group(1),16),n)
def show(a,b):
    for k in [k for k in sorted(idx) if a<=k<b]:
        t=lines[idx[k]].split(';')[0].strip()
        t=re.sub(r'\bl([0-9a-f]{4})h\b',lambda m:'0x'+m.group(1).upper(),t); t=re.sub(r'\b0([0-9a-f]{4})h\b',lambda m:'0x'+m.group(1).upper(),t)
        print(f"{k:04X} {re.sub(chr(9)+'+',' ',t)}")
if __name__=='__main__': show(int(sys.argv[1],16),int(sys.argv[2],16))
