/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Hardware includes. */
#include "stm32f4xx.h"
#include "stm32f4xx_nucleo.h"
#include "system_stm32f4xx.h"

#include "itm.h"
#include "bme680.h"
#include "bme680poll.h"

extern void xPortSysTickHandler(void);

/* Task priorities */
#define mainBME680_POLL_TASK_PRIORITY ( tskIDLE_PRIORITY + 1UL )

/* A block time of zero simply means "don't block". */
#define mainDONT_BLOCK                             (0UL)

/*--------------------------------[ Globals ]---------------------------------*/

BME680_HandleTypeDef hbme = {0};
I2C_HandleTypeDef hi2c = {0};
SPI_HandleTypeDef hspi = {0};
TIM_HandleTypeDef htim6 = {0};

/*-------------------------------[ Prototypes ]-------------------------------*/

static void prvSetupHardware(void);
static void SystemClock_Config(void);
static void Error_Handler(void);
static void prvSetupBME680(void);
static void prvSetupSDCard(void);

/*-------------------------------[ Functions ]--------------------------------*/

int main(void)
{

    /* Configure the hardware */
    prvSetupHardware();
	//ITM_Init();
	prvSetupBME680();
	//prvSetupSDCard();
	
	/* Start tasks */
	vStartBME680PollTask(mainBME680_POLL_TASK_PRIORITY);

    /* Start the scheduler. */
    vTaskStartScheduler();
	
    for(;;)
    {
    }
}

static void prvSetupHardware(void)
{
	uint32_t priority;
    /* Setup STM32 system (HAL, Clock) */
	HAL_Init();
	SystemClock_Config();
    /* Ensure all priority bits are assigned as preemption priority bits. */
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

static void prvSetupBME680(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	/* Enable clocks */
	__HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
	
	/* Configure HAL GPIO Init structure */
	GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* Configure HAL I2C Handle */
	hbme.hi2c = &hi2c;
	hbme.hi2c->Instance = I2C1;
	hbme.hi2c->Init.ClockSpeed = 100000;
	hbme.hi2c->Init.DutyCycle = I2C_DUTYCYCLE_2;
	hbme.hi2c->Init.OwnAddress1 = 0;
	hbme.hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hbme.hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hbme.hi2c->Init.OwnAddress2 = 0;
	hbme.hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hbme.hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	
	if(HAL_I2C_Init(hbme.hi2c) != HAL_OK)
	{
		for( ; ; )
		{
		}
	}
}

static void prvSetupSDCard(void)
{
	/* Enable clocks */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_SPI1_CLK_ENABLE();

    /* PA5 SCK, PA6 MISO, PA7 MOSI */
	GPIO_InitTypeDef GPIO_Init = {0};
	GPIO_Init.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_Init.Mode = GPIO_MODE_AF_PP;       // AF Push-Pull
	GPIO_Init.Pull = GPIO_NOPULL;           // external levels
	GPIO_Init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_Init.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_Init);

    /* PA4 CS as GPIO output */
	GPIO_Init.Pin = GPIO_PIN_4;
	GPIO_Init.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_Init.Pull = GPIO_NOPULL;
	GPIO_Init.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_Init);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); // CS idle high

	hspi.Instance               = SPI1;
	hspi.Init.Mode              = SPI_MODE_MASTER;
	hspi.Init.Direction         = SPI_DIRECTION_2LINES;
	hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
	hspi.Init.CLKPolarity       = SPI_POLARITY_LOW; // CPOL = 0
	hspi.Init.CLKPhase          = SPI_PHASE_1EDGE; // CPHA = 0 → MODE0
	hspi.Init.NSS               = SPI_NSS_SOFT; // Use software CS (GPIO)
	hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; // slow for init
	hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
	hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
	hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
	hspi.Init.CRCPolynomial     = 7;

	if(HAL_SPI_Init(&hspi) != HAL_OK)
	{
		//printf("main: FAILED TO INIT SPI\n");
		for( ; ; )
		{
		}
	}
}

void SysTick_Handler(void)
{
    xPortSysTickHandler();
}

void vApplicationTickHook(void)
{
	
}

void vApplicationMallocFailedHook(void)
{
	/* Hook function that will get called if a call to pvPortMalloc() fails.
     * pvPortMalloc() is called internally by the kernel whenever a task, queue,
     * timer or semaphore is created.  It is also called by various parts of the
     * demo application.  If heap_1.c or heap_2.c are used, then the size of the
     * heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
     * FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
     * to query the size of free heap space that remains (although it does not
     * provide information on how the remaining heap might be fragmented). */
    taskDISABLE_INTERRUPTS();

	//printf("ATTEMPTED TO MALLOC. MALLOC ALWAYS FAILS! SEE syscalls.c!\n");
	
    for(; ;)
    {
    }
}

void vApplicationIdleHook(void)
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
     * to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
     * task. It is essential that code added to this hook function never
	 * attempts to block in any way (for example, call xQueueReceive() with a
	 * block time specified, or call vTaskDelay()). If the application makes use
	 * of the vTaskDelete() API function (as this demo application does) then it
	 * is also important that vApplicationIdleHook() is permitted to return to
	 * its calling function, because it is the responsibility of the idle task to
	 * clean up memory allocated by the kernel to any task that has since been
	 * deleted. */
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask,
                                    char * pcTaskName)
{
    (void) pcTaskName;
    (void) pxTask;

    /* Run time stack overflow checking is performed if
     * configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
     * function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();

    for(; ;)
    {
    }
}

/* Use TIM6 for HAL timebase instead of SysTick */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t uwTimclock = 0;
    uint32_t uwPrescalerValue = 0;
    uint32_t pFLatency;
    
    /* Enable TIM6 clock */
    __HAL_RCC_TIM6_CLK_ENABLE();
    
    /* Get clock configuration */
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
    
    /* Compute TIM6 clock */
    uwTimclock = HAL_RCC_GetPCLK1Freq();

	if(clkconfig.APB1CLKDivider != RCC_HCLK_DIV1)
	{
		uwTimclock *= 2;
	}
	
    /* Compute prescaler to get 1MHz timer clock */
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);
    
    /* Initialize TIM6 */
    htim6.Instance = TIM6;
    htim6.Init.Period = 999U; // 1ms
    htim6.Init.Prescaler = uwPrescalerValue;
    htim6.Init.ClockDivision = 0;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    
    if(HAL_TIM_Base_Init(&htim6) != HAL_OK)
    {
		return HAL_ERROR;
	}
	
	/* Start timer and enable interrupt */
	if(HAL_TIM_Base_Start_IT(&htim6) != HAL_OK)
	{
		return HAL_ERROR;
	}
    
	/* Set interrupt priority */
	HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0);
	HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    
	return HAL_OK;
}

void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM6)
    {
        HAL_IncTick();
    }
}

static void SystemClock_Config(void)
{
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};

	/* Enable Power Control clock */
	__HAL_RCC_PWR_CLK_ENABLE();

	/* The voltage scaling allows optimizing the power consumption when the
	   device is clocked below the maximum system frequency, to update the
	   voltage scaling value regarding system frequency refer to product
	   datasheet.  */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/* Enable HSI Oscillator and activate PLL with HSI as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 128;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 8;
	RCC_OscInitStruct.PLL.PLLR = 0;

	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
		{
			Error_Handler();
		}

	/* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
	   clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
								   RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2);
	
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV8;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	
	if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
		{
			Error_Handler();
		}
}

static void Error_Handler(void)
{
	/* User may add here some code to deal with this error */
	while(1)
		{
		}
}
