#ifndef PROBE_ID_STORAGE_H
#define PROBE_ID_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Retorna ID persistido (1..4) ou 0xFF quando desinstalada/sem registro valido.
 */
uint8_t probe_id_storage_read(void);

/*
 * Persiste o ID da sonda em flash interna.
 * Valores aceitos: 0xFF (desinstalada) e 1..4 (instalada).
 */
bool probe_id_storage_write(uint8_t probe_id);

#endif
