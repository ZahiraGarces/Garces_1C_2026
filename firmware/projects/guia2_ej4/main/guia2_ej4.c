/*! @mainpage Osciloscopio
 *
 * @section genDesc Digitaliza señal analógica (CH1) y la transmite por UART_PC a 500 Hz en formato ASCII compatible con graficador serie
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral   |   ESP32   	 |
 * |:---------------:|:--------------|
 * |Entrada analogica|  CH1(GPIO_34) |
 * |Salida DAC(ECG)  |    DAC_CH1    |
 * |   UART PC       |    UART_PC    |
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 13/05/2026 | Document creation		                         |
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
#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "timer_mcu.h"
/*==================[macros and definitions]=================================*/
/** @brief Frecuencia de muestreo del ADC en Hz */
#define FRECUENCIA_MUESTREO 500

/** @brief Período del Timer A en microsegundos (1/500 Hz = 2000 us) */
#define PERIODO_TIMER_1 2000

/** @brief Período del Timer B en microsegundos (igual al Timer A: 2000 us) */
#define PERIODO_TIMER_2 2000

/** @brief Velocidad de comunicación UART en baudios */
#define VELOCIDAD_UART 115200

/*==================[internal data definition]===============================*/
/** @brief Handle de la tarea de lectura analógica */
TaskHandle_t leerAnalogico_task_handle = NULL;

/** @brief Handle de la tarea de reproducción del ECG por DAC */
TaskHandle_t mostrarECG_task_handle = NULL;

/**
 * @brief Array con una muestra de señal ECG sintética.
 *
 * Contiene 250 muestras de 8 bits que representan un ciclo cardíaco
 * completo. Se reproduce cíclicamente por el DAC a 500 Hz, generando
 * una señal de ~2 Hz (frecuencia cardíaca ~120 BPM).
 */
static unsigned char ECG[] = {
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18,
	18, 18, 18, 17, 17, 16, 16, 16, 16, 17, 17, 18, 18, 18, 17, 17, 17, 17, 18, 18, 19, 21, 22, 24,
	25, 26, 27, 28, 29, 31, 32, 33, 34, 34, 35, 37, 38, 37, 34, 29, 24, 19, 15, 14, 15, 16, 17, 17,
	16, 15, 13, 13, 13, 13, 13, 13, 12, 12, 10, 6, 2, 3, 15, 43, 88, 145, 199, 237, 252, 242, 211,
	167, 117, 70, 35, 16, 14, 22, 32, 38, 37, 32, 27, 24, 24, 26, 27, 28, 28, 27, 28, 28, 30, 31,
	31, 32, 33, 34, 36, 38, 39, 40, 41, 42, 43, 45, 47, 49, 51, 53, 55, 57, 60, 62, 65, 68, 71, 75,
	79, 83, 87, 92, 97, 101, 106, 111, 116, 121, 125, 129, 133, 136, 138, 139, 140, 140, 139,
	137, 133, 129, 123, 117, 109, 101, 92, 84, 77, 70, 64, 58, 52, 47, 42, 39, 36, 34, 31, 30, 28,
	27, 26, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24,
	24, 24, 24, 24, 24, 23, 23, 22, 22, 21, 21, 21, 20, 20, 20, 20, 19, 19, 18, 18, 18, 19, 19, 19,
	19, 18, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17};

/** @brief Cantidad de muestras del array ECG */
#define ECG_LENGTH (sizeof(ECG) / sizeof(ECG[0])) // calcula cuantos elementos tiene el array

/** @brief Índice actual de reproducción del array ECG */
uint16_t ecg_index = 0; // indice para recorrer el array

/*==================[internal functions declaration]=========================*/
void Timer1(void *param);
void Timer2(void *param);
/*==================[internal functions definition]=========================*/
/**
 * @brief Callback de interrupción del Timer 1
 *
 * Esta función se ejecuta cada vez que vence el Timer A.
 * Su función es notificar a la tarea encargada de leer
 * el ADC para que realice una nueva conversión analógica.
 *
 * @param param Parámetro opcional del timer (no utilizado).
 */
void Timer1(void *param)
{
	vTaskNotifyGiveFromISR(leerAnalogico_task_handle, pdFALSE);
}
/**
 * @brief Callback de interrupción del Timer 2.
 *
 * Esta función se ejecuta cada vez que vence el Timer B.
 * Notifica a la tarea encargada de enviar las muestras
 * del ECG al DAC.
 *
 * @param param Parámetro opcional del timer (no utilizado).
 */
