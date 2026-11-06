# Analyse (BS600, Entwurf 48, AP-B0): Verzeichnis, Belegungstabelle und Dateien zweier BS600-Abbilder (flache Ausgabe von dump_sektoren.py) vergleichen.
# Aufruf: python3 -I tools/bs600/analyse_dateisystem.py <bs620.raw> <bs610.raw> [<ausgabeordner>]
# Mit Ausgabeordner werden alle Dateien als <ordner>/<620|610>_<name>.bin (genau Ladelaenge) geschrieben.
import sys,hashlib
def load(p):
    d=open(p,'rb').read()
    def sec(t,s):
        if t==0: return d[(s-1)*128:s*128]
        return d[3328+(t-1)*3840+(s-1)*256:][:256]
    ents=[]
    for s in range(8,26):
        b=sec(0,s)
        for k in range(3):
            e=b[k*42:(k+1)*42]
            ents.append((s,k,e))
        ents.append((s,'rest',b[126:128]))
    return d,sec,ents
def parse(e):
    return dict(name=e[0:6].decode('latin1'),typ=chr(e[6]),st=(e[7],e[8]),en=(e[9],e[10]),date=e[11:17].decode('latin1'),auth=e[17:20].decode('latin1'),load=int(e[20:24],16),end=int(e[24:28],16),start=int(e[28:32],16),rest=e[32:42])
res={}
for tag,p in (('620',sys.argv[1]),('610',sys.argv[2])):
    d,sec,ents=load(p)
    print('=====',tag)
    used=[];restb=set()
    allsec={}
    for s,k,e in ents:
        if k=='rest': restb.add(e); continue
        if e==b' '*42: continue
        if e.strip(b' ')==b'' : print('seltsam',s,k,e)
        f=parse(e); used.append((s,k,f))
    print('Eintraege',len(used),'Reststuecke je Sektor',restb)
    # position/length check
    occ={}
    for s,k,f in used:
        (t0,s0),(t1,s1)=f['st'],f['en']
        n=(t1-t0)*15+(s1-s0)+1
        ln=f['end']-f['load']+1
        need=(ln+255)//256
        flag='' if n==need else ' !! n=%d need=%d'%(n,need)
        # sectors
        secs=[]; t,ss=t0,s0
        for i in range(n):
            secs.append((t,ss)); ss+=1
            if ss>15: ss=1;t+=1
        for x in secs: occ.setdefault(x,[]).append(f['name'])
        f['secs']=secs; f['data']=b''.join(sec(a,b) for a,b in secs)[:ln]
        if len(sys.argv)>3: open('%s/%s_%s.bin'%(sys.argv[3],tag,f['name'].strip()),'wb').write(f['data'])
        f['hash']=hashlib.md5(f['data']).hexdigest()[:8]
        print('%d.%d %-6s %s %02d/%02d-%02d/%02d %s %s L%04X E%04X S%04X len=%5d secs=%3d%s start-in-range=%s'%(s,k,f['name'],f['typ'],t0,s0,t1,s1,f['date'],f['auth'],f['load'],f['end'],f['start'],ln,n,flag, f['load']<=f['start']<=f['end'] if f['start'] else 'start=0'), f['hash'], f['rest'].strip() or '', f['data'][-4:].hex() )
    dup=[x for x,v in occ.items() if len(v)>1]
    print('Doppelbelegung',dup)
    # bitmap
    bm=sec(0,26)
    mism=0
    for tr in range(1,40):
        w=(bm[(tr-1)*2]<<8)|bm[(tr-1)*2+1]
        bits=[(w>>(15-i))&1 for i in range(16)]
        exp=[1 if (tr,i+1) in occ else 0 for i in range(15)]+[0]
        if bits!=exp:
            mism+=1; print('Bitmap abweichend Spur',tr,'map',''.join(map(str,bits)),'exp',''.join(map(str,exp)))
    print('bitmap Abweichungen',mism,'Rest hinter Spur 39:',bm[78:].hex()[:20])
    free=sum(1 for tr in range(1,40) for i in range(1,16) if (tr,i) not in occ)
    print('freie Sektoren (Spur1-39)',free,'=',free*256,'B; belegt',len(occ))
    res[tag]={f['name']:f for s,k,f in used}
for n in sorted(set(res['620'])|set(res['610'])):
    a=res['620'].get(n); b=res['610'].get(n)
    if a and b: print(n,'gleich' if a['hash']==b['hash'] else 'VERSCHIEDEN', 'pos gleich' if (a['st'],a['en'])==(b['st'],b['en']) else 'pos verschieden')
    else: print(n,'nur', '620' if a else '610')
