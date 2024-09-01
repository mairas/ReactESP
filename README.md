# ReactESP

![C++](https://img.shields.io/badge/language-C++-blue.svg)
[![license: MIT](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

By [Matti Airas](https://github.com/mairas)

An asynchronous programming and event loop library for the ESP32 and other microcontrollers using the Arduino framework.

The library is at the core of the [SensESP](https://github.com/SignalK/SensESP) project but is completely generic and can be used for standalone projects without issues.

This library gets much of its inspiration (and some code) from [`Reactduino`](https://github.com/Reactduino/Reactduino). `ReactESP`, however, has been internally re-implemented for maintainability and readability, and has significantly better performance when there are lots of defined events. It also supports arbitrary callables as callbacks, allowing parametric creation of callback functions.

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

using namespace reactesp;

EventLoop event_loop;

setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  event_loop.onRepeat(1000, [] () {
      static bool state = false;
      digitalWrite(LED_BUILTIN, state = !state);
  });
}

void loop() {
  event_loop.tick();
}
```

Instead of directly defining the program logic in the `loop()` function, _events_ are defined in the `setup()` function. An event is a function that is executed when a certain event happens. In this case, the event is that the function should repeat every second, as defined by the `onRepeat()` method call. The second argument to the `onRepeat()` method is a [lambda function](http://en.cppreference.com/w/cpp/language/lambda) that is executed every time the event is triggered. If the syntax feels weird, you can also use regular named functions instead of lambdas.

The `event_loop.tick()` call in the `loop()` function is the main loop of the program. It is responsible for calling the events that have been defined. You can also add additional code to the `loop()` function, any delays or other long-executing code should be avoided.

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

using namespace reactesp;

EventLoop event_loop;

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);

  event_loop.onAvailable(&Serial, [] () {
    Serial.write(Serial.read());
    digitalWrite(LED_BUILTIN, HIGH);

    event_loop.onDelay(1000, [] () { digitalWrite(LED_BUILTIN, LOW); });
  });
}

void loop() {
  event_loop.tick();
}
```

## Advanced callback support

Callbacks can be not just void pointers but any callable supported by `std::function`. This means they can use lambda captures or argument binding using `std::bind`. For example, the following code creates 20 different repeating events updating different fields of an array:

```c++
static int timer_ticks[20];

for (int i=0; i<20; i++) {
  timer_ticks[i] = 0;
  int delay = (i+1)*(i+1);
  event_loop.onRepeat(delay, [i]() {
    timer_ticks[i]++;
  });
}
```

## API

### Namespace use

Note that beginning of ReactESP 2.0.0, the ReactESP library has been wrapped in
a `reactesp` namespace.
This is to avoid name conflicts with other libraries.

The impact to the user is that they need to define the namespace when using the library.
This can be done either globally by placing the following statement in the source code right after the `#include` statements:

    using namespace reactesp;

This shouldn't be done in header files, however! Alternatively, the `reactesp::` prefix can be used when using the library:

    reactesp::EventLoop event_loop;

### Event Registration Functions

All of the registration functions return an `Event` object pointer. This can be used to store and manipulate
the event. `react_callback` is a typedef for `std::function<void()>` and can therefore be any callable supported by the C++ standard template library.

```cpp
DelayEvent event_loop.onDelay(uint32_t t, react_callback cb);
```

Delay the executation of a callback by `t` milliseconds.

```cpp
RepeatEvent event_loop.onRepeat(uint32_t t, react_callback cb);
```

Repeatedly execute a callback every `t` milliseconds.

```cpp
StreamEvent event_loop.onAvailable(Stream *stream, react_callback cb);
```

Execute a callback when there is data available to read on an input stream (such as `&Serial`).

```cpp
ISREvent event_loop.onInterrupt(uint8_t pin_number, int mode, react_callback cb);
```

Execute a callback when an interrupt number fires. This uses the same API as the `attachInterrupt()` Arduino function.

```cpp
TickEvent event_loop.onTick(react_callback cb);
```

Execute a callback on every tick of the event loop.

### Management functions

```cpp
void Event::remove();
```

Remove the event from the execution queue.

*Note*: Calling `remove()` for `DelayEvent` objects is only safe if the event has not been triggered yet. Upon triggering, the `DelayEvent` object is deleted and any pointers to it will be invalidated.

### Examples

- [`Minimal`](examples/minimal/src/main.cpp): A minimal example with two timers switching the LED state.

- [`Torture test`](examples/torture_test/src/main.cpp): A stress test of twenty simultaneous repeat events as well as a couple of interrupts, a stream, and a tick event. For kicks, try changing `NUM_TIMERS` to 200. Program performance will be practically unchanged!

## Changes between version 2 and 3

- Renamed classes from ReactESP to EventLoop and from *Reaction to *Event to better
  reflect their purpose.
- Removed the `app` singleton. The user is now responsible for maintaining the EventLoop object.

## Changes between version 1 and 2

ReactESP version 2 has changed the software initialization approach from version 1.
Version 1 implemented the Arduino framework standard `setup()` and `loop()` functions behind the scenes,
and a user just instantiated an EventLoop object and provided a setup function as an argument:

    ReactESP app([]() {
        app.onDelay(...);
    });

While this approach was "neat", it was also confusing to many users familiar with the Arduino framework. Therefore, ReactESP version 2 has reverted back to the more conventional approach:

    ReactESP app;

    void setup() {
        app.onDelay(...);
    }

    void loop() {
        app.tick();
    }

Note the changes:
- ReactESP app object is instantiated without any arguments
- There is an explicit `setup()` function.
  Its contents can be copied verbatim from the version 1 lambda function.
- There is an explicit `loop()` function.
  `app.tick()` must be called in the loop.
