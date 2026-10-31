import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

class PlotManager:
    def __init__(self, frame):
        # Cria a figura e os eixos para o gráfico
        self.fig, self.ax = plt.subplots(figsize=(10, 5), dpi=100)
        self.canvas = FigureCanvasTkAgg(self.fig, master=frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)
        
        # Configurações iniciais de estilo
        self.fig.patch.set_facecolor('#f4f4f4')
        self.ax.set_facecolor('#ffffff')

    def init(self):
        self.ax.clear()
        self.ax.set_title("Aguardando recebimento de dados via Wi-Fi...")
        self.ax.set_xlabel("Tempo Decorrido (s)")
        self.ax.set_ylabel("Temperatura (°C)")
        self.ax.grid(True, linestyle='--', alpha=0.6)
        self.canvas.draw()

    def update(self, times, temps):
        if not times or not temps:
            return

        # Limpa os eixos para desenhar a nova versão do gráfico
        self.ax.clear()
        
        # Desenha a linha da temperatura
        self.ax.plot(times, temps, color='#d32f2f', linewidth=2, label='Temperatura (°C)')
        
        # Títulos e legendas
        self.ax.set_title("Monitoramento Térmico em Tempo Real", fontsize=12, fontweight='bold')
        self.ax.set_xlabel("Tempo (s)")
        self.ax.set_ylabel("Temperatura (°C)")
        self.ax.legend(loc='upper right')
        self.ax.grid(True, linestyle='--', alpha=0.6)
        
        # Lógica de Janela Deslizante: Mantém os últimos 60 segundos visíveis
        if times[-1] > 60:
            self.ax.set_xlim(times[-1] - 60, times[-1] + 2)
        else:
            self.ax.set_xlim(0, 62)

        # Ajuste dinâmico do eixo Y para a temperatura
        if temps:
            margin = 2.0
            self.ax.set_ylim(min(temps) - margin, max(temps) + margin)
        
        self.canvas.draw()