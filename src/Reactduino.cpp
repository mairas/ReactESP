#include <Arduino.h>
#include <string.h>
#include <FunctionalInterrupt.h>

#include "Reactduino.h"
#include "ReactduinoISR.h"

// Reaction classes define the behaviour of each particular
// Reaction

bool TimedReaction::operator<(const TimedReaction& other) {
    return (this->last_trigger_time + this->interval) <
        (other.last_trigger_time + other.interval);
}

void TimedReaction::alloc() {
    Reactduino::app->timed_queue.push(this);
}

void TimedReaction::free() {
    this->enabled = false;
    // the object will be deleted when it's popped out of the
    // timer queue
}

DelayReaction::DelayReaction(uint32_t interval, const react_callback callback) 
        : TimedReaction(interval, callback) {
    this->last_trigger_time = millis();
}

void DelayReaction::tick() {
    this->last_trigger_time = millis();
    this->callback();
    delete this;
}


void RepeatReaction::tick() {
    this->last_trigger_time = millis();
    this->callback();
    Reactduino::app->timed_queue.push(this);
}


void UntimedReaction::alloc() {
    Reactduino::app->untimed_list.push_front(this);
}

void UntimedReaction::free() {
    Reactduino::app->untimed_list.remove(this);
    delete this;
}


void StreamReaction::tick() {
    if (stream.available()) {
        this->callback();
    }
}


void TickReaction::tick() {
    this->callback();
}


void ISRReaction::alloc() {
    auto handler = [this] () {
        Reactduino::app->isr_pending_list.push_front(this);
        //Serial.printf("Got interrupt for pint %d\n", this->pin_number);
    };
    attachInterrupt(digitalPinToInterrupt(pin_number), handler, mode);
    Reactduino::app->isr_reaction_list.push_front(this);
}

void ISRReaction::free() {
    Reactduino::app->isr_reaction_list.remove(this);
    detachInterrupt(digitalPinToInterrupt(this->pin_number));
    delete this;
}

void ISRReaction::tick() {
    this->callback();
} 


// Need to define the static variable outside of the class
Reactduino* Reactduino::app = NULL;

void setup(void)
{
    Reactduino::app->setup();
}

void loop(void)
{
    Reactduino::app->tick();
    yield();
}

Reactduino::Reactduino(const react_callback cb) : _setup(cb)
{
    app = this;
}

void Reactduino::setup(void)
{
    _setup();
}

void Reactduino::tickTimed() {
    uint32_t now = millis();
    uint32_t trigger_t;
    TimedReaction* top;
    
    while (true) {
        if (timed_queue.empty()) {
            break;
        }
        top = timed_queue.top();
        if (!top->isEnabled()) {
            timed_queue.pop();
            delete top;
            continue;
        }
        trigger_t = top->getTriggerTime();
        if (now>=trigger_t) {
            timed_queue.pop();
            top->tick();
        } else {
            break;
        }
    }
}

void Reactduino::tickUntimed() {
    for (UntimedReaction* re : this->untimed_list) {
        re->tick();
    }
}

void Reactduino::tickISR() {
    ISRReaction* isrre;
    while (!this->isr_pending_list.empty()) {
        isrre = this->isr_pending_list.front();
        this->isr_pending_list.pop_front();
        isrre->tick();
    }
}

void Reactduino::tick() {
    tickTimed();
    tickUntimed();
    tickISR();
}

DelayReaction* Reactduino::onDelay(const uint32_t t, const react_callback cb) {
    DelayReaction* dre = new DelayReaction(t, cb);
    dre->alloc();
    return dre;
}

RepeatReaction* Reactduino::onRepeat(const uint32_t t, const react_callback cb) {
    RepeatReaction* rre = new RepeatReaction(t, cb);
    rre->alloc();
    return rre;
}

StreamReaction* Reactduino::onAvailable(Stream& stream, const react_callback cb) {
    StreamReaction *sre = new StreamReaction(stream, cb);
    sre->alloc();
    return sre;
}

ISRReaction* Reactduino::onInterrupt(const uint8_t pin_number, int mode, const react_callback cb) {
    ISRReaction* isrre = new ISRReaction(pin_number, mode, cb);
    isrre->alloc();
    return isrre;
}

TickReaction* Reactduino::onTick(const react_callback cb) {
    TickReaction* tre = new TickReaction(cb);
    tre->alloc();
    return tre;
}
