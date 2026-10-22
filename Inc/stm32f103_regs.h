#ifndef STM32F103_REGS_H
#define STM32F103_REGS_H

#include <stdint.h>

#define PERIPH_BASE            0x40000000UL
#define APB1PERIPH_BASE        PERIPH_BASE
#define APB2PERIPH_BASE        (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE         (PERIPH_BASE + 0x00020000UL)

#define RCC_BASE               (AHBPERIPH_BASE + 0x00001000UL)
#define FLASH_R_BASE           0x40022000UL
#define AFIO_BASE              (APB2PERIPH_BASE + 0x00000000UL)
#define GPIOA_BASE             (APB2PERIPH_BASE + 0x00000800UL)
#define GPIOB_BASE             (APB2PERIPH_BASE + 0x00000C00UL)
#define GPIOC_BASE             (APB2PERIPH_BASE + 0x00001000UL)
#define USART1_BASE            (APB2PERIPH_BASE + 0x00003800UL)
#define I2C1_BASE              (APB1PERIPH_BASE + 0x00005400UL)
#define TIM2_BASE              (APB1PERIPH_BASE + 0x00000000UL)
#define DMA1_BASE              (AHBPERIPH_BASE + 0x00000000UL)
#define DMA1_Channel5_BASE     (DMA1_BASE + 0x00000058UL)

#define NVIC_ISER0             (*(volatile uint32_t *)0xE000E100UL)
#define NVIC_ISER1             (*(volatile uint32_t *)0xE000E104UL)

typedef struct
{
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
} RCC_TypeDef;

typedef struct
{
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t AR;
    volatile uint32_t RESERVED;
    volatile uint32_t OBR;
    volatile uint32_t WRPR;
} FLASH_TypeDef;

typedef struct
{
    volatile uint32_t EVCR;
    volatile uint32_t MAPR;
    volatile uint32_t EXTICR1;
    volatile uint32_t EXTICR2;
    volatile uint32_t EXTICR3;
    volatile uint32_t EXTICR4;
    volatile uint32_t MAPR2;
} AFIO_TypeDef;

typedef struct
{
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
    volatile uint32_t LCKR;
} GPIO_TypeDef;

typedef struct
{
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} USART_TypeDef;

typedef struct
{
    volatile uint32_t CCR;
    volatile uint32_t CNDTR;
    volatile uint32_t CPAR;
    volatile uint32_t CMAR;
    volatile uint32_t RESERVED;
} DMA_Channel_TypeDef;

typedef struct
{
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t OAR2;
    volatile uint32_t DR;
    volatile uint32_t SR1;
    volatile uint32_t SR2;
    volatile uint32_t CCR;
    volatile uint32_t TRISE;
} I2C_TypeDef;

typedef struct
{
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RESERVED1;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t RESERVED2;
    volatile uint32_t DCR;
    volatile uint32_t DMAR;
} TIM_TypeDef;

#define RCC                  ((RCC_TypeDef *)RCC_BASE)
#define FLASH_R              ((FLASH_TypeDef *)FLASH_R_BASE)
#define AFIO                 ((AFIO_TypeDef *)AFIO_BASE)
#define GPIOA                ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB                ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC                ((GPIO_TypeDef *)GPIOC_BASE)
#define USART1               ((USART_TypeDef *)USART1_BASE)
#define DMA1_Channel5        ((DMA_Channel_TypeDef *)DMA1_Channel5_BASE)
#define I2C1                 ((I2C_TypeDef *)I2C1_BASE)
#define TIM2                 ((TIM_TypeDef *)TIM2_BASE)

#define RCC_CR_HSEON         (1UL << 16)
#define RCC_CR_HSERDY        (1UL << 17)
#define RCC_CR_PLLON         (1UL << 24)
#define RCC_CR_PLLRDY        (1UL << 25)

#define RCC_CFGR_SW_PLL      (0x2UL << 0)
#define RCC_CFGR_SWS_MASK    (0x3UL << 2)
#define RCC_CFGR_SWS_PLL     (0x2UL << 2)
#define RCC_CFGR_HPRE_DIV1   (0x0UL << 4)
#define RCC_CFGR_PPRE1_DIV2  (0x4UL << 8)
#define RCC_CFGR_PPRE2_DIV1  (0x0UL << 11)
#define RCC_CFGR_PLLSRC_HSE  (1UL << 16)
#define RCC_CFGR_PLLMULL8    (0x6UL << 18)

#define FLASH_ACR_PRFTBE     (1UL << 4)
#define FLASH_ACR_LATENCY_2  (0x2UL << 0)

#define RCC_AHBENR_DMA1EN    (1UL << 0)
#define RCC_APB2ENR_AFIOEN   (1UL << 0)
#define RCC_APB2ENR_IOPAEN   (1UL << 2)
#define RCC_APB2ENR_IOPBEN   (1UL << 3)
#define RCC_APB2ENR_IOPCEN   (1UL << 4)
#define RCC_APB2ENR_USART1EN (1UL << 14)
#define RCC_APB1ENR_TIM2EN   (1UL << 0)
#define RCC_APB1ENR_I2C1EN   (1UL << 21)

#define AFIO_MAPR_SWJ_CFG_MASK (0x7UL << 24)
#define AFIO_MAPR_SWJ_CFG_SWD  (0x2UL << 24)

#define USART_SR_PE          (1UL << 0)
#define USART_SR_FE          (1UL << 1)
#define USART_SR_NE          (1UL << 2)
#define USART_SR_ORE         (1UL << 3)
#define USART_SR_RXNE        (1UL << 5)
#define USART_SR_TC          (1UL << 6)
#define USART_SR_TXE         (1UL << 7)
#define USART_CR1_UE         (1UL << 13)
#define USART_CR1_RXNEIE     (1UL << 5)
#define USART_CR1_TE         (1UL << 3)
#define USART_CR1_RE         (1UL << 2)

#define DMA_CCR_EN           (1UL << 0)
#define DMA_CCR_TCIE         (1UL << 1)
#define DMA_CCR_CIRC         (1UL << 5)
#define DMA_CCR_MINC         (1UL << 7)

#define TIM_CR1_CEN          (1UL << 0)
#define TIM_DIER_UIE         (1UL << 0)
#define TIM_SR_UIF           (1UL << 0)
#define TIM_EGR_UG           (1UL << 0)

#define I2C_CR1_PE           (1UL << 0)
#define I2C_CR1_START        (1UL << 8)
#define I2C_CR1_STOP         (1UL << 9)
#define I2C_CR1_ACK          (1UL << 10)
#define I2C_CR1_SWRST        (1UL << 15)

#define I2C_SR1_SB           (1UL << 0)
#define I2C_SR1_ADDR         (1UL << 1)
#define I2C_SR1_BTF          (1UL << 2)
#define I2C_SR1_RXNE         (1UL << 6)
#define I2C_SR1_TXE          (1UL << 7)

#define I2C_CCR_FS           (1UL << 15)

#define FLASH_KEY1           0x45670123UL
#define FLASH_KEY2           0xCDEF89ABUL

#define FLASH_SR_BSY         (1UL << 0)

#define FLASH_CR_PG          (1UL << 0)
#define FLASH_CR_PER         (1UL << 1)
#define FLASH_CR_STRT        (1UL << 6)
#define FLASH_CR_LOCK        (1UL << 7)

#endif
