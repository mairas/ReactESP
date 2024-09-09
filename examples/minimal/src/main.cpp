#include <Arduino.h>
#include <ReactESP.h>

using namespace reactesp;

#define LED_PIN 2

int led_state = 0;

EventLoop event_loop;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  pinMode(LED_PIN, OUTPUT);

  Serial.println("Setting up timed events");

  // toggle LED every 400 ms
  event_loop.onRepeat(400, [] () {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
  });

  // Additionally, toggle LED every 1020 ms.
  // This adds an irregularity to the LED blink pattern.
  event_loop.onRepeat(1020, [] () {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state);
  });
}

void loop() {
  event_loop.tick();
}
