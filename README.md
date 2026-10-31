# 🌡️ Sistema IoT Térmico Laboratorial: Arquitetura Gateway-Edge e Controle RST

## 📖 Sobre o Projeto

Este repositório contém o código-fonte e a documentação do projeto **"Do Sensor à Nuvem"**, focado no desenvolvimento, identificação e controle de uma câmara térmica de baixo custo.

Para contornar os desafios de latência em sistemas de controle de malha fechada na Internet das Coisas (IoT), o projeto adota uma **arquitetura distribuída Gateway-Edge**. As tarefas de comunicação sem fio e roteamento HTTP são segregadas em um microcontrolador ESP32 (Gateway), enquanto um Arduino Mega 2560 (Nó *Edge*) opera em *bare-metal* para garantir o determinismo temporal exigido no acionamento síncrono de um Dimmer AC (*Zero-Cross*).

## ✨ Principais Funcionalidades

* **Identificação de Sistemas:** O modelo dinâmico da câmara térmica foi extraído via resposta ao degrau em malha aberta, resultando em uma função de transferência contínua de primeira ordem (Ganho $K = 0,6418$ e Constante de Tempo $\tau = 213,30s$).


* **Controle Discreto RST:** Controlador polinomial sintetizado analiticamente via alocação de polos (Equação Diofantina), projetado para um tempo de acomodação de 40s e fator de amortecimento de 0,6. Garante rastreamento de *setpoint* com erro nulo em regime permanente.


* **Processamento Digital de Sinais (DSP):** Implementação de filtro de Média Móvel Exponencial (EMA) no microcontrolador *Edge* para atenuação de ruído de quantização sem atrasos de fase severos, além de compensação de dinâmicas não-lineares (PTC).


* **Telemetria e Datalogging:** Interface supervisória *desktop* desenvolvida em Python para monitoramento de dados via gráficos em tempo real e exportação autônoma de parâmetros físicos.



## 🛠️ Tecnologias e Hardware Utilizado

**Hardware:**

* Arduino Mega 2560 (Nó *Edge* para laço de controle)


* ESP32 SoC (Gateway para conectividade Wi-Fi/HTTP)


* Módulos Bluetooth HC-05 e HC-06 (Comunicação serial assíncrona MAC a MAC)


* Módulo Dimmer AC com Triac Optoacoplado


* Driver Ponte H para controle PWM de exaustão


* Atuadores resistivos (lâmpadas de halogênio)



**Software:**

* **C/C++:** Firmware embarcado no Arduino e no ESP32.


* **Python:** Aplicação supervisória utilizando processamento paralelo (*multithreading*), `Matplotlib` (plotagem gráfica) e `Pandas` (registro de dados em arquivos de texto).


* **MATLAB:** Utilizado na etapa de extração matemática de parâmetros (metodologia dos 63,2%) para o modelo contínuo.


## 📚 Referência

Este código é parte integrante da pesquisa apresentada no artigo:

> *Menezes, V. G.; Ayres Júnior, F. A. de C. "Do Sensor à Nuvem: Desenvolvimento, Identificação e Controlo de um Sistema IoT Térmico Laboratorial." Universidade Federal do Amazonas (UFAM).*
