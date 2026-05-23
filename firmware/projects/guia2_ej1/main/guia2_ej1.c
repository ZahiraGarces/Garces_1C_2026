/*! @mainpage Template
 *
 * @section Medidor de distancia por ultrasonido
 *
 * This section describes how the program works.
 *
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
 * | 15/04/2026 | Document creation		                         |
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

/*==================[macros and definitions]=================================*/
/**
 * @brief Período de actualización de la tarea de medición.
 *
 * Expresado en milisegundos.
 */
#define CONFIG_BLINK_PERIOD_LED 1000
/**
 * @brief Período de lectura de teclas.
 *
 * Expresado en milisegundos.
 */
#define CONFIG_BLINK_PERIOD_TECLAS 10
/*==================[internal data definition]===============================*/
/**
 * @brief Variable de control de encendido del sistema.
 *
 * false  -> sistema activo  
 * true   -> sistema detenido
 */
bool encendido = true;
/**
 * @brief Variable de control del modo HOLD.
 *
 * Cuando vale true, la distancia medida se muestra en el LCD.
 */
bool hold = false;
/**
 * @brief Distancia medida por el sensor ultrasónico.
 *
 * Unidad: centímetros.
 */
uint16_t distancia = 0;

/**
 * @brief Handle de la tarea de medición.
 */
TaskHandle_t medirDistancia_task_handle = NULL;
/**
 * @brief Handle de la tarea de lectura de teclas.
 */
TaskHandle_t apretarTeclas_task_handle = NULL;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Tarea encargada de medir distancia y controlar LEDs/LCD.
 *
 * Inicializa:
 * - Sensor ultrasónico HC-SR04
 * - LEDs
 * - Display LCD
 *
 * Luego ejecuta continuamente:
 * - Lectura de distancia
 * - Actualización de LEDs según rango
 * - Escritura en LCD si el modo HOLD está activo
 *
 * @param pvParameter Parámetro de FreeRTOS (no utilizado).
 */
static void medirDistancia(void *pvParameter)
{
	HcSr04Init(GPIO_3, GPIO_2); //Inicializo sensor
	LedsInit(); //Inicializo LEDs
	LcdItsE0803Init(); //Inicializo LCD

	while (1)
	{   
		if (encendido==false)
		{
		/*Lectura de distancia*/
		distancia = HcSr04ReadDistanceInCentimeters();

        /*Control de LEDs segun distancia*/
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

		if (distancia >=30)
		{
			LedOn(LED_1);
			LedOn(LED_2);
			LedOn(LED_3);
		}
    }
	/*Modo HOLD: mostrar distancia en LCD*/
 	if(hold==true)
		{
			LcdItsE0803Write(distancia);
		}
		vTaskDelay(CONFIG_BLINK_PERIOD_LED/portTICK_PERIOD_MS);
    }

}
/**
 * @brief Tarea encargada de leer las teclas.
 *
 * Funciones:
 * - SWITCH_1: cambia el estado de encendido del sistema.
 * - SWITCH_2: activa/desactiva el modo HOLD.
 *
 * @param pvParameter Parámetro de FreeRTOS (no utilizado).
 */
static void apretarTeclas(void *pvParameter)
{
    uint8_t teclas;
	SwitchesInit();

    while(1)    
	{
		/* Lectura de teclas */
    	teclas  = SwitchesRead();
    	switch(teclas)
		{
    		case SWITCH_1:
    			encendido=!encendido;
    		break;
    		case SWITCH_2:
    			hold=!hold;
    		break;
    	}
	    
		vTaskDelay(CONFIG_BLINK_PERIOD_TECLAS/portTICK_PERIOD_MS);
	}
}

/*==================[external functions definition]==========================*/
void app_main(void)
{
/**
 * @brief Crea la tarea de medición de distancia.
 *
 * La tarea mide la distancia con el sensor ultrasónico
 * y actualiza LEDs y display LCD.
 */
xTaskCreate(&medirDistancia, "Tarea 1", 512, NULL, 5, &medirDistancia_task_handle);
/**
 * @brief Crea la tarea de lectura de teclas.
 *
 * La tarea detecta pulsaciones de switches
 * para controlar el encendido y el modo HOLD.
 */
xTaskCreate(&apretarTeclas, "Tarea 2", 512, NULL, 5, &apretarTeclas_task_handle);
}
/*==================[end of file]============================================*/