
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include "singlewirebus.h"
#include "singlewirenet.h"

#define PERIPH_TO_BITPERIPH(addr,bit) ((uint32_t *)(PERIPH_BB_BASE + ((uint32_t)addr & 0x01ffffff) * 32 + bit * 4))
#define SET_WIRE1(b) *PERIPH_TO_BITPERIPH(GPIOE->ODR,14) = b
#define GET_WIRE1(b) *PERIPH_TO_BITPERIPH(GPIOE->IDR,14)

/*
#define BEGIN_HOLD_TIME     100
#define MAX_BEGINE_TIME     150
#define MIN_BEGINE_TIME     20
#define MIN_HOLD_HIGH_TIME  2
#define WAIT_ACK_TIMEOUT    10
#define BEGINE_ONE_BIT_TIME 10
#define ONE_BIT_HOLD_TIME   10
#define ACK_HOLD_TIME       10

*/


void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

}

int test = 0;
uint32_t timeSend = 0;
uint8_t dataValue;
SingleBusStruct receivePacket;
SingleBusStruct sendPacket;

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	
	sendPacket.srcAddr = 0x01;
	sendPacket.disAddr = 0x02;
	sendPacket.length = 1;
	sendPacket.data[0] = 0;
	
	singleWireBusInit();
	singleWireNetInit();
	
	while(1)
	{
		singleWireNetUpdate();
	}
	
	/*
	while(1)
	{
		singleWireBusUpdate();
		test = 0;
		while(test != -1)
		test = getSingleBusData(&receivePacket);
		
		
		
		if(HAL_GetTick() - timeSend >= 100)
		{
			sendPacket.data[0] ++;
			timeSend = HAL_GetTick();
			sendSingleBusData(0,&sendPacket,3);
			sendSingleBusData(1,&sendPacket,3);
			sendSingleBusData(2,&sendPacket,3);
			sendSingleBusData(3,&sendPacket,3);
			sendSingleBusData(4,&sendPacket,3);
			sendSingleBusData(5,&sendPacket,3);
		}
	}
	*/
	/*
	TIM_HandleTypeDef    TimHandle;
	GPIO_InitTypeDef GPIO_InitStruct;
	__GPIOE_CLK_ENABLE();
	
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_14;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
		
	__TIM2_CLK_ENABLE();
	
	TimHandle.Instance = TIM2;
  TimHandle.Init.Period = 0xffff;
  TimHandle.Init.Prescaler = 84 - 1;
  TimHandle.Init.ClockDivision = 0;
  TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
  HAL_TIM_Base_Init(&TimHandle);
  *odreg = 1;
	
	while(1)
	{

		if(*idreg == 0)
		{
			TIM2->CNT = 0;
			TIM2->CR1|=(TIM_CR1_CEN);
			int begintime = 0;
			int step = 0;		
			int bitIndex = 0;
			uint8_t buff[32];
      bool  loopflag = true;
			bool datagood = false;
			dataValue = 0;
			
			while(loopflag)
			{
				int distime = TIM2->CNT - begintime;
				switch(step)
				{
					case 0:
						if(*idreg)
						{
						   if(distime > MIN_BEGINE_TIME)
						   {
								 begintime = TIM2->CNT;
							   step ++;
						   }
							 else
							 {
								 step = -1;
							 }
					  }
						else if(distime > MAX_BEGINE_TIME)
						{
							step = -1;
						}		
						break;
					case 1:
						if(distime >= MIN_HOLD_HIGH_TIME)
						{
							*odreg = 0;
							begintime = TIM2->CNT;
							step ++;
						}
						break;
					case 2:
						if(distime >= ACK_HOLD_TIME)
						{
							step ++;
							*odreg = 1;
							begintime = TIM2->CNT;
						}
						break;
					case 3:
						if(*idreg)
						{
							begintime = TIM2->CNT;
							step ++;
						}
						else if(distime > 10)
						{
							step = -1;
						}
						break;
					case 4:
						if(*idreg == 0)
						{
							begintime = TIM2->CNT;
							step ++;
						}
						else if(distime > 20)
						{
							step = -1;
						}
						break;
					case 5:
						if(distime >= 15)
						{
							int bitValue = *idreg;
							int buffIndex;
							int subbitIndex;
							begintime = TIM2->CNT;
							step = 3;
							buffIndex = bitIndex >> 3;
							subbitIndex = bitIndex & 0x07;
							dataValue |= (bitValue << subbitIndex);
							
							if(subbitIndex == 7)
							{
								buff[buffIndex] = dataValue;
								dataValue = 0;
							}
							
							bitIndex ++;
							if(buffIndex > 3 && bitIndex == (5 + buff[3]) * 8)
							{
									begintime = TIM2->CNT;
							    step = 6;
							}
						}
						break;
					case 6:
						if(distime >= 7)
						{
							begintime = TIM2->CNT;
							step ++;
							*odreg = 0;
						}
						break;
					case 7:
						if(distime >= 10)
						{
							*odreg = 1;
							datagood = true;
							loopflag = false;
						}
						break;
					default:
					  loopflag = false;
					  break;
				}				
			}
			*odreg = 1;
			TIM2->CR1 &= ~(TIM_CR1_CEN);
		}
		
		if(HAL_GetTick() - timeSend >= 100 && *idreg)
		{
			*odreg = 0;
			timeSend = HAL_GetTick();
			
			TIM2->CNT = 0;
			TIM2->CR1|=(TIM_CR1_CEN);
			int begintime = 0;
			int step = 0;		
			int bitIndex = 0;
			uint8_t buff[32] = {0,0,0,12,'H','e','l','l','o',' ','W','o','r','l','d','!',0};
      bool  loopflag = true;
			bool datagood = false;
			int bitValue;
			
			
			while(loopflag)
			{
				int distime = TIM2->CNT - begintime;
				switch(step)
				{
					case 0:
						if(distime >= BEGIN_HOLD_TIME)
						{
							*odreg = 1;
							begintime = TIM2->CNT;
							step ++;
						}
						break;
					case 1:
						if(*idreg)
						{
							begintime = TIM2->CNT;
							step ++;
						}
						else if(distime > 10)
						{
							step = -1;
						}
						break;
					case 2:
						if(*idreg == 0)
						{
							begintime = TIM2->CNT;
							step ++;
						}
						else if(distime > 10)
						{
							step = -1;
						}
						break;
					case 3:
						if(*idreg)
						{
							*otypereg = 0;
							begintime = TIM2->CNT;
							step ++;
						}
						else if(distime > 20)
						{
							step = -1;
						}
						break;
					case 4:
						if(distime >= 2)
						{
							*odreg = 0;
							begintime = TIM2->CNT;
							step ++;
							int buffIndex;
			        int subbitIndex;
							
							buffIndex = bitIndex >> 3;
							subbitIndex = bitIndex & 0x07;
							bitValue = (buff[buffIndex] & (1 << subbitIndex ))? 1 : 0;
						}
						break;
					case 5:
						if(distime >= 10)
						{
							begintime = TIM2->CNT;
							step ++;
              *odreg = bitValue;
						}
						break;
					case 6:
						if(distime >= 10)
						{
							*odreg = 1;
							step = 3;
              begintime = TIM2->CNT;
							bitIndex ++;
							if( bitIndex == (5 + buff[3]) * 8)
							{
								step = 7;
							}
						}
						break;
					case 7:
						if(*idreg)
						{
							*otypereg = 1;
							begintime = TIM2->CNT;
							step ++;
						}
						else if(distime >= 10)
						{
							step = -1;
						}
						break;
					case 8:
						if(*idreg == 0)
						{
							begintime = TIM2->CNT;
							step ++;
						}
						else if(distime >= 10)
						{
							step = -1;
						}
						break;
					case 9:
						if(*idreg)
						{
							datagood = true;
							loopflag = false;
						}
						else if(distime >= 20)
						{
							step = -1;
						}
						break;
					default:
						*otypereg = 1;
					  loopflag = false;
					  break;
				}				
			}
			*odreg = 1;
			TIM2->CR1 &= ~(TIM_CR1_CEN);
			
		}
	}*/
	return 0;
}
