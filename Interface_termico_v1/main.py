import tkinter as tk
from gui import ThermalControlApp

# ==========================
# INICIALIZAÇÃO DA APP
# ==========================
if __name__ == "__main__":
    root = tk.Tk()
    app = ThermalControlApp(root)
    root.mainloop()