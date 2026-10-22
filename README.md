# Sonda TTU - STM32F103

Firmware da sonda de campo para rede RS485, com:
- instalacao e desinstalacao por protocolo
- retencao de ID (1..4) em flash
- leitura de temperatura e umidade do ar via SHT30
- campo de umidade do solo reservado (placeholder)
- LED de status em PC13

## Objetivo

Este projeto implementa o lado da sonda no protocolo descrito para o ecossistema Smart Horta.
A sonda pode operar em dois estados:
- desinstalada (ID 0xFF)
- instalada (ID 1..4)

Quando instalada, ela responde consultas de leitura enderecadas ao proprio ID.
Quando desinstalada, ela responde ao registrador de descoberta `0x11` sem mudar
seu estado; a atribuicao pelo registrador `0x10` grava o novo ID. Mantenha apenas
uma sonda virgem conectada durante a descoberta, pois sondas virgens simultaneas
compartilham o mesmo ID de broadcast e podem responder juntas.

## Arquitetura

### 1) Configuracao de perifericos centralizada

Toda configuracao de hardware fica concentrada em:
- Inc/peripherals_init.h
- Src/peripherals_init.c

Esse modulo centraliza:
- clock (SystemInit, 64 MHz)
- SWD
- timer base de 1 ms (TIM2)
- GPIO do LED (PC13)
- I2C1 (SHT30)
- USART1 em RS485 (PA8 como DE/DTR, PA9 TX, PA10 RX)

### 2) Modulos funcionais separados

- Aplicacao: Inc/application.h + Src/application.c
- Protocolo de probe: Inc/probe_protocol.h + Src/probe_protocol.c
- Persistencia de ID: Inc/probe_id_storage.h + Src/probe_id_storage.c
- Sensor SHT30: Inc/sht30.h + Src/sht30.c

## Mapeamento de pinos

### RS485 (USART1)
- PA8: DE/DTR do transceiver RS485 (controle de direcao)
- PA9: TX USART1
- PA10: RX USART1

### I2C1 (SHT30)
- PB6: SCL
- PB7: SDA

### LED de status
- PC13: LED onboard (ativo em nivel baixo)

### Debug
- PA13: SWDIO
- PA14: SWCLK

## Observacoes eletricas

- O firmware usa SWD (JTAG desabilitado) para liberar pinos e manter depuracao.
- O LED em PC13 e ligado com nivel logico 0 e desligado com nivel logico 1.
- O pino PA8 e usado como DE para RS485: transmissao quando alto, recepcao quando baixo.
- DE e /RE devem estar ligados juntos a PA8. A resposta aguarda 2 ticks de 1 ms
  antes de habilitar TX, e PA8 permanece alto ate USART1 indicar TC.
- A recepcao USART1 usa interrupcao e buffer circular de 128 bytes (127 uteis)
  para preservar comandos durante a conversao bloqueante do SHT30.

## Comportamento funcional

### Instalacao

- Recebe comando de instalacao (registro 0x10) com ID 1..4.
- Persiste o ID em flash.
- Passa a responder leituras para esse ID.
- Repetir o mesmo ID reenvia ACK sem nova gravacao; outro ID e ignorado.
- O ACK inclui o mesmo payload de 6 bytes da leitura para a tela de instalacao.

### Desinstalacao

- Recebe o registrador `0x12` com o proprio ID (1..4).
- Volta para estado desinstalado.
- Persiste estado desinstalado em flash.

### Leitura

- Recebe comando de leitura (registro 0x20) com seu ID.
- Responde com payload:
  - temperatura do ar (x10)
  - umidade do ar (x10)
  - umidade do solo (x10, placeholder atual)

### LED PC13

- Desinstalada: LED aceso continuo.
- Instalada e comunicando: LED pisca 2 vezes a cada comunicacao valida e volta apagado.

## Formato de frame

Formato base:
- byte 0: 0x55
- byte 1: versao (0x01)
- byte 2: tamanho
- byte 3: registrador
- byte 4: id
- byte 5: reservado
- payload
- CRC16 Modbus (low, high)
- byte final: 0x55

Registradores:
- 0x10: atribuicao de ID (virgem instala; instalada confirma somente o mesmo ID)
- 0x11: descoberta broadcast (somente sondas virgens respondem; nao altera estado)
- 0x12: desinstalacao enderecada (ID 1..4)
- 0x20: leitura

## Build

Projeto CMake para ARM GCC.

Arquivos principais de build:
- CMakeLists.txt
- cmake/files.cmake
- cmake/gcc-arm-none-eabi.cmake

## Fluxo de execucao

1. Startup assembly chama SystemInit.
2. main chama application_run.
3. application:
   - inicializa perifericos e protocolo
   - atualiza leituras do SHT30 periodicamente
   - processa protocolo RS485 continuamente

## Observacoes

- Endereco de flash para ID foi definido na ultima pagina do STM32F103C8 (0x0800FC00).
- Umidade do solo permanece como placeholder ate integracao do sensor fisico.
- O parser de frame e nao bloqueante, com recepcao incremental byte a byte.
- Um frame incompleto e descartado apos 30 ms sem progresso do parser.

## Testes Host

Na raiz de `sonda-ttu`, em Linux com compilador C:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I Inc tests/test_probe_protocol.c -o /tmp/test-probe-protocol
/tmp/test-probe-protocol
cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -I Inc tests/test_rs485_uart.c -o /tmp/test-rs485-uart
/tmp/test-rs485-uart
```

Os testes usam o codigo real com UART/persistencia ou registradores simulados.
Cobrem ACK repetido apos perda/reboot, telemetria na instalacao e leitura,
enderecamento, timeout de frame, recepcao por IRQ, erros/overflow e retorno de
DE a RX somente depois de TC. Nao substituem build ARM nem teste no transceiver.
