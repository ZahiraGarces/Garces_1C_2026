/*! @mainpage Medidor de distancia con UART, interrupciones y timer
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 06/05/2026 | Document creation		                         |
 *
 * @author Zahira Garces Antar (zahira.garces@ingenieria.uner.edu.ar)
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"
#include "hc_sr04.h"
#include "lcditse0803.h"
#include "led.h"
#include "switch.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
/*==================[macros and definitions]=================================*/
/**
 * @brief Período del timer de medición.
 *
 * Unidad: microsegundos.
 */
#define CONFIG_BLINK_PERIOD_LED 1000000
/*==================[internal data definition]===============================*/
/**
 * @brief Estado del sistema.
 */
bool encendido = true;
/**
 * @brief Estado del modo HOLD.
 */
bool hold = false;
/**
 * @brief Distancia medida en centímetros.
 */
uint16_t distancia = 0;
/**
 * @brief Handle de la tarea principal.
 */
TaskHandle_t medirDistancia_task_handle = NULL;
/*==================[internal functions declaration]=========================*/
/**
 * @brief Tarea principal de medición.
 *
 * Espera notificaciones del timer para:
 * - Leer el sensor ultrasónico.
 * - Actualizar LEDs.
 * - Mostrar distancia en LCD.
 * - Enviar datos por UART.
 *
 * @param pvParameter Parámetro no utilizado.
 */
static void medirDistancia(void *pvParameter)
{

	while (1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		if (encendido == true)
		{
			distancia = HcSr04ReadDistanceInCentimeters();

			if (distancia < 10)
			{
				LedsOffAll();
			}

			if ((distancia >= 10) && (distancia < 20))
			{
				LedOn(LED_1);
				LedOff(LED_2);
				LedOff(LED_3);
			}

			if ((distancia >= 20) && (distancia < 30))
			{
				LedOn(LED_1);
				LedOn(LED_2);
				LedOff(LED_3);
			}

			if (distancia >= 30)
			{
				LedOn(LED_1);
				LedOn(LED_2);
				LedOn(LED_3);
			}

			UartSendString(UART_PC, (char *)UartItoa(distancia, 10));
			UartSendString(UART_PC, " cm\r\n");
		}
		if (hold == false)
		{
			LcdItsE0803Write(distancia);
		}
	}
}
/**
 * @brief Interrupción asociada a SWITCH_1.
 *
 * Cambia el estado de encendido.
 *
 * @param args Parámetro no utilizado.
 */
static void apretarTecla1(void *args)
{

	encendido = !encendido;
}
/**
 * @brief Interrupción asociada a SWITCH_2.
 *
 * Activa/desactiva el modo HOLD.
 *
 * @param args Parámetro no utilizado.
 */
static void apretarTecla2(void *args)
{
	hold = !hold;
}
/**
 * @brief Callback del timer.
 *
 * Notifica a la tarea de medición.
 *
 * @param args Parámetro no utilizado.
 */
void atenderTimer(void *args)
{
	vTaskNotifyGiveFromISR(medirDistancia_task_handle, pdFALSE);
}
/**
 * @brief Callback de recepción UART.
 *
 * Procesa caracteres recibidos:
 * - 'O': cambia estado de encendido.
 * - 'H': cambia estado HOLD.
 *
 * @param args Parámetro no utilizado.
 */
void recibirCaracter(void *args)
{
	uint8_t c;
	// Lee un byte recibido por UART
	UartReadByte(UART_PC, &c);

	switch (c)
	{
	// Cambia el estado de encendido del sistema
	case 'O':
		encendido = !encendido; // misma lógica que SWITCH_1
		break;
	// Cambia el estado del modo HOLD	
	case 'H':
		hold = !hold; // misma lógica que SWITCH_2
		break;
	}
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
	/**
	 * @brief Inicialización de periféricos.
	 */
	HcSr04Init(GPIO_3, GPIO_2);
	LedsInit();
	LcdItsE0803Init();

	/**
	 * @brief Configuración del timer periódico.
	 */
	timer_config_t timer_led = {
		.timer = TIMER_A,
		.period = CONFIG_BLINK_PERIOD_LED,
		.func_p = atenderTimer,
		.param_p = NULL};

	TimerInit(&timer_led);

	/**
	 * @brief Configuración de UART.
	 *
	 * UART_PC funciona a 115200 baudios.
	 */
	serial_config_t uart_pc = {
		.port = UART_PC,
		.baud_rate = 115200,
		.func_p = recibirCaracter, // callback al recibir un byte
		.param_p = NULL};

	UartInit(&uart_pc);

	/**
	 * @brief Inicialización de switches e interrupciones.
	 *
	 * Se configuran los pulsadores del sistema y
	 * se asocian funciones de interrupción a cada uno:
	 *
	 * - SWITCH_1:
	 *      Ejecuta apretarTecla1() al presionarse.
	 *
	 * - SWITCH_2:
	 *      Ejecuta apretarTecla2() al presionarse.
	 */
	SwitchesInit();
	SwitchActivInt(SWITCH_1, apretarTecla1, NULL);
	SwitchActivInt(SWITCH_2, apretarTecla2, NULL);

	// Inicia el timer periódico
	TimerStart(TIMER_A);

	/**
	 * @brief Creación de la tarea principal.
	 */
	xTaskCreate(&medirDistancia, "Tarea 1", 512, NULL, 5, &medirDistancia_task_handle);
}
/*==================[end of file]============================================*/