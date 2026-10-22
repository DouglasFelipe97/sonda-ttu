#include "peripherals_init.h"

#include "stm32f103_regs.h"

/*
 * Este arquivo centraliza toda a configuracao de perifericos do firmware.
 * Nao deve existir configuracao de hardware espalhada em outros modulos.
 */

/* PA8 controla o pino DE/RE do transceiver RS485 (direcao do barramento). */
#define RS485_DE_PIN 8U
#define RS485_RX_BUFFER_SIZE 128U
#define RS485_TURNAROUND_MS  2U

/* Timeout simples por polling para operacoes I2C. */
#define I2C_TIMEOUT  100000UL

/* Clock global esperado por parte do ecossistema CMSIS/startup. */
volatile uint32_t SystemCoreClock = 64000000UL;

/* Tick de tempo em milissegundos incrementado na interrupcao do TIM2. */
static volatile uint32_t s_ms_tick = 0UL;
static volatile uint8_t s_rs485_rx_buffer[RS485_RX_BUFFER_SIZE];
static volatile uint32_t s_rs485_rx_head;
static volatile uint32_t s_rs485_rx_tail;

/* Mantem somente SWD ativo para debug, liberando pinos de JTAG. */
static void configure_debug_serial_wire(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG_MASK) | AFIO_MAPR_SWJ_CFG_SWD;
}

/* Configura PC13 como saida para LED de status. */
static void configure_led_pc13_output(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* PC13 esta em CRH (pinos 8..15), campo de 4 bits no deslocamento 20. */
    GPIOC->CRH &= ~(0xFUL << 20);
    GPIOC->CRH |= (0x2UL << 20);

    /* Estado inicial: apagado. */
    peripherals_led_pc13_set(false);
}

/* Base de tempo de 1 ms em TIM2 para scheduler cooperativo do firmware. */
static void configure_tim2_1ms(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /*
     * APB1 = 32 MHz e timers em APB1 contam em 64 MHz (x2 quando prescaler APB1 > 1).
     * PSC=63 => contador a 1 MHz.
     * ARR=999 => overflow a cada 1 ms.
     */
    TIM2->PSC = 63U;
    TIM2->ARR = 999U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->DIER = TIM_DIER_UIE;
    TIM2->CR1 = TIM_CR1_CEN;

    /* Habilita IRQ do TIM2 no NVIC (linha 28 para STM32F103). */
    NVIC_ISER0 = (1UL << 28);
}

/* Configura I2C1 em fast mode para comunicacao com SHT30. */
static void configure_i2c1_fast_mode(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* PB6=SCL e PB7=SDA em alternate function open-drain. */
    GPIOB->CRL &= ~((0xFUL << 24) | (0xFUL << 28));
    GPIOB->CRL |= (0xFUL << 24) | (0xFUL << 28);

    /* Mantem linhas em nivel alto por seguranca. */
    GPIOB->BSRR = (1UL << 6) | (1UL << 7);

    /* Reset periferico para partir de estado conhecido. */
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0U;

    /*
     * CR2 = frequencia do PCLK1 em MHz (32).
     * CCR/TRISE ajustados para modo fast em ~400 kHz.
     */
    I2C1->CR2 = 32U;
    I2C1->OAR1 = (1UL << 14);
    I2C1->CCR = I2C_CCR_FS | 27U;
    I2C1->TRISE = 11U;
    I2C1->CR1 = I2C_CR1_PE;
}

/* Coloca transceiver em transmissao. */
static void rs485_set_tx_mode(void)
{
    GPIOA->BSRR = (1UL << RS485_DE_PIN);
}

/* Coloca transceiver em recepcao. */
static void rs485_set_rx_mode(void)
{
    GPIOA->BRR = (1UL << RS485_DE_PIN);
}

