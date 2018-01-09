
#include "singlewirebus.h"
#include <stdbool.h>
#include <string.h>

#define PERIPH_TO_BITPERIPH(addr,bit) ((uint32_t *)(PERIPH_BB_BASE + ((uint32_t)addr & 0x01ffffff) * 32 + bit * 4))


#define CHANNEL_IDLE        0
#define CHANNEL_RECEIVE     1
#define CHANNEL_SEND        2
#define CHANNEL_WIRE_FAULT      3
#define CHANNEL_SEND_ERROR       4



#define START_TIMER()    TIM2->CR1|=(TIM_CR1_CEN)
#define STOP_TIMER()     TIM2->CR1 &= ~(TIM_CR1_CEN)
#define CLEAR_TIMER()    TIM2->CNT = 0
#define GET_TIMER_CNT()  TIM2->CNT

#define GET_CHANNEL_IDR(channel)  *idreg[channel]
#define SET_CHANNEL_ODR(channel,bit)  *odreg[channel] = bit
#define SET_CHANNEL_OD(channel)    *otypereg[channel] = 1
#define SET_CHANNEL_PP(channel)    *otypereg[channel] = 0
/*
#define BEGIN_HOLD_TIME        50
#define MAX_BEGINE_TIME        60
#define MIN_BEGINE_TIME        25
#define MIN_HOLD_HIGH_TIME     5
#define WAIT_ACK_TIMEOUT       10
#define BEGINE_ONE_BIT_TIME    5
#define ONE_BIT_HOLD_TIME      10
#define ACK_HOLD_TIME          5
#define MAX_ACK_HOLD_TIME      20
#define MAX_WIRE_ONE_BIT_TIME  10
#define MAX_WIRE_REC_HIGH_TIME 2
*/

#define BEGIN_HOLD_TIME        50
#define MAX_BEGINE_TIME        60
#define MIN_BEGINE_TIME        25
#define MIN_HOLD_HIGH_TIME     5
#define WAIT_ACK_TIMEOUT       10
#define BEGINE_ONE_BIT_TIME    5
#define ONE_BIT_HOLD_TIME      10
#define ACK_HOLD_TIME          5
#define MAX_ACK_HOLD_TIME      20
#define MAX_WIRE_ONE_BIT_TIME  10
#define MAX_WIRE_REC_HIGH_TIME 2

typedef struct
{
	SingleBusStruct receivePacket;
	SingleBusStruct sendPacket;
	uint32_t  lastRecevieTime;
	uint32_t  lastSendTime;
	uint32_t  receiveErrorTimes;
	uint32_t  sendErrorTimes;
	uint32_t  receivePacketNum;
	uint32_t  sendPacketNum;
	int       beginTime;
	uint8_t       bitIndex;
	uint8_t       buffIndex;
	uint8_t       allDataNum;
	uint8_t       runStep;
	
	uint8_t hasReceiveData;
	uint8_t hasSendData;
	uint8_t retrySendTimes;
	uint8_t channelNum;
	
	uint8_t channelState;
	
	uint8_t dataValueTemp;
}SingleBusChannelStruct;

static SingleBusChannelStruct busChannel[6];
static uint32_t *idreg[6];
static uint32_t *odreg[6];
static uint32_t *otypereg[6];

#define SINGLE_BUS_TEST

#ifdef  SINGLE_BUS_TEST

#define  INC_STEP_OUT(a)   stepOut[a] ++

static int stepOut[9];

#else 

#define  INC_STEP_OUT(a)

#endif

