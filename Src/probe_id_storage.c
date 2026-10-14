#include "probe_id_storage.h"

#include "stm32f103_regs.h"

/*
 * Persistencia de ID da sonda em flash interna (ultima pagina de 1KB).
 * Estrutura simples:
 *   +0: magic (0xA55A)
 *   +2: id (1..4) ou 0xFFFF para desinstalada
 */
#define PROBE_ID_FLASH_PAGE_ADDR 0x0800FC00UL
#define PROBE_ID_MAGIC           0xA55A
#define PROBE_ID_EMPTY           0xFFFF

/* Leitura direta de meia palavra (16 bits) na flash mapeada em memoria. */
static uint16_t flash_read_halfword(uint32_t address)
{
    return *(volatile uint16_t *)address;
}

/* Aguarda fim de operacao de flash antes de novo comando. */
static void flash_wait_ready(void)
{
    while ((FLASH_R->SR & FLASH_SR_BSY) != 0U)
    {
    }
}

/* Destrava controladora de flash para programacao/erase. */
static void flash_unlock(void)
{
    if ((FLASH_R->CR & FLASH_CR_LOCK) != 0U)
    {
        FLASH_R->KEYR = FLASH_KEY1;
        FLASH_R->KEYR = FLASH_KEY2;
    }
}

/* Reaplica lock da flash por seguranca. */
static void flash_lock(void)
{
    FLASH_R->CR |= FLASH_CR_LOCK;
}

/* Apaga pagina inteira (obrigatorio antes de regravar). */
static void flash_erase_page(uint32_t page_address)
{
    flash_wait_ready();
    FLASH_R->CR |= FLASH_CR_PER;
    FLASH_R->AR = page_address;
    FLASH_R->CR |= FLASH_CR_STRT;
    flash_wait_ready();
    FLASH_R->CR &= ~FLASH_CR_PER;
}

/* Programa uma meia palavra em endereco alinhado. */
static void flash_program_halfword(uint32_t address, uint16_t value)
{
    flash_wait_ready();
    FLASH_R->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)address = value;
    flash_wait_ready();
    FLASH_R->CR &= ~FLASH_CR_PG;
}

uint8_t probe_id_storage_read(void)
{
    /* Verifica assinatura para distinguir memoria inicializada de lixo/FF. */
    uint16_t magic = flash_read_halfword(PROBE_ID_FLASH_PAGE_ADDR);
    uint16_t id_word = flash_read_halfword(PROBE_ID_FLASH_PAGE_ADDR + 2U);

    if (magic != PROBE_ID_MAGIC)
    {
        return 0xFFU;
    }

    if ((id_word >= 1U) && (id_word <= 4U))
    {
        return (uint8_t)id_word;
    }

    return 0xFFU;
}

bool probe_id_storage_write(uint8_t probe_id)
{
    uint16_t id_word;

    /* Aceita apenas estados validos do protocolo. */
    if ((probe_id != 0xFFU) && ((probe_id < 1U) || (probe_id > 4U)))
    {
        return false;
    }

    id_word = (probe_id == 0xFFU) ? PROBE_ID_EMPTY : (uint16_t)probe_id;

    /*
     * Estrategia simples e robusta:
     * apaga pagina, grava magic e grava ID.
     */
    flash_unlock();
    flash_erase_page(PROBE_ID_FLASH_PAGE_ADDR);
    flash_program_halfword(PROBE_ID_FLASH_PAGE_ADDR, PROBE_ID_MAGIC);
    flash_program_halfword(PROBE_ID_FLASH_PAGE_ADDR + 2U, id_word);
    flash_lock();

    return true;
}
