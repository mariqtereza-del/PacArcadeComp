import re,sys
lines=open('docs/disasm/pacman.asm').read().split('\n')
rng=[(0x03dc,0x0417),(0x045f,0x051c),(0x0580,0x058e),(0x0593,0x05a5),(0x05bf,0x05e5)]  # N28; 058E-0592 is the N26 handler
rng29=[(0x051c,0x0580),(0x05a5,0x05bf)]   # N29: the demo game steps and the demo's hand-over
rng30=[(0x05f3,0x06a3),(0x06a8,0x06be)]   # N30: credit screen, PUSH START, start of a game (06A3 is the N26 hook)
rng31=[(0x0879,0x0894),(0x0899,0x0ac3)]   # N31: the level start steps of the state byte 4E04 (070E-0795 with 0814 / 083A, the level setup, is already replaced by the readable rules T02; 0894 is the N26 hook, 0AC3 is a ghost routine)
rng32=[(0x1017,0x1066)]   # N32: the gameplay frame update (runs twice per frame)
rng33=[(0x171d,0x1806)]   # N33: the meeting of Pac-Man and a ghost (tile test 171D, pixel test 1789)
rng34=[(0x1235,0x1272),(0x1277,0x1291)]   # N34: the freeze and the score sprite after a ghost is eaten (1272 is the N26 hook)
rng35=[(0x12b7,0x1376)]   # N35: Pac-Man caught: the waiting, the death animation, the life lost
rng36=[(0x08de,0x08eb)]   # N36: the test whether all dots are eaten (4E0E = F4h): the level is cleared (the states after it are the N31 ones 09D8-0A7C)
rng37=[(0x1985,0x19cd)]   # N37: Pac-Man eating the bonus fruit (the end of his move routine: tile store, fruit position test, score sprite, fruit removed)
rng38=[(0x2675,0x26d0)]   # N38: clearing the sprite positions, the start of attract mode after a game, clearing the game memory, the bonus-life digits
rng40=[(0x2108,0x210c),(0x211a,0x216a),(0x216d,0x2196),(0x2199,0x219e)]   # N40: the first intermission (the chase cartoon after level 2), states 4E06 = 0 to 6
rng41=[(0x219e,0x21a6),(0x21c2,0x21dc),(0x21df,0x2265),(0x2268,0x2280),(0x2283,0x2287),(0x228a,0x2297)]   # N41: the second intermission (states 4E07 = 0 to 0Dh)
rng42=[(0x2297,0x229b),(0x22a7,0x22d8),(0x22db,0x2303),(0x2306,0x230b)]   # N42: the third intermission (states 4E08 = 0 to 5; used after levels 9, 13 and 17)
rng43=[(0x2d0c,0x2d44),(0x2dee,0x2f02),(0x2f22,0x2f55)]   # N43: the sound effects (the effect voices driver 2D0C, the voice routine 2DEE and its tail, the volume steps)
rng44=[(0x2cc1,0x2d0c),(0x2d44,0x2d85),(0x2da5,0x2dee),(0x2f55,0x2fba)]   # N44: the tune driver (the three tune voices 2CC1, the voice routine 2D44 and its commands 2F55-2FB9)
rng45=[(0x000c,0x0010),(0x0263,0x0264),(0x0266,0x0267),(0x0792,0x0796),(0x100b,0x1017)]   # N45: the last scheduling glue in the ROM (the timed-event entries 0263 and 100B that queue tasks with RST 28h, the jump-table stubs 000C / 000D, the tail 0792)
rng45a=[(0x0ac3,0x0c42),(0x0e23,0x0e36)]   # N45a: the per-frame game update, part 1 (0AC3 ghost sprite changes, 0BD6 ghost colours, 0C0D the dot flash, 0E23 the ghost animation flag)
rng45b=[(0x0c42,0x0e23),(0x1f2e,0x1f4c),(0x1f55,0x1f73),(0x1f7c,0x1f9a)]   # N45b: the per-frame game update, part 2 (0C42 the ghost house exits and the ghost sprite steps; 1F2E, 1F55, 1F7C turn a ghost round)
rng45c=[(0x0e6c,0x0ead),(0x1ac9,0x1ad9),(0x1efe,0x1f2e),(0x1f4c,0x1f55),(0x1f73,0x1f7c)]   # N45c: the siren selection 0E6C and the ghost turn-around calls 1EFE, 1F25, 1F4C, 1F73 (the helpers 1F2E, 1F55, 1F7C are N45b)
rng45d=[(0x1094,0x1098),(0x109e,0x10a2),(0x10a8,0x10ac),(0x10b4,0x10b8),(0x10c0,0x1235)]   # N45d: the ghost state dispatch (1094, 109E, 10A8, 10B4 read the four state bytes 4DAC-4DAF and jump through the tables 1098, 10A2, 10AC, 10B8) and the ghost-house routines they reach (10C0-1234)
rng45e=[(0x1bd8,0x1c4b),(0x1caf,0x1d22),(0x1d86,0x1df9),(0x1e5d,0x1efe),(0x20d7,0x2108)]   # N45e: the ghost movement helpers each ghost mover calls (1BD8, 1CAF, 1D86, 1E5D with their timers and RST 28h task queueing), the tile test 1ED0 and the dot-count release 20D7
rng45f=[(0x1b36,0x1bd8),(0x1c4b,0x1caf),(0x1d22,0x1d86),(0x1df9,0x1e5d)]   # N45f: the four ghost speed routines (1B36, 1C4B, 1D22, 1DF9: the 32-bit speed counters 4D5A-4D79 that decide whether a ghost moves in this frame, then the mover helpers)
rng45g=[(0x1806,0x1985),(0x19cd,0x1a6a),(0x24d7,0x2675)]   # N45g: the Pac-Man mover 1806 (speed counter, joystick, turn, move, dot eaten tail) and the start-up of the game board 24D7 (a jump table entry)
rng45h=[(0x2730,0x276c),(0x2966,0x2a23),(0x2a5a,0x2aaf)]   # N45h: the Blinky target task 2730, the direction chooser 2966 with its helpers 29EA and 2A12, and 2A5A
rng45i=[(0x283b,0x2966)]   # N45i: the direction tasks of the four ghosts' frightened / demo movement (283B, 2865, 288F, 28B9, 28E3) and the direction helper 291E
rng45j=[(0x23ed,0x23ef),(0x2415,0x2487)]   # N45j: the task-table entries 23ED and 2419 (draw the maze tiles) and 2448 (place the dots) with the loop tails after the clear routines (2415)
rng45k=[(0x26d0,0x2728),(0x2c5e,0x2cc1)]   # N45k: the DIP-switch reader 26D0 and the text printer 2C5E
rng46=[(0x0008,0x000c),(0x0018,0x0020),(0x1291,0x1295),(0x238d,0x23a8)]   # N46: the main loop and the fill/table routines
rng47=[(0x0000,0x0008),(0x230b,0x238d)]   # N47: reset and the boot (the clear of the memory, interrupt mode 2, the first frames); the two prefix bytes of EI/HALT are inside
rng47b=[(0x3174,0x32F9),(0x3AF4,0x3B08)]
rng48=[(0x0010,0x0018),(0x1AD9,0x1AF8),(0x276C,0x27F1),(0x2B7E,0x2C44)]   # N48: the routines the old hooks leave to the ROM when the frame ends inside them   # N47b: the service-switch test screen (every power-on runs it before the game starts) and its text helper 3AF4
rng47a=[(0x3000,0x3154)]   # N47a: the power-on self test (ROM checksum, RAM test, clear, 'memory ok' text)
rng=rng+rng29+rng30+rng31+rng32+rng33+rng34+rng35+rng36+rng37+rng38+rng40+rng41+rng42+rng43+rng44+rng45+rng45a+rng45b+rng45c+rng45d+rng45e+rng45f+rng45g+rng45h+rng45i+rng45j+rng45k+rng46+rng47+rng47a+rng47b+rng48
# N49c: ALL the other code of the program (every instruction of the listing outside the ranges above): the original program has no ROM to fall back to any more
_covered=set()
for _l in lines:
    _m=re.match(r'\t(\S+)\s*(.*?)\s*;([0-9a-f]{4})$',_l)
    if _m and _m.group(1) not in ('defb','defw'):
        _a=int(_m.group(3),16)
        if any(lo<=_a<hi for lo,hi in rng): _covered.add(_a)