static bool RunChannelIdle(SingleBusChannelStruct *p_busChannel)
{
	if(GET_CHANNEL_IDR(p_busChannel->channelNum) == 0)
	{
		if(!p_busChannel->hasReceiveData)
		{
			 p_busChannel->beginTime = GET_TIMER_CNT();
		   p_busChannel->channelState = CHANNEL_RECEIVE;
			 p_busChannel->bitIndex = 0;
			 p_busChannel->buffIndex = 0;
			 p_busChannel->runStep = 0;
			 p_busChannel->dataValueTemp = 0;
			 p_busChannel->allDataNum = 0;
			 return true;
		}
		else 
		{
			 return false;
		}
	}
	else if(p_busChannel->hasSendData)
	{
		SET_CHANNEL_ODR(p_busChannel->channelNum,0);
		p_busChannel->beginTime = GET_TIMER_CNT();
		p_busChannel->channelState = CHANNEL_SEND;
		p_busChannel->bitIndex = 0;
		p_busChannel->buffIndex = 0;
		p_busChannel->runStep = 0;
		p_busChannel->allDataNum = p_busChannel->sendPacket.length + 5;
		return true;
	}
	return false;
}

static bool RunChannelReceive(SingleBusChannelStruct *p_busChannel)
{
		int distime = GET_TIMER_CNT() - p_busChannel->beginTime;
		switch(p_busChannel->runStep)
		{
			case 0:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum))
				{
					if(distime > MIN_BEGINE_TIME)
					{
							p_busChannel->beginTime = GET_TIMER_CNT();
							p_busChannel->runStep ++;
					}
					else
					{
						  INC_STEP_OUT(0);
							p_busChannel->runStep = 0xff;
					}
				}
				else if(distime > MAX_BEGINE_TIME)
				{
					INC_STEP_OUT(1);
					p_busChannel->channelState = CHANNEL_WIRE_FAULT;
				}		
				break;
			case 1:
				if(distime >= MIN_HOLD_HIGH_TIME)
				{
					SET_CHANNEL_ODR(p_busChannel->channelNum,0);
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				break;
			case 2:
				if(distime >= ACK_HOLD_TIME)
				{
					p_busChannel->runStep ++;
				  SET_CHANNEL_ODR(p_busChannel->channelNum,1);
					p_busChannel->beginTime = GET_TIMER_CNT();
				}
				break;
			case 3:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum))
			  {
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				else if(distime > MAX_WIRE_REC_HIGH_TIME)
				{
					INC_STEP_OUT(2);
					p_busChannel->channelState = CHANNEL_WIRE_FAULT;
				}
				break;
			case 4:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum) == 0)
				{
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				else if(distime > (MAX_WIRE_ONE_BIT_TIME + MIN_HOLD_HIGH_TIME +  ONE_BIT_HOLD_TIME / 2))
				{
					INC_STEP_OUT(3);
					p_busChannel->runStep = 0xff;
				}
				break;
			case 5:
				if(distime >= (BEGINE_ONE_BIT_TIME + ONE_BIT_HOLD_TIME / 2))
				{
					int bitValue = GET_CHANNEL_IDR(p_busChannel->channelNum);
					p_busChannel->runStep ++;
					
					p_busChannel->dataValueTemp |= (bitValue << p_busChannel->bitIndex);
							
					if(p_busChannel->bitIndex == 7)
					{
						uint8_t *p = (uint8_t *)&p_busChannel->receivePacket;
						p[p_busChannel->buffIndex] = p_busChannel->dataValueTemp;
						p_busChannel->buffIndex++;
						p_busChannel->bitIndex = 0;
						p_busChannel->dataValueTemp = 0;
						if(p_busChannel->buffIndex == 4)
						{
							p_busChannel->allDataNum = p_busChannel->receivePacket.length + 5;
						}
						else if(p_busChannel->allDataNum == p_busChannel->buffIndex)
						{
							p_busChannel->hasReceiveData = 1;
					    p_busChannel->lastRecevieTime = GET_SYS_CLOCK();
						}
					}
					else
					{
					  p_busChannel->bitIndex ++;
					}
				}
				break;
			case 6:
					if(p_busChannel->hasReceiveData)
					{
						if(distime >= (BEGINE_ONE_BIT_TIME + ONE_BIT_HOLD_TIME))
						{
							p_busChannel->beginTime = GET_TIMER_CNT();
							p_busChannel->receivePacketNum ++;
						  p_busChannel->runStep ++;
						}
					}
					else if(GET_CHANNEL_IDR(p_busChannel->channelNum))
					{
						p_busChannel->beginTime = GET_TIMER_CNT();
						p_busChannel->runStep = 4;
					}
					else if(distime >= (BEGINE_ONE_BIT_TIME + ONE_BIT_HOLD_TIME + MIN_HOLD_HIGH_TIME))
					{
						INC_STEP_OUT(4);
						p_busChannel->runStep = 0xff;
					}
				break;
			case 7:
				if(distime >= MIN_HOLD_HIGH_TIME )
				{
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
					SET_CHANNEL_ODR(p_busChannel->channelNum,0);
				}
				break;
			case 8:
				if(distime >= ACK_HOLD_TIME)
				{
					SET_CHANNEL_ODR(p_busChannel->channelNum,1);
					p_busChannel->runStep ++;
				}
				break;
			case 9:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum))
				{
					p_busChannel->channelState = CHANNEL_IDLE;
				}
				else if(distime > MAX_WIRE_REC_HIGH_TIME)
				{
					INC_STEP_OUT(5);
					p_busChannel->channelState = CHANNEL_WIRE_FAULT;
				}
        break;				
			default:
				p_busChannel->channelState = CHANNEL_IDLE;
			  p_busChannel->receiveErrorTimes ++;
			  SET_CHANNEL_ODR(p_busChannel->channelNum,1);
			  return false;
				break;
		}				 
   return true;
}

