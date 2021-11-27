# ReactESP

![C++](https://img.shields.io/badge/language-C++-blue.svg)
[![license: MIT](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

By [Matti Airas](https://github.com/mairas)

An asynchronous programming library for the ESP32 and other microcontrollers using the Arduino framework.

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

using namespace reactesp;

ReactESP app;

setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  app.onRepeat(1000, [] () {
      static bool state = false;
      digitalWrite(LED_BUILTIN, state = !state);
  });
}

void loop() {
  app.tick();
}
```

Instead of directly defining the program logic in the `loop()` function, _reactions_ are defined in the `setup()` function. A reaction is a function that is executed when a certain event happens. In this case, the event is that the function should repeat every second, as defined by the `onRepeat()` method call. The second argument to the `onRepeat()` method is a [lambda function](http://en.cppreference.com/w/cpp/language/lambda) that is executed every time the reaction is triggered. If the syntax feels weird, you can also use regular named functions instead of lambdas.

The `app.tick()` call in the `loop()` function is the main loop of the program. It is responsible for calling the reactions that have been defined. You can also add additional code to the `loop()` function, any delays or other long-executing code should be avoided.

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

ReactESP app;

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);

  app.onAvailable(&Serial, [] () {
    Serial.write(Serial.read());
    digitalWrite(LED_BUILTIN, HIGH);

    app.onDelay(1000, [] () { digitalWrite(LED_BUILTIN, LOW); });
  });
}

void loop() {
  app.tick();
}
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

### Namespace use

Note that beginning of ReactESP 2.0.0, the ReactESP library has been wrapped in
a `reactesp` namespace.
This is to avoid name conflicts with other libraries.

The impact to the user is that they need to define the namespace when using the library.
This can be done either globally by placing the following statement in the source code right after the `#include` statements:

    using namespace reactesp;

or by using the `reactesp::` prefix when using the library:

    reactesp::ReactESP app;

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

- [`Torture test`](examples/torture_test/src/main.cpp): A stress test of twenty simultaneous repeat reactions as well as a couple of interrupts, a stream, and a tick reaction. For kicks, try changing `NUM_TIMERS` to 200. Program performance will be practically unchanged!

## Changes between version 1 and 2

ReactESP version 2 has changed the software initialization approach from version 1.
Version 1 implemented the Arduino framework standard `setup()` and `loop()` functions behind the scenes,
and a user just instantiated a ReactESP object and provided a setup function as an argument:

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
