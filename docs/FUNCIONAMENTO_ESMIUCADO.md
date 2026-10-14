# Funcionamento Esmiucado - Sonda TTU

Este documento detalha o funcionamento interno do firmware da sonda, com foco em:
- inicializacao
- protocolo RS485
- retencao de ID
- leituras de sensores
- comportamento do LED

## 1. Panorama de modulos

### 1.1 Modulo de hardware centralizado
- Inc/peripherals_init.h
- Src/peripherals_init.c

Responsavel por toda a camada de perifericos:
- clock em 64 MHz
- SWD
- TIM2 em base de 1 ms
- GPIO LED PC13
- I2C1 fast mode
- USART1 + direcao RS485

### 1.2 Modulos funcionais
- Src/application.c: loop principal da aplicacao
- Src/probe_protocol.c: parser e respostas do protocolo
- Src/probe_id_storage.c: persistencia de ID em flash
- Src/sht30.c: leitura de temperatura e umidade do ar

## 2. Sequencia de boot

1. Reset do microcontrolador.
2. Startup assembly executa SystemInit.
3. SystemInit configura:
   - HSE
   - PLL x8 (64 MHz)
   - waits de flash
4. main entra em application_run.
5. application_run executa:
   - peripherals_init()
   - probe_protocol_init()
6. Loop infinito:
   - atualiza medicao (periodo de 1 s)
   - processa protocolo continuamente

## 3. Maquina de estado operacional

O estado principal da sonda e derivado do ID instalado:
- ID = 0xFF: desinstalada
- ID = 1..4: instalada

### 3.1 Estado desinstalada
- LED fica aceso continuo.
- Pode responder discovery e aceitar novo ID.
- Nao responde leitura enderecada por ID de sonda instalada.

### 3.2 Estado instalada
- LED fica apagado em repouso.
- Responde leitura quando o ID recebido bate com seu ID persistido.
- A cada comunicacao valida respondida, faz 2 piscadas.

## 4. Protocolo de comunicacao

### 4.1 Estrutura do frame

Formato logico:
- start: 0x55
- version: 0x01
- len
- reg
- id
- reserved
- payload opcional
- crc_lo
- crc_hi
- end: 0x55

CRC:
- algoritmo: CRC16 Modbus
- janela: de version ate ultimo byte de payload/reserved

### 4.2 Registradores suportados
- 0x10: instalacao/desinstalacao
- 0x20: leitura de telemetria

### 4.3 Fluxo de instalacao

Entrada:
- frame reg=0x10, id=1..4

Acao:
1. grava novo ID em flash
2. muda estado interno para instalado
3. envia ACK de instalacao
4. dispara feedback visual (2 piscadas)

### 4.4 Fluxo de desinstalacao

Entrada:
- frame reg=0x10, id=0xFF

Acao:
1. grava estado desinstalado na flash
2. liga LED fixo
3. envia resposta de discovery/desinstalado
4. permanece aguardando nova instalacao

### 4.5 Fluxo de leitura

Entrada:
- frame reg=0x20, id=ID_da_sonda

Validacao:
- so responde se estiver instalada e o id bater com o persistido

Payload de resposta (8 bytes no perfil atual):
- temperatura do ar x10 (int16 little-endian)
- umidade do ar x10 (uint16 little-endian)
- temperatura do solo x10 (int16 little-endian)
- umidade do solo x10 (uint16 little-endian)

No perfil atual, `PROBE_TYPE_AIR_SOIL_TH`, o payload possui 8 bytes e os quatro
campos seguem nessa ordem. Valores assinados sao usados para temperaturas;
umidades sao sem sinal. Todos os valores usam fator de escala 10.

O tipo da sonda e representado por `probe_type_t` no firmware e deve ser
conhecido pelo mestre/modelo que interpreta a resposta. O campo `reserved` do
frame permanece em zero para manter compatibilidade; ele nao transporta o tipo.
Para adicionar um perfil, declare um novo valor de tipo, defina seu layout e
adicione um caso em `encode_measurement_payload()`. Atualize tambem o mestre e
este documento para que o decoder interprete o payload pelo perfil correto.