static bool RunChannelSend(SingleBusChannelStruct *p_busChannel)
{
	 int distime = GET_TIMER_CNT() - p_busChannel->beginTime;
	  switch(p_busChannel->runStep)
		{
			case 0:
			  if(distime >= BEGIN_HOLD_TIME)
				{
					SET_CHANNEL_ODR(p_busChannel->channelNum,1);
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				break;
			case 1:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum))
				{
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				else if(distime > MAX_WIRE_REC_HIGH_TIME)
				{
					p_busChannel->channelState = CHANNEL_WIRE_FAULT;
				}
				break;
			case 2:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum) == 0)
				{
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				else if(distime > WAIT_ACK_TIMEOUT)
				{
					p_busChannel->runStep = 0xff;
				}
				break;
			case 3:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum))
				{
					SET_CHANNEL_PP(p_busChannel->channelNum);
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				else if(distime >= MIN_BEGINE_TIME)
				{
					p_busChannel->runStep = 0;
					p_busChannel->channelState = CHANNEL_RECEIVE;
				}
				break;
			case 4:
				if(distime >= MIN_HOLD_HIGH_TIME)
				{
					SET_CHANNEL_ODR(p_busChannel->channelNum,0);
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
					
					uint8_t *p = (uint8_t *)&p_busChannel->sendPacket;
					p_busChannel->dataValueTemp = (p[p_busChannel->buffIndex] & (1 << p_busChannel->bitIndex))? 1 : 0;
				}
				break;
			case 5:
				if(distime >= BEGINE_ONE_BIT_TIME)
				{
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
          SET_CHANNEL_ODR(p_busChannel->channelNum,p_busChannel->dataValueTemp);
					
					if(p_busChannel->bitIndex == 7)
					{
						p_busChannel->bitIndex = 0;
						p_busChannel->buffIndex ++;
					}
					else
					{
						p_busChannel->bitIndex ++;
					}
				}
				break;
			case 6:
				if(distime >= ONE_BIT_HOLD_TIME)
				{
					SET_CHANNEL_ODR(p_busChannel->channelNum,1);
					p_busChannel->runStep = 4;
          p_busChannel->beginTime = GET_TIMER_CNT();	
					if( p_busChannel->buffIndex == p_busChannel->allDataNum)
					{
						p_busChannel->runStep = 7;
						SET_CHANNEL_OD(p_busChannel->channelNum);
					}
				}
				break;
			case 7:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum))
				{
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				else if(distime >= MAX_WIRE_REC_HIGH_TIME)
				{
					p_busChannel->channelState = CHANNEL_WIRE_FAULT;
				}
				break;
			case 8:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum) == 0)
				{
					p_busChannel->beginTime = GET_TIMER_CNT();
					p_busChannel->runStep ++;
				}
				else if(distime >= WAIT_ACK_TIMEOUT)
				{
					p_busChannel->runStep = 0xff;
				}
				break;
			case 9:
				if(GET_CHANNEL_IDR(p_busChannel->channelNum))
				{
					p_busChannel->hasSendData = 0;
					p_busChannel->sendPacketNum ++;
					p_busChannel->channelState = CHANNEL_IDLE;
					p_busChannel->lastSendTime = GET_SYS_CLOCK();
				}
				else if(distime >= MAX_ACK_HOLD_TIME)
				{
					p_busChannel->runStep = 0xff;
				}
				break;
			default:
				SET_CHANNEL_OD(p_busChannel->channelNum);
			  p_busChannel->channelState = CHANNEL_SEND_ERROR;			  
			  p_busChannel->retrySendTimes --;
			  if(p_busChannel->retrySendTimes == 0)
				{
					p_busChannel->sendErrorTimes ++;
					p_busChannel->hasSendData = 0;
				}
			  break;
		}		
	return true;
}

