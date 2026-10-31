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

times = []
temperaturas = []

class ThermalControlApp:
    def __init__(self, master):
        self.master = master
        self.master.title("PIBIC - Controle Térmico IoT")
        self.master.geometry("1100x900")
        self.master.configure(bg="#f4f4f4")
        self.master.protocol("WM_DELETE_WINDOW", self.on_close)

        self.wifi = WifiManager()
        self.is_running = False
        self.start_time = None 
        
        # --- Variável Crucial para corrigir texto quebrado ---
        self.stream_buffer = "" 
        
        self.is_recording = False
        self.recording_data = []
        self.command_buttons = []

        os.makedirs("dados", exist_ok=True)
        self.create_widgets()
        self.set_controls_state("disabled")

    def create_widgets(self):
        main_frame = ttk.Frame(self.master, padding=10)
        main_frame.pack(fill="both", expand=True)

        # 1. VALOR
        self.lbl_val = tk.Label(main_frame, text="-- °C", font=("Arial", 40, "bold"), fg="#d32f2f", bg="#f4f4f4")
        self.lbl_val.pack(pady=(5, 10))

        # 2. CONTROLES
        control_top = ttk.Frame(main_frame)
        control_top.pack(fill="x", pady=5)

        # Conexão
        conn = ttk.LabelFrame(control_top, text="Gateway")
        conn.pack(side="left", padx=5)
        self.ip_entry = ttk.Entry(conn, width=18)
        self.ip_entry.insert(0, "esp-thermal.local")
        self.ip_entry.pack(side="left", padx=5, pady=5)
        self.btn_conn = ttk.Button(conn, text="Conectar", command=self.toggle_connection)
        self.btn_conn.pack(side="left", padx=5, pady=5)

        # Atalhos
        cmds = ttk.LabelFrame(control_top, text="Comandos Rápidos")
        cmds.pack(side="left", fill="x", expand=True, padx=5)
        
        # Lista atualizada com comandos importantes
        lista = ["/select 1", "/select 2", "/status", "/def", "/play", "/stop", "/ventoinha on", "/ventoinha off"]
        for t in lista:
            btn = ttk.Button(cmds, text=t, command=lambda cmd=t: self.send(cmd))
            btn.pack(side="left", padx=2)
            self.command_buttons.append(btn)

        # Sessão
        rec = ttk.LabelFrame(control_top, text="Sessão")
        rec.pack(side="right", padx=5)
        self.btn_rec = ttk.Button(rec, text="🔴 Gravar", command=self.start_recording)
        self.btn_rec.pack(side="left", padx=5)
        self.btn_stop = ttk.Button(rec, text="⬛ Salvar", command=self.stop_recording)
        self.btn_stop.pack(side="left", padx=5)

        # 3. GRÁFICO
        tk.Label(main_frame, text="Monitoramento de Temperatura", font=("Arial", 10), bg="#f4f4f4").pack(anchor="w", padx=5)
        self.gframe = ttk.Frame(main_frame)
        self.gframe.pack(fill="both", expand=True, pady=5)
        self.plot = PlotManager(self.gframe)
        self.plot.init()

        # 4. CONSOLES
        manual_frame = ttk.LabelFrame(main_frame, text="Console de Comando")
        manual_frame.pack(fill="x", pady=5)
        self.cmd_var = tk.StringVar()
        self.ent_cmd = ttk.Entry(manual_frame, textvariable=self.cmd_var)
        self.ent_cmd.pack(side="left", fill="x", expand=True, padx=5, pady=10)
        self.ent_cmd.bind("<Return>", self.send_manual)
        self.btn_manual = ttk.Button(manual_frame, text="Enviar", command=self.send_manual)
        self.btn_manual.pack(side="right", padx=5)

        log_frame = ttk.LabelFrame(main_frame, text="Monitor Serial da Interface")
        log_frame.pack(fill="x", pady=5)
        self.log = scrolledtext.ScrolledText(log_frame, height=8, state="disabled", font=("Consolas", 10), bg="white")
        self.log.pack(fill="x", padx=5, pady=5)

    def set_controls_state(self, state):
        for btn in self.command_buttons: btn.config(state=state)
        self.btn_rec.config(state=state)
        self.btn_stop.config(state="disabled")
        self.ent_cmd.config(state=state)
        self.btn_manual.config(state=state)

    def toggle_connection(self):
        if self.is_running: self.disconnect()
        else: self.connect()

    def connect(self):
        ip = self.ip_entry.get().strip()
        if not ip: return
        self.log_msg(f"Conectando a {ip}...\n")
        
        # Limpa o buffer de stream ao conectar
        self.stream_buffer = ""
        
        if self.wifi.connect(ip):
            self.btn_conn.config(text="Desconectar")
            self.set_controls_state("normal")
            self.is_running = True
            self.start_time = None 
            threading.Thread(target=self.update_loop, daemon=True).start()
            self.log_msg("Conectado com sucesso!\n")
        else:
            messagebox.showerror("Erro", "Não foi possível alcançar o ESP32.")

    def disconnect(self):
        self.is_running = False
        self.btn_conn.config(text="Conectar")
        self.set_controls_state("disabled")
        self.log_msg("Sessão terminada.\n")

    def update_loop(self):
        while self.is_running:
            new_data = self.wifi.get_log()
            if new_data:
                self.master.after(0, self.process_new_data, new_data)
            time.sleep(0.5) # Polling mais rápido (500ms) para evitar acumulo

    def process_new_data(self, text):
        if not text: return
        
        # Mostra o texto bruto no log (opcional, bom para debug)
        self.log_msg(text)

        # --- LÓGICA DE REMONTAGEM DE PACOTES ---
        # Adiciona o novo texto ao buffer existente
        self.stream_buffer += text
        
        # Se houver quebras de linha, processamos as linhas completas
        if '\n' in self.stream_buffer:
            lines = self.stream_buffer.split('\n')
            
            # A última parte pode estar incompleta (ex: "DADOS: T=2"), 
            # então guardamos ela de volta no buffer para a próxima rodada
            self.stream_buffer = lines.pop() 
            
            # Processa todas as linhas que estão completas
            for line in lines:
                self.parse_line(line)

    def parse_line(self, line):
        payload = is_data_line(line)
        if payload:
            t, h = parse_payload(payload)
            if t is not None:
                if self.start_time is None:
                    self.start_time = datetime.datetime.now()
                
                t_rel = (datetime.datetime.now() - self.start_time).total_seconds()
                times.append(t_rel)
                temperaturas.append(t)
                
                self.lbl_val.config(text=f"{t:.2f} °C")
                self.plot.update(times, temperaturas)
                
                if self.is_recording:
                    self.recording_data.append((t_rel, t, h if h else 0.0))

    def send(self, cmd):
        self.log_msg(f"Enviando -> {cmd}\n")
        if cmd == "/play" or cmd.startswith("/select"):
            global times, temperaturas
            times.clear(); temperaturas.clear(); self.start_time = None
            self.plot.init()
            
        threading.Thread(target=self.wifi.send_command, args=(cmd,), daemon=True).start()

    def send_manual(self, e=None):
        cmd = self.cmd_var.get().strip()
        if cmd:
            self.send(cmd)
            self.cmd_var.set("")

    def start_recording(self):
        self.send("/play")
        self.is_recording = True
        self.recording_data = []
        self.btn_rec.config(state="disabled")
        self.btn_stop.config(state="normal")

    def stop_recording(self):
        self.is_recording = False
        self.btn_rec.config(state="normal")
        self.btn_stop.config(state="disabled")
        self.send("/stop")
        if not self.recording_data: return
        path = filedialog.asksaveasfilename(defaultextension=".txt", initialdir="./dados")
        if path:
            pd.DataFrame(self.recording_data, columns=["Tempo", "Temp", "Umid"]).to_csv(path, sep="\t", index=False)
            messagebox.showinfo("Sucesso", "Dados guardados!")

    def log_msg(self, msg):
        self.log.config(state="normal")
        self.log.insert("end", msg)
        self.log.see("end")
        self.log.config(state="disabled")

    def on_close(self):
        self.is_running = False
        self.master.destroy()