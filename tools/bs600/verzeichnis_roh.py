# Analyse (BS600, Entwurf 48): Verzeichnis (Spur 0, Sektoren 8..25, 3 Eintraege zu 42 B) aus der flachen Ausgabe von dump_sektoren.py.
# VORLAEUFIG: Eintragslage nur geraten; der Aufruf druckt die Rohfelder.
import sys
d=open(sys.argv[1],'rb').read()
def sec(t,s):  # t 0-based cyl, s 1-based
    if t==0: return d[(s-1)*128:s*128]
    return d[3328+(t-1)*3840+(s-1)*256:][:256]
n=0
for s in range(8,26):
    for k in range(0,126,42):
        e=sec(0,s)[k:k+42]
        if e[:1] in (b'\0',b'\xff') or e.strip(b'\0 ')==b'': continue
        print(s,k//42,e[:6],e[6:7],list(e[7:11]),e[11:17],e[17:20],e[20:32],e[32:].decode("latin1"))
