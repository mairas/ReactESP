#include <Arduino.h>
#include <ReactESP.h>

extern "C" {
#include "user_interface.h"
}

#define LED_PIN 2

int led_state = 0;

ReactESP app([] () {
  Serial.begin(115200);
  Serial.println("Starting");
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("Setting up timed reactions");
  app.onRepeat(400, [] () {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
  });
  app.onRepeat(1020, [] () {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
  });
  
});
