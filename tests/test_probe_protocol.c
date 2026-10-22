#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../Src/probe_protocol.c"

static uint8_t stored_id = 0xFF;
static uint8_t input[32], output[32];
static size_t input_len, input_pos, output_len;
static unsigned writes, sends;
static bool storage_ok = true;

uint8_t probe_id_storage_read(void) { return stored_id; }
bool probe_id_storage_write(uint8_t id)
{
    writes++;
    if (!storage_ok) return false;
    stored_id = id;
    return true;
}
void peripherals_led_pc13_set(bool on) { (void)on; }
void peripherals_led_pc13_toggle(void) {}
bool peripherals_rs485_read_byte(uint8_t *byte)
{
    if (input_pos == input_len) return false;
    *byte = input[input_pos++];
    return true;
}
void peripherals_rs485_send(const uint8_t *data, size_t len)
{
    memcpy(output, data, len);
    output_len = len;
    sends++;
}

static void command(uint8_t reg, uint8_t id)
{
    input_len = build_frame(reg, id, 0, NULL, 0, input, sizeof(input));
    input_pos = 0;
    output_len = 0;
    probe_protocol_process(200);
}

int main(void)
{
    probe_protocol_init();
    command(0x11, 0xFF);
    assert(output_len == 9 && output[3] == 0x11);
    command(0x10, 1);
    assert(stored_id == 1 && writes == 1 && output_len == 15);
    assert(parse_frame(output, output_len, NULL, NULL));
    unsigned previous_sends = sends;
    command(0x10, 1);
    assert(sends == previous_sends + 1 && writes == 1);
    command(0x10, 2);
    assert(output_len == 0 && stored_id == 1 && writes == 1);
    probe_protocol_set_measurements(-5.2f, 55.5f, 42.0f);
    command(0x20, 2);
    assert(output_len == 0);
    command(0x20, 1);
    assert(output_len == 15 && output[3] == 0x20 && output[4] == 1);
    assert(parse_frame(output, output_len, NULL, NULL));
    const uint8_t expected[] = {0xCC, 0xFF, 0x2B, 0x02, 0xA4, 0x01};
    assert(memcmp(output + 6, expected, sizeof(expected)) == 0);
    command(0x10, 1);
    assert(output_len == 15 && output[3] == 0x10);
    assert(memcmp(output + 6, expected, sizeof(expected)) == 0);
    input_len = build_frame(0x20, 1, 0, NULL, 0, input, sizeof(input));
    input_len = 4;
    input_pos = 0;
    probe_protocol_process(200);
    assert(s_ctx.rx_len == 4);
    probe_protocol_process(240);
    assert(s_ctx.rx_len == 0);
    command(0x20, 1);
    assert(output_len == 15);
    probe_protocol_init();
    command(0x10, 1);
    assert(output_len == 15 && writes == 1);
    command(0x12, 2);
    assert(stored_id == 1);
    command(0x12, 1);
    assert(stored_id == 0xFF && writes == 2);
    command(0x20, 1);
    assert(output_len == 0);
    storage_ok = false;
    command(0x10, 1);
    assert(output_len == 0 && s_ctx.installed_id == 0xFF);
    puts("PASS: discovery, ACK retry/reboot, addressing, telemetry CRC, uninstall and storage failure");
    return 0;
}