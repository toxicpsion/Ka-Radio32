// ----------------------------------------------------------------------------
// Rotary Encoder Driver with enc->acceleration
// Supports Click, DoubleClick, Long Click
//
// (c) 2010 karl@pitrich.com
// (c) 2014 karl@pitrich.com
// 
// Timer-based rotary encoder logic by Peter Dannegger
// http://www.mikrocontroller.net/articles/Drehgeber
// ----------------------------------------------------------------------------
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include "ClickButtons.h"
#include "app_main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ----------------------------------------------------------------------------
/*
  int8_t enc->pinA;
  int8_t enc->pinB;
  int8_t enc->pinBTN;
  bool enc->pinsActive;
  volatile int16_t enc->delta;
  volatile int16_t enc->last;
  volatile uint8_t enc->steps;
  volatile uint16_t enc->acceleration;
  bool enc->accelerationEnabled;
  volatile enc->button enc->button;
  bool enc->doubleClickEnabled;
  bool buttonHeldEnabled;
  bool enc->buttonOnPinZeroEnabled = false;
  uint16_t enc->keyDownTicks = 0;
  uint16_t enc->doubleClickTicks = 0;
  uint16_t buttonHoldTime = BTN_HOLDTIME;
  uint16_t buttonDoubleClickTime = BTN_DOUBLECLICKTIME;
  unsigned long enc->lastButtonCheck = 0;
*/


#define TAG "ClickEncoder"



void noInterrupts()
{noInterrupt1Ms();}

void interrupts()
{interrupt1Ms();}
  
// ----------------------------------------------------------------------------

bool getpinsActive(Button_t *enc) {return enc->pinsActive;}

Button_t* ClickButtonsInit(int8_t A, int8_t B, int8_t C)
{
	Button_t* enc = malloc(sizeof(Button_t));
	enc->pinBTN[0] = A; enc->pinBTN[1] = B;
	enc->pinBTN[2] = C;

	enc->pinsActive = LOW; 
	for (int i=0;i<3;i++)
	{
		enc->button[i] = Open;
		enc->keyDownTicks[i] = 0;
		enc->doubleClickTicks[i] = 0;
		enc->lastButtonCheck[i] = 0;
	}
	enc->button[0] = Open;enc->button[1] = Open;enc->button[2] = Open;
	enc->doubleClickEnabled = true; enc->buttonHeldEnabled = true;
	
	gpio_config_t gpio_conf;
	gpio_conf.mode = GPIO_MODE_INPUT;
	gpio_conf.pull_up_en =  (enc->pinsActive == LOW) ?GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
	gpio_conf.pull_down_en = (enc->pinsActive == LOW) ?GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
	gpio_conf.intr_type = GPIO_INTR_DISABLE;
	
  if (enc->pinA >= 0) 
  {
	gpio_conf.pin_bit_mask = BIT(enc->pinA);
	ESP_ERROR_CHECK(gpio_config(&gpio_conf));
  }
  if (enc->pinB >= 0) 
  {
	gpio_conf.pin_bit_mask = BIT(enc->pinB);
	ESP_ERROR_CHECK(gpio_config(&gpio_conf));
  }
  if (enc->pinC > 0) 
  {
	gpio_conf.pin_bit_mask = BIT(enc->pinC);
	ESP_ERROR_CHECK(gpio_config(&gpio_conf));
  }
  return enc;
}

// ----------------------------------------------------------------------------
// call this every 1 millisecond via timer ISR
//
//void (*serviceEncoder)() = NULL;
void service(Button_t *enc)
{
  // handle enc->button
  //
  unsigned long currentMillis = xTaskGetTickCount()* portTICK_PERIOD_MS;
  
  for(uint8_t i = 0; i < 3; i++) 
  {
	if (currentMillis < enc->lastButtonCheck[i]) enc->lastButtonCheck[i] = 0;        // Handle case when millis() wraps back around to zero
	if ((enc->pinBTN[i] > 0 )        // check enc->button only, if a pin has been provided
		&& ((currentMillis - enc->lastButtonCheck[i]) >= ENC_BUTTONINTERVAL))            // checking enc->button is sufficient every 10-30ms
	{ 
		enc->lastButtonCheck[i] = currentMillis;

		bool pinRead = getPinState(enc,i);
    
		if (pinRead == enc->pinsActive) { // key is down
			enc->keyDownTicks[i]++;
			if ((enc->keyDownTicks[i] > (BTN_HOLDTIME / ENC_BUTTONINTERVAL)) && (enc->buttonHeldEnabled)) 
			{
				enc->button[i] = Held;
			}
		}

		if (pinRead == !enc->pinsActive) { // key is now up
			if (enc->keyDownTicks[i] > 1) {               //Make sure key was down through 1 complete tick to prevent random transients from registering as click
			if (enc->button[i] == Held) {
				enc->button[i] = Released;
				enc->doubleClickTicks[i] = 0;
			}
			else {
				#define ENC_SINGLECLICKONLY 1
				if (enc->doubleClickTicks[i] > ENC_SINGLECLICKONLY) {   // prevent trigger in single click mode
					if (enc->doubleClickTicks[i] < (BTN_DOUBLECLICKTIME / ENC_BUTTONINTERVAL)) {
					enc->button[i] = DoubleClicked;
					enc->doubleClickTicks[i] = 0;
					}
				}
				else {
					enc->doubleClickTicks[i] = (enc->doubleClickEnabled) ? (BTN_DOUBLECLICKTIME / ENC_BUTTONINTERVAL) : ENC_SINGLECLICKONLY;
				}
			}
		}
	
		enc->keyDownTicks[i] = 0;
		}
  
		if (enc->doubleClickTicks[i] > 0) {
		enc->doubleClickTicks[i]--;
		if (enc->doubleClickTicks[i] == 0) {
			enc->button[i] = Clicked;
		}
		}
	}
  }
}

// ----------------------------------------------------------------------------
Button getButton(Button_t *enc,uint8_t index)
{
  noInterrupts();
  Button ret = enc->button[index];
  if (enc->button[index] != Held && ret != Open) {
    enc->button [index]= Open; // reset
  }
  interrupts();
  return ret;
}



bool getPinState(Button_t *enc,uint8_t index) {
  bool pinState;
  {
    pinState = digitalRead(enc->pinBTN[index]);
  }
  return pinState;
}

  
