import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import datetime
import pandas as pd
import threading
import time
import os

from serial_manager import SerialManager
from data_parser import parse_payload
from logic import is_data_line
from plot_manager import PlotManager

# --------------------------------------------------
# VARIÁVEIS GLOBAIS DO GRÁFICO
# --------------------------------------------------
times = []
temperaturas = []
umidades = []

BAUD_RATE = 115200


class ThermalControlApp:

    def __init__(self, master):
        self.master = master
        self.master.title("Controle e Monitoramento Térmico v2 (ESP32-Arduino)")
        self.master.geometry("1000x800")
        self.master.protocol("WM_DELETE_WINDOW", self.on_close)

        self.serial = SerialManager()
        self.data_buffer = []
        self.is_running = False
        self.start_time = None
        
        self.command_buttons = []

        os.makedirs("dados", exist_ok=True)

        self.create_widgets()
        self.set_controls_state("disabled")

    # -----------------------------------------------------
    # INTERFACE GRÁFICA (Layout Reorganizado)
    # -----------------------------------------------------
    def create_widgets(self):
        main_frame = ttk.Frame(self.master, padding=10)
        main_frame.pack(fill="both", expand=True)

        # ============================================
        # FRAME SUPERIOR (Fica no topo)
        # ============================================
        top = ttk.Frame(main_frame)
        top.pack(fill="x")

        conn = ttk.LabelFrame(top, text="Conexão ESP32")
        conn.pack(side="left", fill="x", expand=True)

        ttk.Label(conn, text="Porta:").pack(side="left", padx=5)
        self.port = tk.StringVar()
        self.cmb = ttk.Combobox(conn, textvariable=self.port, width=10)
        self.cmb["values"] = self.serial.list_ports()
        if self.cmb["values"]:
            self.cmb.current(0)
        self.cmb.pack(side="left", padx=5)

        self.btn_conn = ttk.Button(conn, text="Conectar", command=self.toggle)
        self.btn_conn.pack(side="left", padx=5)

        # BOTÕES DE COMANDO
        cmds = ttk.LabelFrame(top, text="Controle")
        cmds.pack(side="left", fill="x", expand=True, padx=5)

        def b(txt): 
            btn = ttk.Button(cmds, text=txt, command=lambda t=txt: self.send(t))
            return btn

        for t in ["/select 1", "/select 2", "/def", "/play",
                  "/stop_controle", "/status", "/stop"]:
            btn = b(t)
            btn.pack(side="left", padx=3)
            self.command_buttons.append(btn) 

        # ============================================
        # FRAME DO GRÁFICO (Agora no meio, expansível)
        # ============================================
        bottom = ttk.Frame(main_frame)
        bottom.pack(fill="both", expand=True, pady=10) # <-- Alterado para expandir

        status = ttk.LabelFrame(bottom, text="Leituras Atuais")
        status.pack(fill="x")

        self.lbl_temp = tk.Label(status, text="Temperatura: -- °C",
                                  font=('Arial', 18, 'bold'), fg="red")
        self.lbl_temp.pack(side="left", padx=20)
        self.lbl_hum = tk.Label(status, text="Umidade: -- %",
                                 font=('Arial', 18, 'bold'), fg="blue")
        self.lbl_hum.pack(side="left", padx=20)

        gframe = ttk.LabelFrame(bottom, text="Gráfico")
        gframe.pack(fill="both", expand=True)

        self.plot = PlotManager(gframe)
        self.plot.init()
        
        # ============================================
        # FRAME INFERIOR (Log + Envio) (Movido para baixo)
        # ============================================
        mid = ttk.Frame(main_frame)
        mid.pack(fill="x") # <-- Fica na parte de baixo

        logf = ttk.LabelFrame(mid, text="Log")
        logf.pack(fill="x", expand=True)

        self.log = scrolledtext.ScrolledText(logf, height=8, state="disabled")
        self.log.pack(fill="x", expand=True)

        sendf = ttk.LabelFrame(mid, text="Enviar")
        sendf.pack(fill="x", expand=True)

        self.entry = tk.StringVar()
        self.ent = ttk.Entry(sendf, textvariable=self.entry, state="disabled")
        self.ent.pack(side="left", fill="x", expand=True, padx=5)
        self.ent.bind("<Return>", self.send_manual)

        self.btn_send = ttk.Button(sendf, text="Enviar",
                                  command=self.send_manual, state="disabled")
        self.btn_send.pack(side="left", padx=5)


    # -----------------------------------------------------
    def set_controls_state(self, state):
        """Habilita ou desabilita todos os botões e entradas de controlo"""
        for btn in self.command_buttons:
            btn.config(state=state)
        self.ent.config(state=state)
        self.btn_send.config(state=state)
    
    # -----------------------------------------------------
    # FUNÇÕES DO SERIAL
    # -----------------------------------------------------
    def toggle(self):
        if self.serial.is_open():
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port.get()
        if not port:
            messagebox.showerror("Erro", "Escolha a porta.")
            return

        if self.serial.connect(port, BAUD_RATE):
            self.btn_conn.config(text="Desconectar")
            self.cmb.config(state="disabled")
            self.set_controls_state("normal") # Habilita os controlos
            self.is_running = True
            self.thread = threading.Thread(target=self.read_thread, daemon=True)
            self.thread.start()
            self.log_msg(f"Conectado em {port}\n")
        else:
            messagebox.showerror("Erro", "Falha ao conectar.")

    def disconnect(self):
        self.is_running = False
        self.serial.disconnect()
        self.btn_conn.config(text="Conectar")
        self.cmb.config(state="normal")
        self.set_controls_state("disabled") # Desabilita os controlos
        self.log_msg("Desconectado.\n")
        self.save()
        self.start_time = None # Reseta o tempo

    def read_thread(self):
        while self.is_running:
            line = self.serial.readline()
            if line:
                self.master.after(0, self.process, line)
            elif not self.serial.is_open() and self.is_running:
                # Ocorreu um erro na leitura (ex: cabo desconectado)
                self.master.after(0, self.disconnect)
                break

    # -----------------------------------------------------
    def process(self, line):
        self.log_msg(f"ESP32 -> {line}\n")

        payload = is_data_line(line)
        if not payload:
            return

        try:
            if self.start_time is None:
                self.start_time = datetime.datetime.now()

            temp, hum = parse_payload(payload)
            if temp is None:
                raise ValueError("Temperatura inválida.")

            t = (datetime.datetime.now() - self.start_time).total_seconds()

            times.append(t)
            temperaturas.append(temp)
            umidades.append(hum if hum is not None else 0.0) # Garante que hum não é None
            self.data_buffer.append((t, temp, hum))

            self.lbl_temp.config(text=f"Temperatura: {temp:.2f} °C")
            self.lbl_hum.config(text=f"Umidade: {hum if hum is not None else 0.0:.2f} %")

            self.plot.update(times, temperaturas)

        except Exception as e:
            self.log_msg(f"Erro ao interpretar: {e}\n")

    # -----------------------------------------------------
    def send(self, cmd):
        if self.serial.is_open():
            self.serial.write(cmd)
            self.log_msg(f"Enviando -> {cmd}\n")

    def send_manual(self, event=None):
        txt = self.entry.get().strip()
        if txt and self.serial.is_open():
            self.serial.write(txt)
            self.log_msg(f"Enviando -> {txt}\n")
            self.entry.set("")

    # -----------------------------------------------------
    def save(self):
        if not self.data_buffer:
            return

        df = pd.DataFrame(self.data_buffer,
                          columns=["Tempo", "Temp", "Umid"])
        name = f"dados/data_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        df.to_csv(name, sep="\t", index=False)
        print("Dados salvos:", name)

        self.data_buffer.clear()
        # Limpa os dados do gráfico também
        times.clear()
        temperaturas.clear()
        umidades.clear()
        self.plot.init()


    def log_msg(self, msg):
        self.log.config(state="normal")
        self.log.insert("end", msg)
        self.log.see("end")
        self.log.config(state="disabled")

    def on_close(self):
        self.is_running = False
        if self.serial.is_open():
            self.serial.write("/stop_controle") # Envia comando de paragem
        self.serial.disconnect()
        self.save()
        self.master.destroy()