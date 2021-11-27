#include <Arduino.h>
#include <ReactESP.h>

using namespace reactesp;

#define LED_PIN 2

int led_state = 0;

ReactESP app;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("Setting up timed reactions");

  // toggle LED every 400 ms
  app.onRepeat(400, [] () {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
  });

  // Additionally, toggle LED every 1020 ms.
  // This adds an irregularity to the LED blink pattern.
  app.onRepeat(1020, [] () {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
  });  
}

void loop() {
  app.tick();
}
