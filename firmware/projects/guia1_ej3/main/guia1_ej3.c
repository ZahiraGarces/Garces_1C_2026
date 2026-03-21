/*! @mainpage Template
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
 * | 18/03/2026 | Document creation		                         |
 *
 * @author Zahira Garces Antar (zahira.garces@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "led.h"
#include "switch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*==================[macros and definitions]=================================*/
	enum{ON, OFF,TOGGLE}; //constantes con nombres
/*==================[internal data definition]===============================*/
	struct leds
	{
		uint8_t mode; //ON,OFF,TOGGLE
		uint8_t n_led; //nº de led a controlar
		uint8_t n_ciclos; //cantidad de ciclos de encendido/apagado
		uint16_t periodo; // tiempo de cada ciclo

	} my_leds;
/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/
	void controled(struct leds *leds)
	{
		switch (leds-> mode)
		{
		case ON:
			LedOn(leds->n_led); 
			break;

		case OFF:
			LedOff(leds->n_led); 
			break;

		case TOGGLE:
			for (uint8_t i = 0; i < leds->n_ciclos; i++)
    		{
				LedOn(leds->n_led); //enciende el led
				vTaskDelay(leds->periodo); //espera un periodo
				LedOff(leds->n_led); //lo apaga
				vTaskDelay(leds->periodo); //espera un periodo
		
    		}
    		break;
		
		}
			}



	void app_main(void){

		LedsInit();
		my_leds.mode= TOGGLE;
		my_leds.n_ciclos=10;
		my_leds.n_led= LED_2;
		my_leds.periodo=100;
		controled(&my_leds); //el & es para pasar una direccion no una variable
		
}		
/*==================[end of file]============================================*/