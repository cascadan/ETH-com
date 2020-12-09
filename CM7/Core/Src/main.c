/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <lwip/udp.h>
#include <lwip/debug.h>
#include "lwip/tcp.h"
#include <string.h>
#include <tcp_echoserver.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define HSEM_ID_0 (0U) /* HW semaphore 0*/
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

/* USER CODE BEGIN PV */
osThreadId_t udp_timer_message_task_handle;
const osThreadAttr_t udp_timer_message_task_attributes = {
  .name = "UDP timer",
  .priority = (osPriority_t) osPriorityNormal1,
  .stack_size = 256 * 4
};

osThreadId_t udp_echo_task_handle;
const osThreadAttr_t udp_echo_task_attributes = {
  .name = "UDP echo",
  .priority = (osPriority_t) osPriorityNormal1,
  .stack_size = 256 * 4
};

osThreadId_t telnet_server_task_handle;
const osThreadAttr_t telnet_server_task_attributes = {
  .name = "telnet server",
  .priority = (osPriority_t) osPriorityNormal1,
  .stack_size = 256 * 4
};

osThreadId_t telnet_transmitter_task_handle;
const osThreadAttr_t telnet_transmitter_task_attributes = {
  .name = "TCP transmitter",
  .priority = (osPriority_t) osPriorityNormal1,
  .stack_size = 256 * 8
};

osThreadId_t serial_to_tcp_task_handle;
const osThreadAttr_t serial_to_tcp_task_attributes = {
  .name = "TCP send pkt",
  .priority = (osPriority_t) osPriorityNormal1,
  .stack_size = 256 * 8
};

