#include "probe_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "peripherals_init.h"
#include "probe_id_storage.h"

/*
 * Protocolo da sonda (compatibilidade com mestre do projeto smart-horta):
 * - Delimitador: 0x55
 * - Versao: 0x01
 * - Registrador 0x10: instalacao/desinstalacao
 * - Registrador 0x20: leitura de telemetria
 */

#define PROBE_FRAME_DELIMITER       0x55U
#define PROBE_SW_VERSION            0x01U
#define PROBE_INSTALL_REGISTER      0x10U
#define PROBE_READ_REGISTER         0x20U
#define PROBE_PROTO_FIXED_FIELDS    3U
#define PROBE_FRAME_MAX_SIZE        32U

#define PROBE_DISCOVERY_ID          0xFFU
#define PROBE_ID_MIN                1U
#define PROBE_ID_MAX                4U

/* 2 piscadas = 4 transicoes ON/OFF. */
#define LED_BLINK_STEP_MS           80U

typedef struct
{
    /* 0xFF = desinstalada, 1..4 = instalada. */
    uint8_t installed_id;

    probe_type_t probe_type;
    probe_measurements_t measurements;

    /* Buffer de recepcao incremental de frame. */
    uint8_t rx_buf[PROBE_FRAME_MAX_SIZE];
    uint8_t rx_len;
    uint8_t rx_expected_len;

    /* Controle de animacao de LED por evento de comunicacao. */
    uint8_t led_blink_steps_remaining;
    uint32_t led_step_started_ms;
} probe_context_t;

static probe_context_t s_ctx;