void Timer2(void *param)
{
	vTaskNotifyGiveFromISR(mostrarECG_task_handle, pdFALSE);
}
/**
 * @brief Tarea encargada de leer una señal analógica.
 *
 * La tarea permanece bloqueada hasta recibir una notificación
 * proveniente del Timer1. Luego:
 * - Lee una muestra del ADC en el canal CH1.
 * - Convierte el valor a ASCII.
 * - Envía el dato por UART a la PC.
 *
 * La transmisión se realiza en formato compatible con
 * un graficador serie.
 *
 * @param pvParameter Parámetro de la tarea (no utilizado).
 */
static void leerAnalogico(void *pvParameter)
{
	uint16_t adc_raw = 0; // variable para guardar la lectura del ADC
	while (1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// Leer lo que entró por CH1 (viene del DAC via potenciómetro)
		AnalogInputReadSingle(CH1, &adc_raw);

		// Mandar a la PC
		UartSendString(UART_PC, (const char *)UartItoa(adc_raw, 10));
		UartSendString(UART_PC, "\r\n");
	}
}
/**
 * @brief Tarea encargada de reproducir una señal ECG.
 *
 * La tarea espera una notificación del Timer2. Cada vez que
 * se activa:
 * - Envía un valor del arreglo ECG al DAC.
 * - Incrementa el índice del arreglo.
 * - Reinicia el índice al llegar al final.
 *
 * Esto permite generar una señal periódica simulando un ECG.
 *
 * @param pvParameter Parámetro de la tarea (no utilizado).
 */
static void mostrarECG(void *pvParameter)
{
	while (1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// Enviar muestra actual al DAC
		AnalogOutputWrite(ECG[ecg_index]);

		ecg_index++;
        // Reiniciar índice al finalizar el arreglo
		if (ecg_index >= ECG_LENGTH)
		{
			ecg_index = 0; 
		}
	}
}

/*==================[external functions definition]==========================*/
void app_main(void)
{

	/**
	 * @brief Configuración de la UART.
	 *
	 * Se inicializa la UART utilizada para transmitir
	 * las muestras digitalizadas hacia la PC.
	 */
	serial_config_t uart_cfg = {
		.port = UART_PC,
		.baud_rate = VELOCIDAD_UART,
		.func_p = UART_NO_INT,
		.param_p = NULL,
	};
	UartInit(&uart_cfg);

	/**
	 * @brief Configuración del ADC.
	 *
	 * Inicializa el convertidor analógico-digital
	 * en modo de conversión simple sobre el canal CH1.
	 */
	analog_input_config_t adc_cfg = {
		.input = CH1,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0};

	AnalogInputInit(&adc_cfg);
	/**
	 * @brief Inicialización del DAC.
	 *
	 * Habilita la salida analógica utilizada
	 * para reproducir la señal ECG almacenada
	 * en el arreglo ECG[].
	 */
	AnalogOutputInit();
	/**
	 * @brief Configuración del Timer A.
	 *
	 * Este timer genera interrupciones periódicas
	 * cada 2000 us para activar la tarea encargada
	 * de leer el ADC.
	 */
	timer_config_t timer1_cfg = {
		.timer = TIMER_A,
		.period = PERIODO_TIMER_1,
		.func_p = Timer1,
		.param_p = NULL,
	};

	/**
	 * @brief Configuración del Timer B.
	 *
	 * Genera interrupciones periódicas para activar
	 * la tarea que envía muestras del ECG al DAC.
	 */
	timer_config_t timer2_cfg = {
		.timer = TIMER_B,
		.period = PERIODO_TIMER_2,
		.func_p = Timer2,
		.param_p = NULL,
	};

	// Inicialización de timers
	TimerInit(&timer1_cfg);
	TimerInit(&timer2_cfg);

	/**
	 * @brief Creación de la tarea leerAnalogico.
	 *
	 * Tarea encargada de:
	 * - Leer muestras del ADC.
	 * - Enviar los datos por UART.
	 */
	xTaskCreate(&leerAnalogico, "Leer analogico", 2048, NULL, 5, &leerAnalogico_task_handle);
	/**
	 * @brief Creación de la tarea mostrarECG.
	 *
	 * Tarea encargada de:
	 * - Recorrer el arreglo ECG[].
	 * - Enviar las muestras al DAC.
	 */
	xTaskCreate(&mostrarECG, "Mostrar ECG", 1024, NULL, 5, &mostrarECG_task_handle);
	/**
	 * @brief Inicio de los timers.
	 *
	 * Comienza la ejecución periódica de ambos timers,
	 * habilitando el funcionamiento del sistema.
	 */
	TimerStart(TIMER_A);
	TimerStart(TIMER_B);
}
/*==================[end of file]============================================*/