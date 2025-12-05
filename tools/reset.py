#!/usr/bin/env python3
"""
Reset ESP32 via serial port.
Usage: python3 reset.py [port]
Default port: /dev/ttyUSB0
"""

import sys
import time

try:
    import serial
except ImportError:
    print("Erro: pyserial não instalado.")
    print("Instale com: pip install pyserial")
    sys.exit(1)


def reset_esp32(port="/dev/ttyUSB0", baud=115200):
    """Reset ESP32 using RTS pin."""
    try:
        s = serial.Serial(port, baud, timeout=1)
        s.setDTR(False)
        s.setRTS(True)
        time.sleep(0.1)
        s.setRTS(False)
        s.close()
        print(f"Reset enviado para {port}")
        return True
    except serial.SerialException as e:
        print(f"Erro: {e}")
        return False


if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
    sys.exit(0 if reset_esp32(port) else 1)
