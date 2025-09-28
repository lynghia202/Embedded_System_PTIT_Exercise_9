#include "stm32f10x.h"
#include <stdio.h>

uint16_t ADC_Raw; 

static void USART1_Init(void){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    GPIO_InitTypeDef gpio;
	
    // PA9  = TX (AF push-pull)
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    // PA10 = RX (floating input)
    gpio.GPIO_Pin   = GPIO_Pin_10;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_InitTypeDef us;
    us.USART_BaudRate            = 9600;
    us.USART_WordLength          = USART_WordLength_8b;
    us.USART_StopBits            = USART_StopBits_1;
    us.USART_Parity              = USART_Parity_No;
    us.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    us.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &us);
    USART_Cmd(USART1, ENABLE);
}

int fputc(int ch, FILE *f){
		(void)f;
    while((USART1->SR & USART_SR_TXE)==0);
    USART1->DR = (uint16_t)ch;
    return ch;
}

static void ADC1_Init_CH0(void){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    // PA0 analog input
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin  = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio);
	
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);//Cau hinh Clock cho ADC: Chia tan so Clock APB2 (PCLK2, thuong là 72MHz) cho 6.
									// f_ADC: 72MHz / 6 = 12 MHz (Dam bao duoi gioi han 14MHz).
    ADC_InitTypeDef adc;
    adc.ADC_Mode               = ADC_Mode_Independent; // Che do hoat dong: ADC1 hoat dong doc lap (Khong ghep doi voi ADC khac).
    adc.ADC_ScanConvMode       = DISABLE; // Che do Quet: Tat. Chi doc 1 kenh duy nhat (PA0).
    adc.ADC_ContinuousConvMode = ENABLE;  // Che do Chuyen doi Lien tuc: Bat. ADC tu dong bat dau chuyen doi moi ngay sau khi xong.        
    adc.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None; // Kich hoat ben ngoai: KHONG su dung su kien Timer hay chan ngoai de bat dau chuyen doi.
    adc.ADC_DataAlign          = ADC_DataAlign_Right; // Canh chinh Du lieu: Gia tri RAW 12-bit duoc luu o ben phai (LSB) cua thanh ghi 16-bit.
    adc.ADC_NbrOfChannel       = 1; // So luong Kenh: Dat la 1 vi ta chi doc kenh 0 (PA0).

    ADC_Init(ADC1, &adc); // Khoi tao ADC1 voi cac tham so da cau hinh.

    ADC_Cmd(ADC1, ENABLE);
		ADC_DMACmd(ADC1, ENABLE); //Enable DMA Request

    // Hieu chinh
    ADC_ResetCalibration(ADC1); // Reset lai gia tri hieu chinh cu.
    while(ADC_GetResetCalibrationStatus(ADC1)); // Cho doi qua trinh Reset hieu chinh hoan tat.
    ADC_StartCalibration(ADC1); // Bat dau qua trinh hieu chinh tu dong.
    while(ADC_GetCalibrationStatus(ADC1)); // Cho doi qua trinh hieu chinh moi hoan tat (dam bao do chinh xac).
		
		ADC_SoftwareStartConvCmd(ADC1, ENABLE); 
}

static void DMA_Config(void){
    // Bat clock cho DMA1
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_InitTypeDef dma;
    
    // 1. Đia chi nguon (Peripheral Base Address)
    dma.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR; // Thanh ghi DR (Data Register) cua ADC1
    
    // 2. Đia chi đich (Memory Base Address)
    dma.DMA_MemoryBaseAddr     = (uint32_t)&ADC_Raw;  // Bien toàn cuc raw
    
    // 3. Huong truyen: Peripheral -> Memory
    dma.DMA_DIR                = DMA_DIR_PeripheralSRC; 
    
    // 4. Kich thuoc bo dem
    dma.DMA_BufferSize         = 1; 
    
    // 5. Chuyen doi du lieu 16-bit (uint16_t)
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; 
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;       
    
    // 6. Tang dia chi bo nho (Memory Increment): KHÔNG, vi chi có 1 bien
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable; 
    dma.DMA_MemoryInc          = DMA_MemoryInc_Disable;       
    
    // 7. Che do DMA: Vong lap (de lien tuc cap nhat bien)
    dma.DMA_Mode               = DMA_Mode_Circular; 
    
    // 8. Uu tien
    dma.DMA_Priority           = DMA_Priority_High; 
    
    // 9. Bat ngat khi truyen hoan tat (Transfer Complete Interrupt)
    dma.DMA_M2M                = DMA_M2M_Disable;
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);

    DMA_Init(DMA1_Channel1, &dma);
    
    // Bat DMA Channel 1
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

void NVIC_Config(void) {
    NVIC_InitTypeDef nvic;  
    // Cau hinh NVIC cho DMA1 Channel 1 Interrupt
    nvic.NVIC_IRQChannel                   = DMA1_Channel1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

void DMA1_Channel1_Event(uint32_t event){
    // Kiem tra xem ngat TC (Transfer Complete) co xay ra không
    if(DMA_GetITStatus(DMA1_IT_TC1) == SET){ //Neu co xay ra ngat      			
        uint16_t raw = ADC_Raw; // Gia tri RAW da dc DMA cap nhat tu dong vao bien raw
        float voltage = (3.3f * raw) / 4095.0f; 
        float percent = ((4095.0f - raw) * 100.0f) / 4095.0f; 
        printf("RAW=%4u  V=%.3f V  Light=%.1f %%\r\n", raw, voltage, percent);
        DMA_ClearITPendingBit(DMA1_IT_TC1); //Xoa co ngat de doi ngat tiep theo
    }
}
int main(void){
    USART1_Init();
    ADC1_Init_CH0();
		NVIC_Config();         
    DMA_Config();       
    printf("\r\n--- DMA ADC demo (PA0, Vref=3.3V) ---\r\n");
    while(1){
    }
}
