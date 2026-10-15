#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>


#define QUEUE_SIZE          10
#define DEBOUNCE_TIME_MS    200
#define UART_BAUD_RATE      115200

static QueueHandle_t xQueueCount;
static volatile uint32_t pressCount = 0;

static void GPIO_Config(void)
{

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;


    GPIOA->CRL &= ~(0xFu << 0);
    GPIOA->CRL |=  (0x8u << 0);
    GPIOA->ODR |=  (1u << 0);


    GPIOA->CRH &= ~((0xFu << 4) | (0xFu << 8));
    GPIOA->CRH |=  (0xBu << 4) | (0x4u << 8);
}

static void EXTI_Config(void)
{

    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    EXTI->IMR  |= EXTI_IMR_MR0;
    EXTI->FTSR |= EXTI_FTSR_TR0;
    EXTI->RTSR &= ~EXTI_RTSR_TR0;


    NVIC_SetPriority(EXTI0_IRQn, 6);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

static void UART1_Config(void)
{

    USART1->BRR = SystemCoreClock / UART_BAUD_RATE;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void UART_SendString(const char *s)
{
    while (*s)
    {
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = *s++;
    }
    while (!(USART1->SR & USART_SR_TC));
}

void EXTI0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static TickType_t lastTick = 0;

    if (EXTI->PR & EXTI_PR_PR0)
    {
        EXTI->PR = EXTI_PR_PR0;


        TickType_t now = xTaskGetTickCountFromISR();
        if ((now - lastTick) > pdMS_TO_TICKS(DEBOUNCE_TIME_MS))
        {
            lastTick = now;
            pressCount++;
            uint32_t value = pressCount;
            xQueueSendFromISR(xQueueCount, &value, &xHigherPriorityTaskWoken);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vUartTask(void *pvParameters)
{
    (void)pvParameters;
    uint32_t value;
    char buf[40];

    for (;;)
    {

        if (xQueueReceive(xQueueCount, &value, portMAX_DELAY) == pdPASS)
        {
            sprintf(buf, "So lan nhan nut: %lu\r\n", (unsigned long)value);
            UART_SendString(buf);
        }
    }
}

int main(void)
{
    SystemInit();

    GPIO_Config();
    EXTI_Config();
    UART1_Config();


    xQueueCount = xQueueCreate(QUEUE_SIZE, sizeof(uint32_t));


    xTaskCreate(vUartTask, "UART", 256, NULL, 2, NULL);


    vTaskStartScheduler();

    for (;;);
}