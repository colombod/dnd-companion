"""Bake a numbered icosahedron into small indexed LVGL frames on the build host."""
from itertools import combinations, product
from math import cos, sin, pi, sqrt
from PIL import Image, ImageDraw, ImageFont

SIDE, COUNT, SCALE = 104, 32, 3
GOLD = [(0,0,0), (30,24,28), (49,33,39), (73,39,39), (100,48,39),
        (127,59,38), (154,76,40), (180,96,44), (202,120,53), (222,147,66),
        (237,173,81), (248,198,106), (255,219,142), (255,231,170), (255,243,202), (255,252,230)]

def dot(a,b):
    return sum(x*y for x,y in zip(a,b))

def cross(a,b):
    x,y,z=a
    u,v,w=b
    return (y*w-z*v,z*u-x*w,x*v-y*u)

def unit(v):
    length=sqrt(dot(v,v))
    return tuple(n/length for n in v)

def render():
    phi=(1+sqrt(5))/2
    vertices=[unit(p) for a,b in product((-1,1),repeat=2)
              for p in [(0,a,b*phi),(a,b*phi,0),(b*phi,0,a)]]
    faces=[]
    for t in combinations(range(12),3):
        if all(dot(vertices[a],vertices[b])>0.44 for a,b in combinations(t,2)):
            normal=unit(tuple(sum(vertices[i][j] for i in t) for j in range(3)))
            faces.append((t,normal))
    assert len(faces)==20
    numbers=[0]*20
    low=1
    for i,(_,n) in enumerate(faces):
        if numbers[i]: continue
        opposite=min(range(20),key=lambda j:dot(n,faces[j][1]))
        numbers[i],numbers[opposite]=21-low,low
        low+=1
    forward=faces[0][1]
    right=unit(cross((0,1,0),forward))
    up=cross(forward,right)
    size=SIDE*SCALE
    def project(v):
        factor=size*.455/(1-v[2]*.115)
        return (size/2+factor*v[0],size/2-factor*v[1])
    font=ImageFont.load_default(size=48)
    palette=Image.new("P",(1,1))
    palette.putpalette(bytes(c for color in GOLD for c in color)*16)
    light=unit((-.45,.7,.65))
    for frame in range(COUNT):
        angle=2*pi*frame/COUNT
        tilt=.28*sin(angle)
        def rotate(p):
            x,y,z=dot(p,right),dot(p,up),dot(p,forward)
            x,z=x*cos(angle)+z*sin(angle),z*cos(angle)-x*sin(angle)
            return (x,y*cos(tilt)-z*sin(tilt),y*sin(tilt)+z*cos(tilt))
        im=Image.new("RGB",(size,size))
        draw=ImageDraw.Draw(im)
        for index,(t,n) in sorted(enumerate(faces),key=lambda f:rotate(f[1][1])[2]):
            normal=rotate(n)
            if normal[2]<.06: continue
            pts=[project(rotate(vertices[i])) for i in t]
            shade=3+round(8*max(0,dot(normal,light)))
            draw.polygon(pts,fill=GOLD[shade])
            draw.line(pts+[pts[0]],fill=GOLD[min(14,shade+3)],width=3,joint="curve")
            if normal[2]<.24: continue
            x=sum(p[0] for p in pts)/3
            y=sum(p[1] for p in pts)/3
            stamp=Image.new("L",(84,64))
            ImageDraw.Draw(stamp).text((42,32),str(numbers[index]),font=font,fill=255,anchor="mm",stroke_width=1)
            stamp=stamp.resize((int(84*(.45+.55*normal[2])),int(64*(.65+.35*normal[2]))),Image.Resampling.LANCZOS)
            stamp=stamp.rotate(-normal[0]*24,resample=Image.Resampling.BICUBIC,expand=True)
            im.paste(GOLD[15] if shade<=8 else GOLD[1],(round(x-stamp.width/2),round(y-stamp.height/2)),stamp)
        im=im.resize((SIDE,SIDE),Image.Resampling.LANCZOS)
        yield im.quantize(palette=palette,dither=Image.Dither.NONE)

def lvgl_frames():
    palette=[channel for index,(r,g,b) in enumerate(GOLD) for channel in (b,g,r,255 if index else 0)]
    for image in render():
        data=image.tobytes()
        yield "{"+",".join(map(str,palette+[(a%16)*16+b%16 for a,b in zip(data[::2],data[1::2])]))+"}"
