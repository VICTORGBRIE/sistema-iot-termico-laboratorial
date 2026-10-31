def parse_payload(line):
    """
    Extrai temperatura e umidade de:
      - "T=28.70C, H=33.00%" (do "DADOS:")
      - "Temp: 29.2"
      - "T: 30"
    """
    temp = None
    hum = None # Alterado para None por defeito

    try:
        if ',' in line:
            # Formato "T=28.70C, H=33.00%"
            a, b = line.split(',', 1)
            temp = extract(a)
            hum = extract(b)
        else:
            # Formato "Temp: 29.2"
            temp = extract(line)

    except Exception as e:
        print(f"Erro no parser: {e} | Linha: {line}")
        return None, None

    return temp, hum


def extract(s):
    """Extrai o primeiro número (float ou int) de uma string."""
    num = ""
    found_decimal = False
    found_digit = False
    
    for ch in s:
        if ch.isdigit():
            num += ch
            found_digit = True
        elif ch in ".-" and not num: # Aceita sinal no início
             num += ch
        elif ch == '.' and not found_decimal and found_digit: # Aceita um ponto decimal
            num += ch
            found_decimal = True
        elif num:
            # Se já encontrámos um número e o caracter atual não é
            # um dígito ou ponto, parámos de ler o número.
            break
            
    return float(num) if num and num not in "-" else None