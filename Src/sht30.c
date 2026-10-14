#include "sht30.h"

#include "peripherals_init.h"

/* Endereco padrao do SHT30 quando ADDR esta em GND. */
#define SHT30_ADDR_7BIT 0x44U

/* CRC8 polinomio 0x31, init 0xFF (padrao SHT3x). */
static uint8_t sht30_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFU;
    uint8_t i;
    uint8_t bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/* Delay cooperativo usando tick global de 1 ms. */
static void delay_ms(uint32_t ms)
{
    uint32_t start = peripherals_get_tick_ms();
    while ((peripherals_get_tick_ms() - start) < ms)
    {
    }
}

bool sht30_read_temperature_humidity(float *temperature_c, float *humidity_rh)
{
    /* Comando de medicao single-shot high repeatability, sem clock stretching. */
    uint8_t cmd[2] = {0x2CU, 0x06U};
    uint8_t rx[6];
    uint16_t raw_t;
    uint16_t raw_rh;

    if ((temperature_c == 0) || (humidity_rh == 0))
    {
        return false;
    }

    /* Envia comando de medicao. */
    if (!peripherals_i2c1_write(SHT30_ADDR_7BIT, cmd, 2U))
    {
        return false;
    }

    /* Tempo maximo tipico de conversao para este modo. */
    delay_ms(20U);

    /* Le 6 bytes: T_MSB, T_LSB, CRC, RH_MSB, RH_LSB, CRC. */
    if (!peripherals_i2c1_read(SHT30_ADDR_7BIT, rx, 6U))
    {
        return false;
    }

    /* Valida CRC de temperatura e umidade separadamente. */
    if ((sht30_crc8(&rx[0], 2U) != rx[2]) || (sht30_crc8(&rx[3], 2U) != rx[5]))
    {
        return false;
    }

    raw_t = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    raw_rh = (uint16_t)(((uint16_t)rx[3] << 8) | rx[4]);

    /* Formulas oficiais do datasheet SHT3x. */
    *temperature_c = -45.0f + (175.0f * (float)raw_t / 65535.0f);
    *humidity_rh = 100.0f * (float)raw_rh / 65535.0f;

    return true;
}
