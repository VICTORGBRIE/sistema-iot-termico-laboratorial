import tkinter as tk
from ttkbootstrap import Style
from tkinter import ttk, scrolledtext, messagebox, filedialog
import datetime
import threading
import time
import os
import requests
import serial
import serial.tools.list_ports
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
import logging

logging.getLogger("urllib3").setLevel(logging.ERROR)
logging.basicConfig(level=logging.INFO)

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ==========================================
# 1. GESTORES DE CONEXÃO (WIFI E SERIAL)
# ==========================================
class WifiManager:
    def __init__(self):
        self.base_url = None
        self.session = requests.Session()
        self.session.headers.update({'Connection': 'close'})
        adapter = HTTPAdapter(max_retries=Retry(total=0, backoff_factor=0.1))
        self.session.mount('http://', adapter)
        self._lock = threading.Lock()

    def connect(self, ip):
        url = f"http://{ip.strip().replace('http://', '').replace('/', '')}"
        with self._lock:
            self.session.close()
            self.session = requests.Session()
            self.session.headers.update({'Connection': 'close'})
            self.session.mount('http://', HTTPAdapter(max_retries=Retry(total=0)))
            try:
                if self.session.get(f"{url}/status", timeout=4).status_code == 200:
                    self.base_url = url
                    return True, "Sucesso"
                return False, "Erro HTTP"
            except Exception as e:
                return False, f"Falha de Rede ESP32"

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
                timeout = 10 if "/select" in cmd else 3.0
                return self.session.post(f"{self.base_url}/send", params={'command': cmd}, timeout=timeout).status_code == 200
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
        with self._lock: self.base_url = None

class SerialManager:
    def __init__(self):
        self.ser = None
        self._lock = threading.Lock()

    def connect(self, port, baudrate=115200):
        try:
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            return True, "Sucesso"
        except Exception as e:
            return False, str(e)

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.ser = None

    def send_command(self, cmd):
        if self.ser and self.ser.is_open:
            with self._lock:
                try:
                    self.ser.write((cmd + '\n').encode('utf-8'))
                    return True
                except: pass
        return False

    def get_sync_data(self):
        if self.ser and self.ser.is_open:
            with self._lock:
                try:
                    log_str = ""
                    t, sp, p, pr, i = 0.0, 0.0, 0, 0.0, 0.0
                    has_data = False
                    
                    while self.ser.in_waiting > 0:
                        line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                        if not line: continue
                        
                        if line.startswith("DADOS:"):
                            parts = line.split()
                            for part in parts:
                                if part.startswith("T="): t = float(part[2:])
                                elif part.startswith("SP="): sp = float(part[3:]) 
                                elif part.startswith("P="): p = int(part[2:])
                                elif part.startswith("PR="): pr = float(part[3:])
                                elif part.startswith("I="): i = float(part[2:])
                            has_data = True
                        else:
                            log_str += line + "\n"
                            
                    if has_data or log_str:
                        return {'t': t, 'sp': sp, 'p': p, 'pr': pr, 'i': i, 'log': log_str.strip()}
                except: pass
        return None

class CommManager:
    def __init__(self):
        self.mode = None 
        self.wifi = WifiManager()
        self.serial = SerialManager()

    def connect(self, target):
        if target.upper().startswith("COM") or "/dev/" in target:
            self.mode = 'serial'
            return self.serial.connect(target)
        else:
            self.mode = 'wifi'
            return self.wifi.connect(target)

    def disconnect(self):
        if self.mode == 'wifi': self.wifi.disconnect()
        elif self.mode == 'serial': self.serial.disconnect()
        self.mode = None

    def send_command(self, cmd):
        if self.mode == 'wifi': return self.wifi.send_command(cmd)
        elif self.mode == 'serial': return self.serial.send_command(cmd)
        return False

    def get_sync_data(self):
        if self.mode == 'wifi': return self.wifi.get_sync_data()
        elif self.mode == 'serial': return self.serial.get_sync_data()
        return None
        
    def get_command_status(self):
        if self.mode == 'wifi': return self.wifi.get_command_status()
        return None 

