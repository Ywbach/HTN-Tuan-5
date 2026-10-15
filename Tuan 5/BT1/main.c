#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define STEP        10
#define NUM_LEDS    3

static QueueHandle_t qLed[NUM_LEDS];

static void GPIO_Config(void)
{

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;


    GPIOA->CRL &= ~0x00000FFF;
    GPIOA->CRL |=  0x00000BBB;


    GPIOB->CRH &= ~0x0FFF0000;
    GPIOB->CRH |=  0x08880000;
    GPIOB->ODR |=  (7 << 12);
}

static void PWM_Config(void)
{

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;


    TIM2->PSC = 71;
    TIM2->ARR = 999;


    TIM2->CCMR1 = (6 << 4) | (1 << 3) | (6 << 12) | (1 << 11);
    TIM2->CCMR2 = (6 << 4) | (1 << 3);


    TIM2->CCER  = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;


    TIM2->CCR1 = 0;
    TIM2->CCR2 = 0;
    TIM2->CCR3 = 0;


    TIM2->EGR |= TIM_EGR_UG;
    TIM2->CR1 |= TIM_CR1_CEN;
}

static void LED_SetDuty(uint32_t id, uint8_t duty)
{
    uint16_t ccr = (uint16_t)duty * 10;
    switch (id)
    {
        case 0: TIM2->CCR1 = ccr; break;
        case 1: TIM2->CCR2 = ccr; break;
        case 2: TIM2->CCR3 = ccr; break;
        default: break;
    }
}

static void vLedTask(void *pvParameters)
{
    uint32_t id = (uint32_t)pvParameters;
    uint8_t  duty;

    for (;;)
    {
        if (xQueueReceive(qLed[id], &duty, portMAX_DELAY) == pdPASS)
        {
            LED_SetDuty(id, duty);
        }
    }
}

static uint8_t Btn_Pressed(uint8_t n)
{
    return ((GPIOB->IDR & (1 << (12 + n))) == 0);
}

static void Blink_Select(uint8_t sel, uint8_t bright)
{
    uint8_t flash = (bright >= 50) ? 0 : 100;

    xQueueSend(qLed[sel], &flash, 0);
    vTaskDelay(pdMS_TO_TICKS(150));
    xQueueSend(qLed[sel], &bright, 0);
}

static void vButtonTask(void *pvParameters)
{
    uint8_t bright[NUM_LEDS] = {50, 50, 50};
    uint8_t sel = 0;
    uint8_t now[NUM_LEDS], last[NUM_LEDS] = {0, 0, 0};
    uint8_t i;

    (void)pvParameters;

 
    for (i = 0; i < NUM_LEDS; i++)
    {
        xQueueSend(qLed[i], &bright[i], 0);
    }

    for (;;)
    {

        for (i = 0; i < NUM_LEDS; i++)
        {
            now[i] = Btn_Pressed(i);
        }


        if (now[0] && !last[0])
        {
            sel = (sel + 1) % NUM_LEDS;
            Blink_Select(sel, bright[sel]);
        }


        if (now[1] && !last[1])
        {
            if (bright[sel] <= 100 - STEP) 
            {
                bright[sel] += STEP;
            }
            else                           
            {
                bright[sel] = 100;
            }
            xQueueSend(qLed[sel], &bright[sel], 0);
        }


        if (now[2] && !last[2])
        {
            if (bright[sel] >= STEP) 
            {
                bright[sel] -= STEP;
            }
            else                     
            {
                bright[sel] = 0;
            }
            xQueueSend(qLed[sel], &bright[sel], 0);
        }


        for (i = 0; i < NUM_LEDS; i++)
        {
            last[i] = now[i];
        }


        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

int main(void)
{
    uint32_t i;


    GPIO_Config();
    PWM_Config();


    for (i = 0; i < NUM_LEDS; i++)
    {
        qLed[i] = xQueueCreate(5, sizeof(uint8_t));
    }

    xTaskCreate(vLedTask, "LED1", 128, (void *)0, 2, NULL);
    xTaskCreate(vLedTask, "LED2", 128, (void *)1, 2, NULL);
    xTaskCreate(vLedTask, "LED3", 128, (void *)2, 2, NULL);


    xTaskCreate(vButtonTask, "BTN", 128, NULL, 3, NULL);


    vTaskStartScheduler();


    for (;;);
}