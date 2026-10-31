def is_data_line(line):
    """
    Identifica se a linha contém dados térmicos enviados pelo ESP32 de forma robusta.
    Funciona mesmo que o prefixo varie entre "DADOS", "Dados" ou "dados".
    """
    if not line:
        return None

    line_upper = line.upper()
    
    # Busca a palavra "DADOS" de forma insensível ao caso
    if "DADOS" in line_upper:
        try:
            # Encontra a posição exata onde a palavra "DADOS" começa
            inicio_dados = line_upper.find("DADOS")
            
            # Corta a string a partir do fim da palavra "DADOS" (5 caracteres)
            # Isso ignora prefixos como "Arduino ->" automaticamente
            payload = line[inicio_dados + 5:]
            
            # Remove ":" e espaços que sobram (ex: ": T=25.0" vira "T=25.0")
            payload_limpo = payload.replace(":", "").strip()
            
            if payload_limpo:
                return payload_limpo
        except Exception:
            return None
            
    # Fallback: Se não tem a tag "DADOS", mas tem os identificadores de temperatura
    if "T=" in line_upper or "TEMP" in line_upper:
        return line.strip()
        
    return None