# ==========================================
# 2. COMPONENTES E GRÁFICO
# ==========================================
class ToggleSwitch(tk.Canvas):
    def __init__(self, master, width=50, height=25, bg_off="#cccccc", bg_on="#2196F3", circle_color="white", command=None, *args, **kwargs):
        super().__init__(master, width=width, height=height, highlightthickness=0, *args, **kwargs)
        self.bg_off = bg_off; self.bg_on = bg_on; self.circle_color = circle_color; self.command = command
        self.state = False; self.is_disabled = False
        self.bind("<Button-1>", self.toggle); self.draw()

    def draw(self):
        self.delete("all")
        w, h = int(self['width']), int(self['height']); r = h // 2
        color = self.bg_on if self.state else self.bg_off
        if self.is_disabled: color = "#e0e0e0" if not self.state else "#90caf9"
        circle_c = self.circle_color if not self.is_disabled else "#f5f5f5"

        self.create_oval(0, 0, h, h, fill=color, outline=color)
        self.create_oval(w - h, 0, w, h, fill=color, outline=color)
        self.create_rectangle(r, 0, w - r, h, fill=color, outline=color)
        x0, x1 = (w - h, w) if self.state else (0, h)
        self.create_oval(x0, 0, x1, h, fill=circle_c, outline=circle_c)

    def toggle(self, event=None):
        if self.is_disabled: return
        self.state = not self.state
        self.draw()
        if self.command: self.command(self.state)

    def disable(self): self.is_disabled = True; self.draw()
    def enable(self): self.is_disabled = False; self.draw()