rng49=[]
for _l in lines:
    _m=re.match(r'\t(\S+)\s*(.*?)\s*;([0-9a-f]{4})$',_l)
    if not _m or _m.group(1) in ('defb','defw'): continue
    _a=int(_m.group(3),16)
    if _a in _covered: continue
    if rng49 and _a==rng49[-1][1]: rng49[-1]=(rng49[-1][0],_a+1)   # (end is exclusive; instructions are several bytes long, so ranges are joined loosely below)
    else: rng49.append((_a,_a+1))
# join: widen every range to the start of the next instruction listed (a range ends where the next listed instruction outside it begins)
_addrs=sorted({int(_m.group(3),16) for _m in (re.match(r'\t(\S+)\s*(.*?)\s*;([0-9a-f]{4})$',_l) for _l in lines) if _m and _m.group(1) not in ('defb','defw')})
def _next(a):
    k=_addrs.index(a); return _addrs[k+1] if k+1<len(_addrs) else a+1
rng49=[]
for _a in _addrs:
    if _a in _covered: continue
    n=_next(_a)
    if rng49 and rng49[-1][1]==_a: rng49[-1]=(rng49[-1][0],n)
    else: rng49.append((_a,n))
import os
if os.environ.get('N49_EXCL'):
    _ex=set(int(x) for x in open(os.environ['N49_EXCL']).read().split())
    rng49=[(a,b) for a,b in rng49]
    _keep=[]
    for _a in _addrs:
        if _a in _covered or _a in _ex: continue
        n=_next(_a)
        if _keep and _keep[-1][1]==_a: _keep[-1]=(_keep[-1][0],n)
        else: _keep.append((_a,n))
    rng49=_keep
rng=rng+rng49

N36=lambda a: any(lo<=a<hi for lo,hi in rng36)
N37=lambda a: any(lo<=a<hi for lo,hi in rng37)
N38=lambda a: any(lo<=a<hi for lo,hi in rng38)
N40=lambda a: any(lo<=a<hi for lo,hi in rng40)
N41=lambda a: any(lo<=a<hi for lo,hi in rng41)
N42=lambda a: any(lo<=a<hi for lo,hi in rng42)
N43=lambda a: any(lo<=a<hi for lo,hi in rng43)
N44=lambda a: any(lo<=a<hi for lo,hi in rng44)
N45=lambda a: any(lo<=a<hi for lo,hi in rng45)
N45a=lambda a: any(lo<=a<hi for lo,hi in rng45a)
N45b=lambda a: any(lo<=a<hi for lo,hi in rng45b)
N45c=lambda a: any(lo<=a<hi for lo,hi in rng45c)
N45d=lambda a: any(lo<=a<hi for lo,hi in rng45d)
N45e=lambda a: any(lo<=a<hi for lo,hi in rng45e)
N45f=lambda a: any(lo<=a<hi for lo,hi in rng45f)
N45g=lambda a: any(lo<=a<hi for lo,hi in rng45g)
N45h=lambda a: any(lo<=a<hi for lo,hi in rng45h)
N45i=lambda a: any(lo<=a<hi for lo,hi in rng45i)
N45j=lambda a: any(lo<=a<hi for lo,hi in rng45j)
N45k=lambda a: any(lo<=a<hi for lo,hi in rng45k)
N46=lambda a: any(lo<=a<hi for lo,hi in rng46)
N47=lambda a: any(lo<=a<hi for lo,hi in rng47)
N47a=lambda a: any(lo<=a<hi for lo,hi in rng47a)
N47b=lambda a: any(lo<=a<hi for lo,hi in rng47b)
N48=lambda a: any(lo<=a<hi for lo,hi in rng48)
N49=lambda a: any(lo<=a<hi for lo,hi in rng49)
N35=lambda a: any(lo<=a<hi for lo,hi in rng35)
N34=lambda a: any(lo<=a<hi for lo,hi in rng34)
N33=lambda a: any(lo<=a<hi for lo,hi in rng33)
def parse33(mn,o,num):
    import re
    if mn=='ld' and o.startswith('de,('): return ('LD_DE_MEM',4,num(o[4:-1]))
    if mn=='ld' and o=='b,(hl)': return ('LD_B_HLM',1,0)
    if mn=='ld' and o=='(hl),b': return ('LD_HLM_B',1,0)
    if mn=='ld' and o=='a,c': return ('LD_A_C',1,0)
    if mn=='ld' and o=='(de),a': return ('LD_DEM_A',1,0)
    if mn=='inc' and o=='de': return ('INC_DE',1,0)
    if mn=='and' and o[0] in '0123456789': return ('AND_N',2,num(o))
    if mn=='rrca': return ('RRCA',1,0)
    if mn=='and' and o=='c': return ('AND_C',1,0)
    if mn=='set' and o.endswith(',b'): return ('SET_B',2,int(o.split(',')[0]))
    if mn=='dec' and o=='b': return ('DEC_B',1,0)
    if mn=='inc' and o=='b': return ('INC_B',1,0)
    if mn=='cp' and o=='c': return ('CP_C',1,0)
    if mn=='set' and o.endswith(',(hl)'): return ('SET_HLM',2,int(o.split(',')[0]))
    m=re.fullmatch(r'\(ix\+([0-9a-f]+h)\)',o)
    if mn=='sub' and m: return ('SUB_IXD',3,num(m.group(1)))
    if mn=='jr' and o.startswith('nc,'): return ('JR_NC',2,num(o[3:]))
    if mn=='jp' and o.startswith('c,'): return ('JP_C',3,num(o[2:]))
    if mn=='jp' and o.startswith('nc,'): return ('JP_NC',3,num(o[3:]))
    return None
