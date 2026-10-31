import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

class WifiManager:
    def __init__(self):
        self.base_url = None
        # Configuração de Sessão Persistente (Keep-Alive)
        self.session = requests.Session()
        
        # Retry automático para falhas de rede breves
        retries = Retry(total=3, backoff_factor=0.1, status_forcelist=[500, 502, 503, 504])
        self.session.mount('http://', HTTPAdapter(max_retries=retries))

    def connect(self, ip):
        url = f"http://{ip}" if not ip.startswith("http") else ip
        try:
            # Timeout de 3s é suficiente se a rede estiver boa
            response = self.session.get(url, timeout=3)
            if response.status_code == 200:
                self.base_url = url
                return True
        except: pass
        return False

    def get_log(self):
        if self.base_url:
            try:
                # GET para leitura (rápido devido ao keep-alive)
                r = self.session.get(f"{self.base_url}/log", timeout=3)
                return r.text if r.status_code == 200 else ""
            except: return None
        return None

    def send_command(self, cmd):
        if self.base_url:
            try:
                # POST para comandos (Segurança semântica)
                self.session.post(f"{self.base_url}/send", params={'command': cmd}, timeout=2)
                return True
            except: return False
        return False
        
    def disconnect(self):
        self.base_url = None