class PlotManager:
    def __init__(self, frame):
        self.fig, self.ax = plt.subplots(figsize=(10, 4), dpi=100)
        self.canvas = FigureCanvasTkAgg(self.fig, master=frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)
        self.fig.patch.set_facecolor('#f4f4f4'); self.ax.set_facecolor('#ffffff')
        
        self.line_temp, = self.ax.plot([], [], color='#d32f2f', linewidth=2, label='Temp. Real')
        self.line_sp, = self.ax.plot([], [], color='#4CAF50', linewidth=2, linestyle='--', label='Setpoint')
        self.ax.legend(loc='upper left')

    def init(self):
        self.ax.clear()
        self.ax.set_title("A aguardar ligação...")
        self.ax.grid(True, linestyle='--', alpha=0.6)
        self.line_temp, = self.ax.plot([], [], color='#d32f2f', linewidth=2, label='Temp. Real')
        self.line_sp, = self.ax.plot([], [], color='#4CAF50', linewidth=2, linestyle='--', label='Setpoint')
        self.ax.legend(loc='upper left')
        self.canvas.draw()

    def update(self, times, temps, setpoints, view_full_history):
        if not times or not temps: return
        
        self.ax.set_title("Curva Térmica e Setpoint", fontsize=10, fontweight='bold')
        
        if view_full_history:
            # OPTIMIZAÇÃO DE MEMÓRIA: Se o histórico tiver mais de 5000 pontos, 
            # ele salta pontos na hora de desenhar (Decimação), salvando CPU e RAM.
            # Os dados reais continuam intactos na memória e no TXT.
            step = max(1, len(times) // 5000)
            plot_times = times[::step]
            plot_temps = temps[::step]
            plot_sps = setpoints[::step]
        else:
            window_size = 480
            plot_times = times[-window_size:]
            plot_temps = temps[-window_size:]
            plot_sps = setpoints[-window_size:]

        self.line_temp.set_data(plot_times, plot_temps)
        self.line_sp.set_data(plot_times, plot_sps)
        
        # ... (mantenha o resto da função igual a partir do limite dos eixos xlim/ylim) ...
        if view_full_history:
            self.ax.set_xlim(0, max(60, plot_times[-1] + 2))
        else:
            min_x = max(0, plot_times[-1] - 120)
            self.ax.set_xlim(min_x, max(120, plot_times[-1] + 2))
            
        all_y_vals = plot_temps + [sp for sp in plot_sps if sp > 0] 
        if all_y_vals:
            self.ax.set_ylim(min(all_y_vals) - 3.0, max(all_y_vals) + 3.0)
            
        self.canvas.draw_idle()

# ==========================================
# 3. INTERFACE GRÁFICA (GUI)
# ==========================================
class ThermalControlApp:
    def __init__(self, master):
        self.master = master
        self.master.title("Controlo Térmico PIBIC - Omnicanal")
        self.master.geometry("1000x900")

        self.comm = CommManager()
        self.is_running = False
        self.start_time = None 
        
        self.times = []
        self.temperaturas = []
        self.setpoints = []
        self.potencias_hist = []
        self.potencias_reais_hist = []
        self.correntes_hist = []
        
        self.smoothed_temp = None
        self.last_data_time = None
        self.sistema_ativo = False
        self.monitor_on = False
        self.last_power = 0  
        
        self.view_full_history = False 
        
        # --- NOVAS VARIÁVEIS DE SEGURANÇA DE DADOS ---
        self.auto_save_file = None
        
        self.create_widgets()

    def create_widgets(self):
        main = ttk.Frame(self.master, padding=10)
        main.pack(fill="both", expand=True)

        conn = ttk.LabelFrame(main, text="Configuração de Rede e Hardware")
        conn.pack(fill="x", pady=5)
        ttk.Label(conn, text="IP ou COM:").pack(side="left", padx=5)
        
        portas_com = [port.device for port in serial.tools.list_ports.comports()]
        valores_padrao = ["10.229.1.200", "192.168.4.1"] + portas_com
        
        self.ip_entry = ttk.Combobox(conn, width=15, values=valores_padrao)
        if portas_com:
            self.ip_entry.set(portas_com[-1]) 
        else:
            self.ip_entry.set("10.229.1.200")
            
        self.ip_entry.pack(side="left", padx=5)
        
        self.btn_conn = ttk.Button(conn, text="Ligar", command=self.toggle_connection)
        self.btn_conn.pack(side="left", padx=5)
        
        arduino_frame = ttk.LabelFrame(conn, text="Seletor ESP32")
        arduino_frame.pack(side="left", padx=5)
        self.btn_bt1 = ttk.Button(arduino_frame, text="1", width=3, command=lambda: self.send("/select 1"), state="disabled")
        self.btn_bt1.pack(side="left", padx=2, pady=2)
        self.btn_bt2 = ttk.Button(arduino_frame, text="2", width=3, command=lambda: self.send("/select 2"), state="disabled")
        self.btn_bt2.pack(side="left", padx=2, pady=2)
        
        self.lbl_status = tk.Label(conn, text="● Desligado", fg="red", font=("Arial", 10, "bold"))
        self.lbl_status.pack(side="right", padx=10)

        fan_frame = ttk.Frame(conn)
        fan_frame.pack(side="right", padx=10)
        ttk.Label(fan_frame, text="Ventoinha %:").pack(side="left")
        self.fan_var = tk.IntVar(value=0)
        self.fan_spin = ttk.Spinbox(fan_frame, from_=0, to=100, width=5, textvariable=self.fan_var, command=self.send_fan_pwm, state="disabled")
        self.fan_spin.pack(side="left", padx=2)
        self.fan_spin.bind("<Return>", lambda e: self.send_fan_pwm())
        self.fan_slider = ttk.Scale(fan_frame, from_=0, to=100, orient="horizontal", variable=self.fan_var, state="disabled")
        self.fan_slider.pack(side="left", padx=5)
        self.fan_slider.bind("<ButtonRelease-1>", lambda e: self.send_fan_pwm())

        self.switch_monitor = ToggleSwitch(conn, width=40, height=20, bg_off="#cccccc", bg_on="#4CAF50", circle_color="white", command=self.on_monitor_switch)
        self.switch_monitor.pack(side="right", padx=15)
        ttk.Label(conn, text="Monitorar Temp:").pack(side="right", padx=(0,2))
        self.switch_monitor.disable()

        display = ttk.Frame(main)
        display.pack(fill="x", pady=5)
        self.lbl_temp = tk.Label(display, text="-- °C", font=("Arial", 40, "bold"), fg="#d32f2f")
        self.lbl_temp.pack(side="left", expand=True)
        self.lbl_info = tk.Label(display, text="Pot: --% (-- W) | I: --A", font=("Arial", 14))
        self.lbl_info.pack(side="right", expand=True)

        view_frame = ttk.Frame(main)
        view_frame.pack(fill="x")
        self.switch_view = ToggleSwitch(view_frame, width=40, height=20, bg_off="#9E9E9E", bg_on="#2196F3", command=self.on_view_switch)
        self.switch_view.pack(side="right", padx=5)
        self.lbl_view = ttk.Label(view_frame, text="Janela Deslizante (2 min)", font=("Arial", 9))
        self.lbl_view.pack(side="right", padx=5)

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
        self.btn_export = ttk.Button(rec_frame, text="Exportar Gráfico Completo (TXT)", command=self.export_data, state="disabled")
        self.btn_export.pack(side="left", padx=5)

        # =======================================================
        # PAINEL DE CONTROLO DE MALHA (AUTO / MANUAL)
        # =======================================================
        control_frame = ttk.LabelFrame(bot_frame, text="Controlo do Sistema")
        control_frame.pack(fill="x", pady=(0,5))

        top_control = ttk.Frame(control_frame)
        top_control.pack(fill="x", pady=2)

        self.switch_malha = ToggleSwitch(top_control, width=40, height=20, bg_off="#cccccc", bg_on="#4CAF50", command=self.on_malha_switch)
        self.switch_malha.pack(side="left", padx=10, pady=5)
        self.switch_malha.disable()
        
        self.lbl_malha = ttk.Label(top_control, text="Malha Aberta (Manual)", font=("Arial", 10, "bold"))
        self.lbl_malha.pack(side="left", padx=5)

        ttk.Separator(top_control, orient='vertical').pack(side="left", fill="y", padx=15)

        ttk.Label(top_control, text="Referência (°C):").pack(side="left", padx=2)
        self.ref_entry = ttk.Entry(top_control, width=8)
        self.ref_entry.insert(0, "45.0")
        self.ref_entry.config(state="disabled")
        self.ref_entry.pack(side="left", padx=5)

        self.btn_send_ref = ttk.Button(top_control, text="Enviar Ref", command=self.send_referencia, state="disabled")
        self.btn_send_ref.pack(side="left", padx=5)

        stair_frame = ttk.LabelFrame(bot_frame, text="Ensaio Automático (Degraus)")
        stair_frame.pack(fill="x", pady=(0,5))
        
        ttk.Label(stair_frame, text="Intervalo N (s):").pack(side="left", padx=5)
        self.stair_n_entry = ttk.Entry(stair_frame, width=8)
        self.stair_n_entry.insert(0, "60")
        self.stair_n_entry.pack(side="left", padx=5)
        
        self.btn_stair = ttk.Button(stair_frame, text="Iniciar Loop 0-100-0%", command=self.send_staircase, state="disabled")
        self.btn_stair.pack(side="left", padx=10)

        log_frame = ttk.LabelFrame(bot_frame, text="Monitor Série / Diagnóstico")
        log_frame.pack(fill="x")
        self.log = scrolledtext.ScrolledText(log_frame, height=6, state="disabled", font=("Consolas", 9))
        self.log.pack(fill="x", padx=5, pady=5)

    def log_msg(self, msg):
        self.log.config(state="normal")
        self.log.insert("end", f"[{datetime.datetime.now().strftime('%H:%M:%S')}] {msg}\n")
        self.log.see("end")
        self.log.config(state="disabled")

    def on_view_switch(self, state: bool):
        self.view_full_history = state
        if state:
            self.lbl_view.config(text="Todo o Histórico")
        else:
            self.lbl_view.config(text="Janela Deslizante (2 min)")
        self.plot.update(self.times, self.temperaturas, self.setpoints, self.view_full_history)

    def toggle_connection(self):
        if self.is_running:
            self.is_running = False
            self.comm.disconnect()
            self.btn_conn.config(text="Ligar")
            self.lbl_status.config(text="● Desligado", fg="red")
            self.btn_bt1.config(state="disabled")
            self.btn_bt2.config(state="disabled")
            self.btn_send.config(state="disabled")
            self.fan_spin.config(state="disabled")
            self.fan_slider.config(state="disabled")
            
            self.switch_monitor.disable()
            self.switch_malha.disable()
            self.switch_malha.state = False
            self.switch_malha.draw()
            
            self.lbl_malha.config(text="Malha Aberta (Manual)")
            self.ref_entry.config(state="disabled")
            self.btn_send_ref.config(state="disabled")
            self.btn_stair.config(state="disabled")
            
            self.sistema_ativo = False
            self.handle_ensaio_concluido()
            self.log_msg("Desligado pelo utilizador.")
        else:
            target = self.ip_entry.get().strip()
            if not target: return
                
            self.log_msg(f"A iniciar ligação com {target}...")
            sucesso, msg = self.comm.connect(target)
            
            if sucesso:
                self.is_running = True
                self.btn_conn.config(text="Desligar")
                self.lbl_status.config(text="● Ligado", fg="green")
                
                if self.comm.mode == 'serial':
                    self.btn_bt1.config(state="disabled")
                    self.btn_bt2.config(state="disabled")
                    self.log_msg("Conexão Direta (Cabo/Bluetooth do PC) detetada.")
                else:
                    self.btn_bt1.config(state="normal")
                    self.btn_bt2.config(state="normal")

                self.btn_send.config(state="normal")
                self.fan_spin.config(state="normal")
                self.fan_slider.config(state="normal")
                self.switch_malha.enable()
                self.btn_stair.config(state="normal")
                
                if not self.sistema_ativo:
                    self.switch_monitor.enable()
                    
                threading.Thread(target=self.update_loop, daemon=True).start()
            else:
                self.log_msg(f"✗ Falha: {msg}")

    def send_fan_pwm(self, *args):
        try:
            val = int(float(self.fan_var.get()))
            self.fan_var.set(max(0, min(100, val))) 
            self.send(f"/fan {val}")
        except ValueError: pass

    def update_loop(self):
        while self.is_running:
            delay_time = 0.25 if (self.sistema_ativo or self.monitor_on) else 1.5
            data = self.comm.get_sync_data()

            if data:
                self.last_data_time = time.time()
                t = data.get('t', 0.0)
                sp = data.get('sp', 0.0) 
                p = data.get('p', 0)
                pr = data.get('pr', 0.0)
                i = data.get('i', 0.0)
                self.last_power = p
                self.master.after(0, self.update_gui, t, sp, p, pr, i)

                logs_esp = data.get('log', '').strip()
                if logs_esp:
                    for linha in logs_esp.split('\n'):
                        linha = linha.strip()
                        if not linha: continue
                        if "INFO:" in linha:
                            self.master.after(0, self.log_msg, f"✅ MEGA: {linha}")
                            if "Concluido" in linha or "Interrompido" in linha or "Parado" in linha:
                                self.master.after(0, self.handle_ensaio_concluido)
                        elif "ERRO:" in linha:
                            self.master.after(0, self.log_msg, f"❌ ALERTA MEGA: {linha}")
                        else:
                            self.master.after(0, self.log_msg, f"SYS: {linha}")
            else:
                if self.sistema_ativo and self.last_data_time is not None:
                    if time.time() - self.last_data_time > 4.0:
                        self.master.after(0, self.log_msg, "⚠️ Sem dados do MEGA – encerrando ciclo")
                        self.master.after(0, self.handle_ensaio_concluido)
                        self.last_data_time = None

            time.sleep(delay_time)

    def handle_ensaio_concluido(self):
        self.sistema_ativo = False
        self.switch_monitor.enable()  
        
        self.switch_malha.state = False
        self.switch_malha.draw()
        self.lbl_malha.config(text="Malha Aberta (Manual)")
        self.ref_entry.config(state="disabled")
        self.btn_send_ref.config(state="disabled")
        self.btn_stair.config(state="normal")
        
        if not self.monitor_on:
            self.lbl_temp.config(text="-- °C")
            self.lbl_info.config(text="Pot: --% (-- W) | I: --A")
            self.smoothed_temp = None
        # Fechar arquivo com segurança ao terminar
        if self.auto_save_file and not self.auto_save_file.closed:
            self.auto_save_file.close()
            self.log_msg("💾 Auto-Save finalizado com sucesso.")

    def update_gui(self, t, sp, p, pr, i):
        if t != 0.0 or p > 0 or sp > 0:
            if self.smoothed_temp is None or (not self.sistema_ativo and not self.monitor_on):
                self.smoothed_temp = t
            else:
                alpha = 0.2  
                self.smoothed_temp = (alpha * t) + ((1.0 - alpha) * self.smoothed_temp)
            
            self.lbl_temp.config(text=f"{self.smoothed_temp:.1f} °C")
            self.lbl_info.config(text=f"Potência: {p}% ({pr:.1f} W) | Corrente: {i:.3f} A")
            
            if not self.sistema_ativo and (p > 0 or sp > 0):
                self.sistema_ativo = True
                self.start_time = time.perf_counter()
                self.plot.init() 
                self.reset_master_data()
                self.log_msg("📡 Ensaio detetado a correr! A retomar o gráfico...")
                self.btn_stair.config(state="disabled")
                
                if sp > 0:
                    self.switch_malha.state = True
                    self.switch_malha.draw()
                    self.lbl_malha.config(text="Malha Fechada (Auto)")
                    self.ref_entry.config(state="normal")
                    self.ref_entry.delete(0, "end")
                    self.ref_entry.insert(0, str(sp))
                    self.btn_send_ref.config(state="normal")
        
        if (self.sistema_ativo or self.monitor_on) and (t != 0.0 or p > 0 or sp > 0):
            if self.start_time is None: 
                self.start_time = time.perf_counter()
            
            now = round(time.perf_counter() - self.start_time, 2)
            
            self.times.append(now)
            self.temperaturas.append(self.smoothed_temp) 
            self.setpoints.append(sp)
            self.potencias_hist.append(p)
            self.potencias_reais_hist.append(pr)
            self.correntes_hist.append(i)
            
            # --- NOVO CÓDIGO DE GRAVAÇÃO CONTÍNUA (AUTO-SAVE) ---
            # Garante que salva os dados físicos no arquivo a cada nova leitura
            if hasattr(self, 'auto_save_file') and self.auto_save_file and not self.auto_save_file.closed:
                try:
                    self.auto_save_file.write(f"{now:.2f},{sp:.2f},{p},{pr:.2f},{self.smoothed_temp:.2f},{i:.3f}\n")
                    self.auto_save_file.flush() # Força a gravação no disco
                except: 
                    pass
            # ----------------------------------------------------
                
            self.plot.update(self.times, self.temperaturas, self.setpoints, self.view_full_history)

        if len(self.times) > 0: self.btn_export.config(state="normal")
        else: self.btn_export.config(state="disabled")
    def send(self, cmd):
        def task():
            success = self.comm.send_command(cmd)
            self.master.after(0, self.process_send_result, cmd, success)
        threading.Thread(target=task, daemon=True).start()

    def process_send_result(self, cmd, success):
        if not success:
            self.log_msg(f"✗ Falha ao enviar: {cmd}")
            return

        if "/select" in cmd:
            if self.comm.mode == 'wifi':
                self.log_msg(f"Comando enviado: {cmd}")
                threading.Thread(target=self.monitor_bt_status, daemon=True).start()
            else:
                self.log_msg("Info: Seleção de ESP32 ignorada em conexão direta via Cabo/Bluetooth.")
            
        elif cmd.startswith("/") and not cmd.startswith("/select"):
            if cmd == "/stop":
                self.sistema_ativo = False
                self.switch_monitor.enable() 
                self.log_msg("⏹️ Controle Interrompido!")
                self.btn_stair.config(state="normal")
                if not self.monitor_on:
                    self.lbl_temp.config(text="-- °C")
                    self.lbl_info.config(text="Pot: --% (-- W) | I: --A")
                    self.smoothed_temp = None
        else:
             self.log_msg(f"✅ Comando enviado: {cmd}")

    def send_custom_command(self):
        cmd = self.cmd_entry.get().strip()
        if not cmd: return
        self.send(cmd)
        self.cmd_entry.delete(0, "end")

    def on_monitor_switch(self, state: bool):
        self.monitor_on = state
        self.send("/monitor on" if state else "/monitor off")
        
        if state and not self.sistema_ativo:
            self.start_time = time.perf_counter() 
            self.reset_master_data()
            self.smoothed_temp = None
            self.plot.init() 
            
        if not state and not self.sistema_ativo:
            self.lbl_temp.config(text="-- °C")
            self.lbl_info.config(text="Pot: --% (-- W) | I: --A")
            self.smoothed_temp = None

    def export_data(self):
        if not self.times: return
        
        pasta_script = os.path.dirname(os.path.abspath(__file__))
        pasta_dados = os.path.join(pasta_script, "dados")
        if not os.path.exists(pasta_dados):
            os.makedirs(pasta_dados)
            
        nome_padrao = f"ensaio_completo_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"

        fname = filedialog.asksaveasfilename(
            initialdir=pasta_dados,
            initialfile=nome_padrao,
            defaultextension=".txt", 
            filetypes=[("Text files","*.txt")], 
            title="Salvar Registos do Ensaio Completo"
        )
        if not fname: return
        try:
            with open(fname, "w") as f:
                f.write("tempo,setpoint_C,potencia_pct,potencia_real_W,temperatura,corrente\n")
                for t, sp, p, pr, temp, c in zip(self.times, self.setpoints, self.potencias_hist, self.potencias_reais_hist, self.temperaturas, self.correntes_hist):
                    f.write(f"{t:.2f},{sp:.2f},{p},{pr:.2f},{temp:.2f},{c:.3f}\n")
                    
            self.log_msg(f"💾 Gráfico exportado ({len(self.times)} linhas) para: {fname}")
            messagebox.showinfo("Sucesso", f"Dados exportados com sucesso!\nTotal: {len(self.times)} pontos gravados.")
        except Exception as e:
            messagebox.showerror("Erro", f"Erro ao salvar:\n{e}")

    def on_malha_switch(self, state: bool):
        if state: 
            self.lbl_malha.config(text="Malha Fechada (Auto)")
            self.ref_entry.config(state="normal")
            self.btn_send_ref.config(state="normal")
            try:
                alvo = float(self.ref_entry.get())
                self.send(f"/play 36000 {alvo:.1f}")
                self.reset_master_data()
                self.sistema_ativo = True
                self.btn_stair.config(state="disabled")
            except ValueError:
                messagebox.showerror("Erro", "Valor de referência inválido.")
                self.switch_malha.state = False
                self.switch_malha.draw()
                self.lbl_malha.config(text="Malha Aberta (Manual)")
                self.ref_entry.config(state="disabled")
                self.btn_send_ref.config(state="disabled")
        else:
            self.lbl_malha.config(text="Malha Aberta (Manual)")
            self.ref_entry.config(state="disabled")
            self.btn_send_ref.config(state="disabled")
            self.send("/stop")

    def send_referencia(self):
        try:
            nova_ref = float(self.ref_entry.get())
            self.send(f"/setpoint {nova_ref:.1f}")
        except ValueError:
            messagebox.showerror("Erro", "Por favor, insira um número válido para a referência.")

    def send_staircase(self):
        try:
            n = int(self.stair_n_entry.get())
            self.send(f"/staircase {n}")
            self.reset_master_data()
            self.sistema_ativo = True
            self.btn_stair.config(state="disabled")
            self.switch_malha.disable()
        except: messagebox.showerror("Erro", "Intervalo inválido.")

    def reset_master_data(self):
        self.times = []
        self.temperaturas = []
        self.setpoints = []
        self.potencias_hist = []
        self.potencias_reais_hist = []
        self.correntes_hist = []
        self.plot.init()
        self.start_time = time.perf_counter()
        
        # --- INICIA O AUTO-SAVE ASSIM QUE O GRÁFICO É ZERADO ---
        if self.auto_save_file:
            try: self.auto_save_file.close()
            except: pass
            
        pasta_script = os.path.dirname(os.path.abspath(__file__))
        pasta_dados = os.path.join(pasta_script, "dados")
        if not os.path.exists(pasta_dados): os.makedirs(pasta_dados)
        
        filename = os.path.join(pasta_dados, f"ensaio_AUTOSAVE_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.txt")
        try:
            self.auto_save_file = open(filename, "a", encoding="utf-8")
            self.auto_save_file.write("tempo,setpoint_C,potencia_pct,potencia_real_W,temperatura,corrente\n")
            self.auto_save_file.flush() # Força a criação do ficheiro
            self.log_msg("💾 Auto-Save ativado: Dados protegidos contra falhas.")
        except Exception as e:
            self.log_msg(f"⚠️ Erro ao criar Auto-Save: {e}")

    def monitor_bt_status(self):
        for _ in range(20):
            time.sleep(1)
            status = self.comm.get_command_status()
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
    style = Style(theme='flatly')
    root = style.master
    app = ThermalControlApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()