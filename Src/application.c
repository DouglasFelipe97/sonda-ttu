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
    static probe_measurements_t measurements = {0.0f, 0.0f, 0.0f, 0.0f};

    /* Taxa de amostragem: 1 leitura por segundo. */
    if ((now_ms - last_sample_ms) < 1000U)
    {
        /*
         * Mesmo sem nova leitura, reenviamos o ultimo snapshot para garantir
         * que o protocolo sempre tenha um conjunto consistente de dados.
         */
        probe_protocol_set_measurements(PROBE_TYPE_AIR_SOIL_TH, &measurements);
        return;
    }

    last_sample_ms = now_ms;

    /*
     * Lemos temperatura e umidade do ar do SHT30. Os canais de solo
     * permanecem zerados ate a integracao dos sensores correspondentes.
     */
    if (sht30_read_temperature_humidity(&measurements.air_temperature_c,
                                        &measurements.air_humidity_rh))
    {
        probe_protocol_set_measurements(PROBE_TYPE_AIR_SOIL_TH, &measurements);
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