/* Configura USART1 para 9600 bps em half-duplex externo via RS485. */
static void configure_rs485_uart_9600(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* PA8=DE saida, PA9=TX AF push-pull, PA10=RX entrada flutuante. */
    GPIOA->CRH &= ~((0xFUL << 0) | (0xFUL << 4) | (0xFUL << 8));
    GPIOA->CRH |= (0x2UL << 0);
    GPIOA->CRH |= (0xBUL << 4);
    GPIOA->CRH |= (0x4UL << 8);

    /* Inicia escutando o barramento. */
    rs485_set_rx_mode();

    /* BRR para 9600 bps com clock de 64 MHz. */
    USART1->BRR = 0x1A0BU;
    s_rs485_rx_head = 0U;
    s_rs485_rx_tail = 0U;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_ISER1 = (1UL << (37U - 32U));
}

/* Aguarda flag ligar com timeout para evitar travamentos permanentes. */
static bool i2c_wait_set(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (((*reg & mask) == 0U) && (timeout > 0U))
    {
        timeout--;
    }
    return timeout > 0U;
}

/* Aguarda flag desligar com timeout para evitar travamentos permanentes. */
static bool i2c_wait_clear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (((*reg & mask) != 0U) && (timeout > 0U))
    {
        timeout--;
    }
    return timeout > 0U;
}

/* Emite condicao START e confirma evento SB. */
static bool i2c_start(void)
{
    I2C1->CR1 |= I2C_CR1_START;
    return i2c_wait_set(&I2C1->SR1, I2C_SR1_SB);
}

/* Envia endereco e limpa sequencia ADDR via leitura SR1/SR2. */
static bool i2c_send_addr(uint8_t addr_rw)
{
    volatile uint32_t discard;

    I2C1->DR = addr_rw;
    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_ADDR))
    {
        return false;
    }

    discard = I2C1->SR1;
    discard = I2C1->SR2;
    (void)discard;
    return true;
}

void SystemInit(void)
{
    /* Habilita HSE externo (tipicamente cristal de 8 MHz). */
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0U)
    {
    }

    /* Ajustes de Flash para 64 MHz: prefetch e 2 wait states. */
    FLASH_R->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    /* Configura barramentos e PLL (HSE x8 => 64 MHz). */
    RCC->CFGR &= ~((0xFUL << 4) | (0x7UL << 8) | (0x7UL << 11) | (1UL << 16) | (0xFUL << 18));
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL8;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    /* Troca clock de sistema para PLL e aguarda confirmacao. */
    RCC->CFGR = (RCC->CFGR & ~(0x3UL << 0)) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL)
    {
    }
}

/* Handler da base de tempo global do firmware. */
void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0U)
    {
        TIM2->SR &= ~TIM_SR_UIF;
        s_ms_tick++;
    }
}

void USART1_IRQHandler(void)
{
    uint32_t status = USART1->SR;
    uint32_t errors = USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE;

    if ((status & (USART_SR_RXNE | errors)) != 0U)
    {
        uint8_t byte = (uint8_t)USART1->DR;
        uint32_t next_head = (s_rs485_rx_head + 1U) % RS485_RX_BUFFER_SIZE;

        if (((status & USART_SR_RXNE) != 0U) && ((status & errors) == 0U) &&
            (next_head != s_rs485_rx_tail))
        {
            s_rs485_rx_buffer[s_rs485_rx_head] = byte;
            s_rs485_rx_head = next_head;
        }
    }
}

void peripherals_init(void)
{
    /* Ordem sugerida: debug -> GPIO status -> comunicacao -> tempo -> sensores. */
    configure_debug_serial_wire();
    configure_led_pc13_output();
    configure_rs485_uart_9600();
    configure_tim2_1ms();
    configure_i2c1_fast_mode();
}

uint32_t peripherals_get_tick_ms(void)
{
    return s_ms_tick;
}

void peripherals_led_pc13_set(bool on)
{
    /* PC13 ativo em nivel baixo: BRR liga, BSRR desliga. */
    if (on)
    {
        GPIOC->BRR = (1UL << 13);
    }
    else
    {
        GPIOC->BSRR = (1UL << 13);
    }
}

void peripherals_led_pc13_toggle(void)
{
    /* Le ODR para inverter o estado sem variavel auxiliar. */
    if ((GPIOC->ODR & (1UL << 13)) != 0U)
    {
        GPIOC->BRR = (1UL << 13);
    }
    else
    {
        GPIOC->BSRR = (1UL << 13);
    }
}