static bool UpdataChannel(SingleBusChannelStruct *p_busChannel)
{
	bool ret = false;
	switch(p_busChannel->channelState)
	{
		case CHANNEL_IDLE:
			ret = RunChannelIdle(p_busChannel);
			break;
		case CHANNEL_RECEIVE:
			ret = RunChannelReceive(p_busChannel);
			break;
		case CHANNEL_SEND:
			ret = RunChannelSend(p_busChannel);
			break;
		case CHANNEL_WIRE_FAULT:
			SET_CHANNEL_ODR(p_busChannel->channelNum,1);
		  SET_CHANNEL_OD(p_busChannel->channelNum);
			if(GET_CHANNEL_IDR(p_busChannel->channelNum))
				p_busChannel->channelState = CHANNEL_IDLE;
			break;
		default:
			break;
	}		
	return ret;
}


void singleWireBusInit(void)
{
	TIM_HandleTypeDef    TimHandle;
	GPIO_InitTypeDef GPIO_InitStruct;
	__GPIOE_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	
	idreg[0] = PERIPH_TO_BITPERIPH(&GPIOE->IDR,14);
	odreg[0] = PERIPH_TO_BITPERIPH(&GPIOE->ODR,14);
	otypereg[0] = PERIPH_TO_BITPERIPH(&GPIOE->OTYPER,14);
	
	idreg[1] = PERIPH_TO_BITPERIPH(&GPIOE->IDR,13);
	odreg[1] = PERIPH_TO_BITPERIPH(&GPIOE->ODR,13);
	otypereg[1] = PERIPH_TO_BITPERIPH(&GPIOE->OTYPER,13);
	
	idreg[2] = PERIPH_TO_BITPERIPH(&GPIOE->IDR,11);
	odreg[2] = PERIPH_TO_BITPERIPH(&GPIOE->ODR,11);
	otypereg[2] = PERIPH_TO_BITPERIPH(&GPIOE->OTYPER,11);
	
	idreg[3] = PERIPH_TO_BITPERIPH(&GPIOE->IDR,9);
	odreg[3] = PERIPH_TO_BITPERIPH(&GPIOE->ODR,9);
	otypereg[3] = PERIPH_TO_BITPERIPH(&GPIOE->OTYPER,9);
	
	idreg[4] = PERIPH_TO_BITPERIPH(&GPIOD->IDR,13);
	odreg[4] = PERIPH_TO_BITPERIPH(&GPIOD->ODR,13);
	otypereg[4] = PERIPH_TO_BITPERIPH(&GPIOD->OTYPER,13);
	
	idreg[5] = PERIPH_TO_BITPERIPH(&GPIOD->IDR,14);
	odreg[5] = PERIPH_TO_BITPERIPH(&GPIOD->ODR,14);
	otypereg[5] = PERIPH_TO_BITPERIPH(&GPIOD->OTYPER,14);
	
	for(int i = 0 ; i < 6 ; i ++)
	{
		*odreg[i] = 1;
	}
	
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_14;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
	
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
		
	__TIM2_CLK_ENABLE();
	
	TimHandle.Instance = TIM2;
  TimHandle.Init.Period = 0xffff;
  TimHandle.Init.Prescaler = 84 - 1;
  TimHandle.Init.ClockDivision = 0;
  TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
  HAL_TIM_Base_Init(&TimHandle);
	
	for(int i = 0 ; i < 6 ; i ++)
	{
		busChannel[i].lastRecevieTime = 0;
	  busChannel[i].lastSendTime = 0;
	  busChannel[i].receiveErrorTimes = 0;
	  busChannel[i].sendErrorTimes = 0;
		
	  busChannel[i].hasReceiveData = 0;
	  busChannel[i].hasSendData = 0;
	  busChannel[i].retrySendTimes = 0;
	  busChannel[i].channelNum = i;
	
	  busChannel[i].channelState = CHANNEL_IDLE;
		
		busChannel[i].receivePacketNum = 0;
	  busChannel[i].sendPacketNum = 0 ;
	}
	
#ifdef  SINGLE_BUS_TEST
  for(int i = 0 ; i < 9 ; i ++)
    stepOut[i] = 0;

#endif

}
#ifdef  SINGLE_BUS_TEST
int maxruntime = 0;
int nowtime;
#endif

