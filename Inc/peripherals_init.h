#ifndef PERIPHERALS_INIT_H
#define PERIPHERALS_INIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Executado no reset pelo startup assembly.
 * Configura clock principal para 64 MHz.
 */
void SystemInit(void);

/* Inicializa todos os perifericos usados pelo firmware da sonda. */
void peripherals_init(void);

/* Tick monotonic em milissegundos gerado por TIM2. */
uint32_t peripherals_get_tick_ms(void);

/* Controle do LED em PC13 (ativo em nivel baixo no hardware Blue Pill). */
void peripherals_led_pc13_set(bool on);
void peripherals_led_pc13_toggle(void);

/* API I2C1 em modo master usada pelo SHT30. */
bool peripherals_i2c1_write(uint8_t addr7, const uint8_t *data, uint32_t len);
bool peripherals_i2c1_read(uint8_t addr7, uint8_t *data, uint32_t len);

/* API RS485 sobre USART1 com controle de direcao DE. */
void peripherals_rs485_send(const uint8_t *data, size_t len);
bool peripherals_rs485_read_byte(uint8_t *out_byte);

#endif