def parse45h(mn,o,num):
    """N45h: forms that the earlier parsers do not know (ghost targeting and direction: 16-bit arithmetic, indexed loads, pops and pushes)."""
    simple={('adc','a,(hl)'):('ADC_A_HLM',1),('add','a,(hl)'):('ADD_A_HLM',1),('cp','h'):('CP_H',1),('dec','de'):('DEC_DE',1),('dec','hl'):('DEC_HL',1),
            ('ld','a,(de)'):('LD_A_DEM',1),('ld','a,(bc)'):('LD_A_BCM',1),('ld','a,d'):('LD_A_D',1),('ld','d,a'):('LD_D_A',1),('ld','d,l'):('LD_D_L',1),('ld','e,a'):('LD_E_A',1),('ld','h,a'):('LD_H_A',1),
            ('pop','bc'):('POP_BC',1),('pop','ix'):('POP_IX',2),('pop','iy'):('POP_IY',2),('push','iy'):('PUSH_IY',2),('push','af'):('PUSH_AF',1),('rra',''):('RRA',1),('di',''):('DI',1),('ld','c,b'):('LD_C_B',1),('dec','d'):('DEC_D',1),('dec','sp'):('DEC_SP',1),('exx',''):('EXX',1),('push','bc'):('PUSH_BC',1),('push','de'):('PUSH_DE',1),('or','l'):('OR_L',1),('dec','e'):('DEC_E',1),('rlc','e'):('RLC_E',2),('ld','(hl),c'):('LD_HLM_C',1),('ld','(hl),d'):('LD_HLM_D',1),('add','a,l'):('ADD_A_L',1),('adc','a,h'):('ADC_A_H',1),('add','a,h'):('ADD_A_H',1),('ld','c,(hl)'):('LD_C_HLM',1),('or','b'):('OR_B',1),('ei',''):('EI',1),('halt',''):('HALT',1),('ld','i,a'):('LD_I_A',2),('im','2'):('IM2',2),('out','(000h),a'):('OUT0_A',2),('ld','e,(hl)'):('LD_E_HLM',1),('ld','d,(hl)'):('LD_D_HLM',1),('xor','c'):('XOR_C',1),('cpir',''):('CPIR',2),('pop','af'):('POP_AF',1)}
    if (mn,o) in simple:
        k,sz=simple[(mn,o)]; return (k,sz,0)
    if mn=='ld' and o.startswith('l,') and o[2:3] in '0123456789': return ('LD_L_N',2,num(o[2:]))
    m=re.fullmatch(r'b,\(ix\+([0-9a-f]+h)\)',o) if mn=='ld' else None
    if m: return ('LD_B_IXD',3,num(m.group(1)))
    m=re.fullmatch(r'b,\(iy\+([0-9a-f]+h)\)',o) if mn=='ld' else None
    if m: return ('LD_B_IYD',3,num(m.group(1)))
    if mn=='call' and o.startswith('c,'): return ('CALL_C',3,num(o[2:]))
    if mn=='sub' and o=='c': return ('SUB_C',1,0)
    if mn=='ld' and o.startswith('sp,') and not o.startswith('sp,('): return ('LD_SP',3,num(o[3:]))
    if mn=='bit' and o.endswith(',(hl)'): return ('BIT_HLM',2,int(o.split(',')[0]))
    if mn=='jp' and o.startswith('m,'): return ('JP_M',3,num(o[2:]))
    if mn=='ld' and o.startswith('bc,(') and o.endswith(')'): return ('LD_BC_MEM',4,num(o[4:-1]))
    if mn=='ld' and o.startswith('(') and o.endswith(',bc'): return ('LD_MEM_BC',4,num(o[1:o.index(')')]))
    if mn=='ld' and o.startswith('(') and o.endswith(',de'): return ('LD_MEM_DE',4,num(o[1:o.index(')')]))
    return None