osThreadId_t led_serial_tx_task_handle;
const osThreadAttr_t led_serial_tx_task_attributes = {
  .name = "GreenLED",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

osThreadId_t led_serial_rx_task_handle;
const osThreadAttr_t led_serial_rx_task_attributes = {
  .name = "OrangeLED",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

#define TELNET_ACCEPTED 1
#define TELNET_RECEIVED 2
#define TELNET_CLOSING  3
int telnet_status = 0;

// Task Functions
void udp_timer_message_task(void *argument);
void udp_echo_task(void *argument);
void telnet_server_task(void *argument);
void telnet_transmitter_task(void *argument);
void serial_to_tcp_task(void *argument);
void led_serial_tx_task(void *argument);
void led_serial_rx_task(void *argument);
  
static SemaphoreHandle_t udp_echo_semphr = NULL;
// flags for LED behaivior
static SemaphoreHandle_t led_tx_semphr = NULL;
static SemaphoreHandle_t led_rx_semphr = NULL;
// semaphore to write the TCP data to UART
static SemaphoreHandle_t serial_send_release_semphr = NULL;
// semaphore to write UART data to TCP stack_size
static SemaphoreHandle_t tcp_send_release_semphr = NULL;

static SemaphoreHandle_t received_char_semphr = NULL;



// UDP callbacks

// TCP callbacks
static err_t telnet_accept_callback (void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t telnet_receiver_callback (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void telnet_rx_err_callback (void *arg, err_t err);
static err_t telnet_sent_callback (void *arg, struct tcp_pcb *tpcb, uint16_t len);

// telnet operation functions
void tcp_pbuf_to_serial (struct pbuf* p);
static void tcp_send_pkt (struct tcp_pcb *tpcb, struct pbuf *p);

// global variables to recover TCP data from ISR
char* tcp_data;
uint16_t tcp_data_size = 0;
struct pbuf *host_p;
uint8_t tcp_retries = 0;
  
char serial_to_tcp_buff[1024] = {0};
int serial_to_tcp_buff_count = 0;
int telnet_tx_complete = 0;

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  
  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_0 */
  int32_t timeout;
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
  /* USER CODE BEGIN Boot_Mode_Sequence_2 */
  /* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
  HSEM notification */
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /*Take HSEM */
  HAL_HSEM_FastTake(HSEM_ID_0);
  /*Release HSEM in order to notify the CPU2(CM4)*/
  HAL_HSEM_Release(HSEM_ID_0,0);
  /* wait until CPU2 wakes up from stop mode */
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
  /* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  
  /* USER CODE BEGIN 2 */
  
  // PB0 - GREEN LED / PB14 - RED LED
  GPIO_InitTypeDef GPIO_InitStruct;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14 | GPIO_PIN_0, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // PE1 - ORANGE LED
  __HAL_RCC_GPIOE_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  udp_echo_semphr = xSemaphoreCreateBinary();
  led_tx_semphr = xSemaphoreCreateBinary();
  led_rx_semphr = xSemaphoreCreateBinary();
  serial_send_release_semphr = xSemaphoreCreateBinary();
  tcp_send_release_semphr = xSemaphoreCreateBinary();
  received_char_semphr = xSemaphoreCreateBinary();
  
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  udp_timer_message_task_handle = osThreadNew(udp_timer_message_task, NULL, &udp_timer_message_task_attributes);
  udp_echo_task_handle = osThreadNew(udp_echo_task, NULL, &udp_echo_task_attributes);
  telnet_server_task_handle = osThreadNew(telnet_server_task, NULL, &telnet_server_task_attributes);
  //telnet_transmitter_task_handle = osThreadNew(telnet_transmitter_task, NULL, &telnet_transmitter_task_attributes);
  //serial_to_tcp_task_handle = osThreadNew(serial_to_tcp_task, NULL, &serial_to_tcp_task_attributes);
  led_serial_tx_task_handle = osThreadNew(led_serial_tx_task, NULL, &led_serial_tx_task_attributes);
  led_serial_rx_task_handle = osThreadNew(led_serial_rx_task, NULL, &led_serial_rx_task_attributes);

  // udp_reciever_task_handle = osThreadNew(udp_reciever_task, NULL, &udp_reciever_task_attributes);
  // tcp_task_handle = osThreadNew(tcp_task, NULL, &tcp_task_attributes);
  // send_char_task_handle = osThreadNew(send_char_task, NULL, &send_char_task_attributes);
  
  // Start LwIP
  MX_LWIP_Init();
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Macro to configure the PLL clock source
  */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3|RCC_PERIPHCLK_USB;
  PeriphClkInitStruct.PLL3.PLL3M = 1;
  PeriphClkInitStruct.PLL3.PLL3N = 24;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 4;
  PeriphClkInitStruct.PLL3.PLL3R = 2;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_3;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Enable USB Voltage detector
  */
  HAL_PWREx_EnableUSBVoltageDetector();
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 9;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.battery_charging_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

}

/* USER CODE BEGIN 4 */
/**
 * @brief Sends UDP messages with an aproximate interval of 1s (the value of the timer is sent in the data)
 * @param none
 * @retval none
 */
void udp_timer_message_task(void *argument)
{
  const char* begin_message = "Time of operation "; // hh:mm:ss
  const char* end_message = " - UDP message sent from NUCLEO Board\n\r";
  const char* separation = ":";
  static int hour = 0;
  char h_char[10] = {0};
  static int minute = 0;
  char m_char[4] = {0};
  static int second = 0;
  char s_char[4] = {0};
  char *message;
  message = pvPortMalloc( (strlen(begin_message)) + (strlen(end_message)) +20);
  
  ip_addr_t PC_IPADDR;
  // Set IP addr for the target
  IP_ADDR4(&PC_IPADDR, 192, 168, 1, 106);
  // Create new UDP connection
  struct udp_pcb* my_udp = udp_new();
  udp_connect(my_udp, &PC_IPADDR, 55151); // Messages transmitted at PORT 55151
  struct pbuf* udp_buffer = NULL;
  
  for (;;)
  {
    if (second < 59) {second++;}
    else {second = 0; minute++;}
	
    if (minute == 60) {minute = 0; hour++;}

    sprintf(h_char, "%02d", hour);
    sprintf(m_char, "%02d", minute);
    sprintf(s_char, "%02d", second);
    // Assemble message to be transmitted
    strcat(message, begin_message);
    strcat(message, h_char);
    strcat(message, separation);
    strcat(message, m_char);
    strcat(message, separation);
    strcat(message, s_char);
    strcat(message, end_message);    
    
    udp_buffer = pbuf_alloc(PBUF_TRANSPORT, strlen(message), PBUF_RAM);
    if (udp_buffer != NULL)
    {
      memcpy(udp_buffer->payload, message, strlen(message));
      udp_send(my_udp, udp_buffer);
      pbuf_free(udp_buffer);

    }
    osDelay(999); // 0.999 seconds delay to compensate time to send the message
    *message = '\0'; // clear the array for the next text addition
  }
}

/**
 * @brief UDP callback function for Receiver Mode
 * 
 */
static void udpecho_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, uint16_t port)
{
	static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if (p != NULL)
	{
		// Echo msg
		udp_sendto(pcb, p, addr, port);
		// Free Data Pointer
		pbuf_free(p);

		xSemaphoreGiveFromISR(udp_echo_semphr, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}

/**
 * @brief Create UDP Binding on any IP in Port 7777. Echoes all the messages sent in that port.
 * @param none
 * @retval none
 * 
 */
void udp_echo_task(void *argument)
{
  struct udp_pcb * pcb;
  
  pcb = udp_new();
  if (pcb==NULL)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
    while(1);
  }
  
  err_t bind_ret_val = udp_bind(pcb, IP_ADDR_ANY, 7777);
  if (bind_ret_val != ERR_OK)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
    while(1);
  }
  
  // Set UDP receiver with a callback function
  udp_recv(pcb, udpecho_raw_recv, pcb);
  
  for(;;)
  {
    xSemaphoreTake (udp_echo_semphr, portMAX_DELAY);
    // Return form the ISR.
    osDelay(1);
  }
}

/**
 * @brief This task works as a telnet server capturing the characters from the TCP connection and
 *        sending the data to a serial port (UART) 
 * @note TCP connection is binding on default telnet port (PORT 23)
 */
void telnet_server_task (void *argument)
{
  // char skip_line = 0x0A;
  char carriage_return = 0x0D;

  // Start TCP server
  tcp_echoserver_init(); // echoserver has been modified to send pkt data through serial port (UART)
  
  for(;;)
  {
    // block until tcp_pbuf_to_serial() function releases the semaphore (TCP pkt received)
    xSemaphoreTake ( serial_send_release_semphr, portMAX_DELAY );
		    
    // send TCP data to UART
    if (tcp_data_size > 0)
	{
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // RED LED off
	  // transmmit received data to the serial port
	  for (int k = 0; k < tcp_data_size; k++){
	    HAL_UART_Transmit(&huart3, &tcp_data[k], 1, 1000);}

	  HAL_UART_Transmit(&huart3, &carriage_return, 1, 1000);
	  vPortFree(tcp_data);
	  tcp_data_size = 0;
	}

    tcp_echoserver_init();
  }
}

/**
 * @brief Capture the data from the TCP pkg and store in a global variable. Releases the semaphore to complete the transmission on the serial port.
 * @note this function is called at tcp_echoserver_recv() from the TCP echoserver driver
 */
void tcp_pbuf_to_serial (struct pbuf* p)
{
	static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	char* buff_ptr;

	tcp_data_size = p->len;
	if (tcp_data_size > 0)
	{
		tcp_data = pvPortMalloc(tcp_data_size);
		buff_ptr = (char*)p->payload;
	}

	for (int i = 0; i<tcp_data_size; i++){
		tcp_data[i] = buff_ptr[i];}

	xSemaphoreGiveFromISR(serial_send_release_semphr, &xHigherPriorityTaskWoken);
	//xSemaphoreGiveFromISR(led_tx_semphr, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void telnet_transmitter_task(void *argument)
{
  char single_character;
  static int end_of_msg = 0;
  
  for(;;)
  {
    // capture characters until receive a NULL char or a Carriage Return char
    while (end_of_msg == 0)
    {
      if (HAL_UART_Receive_IT(&huart3, &single_character, 1) != HAL_OK)
    	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET); // RED LED on

      xSemaphoreTake (received_char_semphr, portMAX_DELAY);
      serial_to_tcp_buff[serial_to_tcp_buff_count] = single_character;
      serial_to_tcp_buff_count++;
      
      if ((single_character==0x00) || (single_character==0x0d)) 
      {
        end_of_msg = 1;
      }
    }
    // TODO implement queue system
    xSemaphoreGive(tcp_send_release_semphr);
    end_of_msg = 0;
  }
}

void serial_to_tcp_task(void *argument)
{
  osDelay(100);
  for(;;)
  {
    xSemaphoreTake (tcp_send_release_semphr, portMAX_DELAY);
    telnet_send (serial_to_tcp_buff, serial_to_tcp_buff_count);
  }
}

/**
 * @brief set interruption handler for USART3
 * 
 */
void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart3);
}

/**
 * @brief set callback function for the interrupt handler of USART3
 * 
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
	static BaseType_t xHigherPriorityTaskWoken;
	xHigherPriorityTaskWoken = pdFALSE;

	//keyboard_char_rec = 1;

	xSemaphoreGiveFromISR(received_char_semphr, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *UartHandle)
{
	//keyboard_char_rec = 2;
	//Error_Handler();
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET); // RED LED on
}

/**
 * @brief Task to manage the Transmitter LED behaivior
 * @note Green LED is set for Transmission
 */
void led_serial_tx_task(void *argument)
{
  for(;;)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    xSemaphoreTake (led_tx_semphr, portMAX_DELAY );

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    osDelay(80);
  }
}

/**
 * @brief Task to manage the Receiver LED behaivior
 * @note Orange LED is set for Reception
 */
void led_serial_rx_task(void *argument)
{
  for(;;)
  {
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
    xSemaphoreTake (led_rx_semphr, portMAX_DELAY );
    
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
    osDelay(80);
  }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for LWIP */
  /* USER CODE BEGIN 5 */
  while(1){
  osDelay(1000);}
  /* USER CODE END 5 */
}

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();
  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x30040000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x30040000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256B;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}
/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
  for(;;);
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
