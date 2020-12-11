# ReactESP

![C++](https://img.shields.io/badge/language-C++-blue.svg)
[![license: MIT](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

By [Matti Airas](https://github.com/mairas)

An asynchronous programming library for the ESP8266 and other microcontrollers using the Arduino framework.

The library is at the core of the [SensESP](https://github.com/SignalK/SensESP) project but is completely generic and can be used for standalone projects without issues.

This library gets much of its inspiration (and some code) from [`Reactduino`](https://github.com/Reactduino/Reactduino). `ReactESP`, however, has been internally re-implemented for maintainability and readability, and has significantly better performance when there are lots of defined reactions. It also supports arbitrary callables as callbacks, allowing parametric creation of callback functions.

## Blink

If you have worked with the Arduino framework before, it is likely that you will have come across the [blink sketch](https://www.arduino.cc/en/tutorial/blink). This is a simple program that flashes an LED every second, and it looks something like this:

```cpp
#include <Arduino.h>

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}
```

Using ReactESP, the sketch can be rewritten to the following:

```cpp
#include <ReactESP.h>

ReactESP app([] () {
  pinMode(LED_BUILTIN, OUTPUT);

  app.onRepeat(1000, [] () {
      static bool state = false;
      digitalWrite(LED_BUILTIN, state = !state);
  });
});
```

With ReactESP, the developer creates an `app` object and passes to the constructor a start up function. In the example above, a [lambda function](http://en.cppreference.com/w/cpp/language/lambda) is used.

There is no `setup()` or `loop()`, ReactESP will define these for you. All you need to do is tell ReactESP which events you need to watch, and it will dispatch your handlers/callbacks when they occur.

## Why Bother?

Charlie wants to make a simple program which echoes data on the `Serial` port. Their Arduino sketch will looks like this:

```cpp
#include <Arduino.h>

void setup()
{
    Serial.begin(9600);
}

void loop()
{
    if (Serial.available() > 0) {
        Serial.write(Serial.read());
    }

    yield();
}
```

This works, but Charlie decides that they would like to blink the built-in LED every time it processes data. Now, their sketch looks like this:

```cpp
#include <Arduino.h>

void setup()
{
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    if (Serial.available() > 0) {
        Serial.write(Serial.read());

        digitalWrite(LED_BUILTIN, HIGH);
        delay(20);
        digitalWrite(LED_BUILTIN, LOW);
    }

    yield();
}
```

The problem with this sketch is that whilst the LED is blinking, Charlie's program is not relaying data from the Serial port. The longer Charlie blinks the LED for, the slower the rate of transfer.

To solve this problem, Charlie refactors their code to look something like this:

```cpp
#include <Arduino.h>

uint32_t start;
bool blink = false;

void setup()
{
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    if (Serial.available() > 0) {
        Serial.write(Serial.read());

        blink = true;
        start = millis();
        digitalWrite(LED_BUILTIN, HIGH);
    }

    if (blink && millis() - start > 1000) {
        blink = false;
        digitalWrite(LED_BUILTIN, LOW);
    }

    yield();
}
```

This solves Charlie's problem, but it's quite verbose. Using ReactESP, Charlie can write the same script like this:

```c++
#include <ReactESP.h>

ReactESP app([] () {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);

  app.onAvailable(&Serial, [] () {
    Serial.write(Serial.read());
    digitalWrite(LED_BUILTIN, HIGH);

    app.onDelay(1000, [] () { digitalWrite(LED_BUILTIN, LOW); });
  });
});
```

## Advanced callback support

Callbacks can be not just void pointers but any callable supported by `std::function`. This means they can use lambda captures or argument binding using `std::bind`. For example, the following code creates 20 different repeating reactions updating different fields of an array:

```c++
static int timer_ticks[20];

for (int i=0; i<20; i++) {
  timer_ticks[i] = 0;
  int delay = (i+1)*(i+1);
  app.onRepeat(delay, [i]() {
    timer_ticks[i]++;
  });
}
```

## API

### Event Registration Functions

All of the registration functions return a `Reaction` object pointer. This can be used to store and manipulate
the reaction. `react_callback` is a typedef for `std::function<void()>` and can therefore be any callable supported by the C++ standard template library.

```cpp
DelayReaction app.onDelay(uint32_t t, react_callback cb);
```

Delay the executation of a callback by `t` milliseconds.

```cpp
RepeatReaction app.onRepeat(uint32_t t, react_callback cb);
```

Repeatedly execute a callback every `t` milliseconds.

```cpp
StreamReaction app.onAvailable(Stream *stream, react_callback cb);
```

Execute a callback when there is data available to read on an input stream (such as `&Serial`).

```cpp
ISRReaction app.onInterrupt(uint8_t pin_number, int mode, react_callback cb);
```

Execute a callback when an interrupt number fires. This uses the same API as the `attachInterrupt()` Arduino function.

```cpp
TickReaction app.onTick(react_callback cb);
```

Execute a callback on every tick of the event loop.

### Management functions

```cpp
void Reaction::remove();
```

Remove the reaction from the execution queue.

*Note*: Calling `remove()` for `DelayReaction` objects is only safe if the reaction has not been triggered yet. Upon triggering, the `DelayReaction` object is deleted and any pointers to it will be invalidated.

### Examples

- [`Minimal`](examples/minimal/src/main.cpp): A minimal example with two timers switching the LED state.

- [`Servo`](examples/servo.cpp): Demonstrates several different reaction types for controlling a servo, monitoring inputs and blinking an LED.

- [`Torture test`](examples/torture_test/src/main.cpp): A stress test of twenty simultaneous repeat reactions as well as a couple of interrupts, a stream, and a tick reaction. For kicks, try changing `NUM_TIMERS` to 200. Program performance will be practically unchanged!
