/*! @mainpage Medidor de distancia con interrupciones y timer
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
 * | 29/04/2026 | Document creation		                         |
 *
 * @author Zahira Garces Antar (zahira.garces@ingenieria.uner.edu.ar)
 *
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
 *
 * true  -> sistema activo
 * false -> sistema detenido
 */
bool encendido = true;
/**
 * @brief Estado del modo HOLD.
 *
 * true  -> congela el display
 * false -> actualiza normalmente
 */
bool hold = false;
/**
 * @brief Distancia medida por el sensor.
 *
 * Unidad: centímetros.
 */
uint16_t distancia = 0;
/**
 * @brief Handle de la tarea de medición.
 */
TaskHandle_t medirDistancia_task_handle = NULL;
/*==================[internal functions declaration]=========================*/
/**
 * @brief Tarea de medición de distancia.
 *
 * Espera una notificación del timer para:
 * - Leer el sensor ultrasónico.
 * - Actualizar LEDs.
 * - Mostrar la distancia en LCD.
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
		}
		if (hold == false)
		{
			LcdItsE0803Write(distancia);
		}
	}
}
/**
 * @brief Callback de interrupción para SWITCH_1.
 *
 * Cambia el estado de encendido del sistema.
 *
 * @param args Parámetro no utilizado.
 */
static void apretarTecla1(void *args)
{

	encendido = !encendido;
}
/**
 * @brief Callback de interrupción para SWITCH_2.
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
 * @brief Función de atención del timer.
 *
 * Envía una notificación a la tarea de medición.
 *
 * @param args Parámetro no utilizado.
 */
void atenderTimer(void *args)
{
	vTaskNotifyGiveFromISR(medirDistancia_task_handle, pdFALSE);
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
	// Inicialización de periféricos
	HcSr04Init(GPIO_3, GPIO_2);
	LedsInit();
	LcdItsE0803Init();

	/**
	 * @brief Configuración del timer periódico.
	 *
	 * TIMER_A ejecuta la función atenderTimer()
	 * cada CONFIG_BLINK_PERIOD_LED microsegundos.
	 */
	timer_config_t timer_led = {
		.timer = TIMER_A,
		.period = CONFIG_BLINK_PERIOD_LED,
		.func_p = atenderTimer,
		.param_p = NULL};

	// Inicializa el timer configurado
	TimerInit(&timer_led);

	/**
	 * @brief Inicialización de switches e interrupciones.
	 *
	 * SWITCH_1 ejecuta apretarTecla1().
	 * SWITCH_2 ejecuta apretarTecla2().
	 */
	SwitchesInit();
	SwitchActivInt(SWITCH_1, apretarTecla1, NULL);
	SwitchActivInt(SWITCH_2, apretarTecla2, NULL);

	// Inicia el timer periódico
	TimerStart(TIMER_A);
	/**
	 * @brief Creación de la tarea de medición.
	 *
	 * La tarea mide distancia y actualiza
	 * LEDs y display LCD.
	 */
	xTaskCreate(&medirDistancia, "Tarea 1", 512, NULL, 5, &medirDistancia_task_handle);
}
/*==================[end of file]============================================*/