#include <Arduino.h>
#include <string.h>
#include <FunctionalInterrupt.h>

#include "Reactduino.h"
#include "ReactduinoISR.h"

// Reaction classes define the behaviour of each particular
// Reaction

Reaction* Reaction::free(Reactduino* app, reaction_idx r) {
    this->disable();
    app->_table[r] = nullptr;

    // Move the top of the stack pointer down if we free from the top
    if (app->_top == r + 1) {
        app->_top--;
    }

    return this;
}

DelayReaction::DelayReaction(uint32_t interval, react_callback callback) 
        : TimedReaction(interval, callback) {
    this->last_trigger_time = millis();
}

void DelayReaction::tick(Reactduino *app, reaction_idx r_pos) {
    uint32_t elapsed;
    uint32_t now = millis();
    elapsed = now - this->last_trigger_time;
    if (elapsed >= this->interval) {
        this->last_trigger_time = now;
        app->free(r_pos);
        this->callback();
        delete this;
    }
}

void RepeatReaction::tick(Reactduino *app, reaction_idx r_pos) {
    uint32_t elapsed;
    uint32_t now = millis();
    elapsed = now - this->last_trigger_time;
    if (elapsed >= this->interval) {
        this->last_trigger_time = now;
        this->callback();
    }
}

void StreamReaction::tick(Reactduino *app, reaction_idx r_pos) {
    if (stream->available()) {
        this->callback();
    }
}

void TickReaction::tick(Reactduino *app, reaction_idx r_pos) {
    this->callback();
}

void ISRReaction::tick(Reactduino *app, reaction_idx r_pos) {
    if (react_isr_check(this->pin_number)) {
        this->callback();
    }
}

void ISRReaction::disable() {
    detachInterrupt(this->pin_number);
    react_isr_free(this->isr);
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

Reactduino::Reactduino(react_callback cb) : _setup(cb)
{
    app = this;
}

void Reactduino::setup(void)
{
    _setup();
}

void Reactduino::tick(void)
{
    reaction_idx r;

    for (r = 0; r < _top; r++) {
        Reaction* r_entry = _table[r];

        if (r_entry==nullptr) {
            continue;
        }

        r_entry->tick(app, r);
    }
}

reaction_idx Reactduino::onDelay(uint32_t t, react_callback cb) {
    return alloc(new DelayReaction(t, cb));
}

reaction_idx Reactduino::onRepeat(uint32_t t, react_callback cb) {
    return alloc(new RepeatReaction(t, cb));
}

reaction_idx Reactduino::onAvailable(Stream *stream, react_callback cb)
{
    return alloc(new StreamReaction(stream, cb));
}

reaction_idx Reactduino::onInterrupt(uint8_t number, react_callback cb, int mode)
{
    reaction_idx r;
    int8_t isr;

    // Obtain and use ISR to handle the interrupt, see: ReactduinoISR.c/.h

    isr = react_isr_alloc();

    if (isr == INVALID_ISR) {
        return INVALID_REACTION;
    }

    r = alloc(new ISRReaction(number, isr, cb));

    if (r == INVALID_REACTION) {
        react_isr_free(isr);
        return INVALID_REACTION;
    }

    attachInterrupt(number, react_isr_get(isr), mode);

    return r;
}

reaction_idx Reactduino::onPinRising(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, RISING);
}

reaction_idx Reactduino::onPinFalling(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, FALLING);
}

reaction_idx Reactduino::onPinChange(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, CHANGE);
}

reaction_idx Reactduino::onTick(react_callback cb)
{
    return alloc(new TickReaction(cb));
}

Reaction* Reactduino::free(reaction_idx r)
{
    if (r == INVALID_REACTION) {
        return nullptr;
    }

    Reaction* re = _table[r];

    if (re==nullptr) {
        return nullptr;
    }

    return re->free(app, r);
}

reaction_idx Reactduino::alloc(Reaction *re)
{
    reaction_idx r;

    for (r = 0; r < REACTDUINO_MAX_REACTIONS; r++) {
        // If we're at the top of the stak or the allocated flag isn't set
        if (r >= _top || _table[r] == nullptr) {
            _table[r] = re;
            // Reaction is enabled
            //_table[r]->flags = REACTION_FLAG_ENABLED;

            // Move the stack pointer up if we add to the top
            if (r >= _top) {
                _top = r + 1;
            }

            return r;
        }
    }

    return INVALID_REACTION;
}
