#include <Arduino.h>
#include <ReactESP.h>

using namespace reactesp;

#define LED_PIN 2
#define OUT_PIN 14 // D5
#define INPUT_PIN1 12 // D6
#define INPUT_PIN2 13 // D7


#define NUM_TIMERS 20

int tick_counter = 0;
int timer_ticks[NUM_TIMERS];

EventLoop event_loop;

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

void setup_timers(EventLoop &event_loop) {
  // create twenty timers

  for (int i=0; i<NUM_TIMERS; i++) {
    timer_ticks[i] = 0;
    int delay = (i+1)*(i+1);
    event_loop.onRepeat(delay, [i]() {
      timer_ticks[i]++;
    });
  }

  // create one more timer to report the counted ticks

  event_loop.onRepeat(1000, reporter);
}

void setup_io_pins(EventLoop &event_loop) {
  static ISREvent* ire2 = nullptr;
  static int out_pin_state = 0;


  // change OUT_PIN state every 900 ms
  pinMode(OUT_PIN, OUTPUT);
  event_loop.onRepeat(900, [] () {
    out_pin_state = !out_pin_state;
    digitalWrite(OUT_PIN, out_pin_state);
  });

  auto reporter = [] (int pin) {
    Serial.printf("Pin %d changed state.\n", pin);
  };

  // create an interrupt that always reports if PIN1 is rising
  event_loop.onInterrupt(INPUT_PIN1, RISING, std::bind(reporter, INPUT_PIN1));

  // every 9s, toggle reporting PIN2 falling edge
  event_loop.onRepeat(9000, [&event_loop, &reporter]() {
    if (ire2==nullptr) {
      ire2 = event_loop.onInterrupt(INPUT_PIN2, FALLING, std::bind(reporter, INPUT_PIN2));
    } else {
      ire2->remove();
      ire2 = nullptr;
    }
  });

}

void setup_serial(EventLoop &event_loop) {
  // if something is received on the serial port, turn the led off for one second
  event_loop.onAvailable(Serial, [&event_loop] () {
    static int event_counter = 0;

    Serial.write(Serial.read());
    digitalWrite(LED_PIN, HIGH);

  event_counter++;

    int current = event_counter;

    event_loop.onDelay(1000, [current] () {
      if (event_counter==current) {
        digitalWrite(LED_PIN, LOW);
      }
    });
  });
}

void setup_tick(EventLoop &event_loop) {
  // increase the tick counter on every tick
  event_loop.onTick([]() {
    tick_counter++;
  });
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  pinMode(LED_PIN, OUTPUT);

  setup_timers(event_loop);
  setup_io_pins(event_loop);
  setup_serial(event_loop);
  setup_tick(event_loop);
}

void loop() {
  event_loop.tick();
}
