# Analyse (BS600, Entwurf 48): alle Sektoren einer BS600-Diskette flach ausgeben:
# Spur 0 = 26 x 128 B, Spuren 1..39 = 15 x 256 B.  Aufruf: python3 -I dump_sektoren.py <abbild> <aus.raw>  (aus dem Repo-Wurzelverzeichnis)
import ctypes,sys
L=ctypes.CDLL('build/libk1520disk.so')
L.k1520d_open_raw.restype=ctypes.c_void_p
L.k1520d_open_raw.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_bool]
L.k1520d_sector_read.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_char_p,ctypes.c_int]
h=L.k1520d_open_raw(sys.argv[1].encode(),None,True)
out=open(sys.argv[2],'wb')
for c in range(40):
    n=26 if c==0 else 15
    sz=128 if c==0 else 256
    for i in range(n):
        b=ctypes.create_string_buffer(512)
        r=L.k1520d_sector_read(h,c,0,i,b,512)
        if r!=sz: print('c',c,'i',i,'r',r)
        out.write(b.raw[:sz] if r==sz else b'\0'*sz)
