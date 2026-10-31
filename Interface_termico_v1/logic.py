def is_data_line(line):
    """
    Verifica se uma linha é uma linha de dados.
    Retorna o payload (a parte útil) se for, ou None se não for.
    """
    if line.startswith("DADOS:"):
        return line[len("DADOS:"):].strip()
    
    # Suporta formatos alternativos que o Arduino possa enviar
    lower_line = line.lower()
    if lower_line.startswith("temp:") or lower_line.startswith("t:"):
        return line # Envia a linha inteira para o parser
        
    return None