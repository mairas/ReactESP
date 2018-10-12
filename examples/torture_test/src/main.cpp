#include <Arduino.h>
#include <Reactduino.h>

extern "C" {
#include "user_interface.h"
}

#define LED_PIN 2
#define OUT_PIN 14 // D5
#define INPUT_PIN1 12 // D6
#define INPUT_PIN2 13 // D7


#define NUM_TIMERS 20

int timer_ticks[NUM_TIMERS];
int tick_counter = 0;

void setup_timers(Reactduino &app) {
  for (int i=0; i<NUM_TIMERS; i++) {
    timer_ticks[i] = 0;
    int delay = (i+1)*(i+1);
    app.onRepeat(delay, [i]() {
      timer_ticks[i]++;
    });
  }

  app.onRepeat(1000, []() {
    Serial.printf("Timer ticks: ");
    for (int i=0; i<NUM_TIMERS; i++) {
      Serial.printf("%d ", timer_ticks[i]);
      timer_ticks[i] = 0;
    }
    Serial.printf("\n");
    Serial.printf("Free mem: %d\n", system_get_free_heap_size());
    Serial.printf("Ticks per second: %d\n", tick_counter);
    tick_counter = 0;
  });
}

int out_pin_state = 0;

void setup_io_pins(Reactduino &app) {
  pinMode(OUT_PIN, OUTPUT);
  app.onRepeat(500, [] () {
    out_pin_state = !out_pin_state;
    digitalWrite(OUT_PIN, out_pin_state);
  });
  auto reporter = [] (int pin) {
    Serial.printf("Pin %d changed state.\n", pin);
  };
  app.onPinChange(INPUT_PIN1, std::bind(reporter, INPUT_PIN1));
  app.onPinChange(INPUT_PIN2, std::bind(reporter, INPUT_PIN2));
}

void setup_serial(Reactduino &app) {
  app.onAvailable(Serial, [&app] () {
    static reaction_idx led_off = INVALID_REACTION;

    Serial.write(Serial.read());
    digitalWrite(LED_PIN, HIGH);

    Reaction* re = app.free(led_off); // Cancel previous timer (if set)
    if (re != nullptr) {
      delete re;
    }

    led_off = app.onDelay(1000, [] () { digitalWrite(LED_PIN, LOW); });
  });
}

void setup_tick(Reactduino &app) {
  app.onTick([]() {
    tick_counter++;
  });
}

Reactduino app([] () {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  setup_timers(app);
  setup_io_pins(app);
  setup_serial(app);
  setup_tick(app);
});
