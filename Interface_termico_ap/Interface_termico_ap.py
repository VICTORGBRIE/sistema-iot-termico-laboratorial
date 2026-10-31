import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import datetime
import pandas as pd
import threading
import time
import os
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
import logging

logging.getLogger("urllib3").setLevel(logging.ERROR)
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ==========================================
# 1. GESTOR DE WI-FI (WIFI MANAGER)
# ==========================================
class WifiManager:
    def __init__(self):
        self.base_url = None
        self.session = requests.Session()
        self.session.headers.update({'Connection': 'close'})
        retries = Retry(total=0, backoff_factor=0.1, status_forcelist=[500, 502, 503, 504])
        adapter = HTTPAdapter(max_retries=retries)
        self.session.mount('http://', adapter)
        self._lock = threading.Lock()

    def connect(self, ip):
        if not ip: return False, "IP vazio"
        ip = ip.strip().replace("http://", "").replace("/", "")
        url = f"http://{ip}"
        
        with self._lock:
            self.session.close()
            self.session = requests.Session()
            self.session.headers.update({'Connection': 'close'})
            retries = Retry(total=0, backoff_factor=0.1, status_forcelist=[500, 502, 503, 504])
            adapter = HTTPAdapter(max_retries=retries)
            self.session.mount('http://', adapter)
            
            try:
                response = self.session.get(f"{url}/status", timeout=4)
                if response.status_code == 200:
                    self.base_url = url
                    return True, "Sucesso"
                else:
                    return False, f"Erro HTTP {response.status_code}"
            except requests.exceptions.ConnectTimeout:
                return False, "Timeout: O ESP32 não respondeu."
            except requests.exceptions.ConnectionError as e:
                return False, "Erro de Rede: Não foi possível alcançar o ESP32."
            except Exception as e:
                return False, f"Falha: {str(e)}"

    def get_sync_data(self):
        if not self.base_url: return None
        with self._lock:
            try:
                r = self.session.get(f"{self.base_url}/sync", timeout=3.0)
                if r.status_code == 200: return r.json()
            except: pass
        return None

    def send_command(self, cmd):
        if not self.base_url: return False
        with self._lock:
            try:
                timeout = 10 if "/select" in cmd else 4.0
                r = self.session.post(f"{self.base_url}/send", params={'command': cmd}, timeout=timeout)
                return r.status_code == 200
            except: return False

    def get_command_status(self):
        if not self.base_url: return None
        with self._lock:
            try:
                r = self.session.get(f"{self.base_url}/status", timeout=3.0)
                if r.status_code == 200: return r.json()
            except: pass
        return None

    def disconnect(self):
        with self._lock:
            self.base_url = None

# ==========================================
# 2. COMPONENTES CUSTOMIZADOS E GRÁFICO
# ==========================================
class ToggleSwitch(tk.Canvas):
    def __init__(self, master, width=50, height=25,
                 bg_off="#cccccc", bg_on="#2196F3",
                 circle_color="white", command=None, *args, **kwargs):
        super().__init__(master, width=width, height=height,
                         highlightthickness=0, *args, **kwargs)
        self.bg_off = bg_off
        self.bg_on = bg_on
        self.circle_color = circle_color
        self.command = command
        self.state = False
        self.is_disabled = False
        self.bind("<Button-1>", self.toggle)
        self.draw()

    def draw(self):
        self.delete("all")
        w = int(self['width'])
        h = int(self['height'])
        r = h // 2
        
        color = self.bg_on if self.state else self.bg_off
        if self.is_disabled:
            color = "#e0e0e0" if not self.state else "#90caf9"
            
        circle_c = self.circle_color if not self.is_disabled else "#f5f5f5"

        self.create_oval(0, 0, h, h, fill=color, outline=color)
        self.create_oval(w - h, 0, w, h, fill=color, outline=color)
        self.create_rectangle(r, 0, w - r, h, fill=color, outline=color)
        
        if self.state:
            x0, x1 = w - h, w
        else:
            x0, x1 = 0, h
        self.create_oval(x0, 0, x1, h, fill=circle_c, outline=circle_c)

    def toggle(self, event=None):
        if self.is_disabled: return
        self.state = not self.state
        self.draw()
        if self.command:
            self.command(self.state)

    def disable(self):
        self.is_disabled = True
        self.draw()

    def enable(self):
        self.is_disabled = False
        self.draw()


