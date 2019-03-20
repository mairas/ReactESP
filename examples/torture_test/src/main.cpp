#include <Arduino.h>
#include <ReactESP.h>

extern "C" {
#include "user_interface.h"
}

#define LED_PIN 2
#define OUT_PIN 14 // D5
#define INPUT_PIN1 12 // D6
#define INPUT_PIN2 13 // D7


#define NUM_TIMERS 20

int tick_counter = 0;
int timer_ticks[NUM_TIMERS];

void reporter() {
    Serial.printf("Timer ticks: ");
    for (int i=0; i<NUM_TIMERS; i++) {
      Serial.printf("%d ", timer_ticks[i]);
      timer_ticks[i] = 0;
    }
    Serial.printf("\n");
    Serial.printf("Free mem: %d\n", system_get_free_heap_size());
    Serial.printf("Ticks per second: %d\n", tick_counter);
    tick_counter = 0;
}

void setup_timers(ReactESP &app) {  
  // create twenty timers

  for (int i=0; i<NUM_TIMERS; i++) {
    timer_ticks[i] = 0;
    int delay = (i+1)*(i+1);
    app.onRepeat(delay, [i]() {
      timer_ticks[i]++;
    });
  }

  // create one more timer to report the counted ticks

  app.onRepeat(1000, reporter);
}

void setup_io_pins(ReactESP &app) {
  static ISRReaction* ire2 = nullptr;
  static int out_pin_state = 0;


  // change OUT_PIN state every 900 ms
  pinMode(OUT_PIN, OUTPUT);
  app.onRepeat(900, [] () {
    out_pin_state = !out_pin_state;
    digitalWrite(OUT_PIN, out_pin_state);
  });

  auto reporter = [] (int pin) {
    Serial.printf("Pin %d changed state.\n", pin);
  };

  // create an interrupt that always reports if PIN1 is rising
  app.onInterrupt(INPUT_PIN1, RISING, std::bind(reporter, INPUT_PIN1));

  // every 9s, toggle reporting PIN2 falling edge
  app.onRepeat(9000, [&app, &reporter]() {
    if (ire2==nullptr) {
      ire2 = app.onInterrupt(INPUT_PIN2, FALLING, std::bind(reporter, INPUT_PIN2));
    } else {
      ire2->remove();
      ire2 = nullptr;
    }
  });
  
}

void setup_serial(ReactESP &app) {
  // if something is received on the serial port, turn the led off for one second
  app.onAvailable(Serial, [&app] () {
    static int reaction_counter = 0;
    
    Serial.write(Serial.read());
    digitalWrite(LED_PIN, HIGH);

  reaction_counter++;

    int current = reaction_counter;

    app.onDelay(1000, [current] () {
      if (reaction_counter==current) {
        digitalWrite(LED_PIN, LOW); 
      }
    });
  });
}

void setup_tick(ReactESP &app) {
  // increase the tick counter on every tick
  app.onTick([]() {
    tick_counter++;
  });
}

ReactESP app([] () {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  setup_timers(app);
  setup_io_pins(app);
  setup_serial(app);
  setup_tick(app);
});