void singleWireBusUpdate(void)
{
	bool loopflag;
	
	for(int i = 0 ; i < 6 ; i ++)
	{
		if(busChannel[i].channelState == CHANNEL_SEND_ERROR)
			busChannel[i].channelState = CHANNEL_IDLE;
	}
	
	CLEAR_TIMER();
	START_TIMER();
	do
	{
#ifdef  SINGLE_BUS_TEST
		nowtime = GET_TIMER_CNT();
#endif
		loopflag = false;
	  for(int i = 0 ; i < 6 ; i ++)
	  {
			loopflag |= UpdataChannel(busChannel + i);
	  }
#ifdef  SINGLE_BUS_TEST
		int distime = GET_TIMER_CNT() - nowtime;
		maxruntime = maxruntime > distime?maxruntime:distime;
#endif
  }while(loopflag);
	STOP_TIMER();
	
}

int  getSingleBusData(SingleBusStruct *p_data)
{
	for(int i = 0 ; i < 6 ; i ++)
	{
		if(busChannel[i].hasReceiveData)
		{
			memcpy(p_data,&busChannel[i].receivePacket,busChannel[i].receivePacket.length + 5);
			busChannel[i].hasReceiveData = 0;
			return i;
		}
	}
	return -1;
}

int  sendSingleBusData(int channel,SingleBusStruct *p_data,int retry)
{
	if(busChannel[channel].hasSendData)
		return -1;
	memcpy(&busChannel[channel].sendPacket,p_data,p_data->length + 5);
	busChannel[channel].hasSendData = 1;
	busChannel[channel].retrySendTimes = retry;
	return channel;
}

int  getSendFaileTimes(int channel)
{
	return busChannel[channel].sendErrorTimes;
}

uint32_t getLastReceiveTime(int channel)
{
	return busChannel[channel].lastRecevieTime;
}

uint32_t getLastActiveTime(int channel)
{
	return busChannel[channel].lastRecevieTime > busChannel[channel].lastSendTime ? busChannel[channel].lastRecevieTime : busChannel[channel].lastSendTime;
}

uint32_t getLastSendTime(int channel)
{
	return busChannel[channel].lastSendTime;
}

