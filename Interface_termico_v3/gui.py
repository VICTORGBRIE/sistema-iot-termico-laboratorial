import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import datetime
import pandas as pd
import threading
import time
import os

from wifi_manager import WifiManager 
from data_parser import parse_payload
from logic import is_data_line
from plot_manager import PlotManager

# Listas globais para armazenamento em memória (usadas pelo gráfico)
times = []
temperaturas = []

class ThermalControlApp:
    def __init__(self, master):
        self.master = master
        self.master.title("PIBIC - Sistema de Controle Térmico IoT")
        self.master.geometry("1100x900")
        self.master.protocol("WM_DELETE_WINDOW", self.on_close)

        self.wifi = WifiManager()
        self.is_running = False
        self.start_time = None # Inicializado como None para marcar o início real dos dados
        self.last_log_content = ""
        
        self.is_recording = False
        self.recording_data = []
        self.command_buttons = []

        os.makedirs("dados", exist_ok=True)
        self.create_widgets()
        self.set_controls_state("disabled")

    def create_widgets(self):
        main_frame = ttk.Frame(self.master, padding=10)
        main_frame.pack(fill="both", expand=True)

        # 1. Painel Superior (Conexão, Setpoints e Gravação)
        top = ttk.Frame(main_frame)
        top.pack(side="top", fill="x")

        # --- Conexão ---
        conn = ttk.LabelFrame(top, text="Conexão Gateway (ESP32)")
        conn.pack(side="left", fill="y", padx=5)
        ttk.Label(conn, text="IP:").grid(row=0, column=0, padx=5, pady=5)
        self.ip_entry = ttk.Entry(conn, width=15)
        self.ip_entry.insert(0, "esp-thermal.local")
        self.ip_entry.grid(row=0, column=1, padx=5, pady=5)
        self.btn_conn = ttk.Button(conn, text="Conectar", command=self.toggle_connection)
        self.btn_conn.grid(row=1, column=0, columnspan=2, padx=5, pady=5, sticky="ew")

        # --- Setpoints ---
        sets = ttk.LabelFrame(top, text="Ajuste de Setpoints")
        sets.pack(side="left", fill="y", padx=5)
        ttk.Label(sets, text="T. Max (C):").grid(row=0, column=0, padx=5)
        self.ent_max = ttk.Entry(sets, width=8); self.ent_max.insert(0, "60.0")
        self.ent_max.grid(row=0, column=1, padx=5)
        ttk.Label(sets, text="T. Min (C):").grid(row=1, column=0, padx=5)
        self.ent_min = ttk.Entry(sets, width=8); self.ent_min.insert(0, "30.0")
        self.ent_min.grid(row=1, column=1, padx=5)
        self.btn_set = ttk.Button(sets, text="Enviar", command=self.enviar_setpoints)
        self.btn_set.grid(row=0, column=2, rowspan=2, padx=10, sticky="ns")

        # --- Gravação ---
        rec = ttk.LabelFrame(top, text="Sessão de Dados")
        rec.pack(side="left", fill="y", padx=5)
        self.btn_rec = ttk.Button(rec, text="🔴 Gravar", command=self.start_recording)
        self.btn_rec.pack(fill="x", padx=5, pady=2)
        self.btn_stop = ttk.Button(rec, text="⬛ Parar e Salvar", command=self.stop_recording)
        self.btn_stop.pack(fill="x", padx=5, pady=2)
        self.lbl_rec = tk.Label(rec, text="Ocioso", fg="gray"); self.lbl_rec.pack()

        # 2. Comandos de Atalho (Atualizado com Ventoinha)
        cmds = ttk.LabelFrame(main_frame, text="Comandos de Atalho")
        cmds.pack(side="top", fill="x", pady=10)
        
        # Lista de comandos incluindo os novos da ventoinha
        lista_comandos = [
            "/select 1", "/select 2", "/def", "/play", 
            "/ventoinha on", "/ventoinha off", "/status", "/stop"
        ]
        
        for t in lista_comandos:
            btn = ttk.Button(cmds, text=t, command=lambda cmd=t: self.send(cmd))
            btn.pack(side="left", padx=3, pady=5)
            self.command_buttons.append(btn)

        # 3. Gráfico Central
        self.lbl_val = tk.Label(main_frame, text="-- °C", font=("Arial", 28, "bold"), fg="#d32f2f")
        self.lbl_val.pack(pady=5)
        
        self.gframe = ttk.LabelFrame(main_frame, text="Monitoramento de Temperatura")
        self.gframe.pack(fill="both", expand=True)
        self.plot = PlotManager(self.gframe); self.plot.init()

        # 4. Console e Log
        bot = ttk.Frame(main_frame)
        bot.pack(side="bottom", fill="x")
        
        manual = ttk.LabelFrame(bot, text="Console de Comando")
        manual.pack(fill="x", pady=(0, 5))
        self.cmd_var = tk.StringVar()
        self.ent_cmd = ttk.Entry(manual, textvariable=self.cmd_var)
        self.ent_cmd.pack(side="left", fill="x", expand=True, padx=5, pady=5)
        self.ent_cmd.bind("<Return>", self.send_manual)
        self.btn_manual = ttk.Button(manual, text="Enviar", command=self.send_manual)
        self.btn_manual.pack(side="right", padx=5)

        logf = ttk.LabelFrame(bot, text="Console do Sistema (Wi-Fi)")
        logf.pack(fill="x")
        self.log = scrolledtext.ScrolledText(logf, height=8, state="disabled", font=("Consolas", 10))
        self.log.pack(fill="x", padx=5, pady=5)

    def set_controls_state(self, state):
        for btn in self.command_buttons: btn.config(state=state)
        self.btn_rec.config(state=state); self.btn_stop.config(state=state)
        self.btn_set.config(state=state); self.ent_cmd.config(state=state)
        self.btn_manual.config(state=state)

    def toggle_connection(self):
        if self.is_running: self.disconnect()
        else: self.connect()

    def connect(self):
        ip = self.ip_entry.get().strip()
        if not ip: return
        self.log_msg(f"Conectando a {ip}...\n")
        if self.wifi.connect(ip):
            self.btn_conn.config(text="Desconectar"); self.ip_entry.config(state="disabled")
            self.set_controls_state("normal"); self.is_running = True
            self.start_time = None 
            threading.Thread(target=self.update_loop, daemon=True).start()
            self.log_msg("Conectado com sucesso! Aguardando dados para iniciar cronômetro...\n")
        else:
            messagebox.showerror("Erro", "Não foi possível alcançar o ESP32.")

    def disconnect(self):
        self.is_running = False; self.wifi.disconnect()
        self.btn_conn.config(text="Conectar"); self.ip_entry.config(state="normal")
        self.set_controls_state("disabled")
        self.log_msg("Sessão encerrada.\n")

    def update_loop(self):
        while self.is_running:
            raw_log = self.wifi.get_log()
            if raw_log: self.master.after(0, self.process_log, raw_log)
            time.sleep(1.2)

    def process_log(self, full_log):
        if len(full_log) < len(self.last_log_content): self.last_log_content = ""
        new_text = full_log[len(self.last_log_content):]
        if not new_text: return
        self.last_log_content = full_log
        self.log_msg(new_text)
        
        for line in new_text.split('\n'):
            payload = is_data_line(line)
            if payload:
                t_val, h_val = parse_payload(payload)
                if t_val is not None:
                    if self.start_time is None:
                        self.start_time = datetime.datetime.now()
                        self.log_msg("[SISTEMA] Primeiro dado recebido. Cronômetro iniciado.\n")
                    
                    t_rel = (datetime.datetime.now() - self.start_time).total_seconds()
                    times.append(t_rel); temperaturas.append(t_val)
                    self.lbl_val.config(text=f"{t_val:.2f} °C")
                    self.plot.update(times, temperaturas)
                    if self.is_recording: self.recording_data.append((t_rel, t_val, h_val if h_val else 0.0))
        
        if len(full_log) > 3500: self.wifi.clear_esp_log(); self.last_log_content = ""

    def enviar_setpoints(self):
        cmd = f"/def {self.ent_max.get()} {self.ent_min.get()}"
        self.send(cmd)

    def send(self, cmd):
        if self.is_running: 
            threading.Thread(target=self.wifi.send_command, args=(cmd,), daemon=True).start()
            self.log_msg(f"Enviando -> {cmd}\n")

    def send_manual(self, e=None):
        cmd = self.cmd_var.get().strip()
        if cmd: self.send(cmd); self.cmd_var.set("")

    def start_recording(self):
        self.is_recording = True; self.recording_data = []
        self.btn_rec.config(state="disabled"); self.btn_stop.config(state="normal")
        self.lbl_rec.config(text="GRAVANDO...", fg="red")

    def stop_recording(self):
        self.is_recording = False; self.btn_rec.config(state="normal"); self.btn_stop.config(state="disabled")
        self.lbl_rec.config(text="Ocioso", fg="gray")
        if not self.recording_data: return
        path = filedialog.asksaveasfilename(defaultextension=".txt", initialdir="./dados")
        if path:
            pd.DataFrame(self.recording_data, columns=["Tempo", "Temp", "Umid"]).to_csv(path, sep="\t", index=False)
            messagebox.showinfo("Sucesso", "Dados exportados!")

    def log_msg(self, msg):
        self.log.config(state="normal"); self.log.insert("end", msg); self.log.see("end"); self.log.config(state="disabled")

    def on_close(self):
        self.is_running = False; self.master.destroy()