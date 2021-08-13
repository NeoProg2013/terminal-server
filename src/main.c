//  ***************************************************************************
/// @file    main.c
/// @author  NeoProg
//  ***************************************************************************
#include "project_base.h"
#include "systimer.h"
#include "term-srv.h"

static void system_init(void);


#define USART_TX_PIN                    (2)  // PA2
#define USART_RX_PIN                    (3)  // PA3


void send_data(const char* data, uint16_t size) {
    while (size) {
        while ((USART1->ISR & USART_ISR_TXE) != USART_ISR_TXE);
        USART1->TDR = *data;
        ++data;
        --size;
    }
}

void test_cmd1(const char* data) {
    send_data("test_cmd1", strlen("test_cmd1"));
}
void test_cmd2(const char* data) {
    send_data("test_cmd2", strlen("test_cmd2"));
}

term_srv_cmd_t cmd_list[] = {
    { .cmd = "command1", .len = 8, .handler = test_cmd1 },
    { .cmd = "command2", .len = 8, .handler = test_cmd2 },
};

//  ***************************************************************************
/// @brief  Program entry point
/// @param  none
/// @return none
//  ***************************************************************************
int main() {
    
    system_init();
    systimer_init();
    term_srv_init(send_data, cmd_list, 2);
    
    
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    while ((RCC->APB2ENR & RCC_APB2ENR_USART1EN) == 0);
    
    // Setup TX pin
    gpio_set_mode        (GPIOA, USART_TX_PIN, GPIO_MODE_AF);
    gpio_set_output_speed(GPIOA, USART_TX_PIN, GPIO_SPEED_HIGH);
    gpio_set_pull        (GPIOA, USART_TX_PIN, GPIO_PULL_NO);
    gpio_set_af          (GPIOA, USART_TX_PIN, 1);
    
    // Setup RX pin
    gpio_set_mode        (GPIOA, USART_RX_PIN, GPIO_MODE_AF);
    gpio_set_output_speed(GPIOA, USART_RX_PIN, GPIO_SPEED_HIGH);
    gpio_set_pull        (GPIOA, USART_RX_PIN, GPIO_PULL_UP);
    gpio_set_af          (GPIOA, USART_RX_PIN, 1);

    // Setup USART: 8N1
    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;
    USART1->BRR = SYSTEM_CLOCK_FREQUENCY / 115200;

    // Enable USART
    USART1->CR1 |= USART_CR1_UE;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;
    
    
    while (true) {
        if (USART1->ISR & USART_ISR_RXNE) {
            uint8_t data = USART1->RDR;
            term_srv_process(data);
        }
    }
}

//  ***************************************************************************
/// @brief  System initialization
/// @param  none
/// @return none
//  ***************************************************************************
static void system_init(void) {
    
    // Enable Prefetch Buffer
    FLASH->ACR = FLASH_ACR_PRFTBE;
    
    // Switch USARTx clock source to system clock
    RCC->CFGR3 |= RCC_CFGR3_USART1SW_0;

    // Enable GPIO clocks
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    RCC->AHBENR |= RCC_AHBENR_GPIODEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOEEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOFEN;
    
    // Enable clocks for USART1
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    while ((RCC->APB2ENR & RCC_APB2ENR_USART1EN) == 0);
}
