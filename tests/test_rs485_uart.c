#define _DEFAULT_SOURCE
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "../Src/peripherals_init.c"

static volatile sig_atomic_t held_until_tc;

static void tick(int signal_number)
{
    (void)signal_number;
    TIM2->SR = TIM_SR_UIF;
    TIM2_IRQHandler();
    if (s_ms_tick == 3U)
    {
        USART1->SR = USART_SR_TXE;
    }
    if (s_ms_tick == 4U)
    {
        held_until_tc = GPIOA->BSRR == (1UL << 8) && GPIOA->BRR == 0U;
        USART1->SR |= USART_SR_TC;
    }
}

static void receive(uint8_t byte, uint32_t status)
{
    USART1->DR = byte;
    USART1->SR = status;
    USART1_IRQHandler();
}

int main(void)
{
    assert(mmap((void *)PERIPH_BASE, 0x24000, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == (void *)PERIPH_BASE);
    assert(mmap((void *)0xE000E000UL, 0x2000, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == (void *)0xE000E000UL);
    peripherals_init();
    assert((USART1->CR1 & USART_CR1_RXNEIE) != 0U);
    assert(NVIC_ISER1 == (1UL << 5));
    assert((GPIOA->CRH & 0xFFFU) == 0x4B2U);
    assert(GPIOA->BRR == (1UL << 8));
    uint8_t byte;
    assert(!peripherals_rs485_read_byte(NULL));
    assert(!peripherals_rs485_read_byte(&byte));
    for (unsigned index = 0; index < 30; index++) receive((uint8_t)index, USART_SR_RXNE);
    for (unsigned index = 0; index < 30; index++) {
        assert(peripherals_rs485_read_byte(&byte));
        assert(byte == index);
    }
    assert(!peripherals_rs485_read_byte(&byte));
    receive(0xAA, USART_SR_RXNE | USART_SR_ORE);
    assert(!peripherals_rs485_read_byte(&byte));
    receive(0x55, USART_SR_RXNE);
    assert(peripherals_rs485_read_byte(&byte) && byte == 0x55);
    for (unsigned cycle = 0; cycle < 5; cycle++) {
        for (unsigned index = 0; index < 150; index++) receive((uint8_t)index, USART_SR_RXNE);
        for (unsigned index = 0; index < 127; index++) {
            assert(peripherals_rs485_read_byte(&byte));
            assert(byte == index);
        }
        assert(!peripherals_rs485_read_byte(&byte));
    }
    GPIOA->BSRR = 0;
    GPIOA->BRR = 0;
    USART1->SR = 0;
    struct sigaction action = {0};
    action.sa_handler = tick;
    assert(sigaction(SIGALRM, &action, NULL) == 0);
    struct itimerval timer = {{0, 1000}, {0, 1000}};
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    const uint8_t frame[] = {0x55, 1, 3, 0x10, 1, 0, 0, 0, 0x55};
    peripherals_rs485_send(frame, sizeof(frame));
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_usec = 0;
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    assert(held_until_tc);
    assert(GPIOA->BRR == (1UL << 8));
    assert(USART1->DR == frame[sizeof(frame) - 1]);
    puts("PASS: USART1 IRQ, buffered RX, overflow/wrap, UART error recovery, turnaround and DE held until TC");
    return 0;
}