class PlotManager:
    def __init__(self, frame):
        self.fig, self.ax = plt.subplots(figsize=(10, 4), dpi=100)
        self.canvas = FigureCanvasTkAgg(self.fig, master=frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)
        self.fig.patch.set_facecolor('#f4f4f4')
        self.ax.set_facecolor('#ffffff')

    def init(self):
        self.ax.clear()
        self.ax.set_title("A aguardar ligação...")
        self.ax.grid(True, linestyle='--', alpha=0.6)
        self.canvas.draw()

    def update(self, times, temps):
        if not times or not temps: return
        self.ax.clear()
        self.ax.plot(times, temps, color='#d32f2f', linewidth=2)
        self.ax.set_title("Temperatura em Tempo Real", fontsize=10, fontweight='bold')
        self.ax.grid(True, linestyle='--', alpha=0.6)
        
        if times[-1] > 60: self.ax.set_xlim(times[-1] - 60, times[-1] + 2)
        else: self.ax.set_xlim(0, 62)
        
        margin = 3.0
        self.ax.set_ylim(min(temps) - margin, max(temps) + margin)
        self.canvas.draw()

# ==========================================
# 3. INTERFACE GRÁFICA (GUI)
# ==========================================
class ThermalControlApp:
    def __init__(self, master):
        self.master = master
        self.master.title("Controlo Térmico PIBIC - Sistema Resiliente")
        self.master.geometry("1000x850")

        self.wifi = WifiManager()
        self.is_running = False
        self.start_time = None 
        self.times = []
        self.temperaturas = []
        
        self.sistema_ativo = False
        self.ventoinha_on = False
        self.monitor_on = False
        
        self.recording = False
        self.record_file = None

        self.create_widgets()

    def create_widgets(self):
        main = ttk.Frame(self.master, padding=10)
        main.pack(fill="both", expand=True)

        conn = ttk.LabelFrame(main, text="Configuração de Rede")
        conn.pack(fill="x", pady=5)
        ttk.Label(conn, text="IP Numérico:").pack(side="left", padx=5)
        
        # --- ATUALIZAÇÃO DA CAIXA DE IP PARA COMBOBOX ---
        self.ip_entry = ttk.Combobox(conn, width=15, values=["10.229.1.200", "192.168.4.1"])
        self.ip_entry.set("10.229.1.200") 
        self.ip_entry.pack(side="left", padx=5)
        # ------------------------------------------------
        
        self.btn_conn = ttk.Button(conn, text="Ligar", command=self.toggle_connection)
        self.btn_conn.pack(side="left", padx=5)
        
        arduino_frame = ttk.LabelFrame(conn, text="Arduinos")
        arduino_frame.pack(side="left", padx=5)
        self.btn_bt1 = ttk.Button(arduino_frame, text="1", width=3, command=lambda: self.send("/select 1"), state="disabled")
        self.btn_bt1.pack(side="left", padx=2, pady=2)
        self.btn_bt2 = ttk.Button(arduino_frame, text="2", width=3, command=lambda: self.send("/select 2"), state="disabled")
        self.btn_bt2.pack(side="left", padx=2, pady=2)
        
        self.lbl_status = tk.Label(conn, text="● Desligado", fg="red", font=("Arial", 10, "bold"))
        self.lbl_status.pack(side="right", padx=10)
        
        # Botão da Ventoinha
        self.switch_fan = ToggleSwitch(conn, width=40, height=20, bg_off="#cccccc", bg_on="#2196F3", circle_color="white", command=self.on_fan_switch)
        self.switch_fan.pack(side="right", padx=5)
        ttk.Label(conn, text="Ventoinha:").pack(side="right", padx=(0,2))
        self.switch_fan.disable()

        # Botão de Monitoramento Livre
        self.switch_monitor = ToggleSwitch(conn, width=40, height=20, bg_off="#cccccc", bg_on="#4CAF50", circle_color="white", command=self.on_monitor_switch)
        self.switch_monitor.pack(side="right", padx=15)
        ttk.Label(conn, text="Monitorar Temp:").pack(side="right", padx=(0,2))
        self.switch_monitor.disable()

        display = ttk.Frame(main)
        display.pack(fill="x", pady=5)
        self.lbl_temp = tk.Label(display, text="-- °C", font=("Arial", 40, "bold"), fg="#d32f2f")
        self.lbl_temp.pack(side="left", expand=True)
        self.lbl_info = tk.Label(display, text="Pot: --% | I: --A", font=("Arial", 14))
        self.lbl_info.pack(side="right", expand=True)

        g_frame = ttk.Frame(main)
        g_frame.pack(fill="both", expand=True)
        self.plot = PlotManager(g_frame); self.plot.init()

        bot_frame = ttk.Frame(main)
        bot_frame.pack(side="bottom", fill="x", pady=5)
        
        cmd_frame = ttk.Frame(bot_frame)
        cmd_frame.pack(fill="x", pady=(0, 5))
        ttk.Label(cmd_frame, text="Enviar Comando:").pack(side="left", padx=5)
        self.cmd_entry = ttk.Entry(cmd_frame)
        self.cmd_entry.pack(side="left", padx=5, fill="x", expand=True)
        self.cmd_entry.bind("<Return>", lambda e: self.send_custom_command())
        self.btn_send = ttk.Button(cmd_frame, text="Enviar", command=self.send_custom_command, state="disabled")
        self.btn_send.pack(side="right", padx=5)

        rec_frame = ttk.Frame(bot_frame)
        rec_frame.pack(fill="x", pady=(0,5))
        self.btn_start_rec = ttk.Button(rec_frame, text="Gravar", command=self.start_recording, state="disabled")
        self.btn_start_rec.pack(side="left", padx=5)
        self.btn_stop_rec = ttk.Button(rec_frame, text="Parar", command=self.stop_recording, state="disabled")
        self.btn_stop_rec.pack(side="left", padx=5)

        log_frame = ttk.LabelFrame(bot_frame, text="Monitor Série / Diagnóstico")
        log_frame.pack(fill="x")
        self.log = scrolledtext.ScrolledText(log_frame, height=10, state="disabled", font=("Consolas", 9))
        self.log.pack(fill="x", padx=5, pady=5)

    def log_msg(self, msg):
        self.log.config(state="normal")
        self.log.insert("end", f"[{datetime.datetime.now().strftime('%H:%M:%S')}] {msg}\n")
        self.log.see("end")
        self.log.config(state="disabled")

    def toggle_connection(self):
        if self.is_running:
            self.is_running = False
            self.btn_conn.config(text="Ligar")
            self.lbl_status.config(text="● Desligado", fg="red")
            if self.recording: self.stop_recording()
            
            self.btn_bt1.config(state="disabled")
            self.btn_bt2.config(state="disabled")
            self.btn_send.config(state="disabled")
            self.switch_fan.disable()
            self.switch_monitor.disable()
            self.btn_start_rec.config(state="disabled")
            self.btn_stop_rec.config(state="disabled")
            self.log_msg("Desligado pelo utilizador.")
        else:
            ip = self.ip_entry.get().strip()
            if "x" in ip or not ip:
                messagebox.showwarning("Aviso", "Por favor, introduza um IP válido")
                return
                
            self.log_msg(f"A iniciar ligação com {ip}...")
            sucesso, msg = self.wifi.connect(ip)
            if sucesso:
                self.is_running = True
                self.btn_conn.config(text="Desligar")
                self.lbl_status.config(text="● Ligado", fg="green")
                self.btn_bt1.config(state="normal")
                self.btn_bt2.config(state="normal")
                self.btn_send.config(state="normal")
                self.switch_fan.enable()
                
                # Só permite ligar o monitor se não houver um ciclo ativo
                if not self.sistema_ativo:
                    self.switch_monitor.enable()
                    
                self.btn_start_rec.config(state="normal")
                self.log_msg("✓ Ligado com sucesso ao Gateway ESP32!")
                threading.Thread(target=self.update_loop, daemon=True).start()
            else:
                self.log_msg(f"✗ Falha: {msg}")

    def update_loop(self):
        while self.is_running:
            delay_time = 1.0 if self.sistema_ativo else 2.0
            data = self.wifi.get_sync_data()
            
            if data:
                t = data.get('t', 0.0)
                p = data.get('p', 0)
                i = data.get('i', 0.0)
                self.master.after(0, self.update_gui, t, p, i)
                
                logs_esp = data.get('log', '').strip()
                if logs_esp:
                    for linha in logs_esp.split('\n'):
                        linha = linha.strip()
                        if not linha: continue
                        
                        if "INFO:" in linha:
                            self.master.after(0, self.log_msg, f"✅ MEGA: {linha}")
                            # Se o MEGA anunciar que o ensaio acabou por tempo, desativa o sistema na interface
                            if "Ensaio Concluido" in linha:
                                self.master.after(0, self.handle_ensaio_concluido)
                        elif "ERRO:" in linha:
                            self.master.after(0, self.log_msg, f"❌ ALERTA MEGA: {linha}")
                        else:
                            self.master.after(0, self.log_msg, f"GATEWAY: {linha}")
                    
            time.sleep(delay_time)

    def handle_ensaio_concluido(self):
        self.sistema_ativo = False
        self.switch_monitor.enable() # Destrava o botão de monitoramento
        
        # Se o monitoramento NÃO estiver ativo, limpa a tela para zerar
        if not self.monitor_on:
            self.lbl_temp.config(text="-- °C")
            self.lbl_info.config(text="Pot: --% | I: --A")

    def update_gui(self, t, p, i):
        if t != 0.0 or p > 0:
            self.lbl_temp.config(text=f"{t:.1f} °C")
            self.lbl_info.config(text=f"Potência: {p}% | Corrente: {i:.3f} A")
            
            if not self.sistema_ativo and p > 0:
                self.sistema_ativo = True
                self.start_time = time.time()
                self.plot.ax.clear()
                self.times = []
                self.temperaturas = []
                self.log_msg("📡 Ensaio detetado a correr! A retomar o gráfico...")
        
        if (self.sistema_ativo or self.monitor_on) and (t != 0.0 or p > 0):
            now = time.time() - self.start_time
            self.times.append(now)
            self.temperaturas.append(t)
            if len(self.times) > 300:
                self.times.pop(0)
                self.temperaturas.pop(0)
            self.plot.update(self.times, self.temperaturas)

        if self.recording and self.record_file:
            elapsed = time.time() - (self.start_time or time.time())
            try:
                self.record_file.write(f"{elapsed:.2f},{p},{t:.2f},{i:.3f}\n")
            except Exception: pass

    def send(self, cmd):
        if self.wifi.send_command(cmd):
            if "/select" in cmd:
                self.log_msg(f"Comando enviado: {cmd}")
                self.log_msg("⏳ A aguardar Bluetooth...")
                threading.Thread(target=self.monitor_bt_status, daemon=True).start()
                
            elif cmd.startswith("/") and not cmd.startswith("/select"):
                self.log_msg(f"📡 A enviar '{cmd}'... a aguardar resposta do Mega.")
                
                if cmd == "/play":
                    self.sistema_ativo = True
                    self.switch_monitor.disable() # Bloqueia o botão durante o ciclo
                    self.start_time = time.time()
                    self.times = []
                    self.temperaturas = []
                    self.plot.ax.clear()
                    
                elif cmd == "/stop":
                    self.sistema_ativo = False
                    self.switch_monitor.enable() # Desbloqueia o botão
                    # Limpa as variáveis gráficas mas NÃO limpa o gráfico (para você poder analisar depois)
                    self.times = []
                    self.temperaturas = []
                    
                    if not self.monitor_on:
                        self.master.after(0, lambda: self.lbl_temp.config(text="-- °C"))
                        self.master.after(0, lambda: self.lbl_info.config(text="Pot: --% | I: --A"))
            else:
                 self.log_msg(f"Comando enviado: {cmd}")
        else:
            self.log_msg(f"✗ Falha de rede ao enviar: {cmd}")
    
    def send_custom_command(self):
        cmd = self.cmd_entry.get().strip()
        if not cmd: return
        
        self.send(cmd)
        self.cmd_entry.delete(0, "end")
        
        self.btn_send.config(state="disabled")
        self.cmd_entry.config(state="disabled")
        self.master.after(1500, lambda: self.btn_send.config(state="normal"))
        self.master.after(1500, lambda: self.cmd_entry.config(state="normal"))

    def on_fan_switch(self, state: bool):
        self.ventoinha_on = state
        cmd = "/ventoinha on" if state else "/ventoinha off"
        self.send(cmd)

    def on_monitor_switch(self, state: bool):
        self.monitor_on = state
        cmd = "/monitor on" if state else "/monitor off"
        self.send(cmd)
        
        # Se for ligado, inicializa as variáveis do gráfico
        if state:
            self.start_time = time.time()
            self.times = []
            self.temperaturas = []
            self.plot.ax.clear()
        
        # Se for desligado manualmente e não houver ciclo correndo, zera os números
        if not state and not self.sistema_ativo:
            self.lbl_temp.config(text="-- °C")
            self.lbl_info.config(text="Pot: --% | I: --A")

    def start_recording(self):
        if self.recording: return
        fname = filedialog.asksaveasfilename(defaultextension=".txt",
                    filetypes=[("Text files","*.txt")], title="Salvar registo")
        if not fname: return
        try:
            self.record_file = open(fname, "w")
            self.record_file.write("tempo,potencia,temperatura,corrente\n")
            self.recording = True
            self.start_time = time.time() 
            self.log_msg(f"🎙️ Gravação iniciada em {fname}")
            self.btn_start_rec.config(state="disabled")
            self.btn_stop_rec.config(state="normal")
        except Exception as e:
            messagebox.showerror("Erro", f"Não foi possível abrir ficheiro: {e}")

    def stop_recording(self):
        if not self.recording: return
        try: self.record_file.close()
        except Exception: pass
        self.recording = False
        self.record_file = None
        self.log_msg("⏹️ Gravação terminada.")
        self.btn_start_rec.config(state="normal")
        self.btn_stop_rec.config(state="disabled")

    def monitor_bt_status(self):
        for _ in range(20):
            time.sleep(1)
            status = self.wifi.get_command_status()
            if status:
                resposta = status.get('resposta', '')
                if status.get('arduino_conectado', False) or "Sucesso" in resposta:
                    self.master.after(0, self.log_msg, f"✓ {resposta}")
                    return
                elif "Falha" in resposta or "Erro" in resposta:
                    self.master.after(0, self.log_msg, f"✗ {resposta}")
                    return

    def on_close(self):
        self.is_running = False
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = ThermalControlApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()