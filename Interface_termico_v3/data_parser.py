import re

def parse_payload(line):
    """
    Utiliza Expressões Regulares (Regex) para extrair valores numéricos decimais.
    Converte strings como "T=28.50C, H=68.00%" em números (floats).
    """
    temp = None
    hum = None

    try:
        # Regex para encontrar números inteiros ou decimais (incluindo negativos)
        # Encontra padrões como 28.50, 68, -5.0 etc.
        numeros = re.findall(r"[-+]?\d*\.\d+|\d+", line)
        
        if len(numeros) >= 1:
            temp = float(numeros[0]) # O primeiro número é sempre a Temperatura
            
        if len(numeros) >= 2:
            hum = float(numeros[1])  # O segundo número (se existir) é a Umidade
            
    except Exception as e:
        # Silencia erros de parse para não travar a interface
        pass

    return temp, hum