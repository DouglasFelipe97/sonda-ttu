#ifndef SHT30_H
#define SHT30_H

#include <stdbool.h>

/*
 * Le temperatura e umidade do ar via SHT30.
 * Retorna true quando a leitura e valida (incluindo CRC).
 */
bool sht30_read_temperature_humidity(float *temperature_c, float *humidity_rh);

#endif
