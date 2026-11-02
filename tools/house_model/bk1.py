"""BK1 sprite pack reader: .san chunks + uncompressed _h.dds, as tools/zig/bk_views_pack.zig."""
import struct, re, os, numpy as np
ROOT=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','Data').replace(os.sep,'/')
def chunks(b):
    pos=0; out=[]
    while pos+2<=len(b):
        cid=b[pos]; ln=b[pos+1]; at=pos+2
        if ln&1:
            if pos+5>len(b): break
            ln=struct.unpack_from('<I',b,pos+1)[0]; at=pos+5
        ln>>=1
        if at+ln>len(b): break
        out.append((cid,b[at:at+ln])); pos=at+ln
    return out
def find(b,cid,nth=0):
    s=0
    for c,bd in chunks(b):
        if c==cid:
            if s==nth: return bd
            s+=1
    return None
def readdds(p):
    b=open(p,'rb').read(); h,w=struct.unpack_from('<II',b,12)
    a=np.frombuffer(b,dtype=np.uint8,count=w*h*4,offset=128).reshape(h,w,4)
    return a[:,:,[2,1,0,3]].copy()
def pack(dirp,name):
    b=open(os.path.join(dirp,name+'.san'),'rb').read()
    atlas=readdds(os.path.join(dirp,name+'_h.dds'))
    data=find(b,1); pk=find(data,1); lst=find(pk,1)
    res=[]
    n=sum(1 for c,_ in chunks(lst) if c==1)
    for i in range(n):
        body=find(lst,1,i); sq=find(body,1)
        cnt=struct.unpack('<i',find(sq,1))[0] if find(sq,1) else 0
        raw=find(sq,2) or b''
        sqs=np.frombuffer(raw,dtype='<f4').reshape(-1,9) if cnt else np.zeros((0,9))
        if len(sqs)==0: res.append(None); continue
        xs=sqs[:,0].astype(int); ys=sqs[:,1].astype(int); xe=(sqs[:,0]+sqs[:,2]).astype(int); ye=(sqs[:,1]+sqs[:,2]).astype(int)
        mnx,mny,mxx,mxy=xs.min(),ys.min(),xe.max(),ye.max()
        img=np.zeros((mxy-mny,mxx-mnx,4),np.uint8)
        H,W=atlas.shape[:2]
        for s in sqs:
            sx=int(s[3]*W); sy=int(s[4]*H); w=int(s[5]*W)-sx; h=int(s[6]*H)-sy
            dx=int(s[0])-mnx; dy=int(s[1])-mny
            hh=min(h,img.shape[0]-dy); ww=min(w,img.shape[1]-dx)
            img[dy:dy+hh,dx:dx+ww]=atlas[sy:sy+hh,sx:sx+ww]
        res.append(dict(img=img,ox=mnx,oy=mny))   # image pixel (0,0) is at (ox,oy) from the zero cross
    return res
def stats(dirp):
    x=open(os.path.join(dirp,'1.xml'),encoding='latin1').read()
    g=re.search(r'<passability>(.*?)</passability>',x,re.S)
    rows=[]
    if g:
        rows=[[int(r[i:i+2],16) for i in range(0,len(r),2)] for r in re.findall(r'<item>([0-9a-fA-F]+)</item>',g.group(1))]
    o=re.search(r'<origin x="([^"]*)" y="([^"]*)"',x)
    return dict(grid=rows,origin=(float(o.group(1)),float(o.group(2))) if o else (0,0))
