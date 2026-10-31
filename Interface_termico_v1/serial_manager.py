import serial
import serial.tools.list_ports
import time

class SerialManager:

    def __init__(self):
        self.ser = None
        self.baud = None

    def list_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud):
        try:
            self.ser = serial.Serial(port, baud, timeout=1)
            time.sleep(2) # Espera o ESP32 resetar
            return True
        except Exception as e:
            print("Erro ao conectar:", e)
            self.ser = None
            return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.ser = None

    def is_open(self):
        return self.ser and self.ser.is_open

    def readline(self):
        if self.is_open():
            try:
                return self.ser.readline().decode("utf-8", errors="ignore").strip()
            except Exception as e:
                print(f"Erro de leitura: {e}")
                self.ser = None # Força a desconexão
                return ""
        return ""

    def write(self, txt):
        if self.is_open():
            try:
                self.ser.write((txt + "\n").encode())
            except Exception as e:
                print(f"Erro de escrita: {e}")
                self.ser = None # Força a desconexão