#ifndef REACTDUINO_H_
#define REACTDUINO_H_

#include <Arduino.h>
#include <stdint.h>

#include <functional>

#define INVALID_REACTION -1

#ifndef REACTDUINO_MAX_REACTIONS
#define REACTDUINO_MAX_REACTIONS 50
#endif

typedef std::function<void()> react_callback;
typedef int32_t reaction_idx;

#define REACTION_FLAG_ENABLED 0x40
#define REACTION_TYPE_MASK 0x3F

#define REACTION_TYPE_DELAY 0
#define REACTION_TYPE_REPEAT 1
#define REACTION_TYPE_STREAM 2
#define REACTION_TYPE_INTERRUPT 3
#define REACTION_TYPE_TICK 4

#define REACTION_TYPE(x) ((x) & REACTION_TYPE_MASK)

#define INPUT_STATE_HIGH    HIGH
#define INPUT_STATE_LOW     LOW
#define INPUT_STATE_ANY     0xFF
#define INPUT_STATE_UNSET   0xFE


// forward declarations

class Reactduino;

///////////////////////////////////////
// Reaction classes define the reaction behaviour

class Reaction {
protected:
    const react_callback callback;
public:
    Reaction(const react_callback callback)
    : callback(callback) {}
    virtual void alloc(Reactduino& app);
    void free(Reactduino& app, reaction_idx r);
    virtual void disable() {}
    uint8_t flags;
    virtual void tick(Reactduino& app, const reaction_idx r_pos) {}
};

class TimedReaction : public Reaction {
protected:
    const uint32_t interval;
    uint32_t last_trigger_time;
public:
    TimedReaction(uint32_t interval, react_callback callback) 
    : interval(interval), Reaction(callback) {}
};

class DelayReaction : public TimedReaction {
public:
    DelayReaction(const uint32_t interval, const react_callback callback); 
    void tick(Reactduino& app, const reaction_idx r_pos);
};

class RepeatReaction: public TimedReaction {
public:
    RepeatReaction(const uint32_t interval, const react_callback callback) 
    : TimedReaction(interval, callback) {}
    void tick(Reactduino& app, const reaction_idx r_pos);
};

class UntimedReaction : public Reaction {
public:
    UntimedReaction(const react_callback callback)
    : Reaction(callback) {}
};

class StreamReaction : public UntimedReaction {
private:
    Stream& stream;
public:
    StreamReaction(Stream& stream, const react_callback callback)
    : stream(stream), UntimedReaction(callback) {}
    void tick(Reactduino& app, const reaction_idx r_pos);
};

class TickReaction : public UntimedReaction {
public:
    TickReaction(const react_callback callback)
    : UntimedReaction(callback) {}
    void tick(Reactduino& app, const reaction_idx r_pos);
};

class ISRReaction : public UntimedReaction {
private:
    const uint32_t pin_number;
public:
    ISRReaction(uint32_t pin_number, int8_t isr, react_callback callback)
    : pin_number(pin_number), isr(isr), UntimedReaction(callback) {}
    int8_t isr;
    void disable();
    void tick(Reactduino& app, const reaction_idx r_pos);
};


///////////////////////////////////////
// Reactduino main class implementation

class Reactduino
{
    friend class Reaction;
public:
    Reactduino(react_callback cb);
    void setup(void);
    void tick(void);

    // static singleton reference to the instantiated Reactduino object
    static Reactduino* app;

    // Public API
    reaction_idx onDelay(const uint32_t t, const react_callback cb);
    reaction_idx onRepeat(const uint32_t t, const react_callback cb);
    reaction_idx onAvailable(Stream& stream, const react_callback cb);
    reaction_idx onInterrupt(const uint8_t number, const react_callback cb, int mode);
    reaction_idx onPinRising(const uint8_t pin, const react_callback cb);
    reaction_idx onPinFalling(const uint8_t pin, const react_callback cb);
    reaction_idx onPinChange(const uint8_t pin, const react_callback cb);
    reaction_idx onTick(const react_callback cb);

    Reaction* free(const reaction_idx r);

private:
    const react_callback _setup;
    Reaction* _table[REACTDUINO_MAX_REACTIONS];
    reaction_idx _top = 0;

    void alloc(Reaction* re);

};

#endif
