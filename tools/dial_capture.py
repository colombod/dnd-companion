"""Capture the actual LVGL flush pixels over the dial's USB debug port."""
import argparse
import os
from pathlib import Path
import select
import termios
import time
import tty
from PIL import Image, ImageDraw

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--command",action="append",default=[],help="Prepare a particular device screen before capture")
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    original = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        time.sleep(.25)
        termios.tcflush(fd, termios.TCIFLUSH)
        for command in args.command:
            os.write(fd,(command+"\n").encode())
            time.sleep(.15)
        os.write(fd, b"CAPTURE\n")
        raw, data = bytearray(), None
        header = b"RGB565 360 360\r\n"
        end = time.monotonic()+14
        while time.monotonic()<end:
            if not select.select([fd], [], [], .5)[0]:
                continue
            chunk = os.read(fd, 32768)
            if not chunk:
                break
            if data is None:
                raw.extend(chunk)
                at = raw.find(header)
                if at >= 0:
                    data = raw[at+len(header):]
            else:
                data.extend(chunk)
            if data is not None and len(data) >= 259200:
                break
        if data is None or len(data)<259200:
            raise SystemExit("Incomplete frame; keep the USB console closed and try again.")
        rgb = bytearray()
        for i in range(0,259200,2):
            v = data[i]<<8 | data[i+1]
            rgb.extend(((v>>11)*255//31, ((v>>5)&63)*255//63, (v&31)*255//31))
        im = Image.frombytes("RGB",(360,360),bytes(rgb))
        mask = Image.new("L", im.size)
        ImageDraw.Draw(mask).ellipse((0,0,359,359),fill=255)
        im.putalpha(mask)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        im.save(args.output)
        print("Captured", args.output)
    finally:
        termios.tcsetattr(fd,termios.TCSANOW,original)
        os.close(fd)

if __name__ == "__main__":
    main()
