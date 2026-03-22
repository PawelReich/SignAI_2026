import serial
import struct
import sys
# --- CONFIGURATION ---
# Windows: 'COM3', 'COM4', etc.
# Mac/Linux: '/dev/ttyUSB0' or '/dev/ttyACM0'
PORT = sys.argv[1] 
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD)
    print(f"Listening on {PORT}...")
except Exception as e:
    print(f"Failed to open {PORT}. Is the board plugged in? Error: {e}")
    exit()

# The byte sequence for 0xAABBCCDD in Little-Endian format
SYNC_BYTES = b'\xdd\xcc\xbb\xaa'

while True:
    buffer = bytearray()
    
    # 1. THE SYNC LOOP: Read 1 byte at a time until we find the header
    while len(buffer) < 4:
        buffer += ser.read(1)
        # If we have 4 bytes but they don't match the header, drop the oldest byte
        if len(buffer) == 4 and buffer != SYNC_BYTES:
            buffer.pop(0) 
            
    # 2. HEADER FOUND! Now read the remaining 32 bytes of the payload
    payload = ser.read(26)
    
    if len(payload) == 26:
        # 3. UNPACK THE DATA
        # '<' means Little-Endian
        # '8i' means 8 signed 32-bit integers
        data = struct.unpack('<6i1h', payload)
        
        # Assign to readable variables
        gyroX, gyroY, gyroZ, accelX, accelY, accelZ, presVal = data
        
        # Print it out! (Carriage return \r overwrites the same line for a clean display)
        print(f"SYNCED! | Accel: X={accelX:6} Y={accelY:6} Z={accelZ:6} | Presence: {presVal}", end='\r')