Os canais de solo ainda sao placeholders neste hardware: a aplicacao envia
0.0 ate serem conectados sensores reais. Isso nao deve ser interpretado como
uma medicao validada do solo.

## 5. Parser incremental da UART

Implementacao em Src/probe_protocol.c.

Estrategia:
1. Le bytes enquanto houver RXNE.
2. Ignora lixo ate achar delimitador 0x55.
3. Ao receber o campo len, calcula tamanho total esperado.
4. Acumula ate frame completo.
5. Valida delimitadores, tamanho e CRC.
6. Despacha por registrador.
7. Reinicia parser para proximo frame.

Vantagens:
- nao bloqueante
- robusto a ruido parcial
- simples de integrar ao loop cooperativo

## 6. Persistencia de ID em flash

Implementacao em Src/probe_id_storage.c.

### 6.1 Endereco e layout
- pagina: 0x0800FC00
- +0: magic 0xA55A
- +2: ID (1..4) ou 0xFFFF (desinstalada)

### 6.2 Escrita
1. valida ID
2. unlock flash
3. erase pagina
4. grava magic
5. grava ID
6. lock flash

### 6.3 Leitura
1. le magic
2. se magic invalido -> 0xFF
3. se ID em 1..4 -> retorna ID
4. senao -> 0xFF

## 7. Sensor SHT30

Implementacao em Src/sht30.c.

Fluxo:
1. envia comando 0x2C 0x06
2. aguarda 20 ms
3. le 6 bytes
4. valida CRC dos pares
5. converte para engenharia:
   - T = -45 + 175 * raw_t / 65535
   - RH = 100 * raw_rh / 65535

## 8. LED PC13

Regra funcional:
- desinstalada: aceso continuo
- instalada em repouso: apagado
- em comunicacao: 2 piscadas por evento

Mecanismo interno:
- contador de passos de piscada (4 passos)
- alternancia por tempo fixo (80 ms)
- retorno automatico ao estado apagado ao final

## 9. Temporizacao do sistema

Fonte de tempo:
- TIM2 interrupcao a cada 1 ms

Uso:
- scheduler cooperativo no loop principal
- delay de leitura do SHT30
- controle de animacao do LED

## 10. Integracao entre modulos

1. application coleta medicao periodica.
2. application atualiza snapshot no probe_protocol.
3. probe_protocol recebe frame RS485.
4. probe_protocol decide acao (instalar, desinstalar, ler).
5. se preciso, probe_protocol grava ID em flash.
6. response frame e enviado pela camada de perifericos.
7. LED sinaliza estado e comunicacao.

## 11. Pontos de extensao

1. Medicoes reais do solo
- integrar sensores de temperatura e umidade do solo
- alimentar os dois campos de solo do snapshot
- manter o perfil e escala x10 acordados com o mestre

2. Novos tipos de sonda
- adicionar o tipo ao `probe_type_t`
- definir o layout e encoder do payload daquele tipo
- configurar o mestre para selecionar o decoder pelo tipo da sonda
- manter framing, CRC e registradores comuns sempre que possivel

3. Controle de erro mais detalhado
- counters de CRC invalido
- counters de timeout e frames truncados

4. Parametrizacao de pinos
- mover pinos para tabela de configuracao central

5. Seguranca de escrita em flash
- adicionar verificacao pos-gravacao
- implementar politica de wear leveling se necessaria

## 12. Checklist de validacao em bancada

1. Boot sem comandos RS485:
- LED aceso se desinstalada
- LED apagado se instalada

2. Enviar instalacao ID=2:
- grava em flash
- apos reset, continua ID=2

3. Enviar leitura para ID=2:
- resposta com T/U e soil placeholder
- LED faz 2 piscadas

4. Enviar desinstalar ID=0xFF:
- volta estado desinstalada
- LED aceso continuo

5. Injetar frame com CRC errado:
- sem resposta
- parser continua funcional para proximo frame valido
