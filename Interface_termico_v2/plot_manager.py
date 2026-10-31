import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

class PlotManager:

    def __init__(self, frame):
        self.fig, self.ax = plt.subplots(figsize=(14, 6))
        self.canvas = FigureCanvasTkAgg(self.fig, master=frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

    def init(self):
        self.ax.clear()
        self.ax.set_title("Aguardando dados...")
        self.ax.set_xlabel("Tempo (s)")
        self.ax.set_ylabel("Temperatura (°C)")
        self.ax.grid(True)
        self.canvas.draw()

    def update(self, times, temps):
        self.ax.clear()
        # --- CORREÇÃO: Adiciona título ao gráfico atualizado ---
        self.ax.set_title("Temperatura em Tempo Real")
        self.ax.plot(times, temps, label='Temperatura (°C)', color='red', linewidth=2)
        self.ax.set_xlabel("Tempo (s)")
        self.ax.set_ylabel("Temperatura (°C)")
        self.ax.legend(loc='upper right')
        self.ax.grid(True)
        if times:
            self.ax.set_xlim(left=max(0, times[-1] - 100), right=times[-1] + 5)
        if temps:
            self.ax.set_ylim(bottom=min(temps) - 2, top=max(temps) + 2)
            
        self.canvas.draw()