/* CRC16 Modbus para validacao de integridade do frame. */
static uint16_t crc16_modbus(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    size_t i;
    uint8_t bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

/* Conversao float -> inteiro em decimos com arredondamento simetrico. */
static int16_t to_x10(float value)
{
    if (value >= 0.0f)
    {
        return (int16_t)(value * 10.0f + 0.5f);
    }
    return (int16_t)(value * 10.0f - 0.5f);
}

/* Conversao para valor sem sinal em decimos com saturacao no limite de uint16. */
static uint16_t to_u16_x10(float value)
{
    int32_t scaled;

    if (value < 0.0f)
    {
        value = 0.0f;
    }

    scaled = (int32_t)(value * 10.0f + 0.5f);
    if (scaled > 65535)
    {
        scaled = 65535;
    }

    return (uint16_t)scaled;
}

/*
 * Monta frame de resposta.
 * layout:
 * [0]=0x55 [1]=versao [2]=len [3]=reg [4]=id [5]=reserved [payload] [crc_lo] [crc_hi] [0x55]
 */
static size_t build_frame(uint8_t reg,
                          uint8_t id,
                          uint8_t reserved,
                          const uint8_t *payload,
                          uint8_t payload_len,
                          uint8_t *out_buf,
                          size_t out_buf_size)
{
    uint8_t proto_len = (uint8_t)(PROBE_PROTO_FIXED_FIELDS + payload_len);
    size_t total_len = (size_t)proto_len + 6U;
    uint16_t crc;
    size_t i;

    if ((out_buf == 0) || (out_buf_size < total_len))
    {
        return 0U;
    }

    out_buf[0] = PROBE_FRAME_DELIMITER;
    out_buf[1] = PROBE_SW_VERSION;
    out_buf[2] = proto_len;
    out_buf[3] = reg;
    out_buf[4] = id;
    out_buf[5] = reserved;

    for (i = 0U; i < payload_len; i++)
    {
        out_buf[6U + i] = payload[i];
    }

    crc = crc16_modbus(&out_buf[1], (size_t)proto_len + 2U);
    out_buf[6U + payload_len] = (uint8_t)(crc & 0xFFU);
    out_buf[7U + payload_len] = (uint8_t)((crc >> 8U) & 0xFFU);
    out_buf[8U + payload_len] = PROBE_FRAME_DELIMITER;

    return total_len;
}

/*
 * Valida enquadramento e CRC de um frame completo ja acumulado.
 * Nao interpreta payload, apenas confirma estrutura e extrai reg/id.
 */
static bool parse_frame(const uint8_t *frame,
                        size_t frame_len,
                        uint8_t *out_reg,
                        uint8_t *out_id)
{
    uint8_t proto_len;
    size_t total_len;
    size_t crc_pos;
    uint16_t crc_rx;
    uint16_t crc_calc;

    if ((frame == 0) || (frame_len < 9U))
    {
        return false;
    }

    if ((frame[0] != PROBE_FRAME_DELIMITER) || (frame[1] != PROBE_SW_VERSION))
    {
        return false;
    }

    proto_len = frame[2];
    total_len = (size_t)proto_len + 6U;
    if (total_len != frame_len)
    {
        return false;
    }

    if (frame[frame_len - 1U] != PROBE_FRAME_DELIMITER)
    {
        return false;
    }

    if (proto_len < PROBE_PROTO_FIXED_FIELDS)
    {
        return false;
    }

    crc_pos = frame_len - 3U;
    crc_rx = (uint16_t)frame[crc_pos] | (uint16_t)((uint16_t)frame[crc_pos + 1U] << 8U);
    crc_calc = crc16_modbus(&frame[1], crc_pos - 1U);
    if (crc_rx != crc_calc)
    {
        return false;
    }

    if (out_reg != 0)
    {
        *out_reg = frame[3];
    }
    if (out_id != 0)
    {
        *out_id = frame[4];
    }

    return true;
}

/* Dispara padrao de piscada de comunicacao bem-sucedida. */
static void trigger_comm_led_feedback(uint32_t now_ms)
{
    s_ctx.led_blink_steps_remaining = 4U;
    s_ctx.led_step_started_ms = now_ms;
    peripherals_led_pc13_set(true);
}

/*
 * Politica de LED:
 * - desinstalada: aceso continuo
 * - instalada sem evento: apagado
 * - evento de comunicacao: 2 piscadas e volta apagado
 */
static void update_led_state(uint32_t now_ms)
{
    if (s_ctx.installed_id == PROBE_DISCOVERY_ID)
    {
        peripherals_led_pc13_set(true);
        s_ctx.led_blink_steps_remaining = 0U;
        return;
    }

    if (s_ctx.led_blink_steps_remaining == 0U)
    {
        peripherals_led_pc13_set(false);
        return;
    }

    if ((now_ms - s_ctx.led_step_started_ms) >= LED_BLINK_STEP_MS)
    {
        peripherals_led_pc13_toggle();
        s_ctx.led_step_started_ms = now_ms;
        s_ctx.led_blink_steps_remaining--;

        if (s_ctx.led_blink_steps_remaining == 0U)
        {
            peripherals_led_pc13_set(false);
        }
    }
}

/* Resposta para broadcast de discovery/estado virgem. */
static void send_discovery_response(uint32_t now_ms)
{
    uint8_t tx[9];
    size_t len = build_frame(PROBE_INSTALL_REGISTER,
                             PROBE_DISCOVERY_ID,
                             0x00U,
                             0,
                             0U,
                             tx,
                             sizeof(tx));
    if (len > 0U)
    {
        peripherals_rs485_send(tx, len);
        trigger_comm_led_feedback(now_ms);
    }
}

/* ACK de atribuicao de ID durante instalacao. */
static void send_install_ack(uint32_t now_ms)
{
    uint8_t tx[9];
    size_t len = build_frame(PROBE_INSTALL_REGISTER,
                             s_ctx.installed_id,
                             0x00U,
                             0,
                             0U,
                             tx,
                             sizeof(tx));
    if (len > 0U)
    {
        peripherals_rs485_send(tx, len);
        trigger_comm_led_feedback(now_ms);
    }
}

static uint8_t encode_measurement_payload(uint8_t *payload, size_t payload_capacity)
{
    uint16_t values[4];
    size_t index;

    if ((payload == 0) || (payload_capacity < 8U))
    {
        return 0U;
    }

    switch (s_ctx.probe_type)
    {
        case PROBE_TYPE_AIR_SOIL_TH:
            values[0] = (uint16_t)to_x10(s_ctx.measurements.air_temperature_c);
            values[1] = to_u16_x10(s_ctx.measurements.air_humidity_rh);
            values[2] = (uint16_t)to_x10(s_ctx.measurements.soil_temperature_c);
            values[3] = to_u16_x10(s_ctx.measurements.soil_humidity_rh);
            break;

        case PROBE_TYPE_UNKNOWN:
        default:
            return 0U;
    }

    for (index = 0U; index < 4U; index++)
    {
        payload[index * 2U] = (uint8_t)(values[index] & 0xFFU);
        payload[(index * 2U) + 1U] = (uint8_t)(values[index] >> 8U);
    }

    return 8U;
}

/* Resposta de telemetria do perfil ar/solo em x10 little-endian. */
static void send_read_response(uint32_t now_ms)
{
    uint8_t tx[17];
    uint8_t payload[8];
    uint8_t payload_len = encode_measurement_payload(payload, sizeof(payload));
    size_t len;

    if (payload_len == 0U)
    {
        return;
    }

    len = build_frame(PROBE_READ_REGISTER,
                      s_ctx.installed_id,
                      0x00U,
                      payload,
                      payload_len,
                      tx,
                      sizeof(tx));
    if (len > 0U)
    {
        peripherals_rs485_send(tx, len);
        trigger_comm_led_feedback(now_ms);
    }
}

/*
 * Comando de instalacao (reg 0x10):
 * - id=0xFF: desinstalar/estado discovery
 * - id=1..4: instalar com ID fixo e persistir em flash
 */
static void handle_install_command(uint8_t received_id, uint32_t now_ms)
{
    if (received_id == PROBE_DISCOVERY_ID)
    {
        if (s_ctx.installed_id == PROBE_DISCOVERY_ID)
        {
            send_discovery_response(now_ms);
        }
        else
        {
            s_ctx.installed_id = PROBE_DISCOVERY_ID;
            probe_id_storage_write(PROBE_DISCOVERY_ID);
            peripherals_led_pc13_set(true);
            send_discovery_response(now_ms);
        }
        return;
    }

    if ((received_id >= PROBE_ID_MIN) && (received_id <= PROBE_ID_MAX))
    {
        s_ctx.installed_id = received_id;
        probe_id_storage_write(received_id);
        send_install_ack(now_ms);
    }
}

void probe_protocol_init(void)
{
    /* Recupera estado persistido apos reboot/power cycle. */
    s_ctx.installed_id = probe_id_storage_read();
    s_ctx.probe_type = PROBE_TYPE_AIR_SOIL_TH;
    s_ctx.measurements.air_temperature_c = 0.0f;
    s_ctx.measurements.air_humidity_rh = 0.0f;
    s_ctx.measurements.soil_temperature_c = 0.0f;
    s_ctx.measurements.soil_humidity_rh = 0.0f;
    s_ctx.rx_len = 0U;
    s_ctx.rx_expected_len = 0U;
    s_ctx.led_blink_steps_remaining = 0U;
    s_ctx.led_step_started_ms = 0U;

    if (s_ctx.installed_id == PROBE_DISCOVERY_ID)
    {
        peripherals_led_pc13_set(true);
    }
    else
    {
        peripherals_led_pc13_set(false);
    }
}

void probe_protocol_set_measurements(probe_type_t probe_type,
                                     const probe_measurements_t *measurements)
{
    if ((measurements == 0) || (probe_type != PROBE_TYPE_AIR_SOIL_TH))
    {
        return;
    }

    s_ctx.probe_type = probe_type;
    s_ctx.measurements = *measurements;
}

void probe_protocol_process(uint32_t now_ms)
{
    uint8_t byte;

    /*
     * Parser incremental nao bloqueante:
     * consome todos os bytes atualmente disponiveis no UART.
     */
    while (peripherals_rs485_read_byte(&byte))
    {
        /* Sincroniza inicio de frame no delimitador 0x55. */
        if ((s_ctx.rx_len == 0U) && (byte != PROBE_FRAME_DELIMITER))
        {
            continue;
        }

        if (s_ctx.rx_len == 0U)
        {
            s_ctx.rx_expected_len = 0U;
        }

        if (s_ctx.rx_len < PROBE_FRAME_MAX_SIZE)
        {
            s_ctx.rx_buf[s_ctx.rx_len++] = byte;
        }
        else
        {
            s_ctx.rx_len = 0U;
            s_ctx.rx_expected_len = 0U;
            continue;
        }

        if (s_ctx.rx_len == 3U)
        {
            /* Ao receber campo de tamanho, ja definimos o tamanho total esperado. */
            s_ctx.rx_expected_len = (uint8_t)(s_ctx.rx_buf[2] + 6U);
            if ((s_ctx.rx_expected_len < 9U) || (s_ctx.rx_expected_len > PROBE_FRAME_MAX_SIZE))
            {
                s_ctx.rx_len = 0U;
                s_ctx.rx_expected_len = 0U;
                continue;
            }
        }

        if ((s_ctx.rx_expected_len > 0U) && (s_ctx.rx_len == s_ctx.rx_expected_len))
        {
            uint8_t reg;
            uint8_t id;
            bool ok = parse_frame(s_ctx.rx_buf, s_ctx.rx_len, &reg, &id);

            /* Reseta parser para proximo frame, independente de sucesso. */
            s_ctx.rx_len = 0U;
            s_ctx.rx_expected_len = 0U;

            if (!ok)
            {
                continue;
            }

            if (reg == PROBE_INSTALL_REGISTER)
            {
                handle_install_command(id, now_ms);
            }
            else if ((reg == PROBE_READ_REGISTER) &&
                     (s_ctx.installed_id != PROBE_DISCOVERY_ID) &&
                     (id == s_ctx.installed_id))
            {
                /* Apenas sonda instalada responde leitura enderecada ao proprio ID. */
                send_read_response(now_ms);
            }
        }
    }

    /* Atualiza animacao/estado visual do LED a cada ciclo do loop principal. */
    update_led_state(now_ms);
}
