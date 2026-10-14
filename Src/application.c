#include "application.h"

#include "peripherals_init.h"
#include "probe_protocol.h"
#include "sht30.h"

/*
 * Camada de aplicacao:
 * - Inicializa hardware e protocolo.
 * - Faz amostragem periodica do SHT30.
 * - Mantem o loop principal nao bloqueante para atendimento rapido da UART.
 */

static void update_measurements_if_due(uint32_t now_ms)
{
    /*
     * Variaveis estaticas preservam o ultimo estado entre chamadas sem
     * expor dados globais para outros modulos.
     */
    static uint32_t last_sample_ms = 0U;
    static float last_temperature_c = 0.0f;
    static float last_humidity_rh = 0.0f;
    static float last_soil_humidity = 0.0f;

    /* Taxa de amostragem: 1 leitura por segundo. */
    if ((now_ms - last_sample_ms) < 1000U)
    {
        /*
         * Mesmo sem nova leitura, reenviamos o ultimo snapshot para garantir
         * que o protocolo sempre tenha um conjunto consistente de dados.
         */
        probe_protocol_set_measurements(last_temperature_c, last_humidity_rh, last_soil_humidity);
        return;
    }

    last_sample_ms = now_ms;

    /*
     * Lemos temperatura e umidade do ar do SHT30.
     * Umidade do solo fica como placeholder por enquanto.
     */
    if (sht30_read_temperature_humidity(&last_temperature_c, &last_humidity_rh))
    {
        probe_protocol_set_measurements(last_temperature_c, last_humidity_rh, last_soil_humidity);
    }
}

void application_run(void)
{
    /* Inicializacao unica do hardware centralizado e da FSM do protocolo. */
    peripherals_init();
    probe_protocol_init();

    /*
     * Loop principal infinito:
     * - Atualiza medicao quando devido.
     * - Processa bytes recebidos e respostas RS485.
     */
    for (;;)
    {
        uint32_t now_ms = peripherals_get_tick_ms();

        update_measurements_if_due(now_ms);
        probe_protocol_process(now_ms);
    }
}
