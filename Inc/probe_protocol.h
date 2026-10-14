#ifndef PROBE_PROTOCOL_H
#define PROBE_PROTOCOL_H

#include <stdint.h>

typedef enum
{
	PROBE_TYPE_UNKNOWN = 0,
	PROBE_TYPE_AIR_SOIL_TH = 1
} probe_type_t;

typedef struct
{
	float air_temperature_c;
	float air_humidity_rh;
	float soil_temperature_c;
	float soil_humidity_rh;
} probe_measurements_t;

/* Carrega ID persistido e prepara estado interno da maquina de protocolo. */
void probe_protocol_init(void);

/* Atualiza tipo e snapshot de medicao enviados em respostas de leitura. */
void probe_protocol_set_measurements(probe_type_t probe_type,
									 const probe_measurements_t *measurements);

/*
 * Processa o protocolo de forma cooperativa:
 * - consome bytes da UART RS485
 * - interpreta frames
 * - executa instalar/desinstalar/leitura
 * - atualiza feedback do LED
 */
void probe_protocol_process(uint32_t now_ms);

#endif
