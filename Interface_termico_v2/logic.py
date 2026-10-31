def is_data_line(line):
    # O ESP32 adiciona "Arduino -> " antes da linha original
    # Vamos procurar por "DADOS:" em qualquer lugar da linha
    if "DADOS:" in line:
        # Pega tudo depois de "DADOS:"
        return line.split("DADOS:")[1].strip()
    
    return None