def parse43(mn,o,num):
    """N43 (sound effects): the instruction kinds that are new here."""
    import re
    mx=re.fullmatch(r'\(ix\+([0-9a-f]+h)\)',o)
    if mn=='ld':
        d,sx=o.split(',',1)
        if d=='e' and sx=='b': return ('LD_E_B',1,0)
        if d=='e' and not sx.startswith('('): return ('LD_E_N',2,num(sx))
        if d=='h' and not sx.startswith('('): return ('LD_H_N',2,num(sx))
        if d=='a' and sx in ('e','h','l'): return ('LD_A_'+sx.upper(),1,0)
        if d=='l' and sx=='a': return ('LD_L_A',1,0)
        m=re.fullmatch(r'\(ix\+([0-9a-f]+h)\)',d)
        if m and sx=='b': return ('LD_IXD_B',3,num(m.group(1)))
        m=re.fullmatch(r'\(iy\+([0-9a-f]+h)\)',d)
        if m and sx in ('a','h','l'): return ('LD_IYD_'+sx.upper(),3,num(m.group(1)))
    if mn=='add' and o.startswith('a,') and re.fullmatch(r'a,\(ix\+([0-9a-f]+h)\)',o): return ('ADD_A_IXD',3,num(o[6:-1]))
    if mn=='and' and mx: return ('AND_IXD',3,num(mx.group(1)))
    if mn=='or' and mx: return ('OR_IXD',3,num(mx.group(1)))
    if mn=='and' and o=='e': return ('AND_E',1,0)
    if mn=='dec' and mx: return ('DEC_IXD',3,num(mx.group(1)))
    m=re.fullmatch(r'([0-7]),\(ix\+([0-9a-f]+h)\)',o)
    if mn in('bit','set','res') and m: return (mn.upper()+'_IXD',4,(num(m.group(2))<<8)|int(m.group(1)))
    if mn=='cp' and mx: return ('CP_IXD',3,num(mx.group(1)))
    my=re.fullmatch(r'\(iy\+([0-9a-f]+h)\)',o)
    if mn=='cp' and my: return ('CP_IYD',3,num(my.group(1)))
    if mn=='dec' and my: return ('DEC_IYD',3,num(my.group(1)))
    if mn=='res' and o.endswith(',(hl)'): return ('RES_HLM',2,int(o.split(',')[0]))
    if mn=='call' and o.startswith('z,'): return ('CALL_Z',3,num(o[2:]))
    if mn=='ret' and o=='nc': return ('RET_NC',1,0)
    if mn=='ret' and o=='c': return ('RET_C',1,0)
    if mn=='ret' and o=='m': return ('RET_M',1,0)
    if mn=='cpl': return ('CPL',1,0)
    if mn=='adc' and o=='hl,hl': return ('ADC_HL_HL',2,0)
    if mn=='scf': return ('SCF',1,0)
    if mn=='neg': return ('NEG',2,0)
    if mn=='srl' and o=='e': return ('SRL_E',2,0)
    if mn=='srl' and o=='a': return ('SRL_A',2,0)
    if mn=='srl' and o=='c': return ('SRL_C',2,0)
    if mn=='dec' and o=='c': return ('DEC_C',1,0)
    if mn=='dec' and o=='h': return ('DEC_H',1,0)
    if mn=='inc' and o=='h': return ('INC_H',1,0)
    if mn=='ld' and o=='e,b': return ('LD_E_B',1,0)
    if mn=='inc' and mx: return ('INC_IXD',3,num(mx.group(1)))
    if mn=='rlca': return ('RLCA',1,0)
    if mn=='add' and o=='hl,hl': return ('ADD_HL_HL',1,0)
    if mn=='push' and o=='hl': return ('PUSH_HL',1,0)
    if mn=='push' and o=='ix': return ('PUSH_IX',2,0)
    if mn=='pop' and o in('de','hl'): return ('POP_'+o.upper(),1,0)
    return None
N32=lambda a: any(lo<=a<hi for lo,hi in rng32)
N31=lambda a: any(lo<=a<hi for lo,hi in rng31)
N29=lambda a: any(lo<=a<hi for lo,hi in rng29)
N30=lambda a: any(lo<=a<hi for lo,hi in rng30)
def num(s,labels=None):
    s=s.strip()
    m=re.fullmatch(r'0?([0-9a-f]+)h',s)
    if m: return int(m.group(1),16)
    m=re.fullmatch(r'(?:l|sub_)([0-9a-f]{4})h(?:\+(\d+))?',s)
    if m: return int(m.group(1),16)+(int(m.group(2)) if m.group(2) else 0)
    raise ValueError(s)

def regname(x): return x in ('a','b','c','d','e','h','l')
def parse31(mn,o,num):
    """Instruction kinds for N31: returns (op,size,arg) or None."""
    import re
    m=re.fullmatch(r'\((i[xy])\+([0-9a-f]+h)\)',o.split(',')[0]) if mn!='ld' else None
    if mn=='ld':
        d,sx=o.split(',',1)
        mi=re.fullmatch(r'\((i[xy])\+([0-9a-f]+h)\)',sx); md=re.fullmatch(r'\((i[xy])\+([0-9a-f]+h)\)',d)
        if mi and regname(d): return ('LD_%s_%sD'%(d.upper(),mi.group(1).upper()),3,num(mi.group(2)))
        if md and regname(sx): return ('LD_%sD_%s'%(md.group(1).upper(),sx.upper()),3,num(md.group(2)))
        if md: return ('LD_%sD_N'%md.group(1).upper(),4,(num(md.group(2))<<8)|num(sx))
        if d in('ix','iy') : return ('LD_%s'%d.upper(),4,num(sx))
        if d=='hl' and sx.startswith('('): return ('LD_HL_MEM',3,num(sx[1:-1]))
        if d=='a' and sx=='(hl)': return ('LD_A_HLM',1,0)
        if regname(d) and regname(sx) and (d,sx) in (('a','b'),('b','a'),('c','a'),('e','a')): return ('LD_%s_%s'%(d.upper(),sx.upper()),1,0)
        if d=='d' and not sx.startswith('('): return ('LD_D',2,num(sx))
    if mn=='ldir': return ('LDIR',2,0)
    if mn=='ldi': return ('LDI',2,0)
    if mn=='adc' and o.startswith('a,'): return ('ADC_A_N',2,num(o[2:]))
    if mn=='add' and o=='a,a': return ('ADD_A_A',1,0)
    if mn=='add' and o=='iy,de': return ('ADD_IY_DE',2,0)
    if mn=='and' and o=='(hl)': return ('AND_HLM',1,0)
    if mn=='and' and o=='b': return ('AND_B',1,0)
    if mn=='cp' and o=='(hl)': return ('CP_HLM',1,0)
    if mn=='dec' and o=='a': return ('DEC_A',1,0)
    if mn=='ex' and o=='de,hl': return ('EX_DE_HL',1,0)
    if mn=='inc' and o=='hl': return ('INC_HL',1,0)
    if mn=='inc' and o in('ix','iy'): return ('INC_'+o.upper(),2,0)
    if mn=='jp' and o.startswith('nz,'): return ('JP_NZ',3,num(o[3:]))
    if mn=='jr' and o.startswith('c,'): return ('JR_C',2,num(o[2:]))
    if mn=='jr' and ',' not in o: return ('JR',2,num(o))
    if mn=='or' and o=='c': return ('OR_C',1,0)
    if mn=='sbc' and o=='hl,de': return ('SBC_HL_DE',2,0)
    if mn=='sub' and o=='(hl)': return ('SUB_HLM',1,0)
    if mn=='sub' and regname(o)==False and not o.startswith('('): return ('SUB_N',2,num(o))
    if mn=='set' and o.endswith(',a'): return ('SET_A',2,int(o.split(',')[0]))
    if mn=='rst' and o=='8': return ('RST',1,8)
    return None
