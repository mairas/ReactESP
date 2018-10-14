#include <Arduino.h>
#include <string.h>
#include <FunctionalInterrupt.h>

#include "ReactESP.h"

// Reaction classes define the behaviour of each particular
// Reaction

bool TimedReaction::operator<(const TimedReaction& other) {
    return (this->last_trigger_time + this->interval) <
        (other.last_trigger_time + other.interval);
}

void TimedReaction::add() {
    ReactESP::app->timed_queue.push(this);
}

void TimedReaction::remove() {
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
    ReactESP::app->timed_queue.push(this);
}


void UntimedReaction::add() {
    ReactESP::app->untimed_list.push_front(this);
}

void UntimedReaction::remove() {
    ReactESP::app->untimed_list.remove(this);
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


void ISRReaction::add() {
    auto handler = [this] () {
        ReactESP::app->isr_pending_list.push_front(this);
        //Serial.printf("Got interrupt for pint %d\n", this->pin_number);
    };
    attachInterrupt(digitalPinToInterrupt(pin_number), handler, mode);
    ReactESP::app->isr_reaction_list.push_front(this);
}

void ISRReaction::remove() {
    ReactESP::app->isr_reaction_list.remove(this);
    ReactESP::app->isr_pending_list.remove(this);
    detachInterrupt(digitalPinToInterrupt(this->pin_number));
    delete this;
}

void ISRReaction::tick() {
    this->callback();
} 


// Need to define the static variable outside of the class
ReactESP* ReactESP::app = NULL;

void setup(void)
{
    ReactESP::app->setup();
}

void loop(void)
{
    ReactESP::app->tick();
    yield();
}

void ReactESP::tickTimed() {
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

void ReactESP::tickUntimed() {
    for (UntimedReaction* re : this->untimed_list) {
        re->tick();
    }
}

void ReactESP::tickISR() {
    ISRReaction* isrre;
    while (!this->isr_pending_list.empty()) {
        isrre = this->isr_pending_list.front();
        this->isr_pending_list.pop_front();
        isrre->tick();
    }
}

void ReactESP::tick() {
    tickISR();
    tickUntimed();
    tickTimed();
}

DelayReaction* ReactESP::onDelay(const uint32_t t, const react_callback cb) {
    DelayReaction* dre = new DelayReaction(t, cb);
    dre->add();
    return dre;
}

RepeatReaction* ReactESP::onRepeat(const uint32_t t, const react_callback cb) {
    RepeatReaction* rre = new RepeatReaction(t, cb);
    rre->add();
    return rre;
}

StreamReaction* ReactESP::onAvailable(Stream& stream, const react_callback cb) {
    StreamReaction *sre = new StreamReaction(stream, cb);
    sre->add();
    return sre;
}

ISRReaction* ReactESP::onInterrupt(const uint8_t pin_number, int mode, const react_callback cb) {
    ISRReaction* isrre = new ISRReaction(pin_number, mode, cb);
    isrre->add();
    return isrre;
}

TickReaction* ReactESP::onTick(const react_callback cb) {
    TickReaction* tre = new TickReaction(cb);
    tre->add();
    return tre;
}
