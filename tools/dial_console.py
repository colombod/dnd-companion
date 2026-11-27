"""Send picker diagnostics on macOS USB serial and observe physical input events."""
import argparse
import os
import select
import termios
import time
import tty

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--watch", type=float, default=3)
    ap.add_argument("commands", nargs="*", help='STATUS, LIST, PICK, OPEN <id>, REFRESH or HAPTIC')
    args = ap.parse_args()
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        flags = termios.tcgetattr(fd)
        flags[2] |= termios.CLOCAL
        flags[2] &= ~termios.HUPCL
        flags[4] = flags[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, flags)
        time.sleep(0.25)
        for cmd in args.commands:
            os.write(fd, (cmd + "\n").encode())
            time.sleep(0.12)
        end = time.monotonic() + args.watch
        while time.monotonic() < end:
            if select.select([fd], [], [], max(0, end-time.monotonic()))[0]:
                data = os.read(fd, 4096)
                if not data:
                    break
                print(data.decode(errors="replace"), end="", flush=True)
    finally:
        try:
            termios.tcsetattr(fd, termios.TCSANOW, old)
        finally:
            os.close(fd)

if __name__ == "__main__":
    main()