def parse49(mn,o,num):
    """N49c: the last instruction forms (everything else of the program)"""
    simple={('jp','(hl)'):('JP_HLM',1),('inc','e'):('INC_E',1),('inc','c'):('INC_C',1),('inc','d'):('INC_D',1),('rla',''):('RLA',1),('rlc','b'):('RLC_B',2),('rl','b'):('RL_B',2),('sla','h'):('SLA_H',2),('add','a,e'):('ADD_A_E',1),('add','a,d'):('ADD_A_D',1),('ccf',''):('CCF',1),
        ('ld','c,h'):('LD_C_H',1),('ld','d,h'):('LD_D_H',1),('ld','e,l'):('LD_E_L',1),('ld','l,e'):('LD_L_E',1),('ld','h,d'):('LD_H_D',1),('ld','b,h'):('LD_B_H',1),('ld','(bc),a'):('LD_BCM_A',1)}
    if (mn,o) in simple:
        op,sz=simple[(mn,o)]; return (op,sz,0)
    if mn=='call' and o.startswith('nz,'): return ('CALL_NZ',3,num(o[3:]))
    if mn=='bit' and o.endswith(',d'): return ('BIT_D',2,int(o.split(',')[0]))
    if mn=='set' and o in ('7,e','6,e'): return ('SET_E',2,int(o[0]))
    m=re.fullmatch(r'\(ix\+([0-9a-f]+h)\)',o) if mn=='rl' else None
    if m: return ('RL_IXD',4,num(m.group(1)))
    return None
ins=[]
for l in lines:
    m=re.match(r'\t(\S+)\s*(.*?)\s*;([0-9a-f]{4})$',l)
    if not m: continue
    a=int(m.group(3),16)
    if not any(lo<=a<hi for lo,hi in rng): continue
    mn,opr=m.group(1),m.group(2)
    if mn=='defb' or mn=='defw': continue
    ins.append((a,mn,opr))
out=[]
for a,mn,opr in ins:
    CUR=(a,mn,opr)
    o=opr.replace(' ','')
    def add(op,size,arg=0): out.append((a,op,size,arg))
    if N31(a) or N32(a) or N33(a) or N34(a) or N35(a) or N37(a) or N38(a) or N40(a) or N41(a) or N42(a) or N43(a) or N44(a) or N45(a) or N45a(a) or N45b(a) or N45c(a) or N45d(a) or N45e(a) or N45f(a) or N45g(a) or N45h(a) or N45i(a) or N45j(a) or (N45k(a) or (N46(a) or (N47(a) or N47a(a) or N47b(a) or N48(a) or N49(a)))):
        r=(parse49(mn,o,num) if N49(a) else None) or (parse45h(mn,o,num) if (N45h(a) or N45i(a) or N45j(a) or (N45k(a) or (N46(a) or (N47(a) or N47a(a) or N47b(a) or N48(a) or N49(a)))))  else None) or (parse43(mn,o,num) if (N43(a) or N44(a) or N45a(a) or N45b(a) or N45c(a) or N45d(a) or N45e(a) or N45f(a) or N45g(a) or N45h(a) or N45i(a) or N45j(a) or (N45k(a) or (N46(a) or (N47(a) or N47a(a) or N47b(a) or N48(a) or N49(a)))))  else None) or (parse33(mn,o,num) if (N33(a) or N34(a) or N35(a) or N37(a) or N38(a) or N40(a) or N41(a) or N42(a) or N43(a) or N44(a) or N45(a) or N45a(a) or N45b(a) or N45c(a) or N45d(a) or N45e(a) or N45f(a) or N45g(a) or N45h(a) or N45i(a) or N45j(a) or (N45k(a) or (N46(a) or (N47(a) or N47a(a) or N47b(a) or N48(a) or N49(a)))))  else None) or parse31(mn,o,num)
        if r: add(*r); continue
    if mn=='ld':
        d,s=o.split(',',1)
        if d=='c' and s=='a': add('LD_C_A',1)
        elif d=='b' and s=='a': add('LD_B_A',1)
        elif d=='c': add('LD_C',2,num(s))
        elif d=='b': add('LD_B',2,num(s))
        elif d=='a' and s.startswith('('): add('LD_A_MEM',3,num(s[1:-1]))
        elif d=='a': add('LD_A',2,num(s))
        elif d=='hl': add('LD_HL',3,num(s))
        elif d=='de': add('LD_DE',3,num(s))
        elif d=='bc': add('LD_BC',3,num(s))
        elif d=='ix': add('LD_IX',4,num(s))
        elif d.startswith('(ix+'): add('LD_IXD_A',3,num(d[4:-1]))
        elif d=='(hl)' and s=='a': add('LD_HLM_A',1)
        elif d=='(hl)': add('LD_HLM_N',2,num(s))
        elif s=='hl' and d.startswith('('): add('LD_MEM_HL',3,num(d[1:-1]))
        elif d.startswith('(0') or (d.startswith('(') and d.endswith('h)')): add('LD_MEM_A',3,num(d[1:-1]))
        else: raise SystemExit('ld '+o)
    elif mn=='inc': add({'(hl)':'INC_HLM','l':'INC_L','a':'INC_A','bc':'INC_BC'}[o],1)
    elif mn=='dec': add({'(hl)':'DEC_HLM','l':'DEC_L'}[o],1)
    elif mn=='daa': add('DAA',1)
    elif mn=='bit': add('BIT_A',2,int(o.split(',')[0]))
    elif mn=='xor' and o=='a': add('XOR_A',1)
    elif mn=='xor': add('XOR_N',2,num(o))
    elif mn=='sub' and o=='b': add('SUB_B',1)
    elif mn=='cp': add('CP_N',2,num(o))
    elif mn=='and' and o=='a': add('AND_A',1)
    elif mn=='add' and o.startswith('a,') and o not in ('a,b','a,c'): add('ADD_A_N',2,num(o[2:]))
    elif mn=='add':
        add({'a,b':'ADD_A_B','a,c':'ADD_A_C','hl,bc':'ADD_HL_BC','hl,de':'ADD_HL_DE','ix,de':'ADD_IX_DE'}[o],{'a,b':1,'a,c':1,'hl,bc':1,'hl,de':1,'ix,de':2}[o])
    elif mn=='sbc' and o=='hl,bc': add('SBC_HL_BC',2)
    elif mn=='jr': add({'z':'JR_Z','nz':'JR_NZ'}[o.split(',')[0]],2,num(o.split(',')[1]))
    elif mn=='djnz': add('DJNZ',2,num(o))
    elif mn=='call': add('CALL',3,num(o))
    elif mn=='jp' and o.startswith('z,'): add('JP_Z',3,num(o[2:]))
    elif mn=='jp': add('JP',3,num(o))
    elif mn=='ret' and o=='z': add('RET_Z',1)
    elif mn=='ret' and o=='nz': add('RET_NZ',1)
    elif mn=='ret' and o=='': add('RET',1)
    elif mn=='rst': add('RST',1,num(o))
    else: raise SystemExit('unknown '+mn+' '+o)
