import requests
import time

class WifiManager:
    def __init__(self):
        self.ip = None
        self.base_url = None

    def connect(self, ip):
        """
        Tenta conectar verificando se a página principal responde.
        """
        try:
            # Formata o URL (adiciona http:// se faltar)
            if not ip.startswith("http"):
                url = f"http://{ip}"
            else:
                url = ip
            
            # Tenta um ping rápido na raiz
            response = requests.get(url, timeout=3)
            if response.status_code == 200:
                self.ip = ip
                self.base_url = url
                return True
        except Exception as e:
            print(f"Erro ao conectar ao IP {ip}: {e}")
        return False

    def disconnect(self):
        self.ip = None
        self.base_url = None

    def is_connected(self):
        return self.base_url is not None

    def get_log(self):
        """
        Baixa o conteúdo atual do /log do ESP32
        """
        if self.is_connected():
            try:
                response = requests.get(f"{self.base_url}/log", timeout=2)
                if response.status_code == 200:
                    return response.text
            except Exception as e:
                print(f"Erro ao ler log: {e}")
                return None
        return None

    def send_command(self, cmd):
        """
        Envia comando para /send?command=...
        """
        if self.is_connected():
            try:
                # Envia via GET params
                payload = {'command': cmd}
                requests.get(f"{self.base_url}/send", params=payload, timeout=2)
                return True
            except Exception as e:
                print(f"Erro ao enviar comando: {e}")
        return False
    
    def clear_esp_log(self):
        """
        Chama /clear no ESP32 para limpar a memória dele
        """
        if self.is_connected():
            try:
                requests.get(f"{self.base_url}/clear", timeout=1)
            except:
                pass