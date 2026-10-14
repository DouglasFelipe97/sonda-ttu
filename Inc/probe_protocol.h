#ifndef PROBE_PROTOCOL_H
#define PROBE_PROTOCOL_H

#include <stdint.h>

/* Carrega ID persistido e prepara estado interno da maquina de protocolo. */
void probe_protocol_init(void);

/* Atualiza o snapshot de medicao que sera enviado em respostas de leitura. */
void probe_protocol_set_measurements(float temperature_c, float humidity_rh, float soil_humidity);

/*
 * Processa o protocolo de forma cooperativa:
 * - consome bytes da UART RS485
 * - interpreta frames
 * - executa instalar/desinstalar/leitura
 * - atualiza feedback do LED
 */
void probe_protocol_process(uint32_t now_ms);

#endif