# checks: sizes consecutive (except across inline data after rst 28/30 and gaps)
for i in range(len(out)-1):
    a,op,sz,arg=out[i]; na=out[i+1][0]
    exp=a+sz
    if op=='RST' and arg==0x28: exp=a+3
    if op=='RST' and arg==0x30: exp=a+4
    if op=='RST' and arg==0x20: continue
    if op in ('RST',) and arg==0x18: pass
    if op in ('RET','JP'): continue
    if na!=exp: print("GAP after %04X (%s): next %04X expected %04X"%(a,op,na,exp),file=sys.stderr)
# starts: entry points
text=open('docs/notes/rom_routines.md').read()
mapentries={int(m.group(1),16) for m in re.finditer(r'^\| ([0-9A-F]{4}) \|',text,re.M)}
addrs={x[0] for x in out}
starts=set()
for lo,hi in rng: starts.add(lo)
for a,op,sz,arg in out:
    if op=='CALL' and arg in addrs: starts.add(arg)
    if op=='JP' and arg in addrs: starts.add(arg)
    if op in ('JP_Z','JP_NZ','JP_C','JP_NC','JP_M') and arg in addrs and (N32(arg) or N33(arg) or N36(arg) or N38(arg) or N40(arg) or N41(arg) or N42(arg) or N43(arg) or N44(arg) or N45(arg) or N45a(arg) or N45b(arg) or N45c(arg) or N45d(arg) or N45e(arg) or N45f(arg) or N45g(arg) or N45h(arg) or N45i(arg) or N45j(arg) or (N45k(arg) or (N46(arg) or (N47(arg) or N47a(arg))))): starts.add(arg)
    if op in ('CALL_Z','CALL_NZ') and arg in addrs: starts.add(arg)
    if op in ('CALL','CALL_Z','CALL_NZ'): starts.add(a+3)
    if op=='RST' and arg==0x28: starts.add(a+3)
    if op=='RST' and arg==0x30: starts.add(a+4)
    if op=='RST' and arg in (0x08,0x10,0x18): starts.add(a+1)
for e in (0x234b,0x245c,0x2df4,0x2ee4,0x2ee8,0x2d6c,0x10c0,0x10d2,0x1118,0x112a,0x115c,0x116e,0x118f,0x11c9,0x11db,0x11fc,0x1ae8): starts.add(e)   # N43: the tune routine (N44) jumps into the effect code here
for e in mapentries:
    if e in addrs: starts.add(e)