bool peripherals_i2c1_write(uint8_t addr7, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    if ((data == 0) && (len > 0U))
    {
        return false;
    }

    /* Aguarda barramento livre (BUSY=0). */
    if (!i2c_wait_clear(&I2C1->SR2, (1UL << 1)))
    {
        return false;
    }

    /* START + endereco de escrita + bytes + STOP. */
    if (!i2c_start())
    {
        return false;
    }

    if (!i2c_send_addr((uint8_t)(addr7 << 1)))
    {
        I2C1->CR1 |= I2C_CR1_STOP;
        return false;
    }

    for (i = 0U; i < len; i++)
    {
        if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_TXE))
        {
            I2C1->CR1 |= I2C_CR1_STOP;
            return false;
        }
        I2C1->DR = data[i];
    }

    if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_BTF))
    {
        I2C1->CR1 |= I2C_CR1_STOP;
        return false;
    }

    I2C1->CR1 |= I2C_CR1_STOP;
    return true;
}

bool peripherals_i2c1_read(uint8_t addr7, uint8_t *data, uint32_t len)
{
    uint32_t i;

    if ((data == 0) || (len == 0U))
    {
        return false;
    }

    /* Aguarda barramento livre (BUSY=0). */
    if (!i2c_wait_clear(&I2C1->SR2, (1UL << 1)))
    {
        return false;
    }

    if (!i2c_start())
    {
        return false;
    }

    /*
     * Para leitura de 1 byte, ACK deve estar limpo antes do endereco.
     * Para multiplos bytes, ACK ligado ate o ultimo byte.
     */
    if (len == 1U)
    {
        I2C1->CR1 &= ~I2C_CR1_ACK;
    }
    else
    {
        I2C1->CR1 |= I2C_CR1_ACK;
    }

    if (!i2c_send_addr((uint8_t)((addr7 << 1) | 0x01U)))
    {
        I2C1->CR1 |= I2C_CR1_STOP;
        return false;
    }

    for (i = 0U; i < len; i++)
    {
        /* Ultimo byte: desliga ACK e emite STOP antes de ler DR. */
        if (i == (len - 1U))
        {
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |= I2C_CR1_STOP;
        }

        if (!i2c_wait_set(&I2C1->SR1, I2C_SR1_RXNE))
        {
            return false;
        }

        data[i] = (uint8_t)I2C1->DR;
    }

    I2C1->CR1 |= I2C_CR1_ACK;
    return true;
}

void peripherals_rs485_send(const uint8_t *data, size_t len)
{
    size_t i;

    if ((data == 0) || (len == 0U))
    {
        return;
    }

    uint32_t started_ms = peripherals_get_tick_ms();
    while ((peripherals_get_tick_ms() - started_ms) < RS485_TURNAROUND_MS)
    {
    }

    /*
     * Em RS485 half-duplex, primeiro tomamos o barramento (DE=1),
     * transmitimos todos os bytes e so depois voltamos para recepcao.
     */
    rs485_set_tx_mode();

    for (i = 0U; i < len; i++)
    {
        while ((USART1->SR & USART_SR_TXE) == 0U)
        {
        }
        USART1->DR = data[i];
    }

    /* TC garante shift register vazio antes de soltar DE. */
    while ((USART1->SR & USART_SR_TC) == 0U)
    {
    }

    rs485_set_rx_mode();
}

bool peripherals_rs485_read_byte(uint8_t *out_byte)
{
    if (out_byte == 0)
    {
        return false;
    }

    /* RX nao bloqueante: retorna false quando nao ha byte disponivel. */
    if (s_rs485_rx_head == s_rs485_rx_tail)
    {
        return false;
    }

    *out_byte = s_rs485_rx_buffer[s_rs485_rx_tail];
    s_rs485_rx_tail = (s_rs485_rx_tail + 1U) % RS485_RX_BUFFER_SIZE;
    return true;
}
