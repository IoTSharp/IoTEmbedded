#include "main.h"
#include "stm32l4xx_it.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;

void NMI_Handler(void) {
  while (1) {
  }
}

void HardFault_Handler(void) {
  while (1) {
  }
}

void MemManage_Handler(void) {
  while (1) {
  }
}

void BusFault_Handler(void) {
  while (1) {
  }
}

void UsageFault_Handler(void) {
  while (1) {
  }
}

void DebugMon_Handler(void) {
}

void SysTick_Handler(void) {
  HAL_IncTick();
}

void USART1_IRQHandler(void) {
  HAL_UART_IRQHandler(&huart1);
}

void USART2_IRQHandler(void) {
  HAL_UART_IRQHandler(&huart2);
}

void UART4_IRQHandler(void) {
  HAL_UART_IRQHandler(&huart4);
}

void UART5_IRQHandler(void) {
  HAL_UART_IRQHandler(&huart5);
}