jrt={arg for a,op,sz,arg in out if op in ('JR_Z','JR_NZ','DJNZ')}
print("jr targets also starts:",[hex(x) for x in sorted(jrt&starts)],file=sys.stderr)
# (a JR target that is also reached otherwise stays a start: the runner stops when a jump lands on a start address)
# a start must be an instruction address
starts&=addrs
print("instructions %d, starts %d"%(len(out),len(starts)),file=sys.stderr)
h=open('src/game/attract_prog.h','w')
h.write("// GENERATED by tools/gen_attract.py from the traced code listing (docs/disasm, private): the instruction list of the attract-mode routines of N28 as C++ data\n// (address, kind, size, operand): the rewrite of the code, no ROM bytes. Used by src/game/attract.h.\n#pragma once\n#include <cstdint>\nnamespace game {\nnamespace att {\n// S(): an entry address (written like this so that a list of entry addresses is never mistaken for a copy of a ROM jump table by the export scan)\nconstexpr uint16_t S(uint16_t a) { return a; }\nenum Op : uint8_t { LD_C, LD_B, LD_A, LD_HL, LD_DE, LD_BC, LD_IX, LD_A_MEM, LD_MEM_A, INC_HLM, DEC_HLM, LD_HLM_N, LD_HLM_A, XOR_A, AND_A, JR_Z, INC_L, DEC_L, ADD_HL_BC, ADD_HL_DE, SBC_HL_BC, ADD_IX_DE, LD_IXD_A, ADD_A_C, LD_C_A, DJNZ, CALL, RET, JP, RST, LD_B_A, LD_MEM_HL, XOR_N, SUB_B, CP_N, ADD_A_B, JR_NZ, JP_Z, RET_Z, DAA, BIT_A, INC_A, ADD_A_N, RET_NZ, LD_D_IXD, LD_E_IXD, LD_L_IXD, LD_E_IYD, LD_H_IYD, LD_L_IYD, LD_IXD_E, LD_IYD_D, LD_IXD_N, LD_IY, LD_HL_MEM, LD_A_HLM, LD_A_B, LD_E_A, LD_D, LDIR, ADC_A_N, ADD_A_A, ADD_IY_DE, AND_HLM, AND_B, CP_HLM, DEC_A, EX_DE_HL, INC_HL, INC_IX, INC_IY, JP_NZ, JR_C, JR, OR_C, SBC_HL_DE, SUB_HLM, SUB_N, SET_A, LD_A_IXD, LD_DE_MEM, LD_B_HLM, DEC_B, INC_B, CP_C, SET_HLM, SUB_IXD, JR_NC, JP_C, LD_HLM_B, AND_C, SET_B, LD_A_C, LD_DEM_A, INC_DE, AND_N, RRCA, LD_IYD_N, LD_E_N, LD_H_N, LD_A_E, LD_A_H, LD_A_L, LD_L_A, LD_IXD_B, LD_IYD_A, LD_IYD_H, LD_IYD_L, ADD_A_IXD, AND_IXD, AND_E, DEC_IXD, BIT_IXD, SET_IXD, RES_IXD, NEG, SRL_E, RLCA, ADD_HL_HL, PUSH_HL, PUSH_IX, POP_DE, POP_HL, CPL, LD_H_IXD, LD_C_IXD, LD_IXD_L, LD_IXD_H, LD_IXD_C, CP_IXD, CP_IYD, RES_HLM, DEC_IYD, CALL_Z, RET_NC, OR_IXD, JP_NC, RET_C, SCF, ADC_HL_HL, SRL_A, SRL_C, DEC_C, DEC_H, INC_H, LD_E_B, INC_IXD, ADC_A_HLM, ADD_A_HLM, CP_H, DEC_DE, DEC_HL, LD_A_DEM, LD_A_D, LD_D_A, LD_D_L, LD_H_A, POP_BC, POP_IX, POP_IY, PUSH_IY, LD_L_N, LD_B_IXD, LD_B_IYD, CALL_C, LD_MEM_DE, SUB_C, LD_BC_MEM, JP_M, LD_A_BCM, INC_BC, PUSH_AF, POP_AF, LDI, RRA, CPIR, BIT_HLM, XOR_C, LD_E_HLM, LD_D_HLM, DI, EI, HALT, LD_I_A, IM2, OUT0_A, LD_SP, LD_C_B, DEC_D, DEC_SP, EXX, PUSH_BC, OR_B, LD_MEM_BC, PUSH_DE, OR_L, DEC_E, RLC_E, LD_HLM_C, LD_HLM_D, ADD_A_L, ADC_A_H, BIT_D, LD_A_IYD, LD_IXD_D, JP_HLM, INC_E, INC_C, INC_D, RLA, RLC_B, RL_B, SLA_H, ADD_A_E, ADD_A_D, CCF, LD_C_H, LD_D_H, LD_E_L, LD_L_E, LD_H_D, LD_B_H, LD_BCM_A, CALL_NZ, SET_E, RL_IXD, RET_M, ADD_A_H, LD_C_HLM };\nstruct Ins { uint16_t addr; Op op; uint8_t size; uint16_t arg; };\n")
h.write("constexpr Ins kProg[] = {\n")
for a,op,sz,arg in out: h.write("    {0x%04X, %s, %d, 0x%04X},\n"%(a,op,sz,arg))
h.write("};\nconstexpr int kProgN = %d;\n"%len(out))
s28=[x for x in sorted(starts) if not N29(x) and not N30(x) and not N31(x) and not N32(x) and not N33(x) and not N34(x) and not N35(x) and not N36(x) and not N37(x) and not N38(x) and not N40(x) and not N41(x) and not N42(x) and not N43(x) and not N44(x) and not N45(x) and not N45a(x) and not N45b(x) and not N45c(x) and not N45d(x) and not N45e(x) and not N45f(x) and not N45g(x) and not N45h(x) and not N45i(x) and not N45j(x) and not N45k(x) and not N46(x) and not N47(x) and not N47a(x) and not N47b(x) and not N48(x) and not N49(x)]; s29=[x for x in sorted(starts) if N29(x)]; s30=[x for x in sorted(starts) if N30(x)]; s31=[x for x in sorted(starts) if N31(x) and not N36(x) and not N37(x) and not N38(x) and not N40(x) and not N41(x) and not N42(x) and not N43(x) and not N44(x) and not N45(x) and not N45a(x) and not N45b(x) and not N45c(x) and not N45d(x) and not N45e(x) and not N45f(x) and not N45g(x) and not N45h(x) and not N45i(x) and not N45j(x) and not N45k(x) and not N46(x) and not N47(x) and not N47a(x) and not N47b(x) and not N48(x) and not N49(x)]; s32=[x for x in sorted(starts) if N32(x)]; s33=[x for x in sorted(starts) if N33(x)]; s34=[x for x in sorted(starts) if N34(x)]; s35=[x for x in sorted(starts) if N35(x)]; s36=[x for x in sorted(starts) if N36(x)]; s37=[x for x in sorted(starts) if N37(x)]; s38=[x for x in sorted(starts) if N38(x)]; s40=[x for x in sorted(starts) if N40(x)]; s41=[x for x in sorted(starts) if N41(x)]; s42=[x for x in sorted(starts) if N42(x)]; s43=[x for x in sorted(starts) if N43(x)]; s44=[x for x in sorted(starts) if N44(x)]; s45=[x for x in sorted(starts) if N45(x)]; s45a=[x for x in sorted(starts) if N45a(x)]; s45b=[x for x in sorted(starts) if N45b(x)]; s45c=[x for x in sorted(starts) if N45c(x)]; s45d=[x for x in sorted(starts) if N45d(x)]; s45e=[x for x in sorted(starts) if N45e(x)]; s45f=[x for x in sorted(starts) if N45f(x)]; s45g=[x for x in sorted(starts) if N45g(x)]; s45h=[x for x in sorted(starts) if N45h(x)]; s45i=[x for x in sorted(starts) if N45i(x)]; s45j=[x for x in sorted(starts) if N45j(x)]; s45k=[x for x in sorted(starts) if N45k(x)]
s46=[x for x in sorted(starts) if N46(x)]
s47=[x for x in sorted(starts) if N47(x)]
s47a=[x for x in sorted(starts) if N47a(x)]
s47b=[x for x in sorted(starts) if N47b(x)]
s48=[x for x in sorted(starts) if N48(x)]
s49=[x for x in sorted(starts) if N49(x)]
# the lists are written in descending order: in ascending order the list of one step is byte for byte a run of the ROM's own jump table (the scan of the test package would flag it)
s28,s29,s30,s31,s32,s33,s34,s35,s36,s37,s38,s40,s41,s42,s43,s44,s45,s45a,s45b,s45c,s45d,s45e,s45f,s45g,s45h,s45i,s45j,s45k,s46,s47,s47a,s47b,s48,s49=[l[::-1] for l in (s28,s29,s30,s31,s32,s33,s34,s35,s36,s37,s38,s40,s41,s42,s43,s44,s45,s45a,s45b,s45c,s45d,s45e,s45f,s45g,s45h,s45i,s45j,s45k,s46,s47,s47a,s47b,s48,s49)]
h.write("// the addresses where the ROM can start (or resume) running this code: the hooks (N28: the introduction; N29: the demo game steps)\nconstexpr uint16_t kStarts[] = {%s};\nconstexpr int kStartsN = %d;\nconstexpr uint16_t kStartsDemo[] = {%s};\nconstexpr int kStartsDemoN = %d;\nconstexpr uint16_t kStartsGame[] = {%s};\nconstexpr int kStartsGameN = %d;\nconstexpr uint16_t kStartsLevel[] = {%s};\nconstexpr int kStartsLevelN = %d;\nconstexpr uint16_t kStartsFrame[] = {%s};\nconstexpr int kStartsFrameN = %d;\nconstexpr uint16_t kStartsMeet[] = {%s};\nconstexpr int kStartsMeetN = %d;\nconstexpr uint16_t kStartsEaten[] = {%s};\nconstexpr int kStartsEatenN = %d;\nconstexpr uint16_t kStartsDeath[] = {%s};\nconstexpr int kStartsDeathN = %d;\nconstexpr uint16_t kStartsClear[] = {%s};\nconstexpr int kStartsClearN = %d;\nconstexpr uint16_t kStartsFruit[] = {%s};\nconstexpr int kStartsFruitN = %d;\nconstexpr uint16_t kStartsOver[] = {%s};\nconstexpr int kStartsOverN = %d;\nconstexpr uint16_t kStartsInter1[] = {%s};\nconstexpr int kStartsInter1N = %d;\nconstexpr uint16_t kStartsInter2[] = {%s};\nconstexpr int kStartsInter2N = %d;\nconstexpr uint16_t kStartsInter3[] = {%s};\nconstexpr int kStartsInter3N = %d;\nconstexpr uint16_t kStartsSnd[] = {%s};\nconstexpr int kStartsSndN = %d;\nconstexpr uint16_t kStartsTune[] = {%s};\nconstexpr int kStartsTuneN = %d;\nconstexpr uint16_t kStartsSched[] = {%s};\nconstexpr int kStartsSchedN = %d;\nconstexpr uint16_t kStarts45a[] = {%s};\nconstexpr int kStarts45aN = %d;\nconstexpr uint16_t kStarts45b[] = {%s};\nconstexpr int kStarts45bN = %d;\nconstexpr uint16_t kStarts45c[] = {%s};\nconstexpr int kStarts45cN = %d;\nconstexpr uint16_t kStarts45d[] = {%s};\nconstexpr int kStarts45dN = %d;\nconstexpr uint16_t kStarts45e[] = {%s};\nconstexpr int kStarts45eN = %d;\nconstexpr uint16_t kStarts45f[] = {%s};\nconstexpr int kStarts45fN = %d;\nconstexpr uint16_t kStarts45g[] = {%s};\nconstexpr int kStarts45gN = %d;\nconstexpr uint16_t kStarts45h[] = {%s};\nconstexpr int kStarts45hN = %d;\nconstexpr uint16_t kStarts45i[] = {%s};\nconstexpr int kStarts45iN = %d;\nconstexpr uint16_t kStarts45j[] = {%s};\nconstexpr int kStarts45jN = %d;\nconstexpr uint16_t kStarts45k[] = {%s};\nconstexpr int kStarts45kN = %d;\nconstexpr uint16_t kStarts46[] = {%s};\nconstexpr int kStarts46N = %d;\nconstexpr uint16_t kStarts47[] = {%s};\nconstexpr int kStarts47N = %d;\nconstexpr uint16_t kStarts47a[] = {%s};\nconstexpr int kStarts47aN = %d;\nconstexpr uint16_t kStarts47b[] = {%s};\nconstexpr int kStarts47bN = %d;\nconstexpr uint16_t kStarts48[] = {%s};\nconstexpr int kStarts48N = %d;\nconstexpr uint16_t kStarts49[] = {%s};\nconstexpr int kStarts49N = %d;\n"%(", ".join("S(0x%04X)"%x for x in s28),len(s28),", ".join("S(0x%04X)"%x for x in s29),len(s29),", ".join("S(0x%04X)"%x for x in s30),len(s30),", ".join("S(0x%04X)"%x for x in s31),len(s31),", ".join("S(0x%04X)"%x for x in s32),len(s32),", ".join("S(0x%04X)"%x for x in s33),len(s33),", ".join("S(0x%04X)"%x for x in s34),len(s34),", ".join("S(0x%04X)"%x for x in s35),len(s35),", ".join("S(0x%04X)"%x for x in s36),len(s36),", ".join("S(0x%04X)"%x for x in s37),len(s37),", ".join("S(0x%04X)"%x for x in s38),len(s38),", ".join("S(0x%04X)"%x for x in s40),len(s40),", ".join("S(0x%04X)"%x for x in s41),len(s41),", ".join("S(0x%04X)"%x for x in s42),len(s42),", ".join("S(0x%04X)"%x for x in s43),len(s43),", ".join("S(0x%04X)"%x for x in s44),len(s44),", ".join("S(0x%04X)"%x for x in s45),len(s45),", ".join("S(0x%04X)"%x for x in s45a),len(s45a),", ".join("S(0x%04X)"%x for x in s45b),len(s45b),", ".join("S(0x%04X)"%x for x in s45c),len(s45c),", ".join("S(0x%04X)"%x for x in s45d),len(s45d),", ".join("S(0x%04X)"%x for x in s45e),len(s45e),", ".join("S(0x%04X)"%x for x in s45f),len(s45f),", ".join("S(0x%04X)"%x for x in s45g),len(s45g),", ".join("S(0x%04X)"%x for x in s45h),len(s45h),", ".join("S(0x%04X)"%x for x in s45i),len(s45i),", ".join("S(0x%04X)"%x for x in s45j),len(s45j),", ".join("S(0x%04X)"%x for x in s45k),len(s45k),", ".join("S(0x%04X)"%x for x in s46),len(s46),", ".join("S(0x%04X)"%x for x in s47),len(s47),", ".join("S(0x%04X)"%x for x in s47a),len(s47a),", ".join("S(0x%04X)"%x for x in s47b),len(s47b),", ".join("S(0x%04X)"%x for x in s48),len(s48),", ".join("S(0x%04X)"%x for x in s49),len(s49)))
pre=sorted(set(a+1 for a,mn,opr in ins if re.search(r'(^|[,(])(ix|iy)',opr.replace(' ',''))),reverse=True)
h.write("// N48: the address right behind the prefix byte (DD / FD) of every indexed instruction in the list: a frame end can stop the core there (the prefix is a step of its own); the native code finishes the instruction from there\nconstexpr uint16_t kPrefixMid[] = {%s};\nconstexpr int kPrefixMidN = %d;\n"%(", ".join("S(0x%04X)"%x for x in pre),len(pre)))
h.write("} // namespace att\n} // namespace game\n")
