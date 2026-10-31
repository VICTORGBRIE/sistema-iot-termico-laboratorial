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

# Variáveis Globais para o gráfico em tempo real (exibição contínua)
times = []
temperaturas = []
umidades = []

class ThermalControlApp:
    def __init__(self, master):
        self.master = master
        self.master.title("Monitoramento Térmico via Wi-Fi (ESP32)")
        self.master.geometry("1100x850")
        self.master.protocol("WM_DELETE_WINDOW", self.on_close)

        self.wifi = WifiManager()
        self.is_running = False
        self.start_time = None
        
        # --- Lógica de Gravação de Dados ---
        self.is_recording = False
        self.recording_data = [] # Buffer específico para a gravação manual
        
        self.command_buttons = []
        self.last_log_content = ""

        os.makedirs("dados", exist_ok=True)

        self.create_widgets()
        self.set_controls_state("disabled")

    def create_widgets(self):
        main_frame = ttk.Frame(self.master, padding=10)
        main_frame.pack(fill="both", expand=True)

        # 1. FRAME SUPERIOR (Conexão e Controles)
        top = ttk.Frame(main_frame)
        top.pack(side="top", fill="x")

        # --- Conexão ---
        conn = ttk.LabelFrame(top, text="Conexão Wi-Fi")
        conn.pack(side="left", fill="x", expand=True, padx=(0, 5))

        ttk.Label(conn, text="IP:").pack(side="left", padx=5)
        self.ip_entry = ttk.Entry(conn, width=15)
        self.ip_entry.insert(0, "10.228.1.92")
        self.ip_entry.pack(side="left", padx=5)

        self.btn_conn = ttk.Button(conn, text="Conectar", command=self.toggle)
        self.btn_conn.pack(side="left", padx=5)

        # --- Gravação ---
        rec_frame = ttk.LabelFrame(top, text="Sessão de Dados")
        rec_frame.pack(side="left", fill="x", expand=True, padx=5)

        self.btn_record = ttk.Button(rec_frame, text="🔴 Gravar", command=self.start_recording)
        self.btn_record.pack(side="left", padx=5, pady=5)

        self.btn_stop_rec = ttk.Button(rec_frame, text="⬛ Parar e Salvar", command=self.stop_recording)
        self.btn_stop_rec.pack(side="left", padx=5, pady=5)
        self.btn_stop_rec.config(state="disabled")

        # --- Status da Gravação ---
        self.rec_status_label = tk.Label(rec_frame, text="Status: Pronto", fg="gray")
        self.rec_status_label.pack(side="left", padx=10)

        # --- Comandos ESP32 ---
        cmds = ttk.LabelFrame(main_frame, text="Comandos de Controle")
        cmds.pack(side="top", fill="x", pady=5)

        def make_cmd_btn(cmd_text):
            btn = ttk.Button(cmds, text=cmd_text, command=lambda: self.send(cmd_text))
            btn.pack(side="left", padx=3, pady=5)
            self.command_buttons.append(btn)
            return btn

        for t in ["/select 1", "/select 2", "/def", "/play", "/stop_controle", "/status", "/stop"]:
            make_cmd_btn(t)

        # 2. FRAME CENTRAL (Gráfico)
        bottom = ttk.Frame(main_frame)
        bottom.pack(side="top", fill="both", expand=True, pady=5)

        status = ttk.LabelFrame(bottom, text="Leituras Atuais")
        status.pack(fill="x")

        self.lbl_temp = tk.Label(status, text="Temperatura: -- °C", font=('Arial', 22, 'bold'), fg="red")
        self.lbl_temp.pack(side="left", padx=30, pady=5)
        self.lbl_hum = tk.Label(status, text="Umidade: -- %", font=('Arial', 22, 'bold'), fg="blue")
        self.lbl_hum.pack(side="left", padx=30, pady=5)

        gframe = ttk.LabelFrame(bottom, text="Monitoramento Gráfico")
        gframe.pack(fill="both", expand=True)

        self.plot = PlotManager(gframe)
        self.plot.init()

        # 3. FRAME INFERIOR (Log)
        mid = ttk.Frame(main_frame)
        mid.pack(side="bottom", fill="x")

        logf = ttk.LabelFrame(mid, text="Console do Sistema (Wi-Fi)")
        logf.pack(fill="x", expand=True)

        self.log = scrolledtext.ScrolledText(logf, height=6, state="disabled", font=("Consolas", 10))
        self.log.pack(fill="x", expand=True)

    # --- Lógica de Gravação ---
    def start_recording(self):
        """Inicia a captura de dados para o arquivo"""
        self.is_recording = True
        self.recording_data = [] # Limpa buffer de gravação
        self.btn_record.config(state="disabled")
        self.btn_stop_rec.config(state="normal")
        self.rec_status_label.config(text="Status: GRAVANDO...", fg="red")
        self.log_msg("\n[SISTEMA] Gravação iniciada. Capturando dados...\n")

    def stop_recording(self):
        """Para a captura e abre janela para salvar"""
        if not self.is_recording:
            return

        self.is_recording = False
        self.btn_record.config(state="normal")
        self.btn_stop_rec.config(state="disabled")
        self.rec_status_label.config(text="Status: Pronto", fg="gray")

        if not self.recording_data:
            messagebox.showwarning("Aviso", "Nenhum dado foi capturado durante a gravação.")
            return

        # Abre diálogo para nomear o arquivo
        file_path = filedialog.asksaveasfilename(
            initialdir="./dados",
            title="Salvar Dados da Gravação",
            defaultextension=".txt",
            filetypes=(("Arquivo de Texto", "*.txt"), ("Todos os arquivos", "*.*"))
        )

        if file_path:
            try:
                df = pd.DataFrame(self.recording_data, columns=["Tempo (s)", "Temperatura (C)", "Umidade (%)"])
                df.to_csv(file_path, sep="\t", index=False)
                messagebox.showinfo("Sucesso", f"Dados salvos com sucesso em:\n{file_path}")
                self.log_msg(f"[SISTEMA] Gravação salva: {file_path}\n")
            except Exception as e:
                messagebox.showerror("Erro", f"Falha ao salvar arquivo: {e}")
        
        self.recording_data = []

    # --- Funções de Estado e Conexão ---
    def set_controls_state(self, state):
        for btn in self.command_buttons:
            btn.config(state=state)
        self.btn_record.config(state=state)

    def toggle(self):
        if self.wifi.is_connected():
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        ip = self.ip_entry.get().strip()
        if not ip:
            messagebox.showerror("Erro", "Digite o IP do ESP32.")
            return
        self.log_msg(f"Conectando a {ip}...\n")
        threading.Thread(target=self._connect_thread, args=(ip,), daemon=True).start()

    def _connect_thread(self, ip):
        if self.wifi.connect(ip):
            self.master.after(0, self._on_connect_success)
        else:
            self.master.after(0, lambda: messagebox.showerror("Erro", "Falha ao conectar no IP."))

    def _on_connect_success(self):
        self.btn_conn.config(text="Desconectar")
        self.ip_entry.config(state="disabled")
        self.set_controls_state("normal")
        self.is_running = True
        threading.Thread(target=self.read_thread, daemon=True).start()
        self.log_msg(f"Conectado ao Gateway!\n")

    def disconnect(self):
        self.is_running = False
        if self.is_recording:
            self.stop_recording()
        self.wifi.disconnect()
        self.btn_conn.config(text="Conectar")
        self.ip_entry.config(state="normal")
        self.set_controls_state("disabled")
        self.log_msg("Desconectado.\n")
        self.start_time = None

    def read_thread(self):
        while self.is_running:
            full_log = self.wifi.get_log()
            if full_log is not None:
                self.master.after(0, self.process_log_chunk, full_log)
            time.sleep(1.2)

    def process_log_chunk(self, full_log):
        if len(full_log) < len(self.last_log_content):
            self.last_log_content = ""
        
        new_data = full_log[len(self.last_log_content):]
        if not new_data: return

        self.last_log_content = full_log
        self.log_msg(new_data)

        for line in new_data.split('\n'):
            self.process_data_line(line)
        
        if len(full_log) > 3000:
            threading.Thread(target=self.wifi.clear_esp_log, daemon=True).start()
            self.last_log_content = ""

    def process_data_line(self, line):
        payload = is_data_line(line)
        if not payload: return

        try:
            if self.start_time is None:
                self.start_time = datetime.datetime.now()

            temp, hum = parse_payload(payload)
            if temp is None: return

            t_rel = (datetime.datetime.now() - self.start_time).total_seconds()
            h_val = hum if hum is not None else 0.0

            # 1. Atualiza Gráfico e Labels (Sempre)
            times.append(t_rel)
            temperaturas.append(temp)
            self.lbl_temp.config(text=f"{temp:.2f} °C")
            self.lbl_hum.config(text=f"{h_val:.2f} %")
            self.plot.update(times, temperaturas)

            # 2. SE estiver gravando, adiciona ao buffer de gravação
            if self.is_recording:
                self.recording_data.append((t_rel, temp, h_val))

        except Exception:
            pass 

    def send(self, cmd):
        if self.wifi.is_connected():
            threading.Thread(target=self.wifi.send_command, args=(cmd,), daemon=True).start()
            self.log_msg(f"Comando -> {cmd}\n")

    def send_manual(self, event=None):
        txt = self.entry.get().strip()
        if txt and self.wifi.is_connected():
            self.send(txt)
            self.entry.set("")

    def log_msg(self, msg):
        self.log.config(state="normal")
        self.log.insert("end", msg)
        self.log.see("end")
        self.log.config(state="disabled")

    def on_close(self):
        self.is_running = False
        self.master.destroy()