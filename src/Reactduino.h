#ifndef REACTDUINO_H_
#define REACTDUINO_H_

#include <Arduino.h>

#include <functional>
#include <forward_list>
#include <queue>

typedef std::function<void()> react_callback;

// forward declarations

class Reactduino;

///////////////////////////////////////
// Reaction classes define the reaction behaviour

class Reaction {
protected:
    const react_callback callback;
public:
    Reaction(react_callback callback)
    : callback(callback) {}
    // FIXME: why do these have to be defined?
    virtual void alloc() = 0;
    virtual void free() = 0;
    virtual void tick() = 0;
};

class TimedReaction : public Reaction {
    friend class Reactduino; // FIXME: remove after debugging
protected:
    const uint32_t interval;
    uint32_t last_trigger_time;
    bool enabled;
public:
    TimedReaction(const uint32_t interval, const react_callback callback) 
    : interval(interval), Reaction(callback) {
        last_trigger_time = millis();
        enabled = true;
    }
    bool operator<(const TimedReaction& other);
    void alloc();
    void free();
    uint32_t getTriggerTime() { return last_trigger_time + interval; }
    bool isEnabled() { return enabled; }
    virtual void tick() = 0;
};

struct TriggerTimeCompare
{
    bool operator()(TimedReaction* a, TimedReaction* b)
    {
        return *b < *a;
    }
};


class DelayReaction : public TimedReaction {
public:
    DelayReaction(const uint32_t interval, const react_callback callback); 
    void tick();
};

class RepeatReaction: public TimedReaction {
public:
    RepeatReaction(const uint32_t interval, const react_callback callback) 
    : TimedReaction(interval, callback) {}
    void tick();
};

class UntimedReaction : public Reaction {
public:
    UntimedReaction(const react_callback callback)
    : Reaction(callback) {}
    virtual void alloc();
    virtual void free();
    virtual void tick() = 0;
};

class StreamReaction : public UntimedReaction {
private:
    Stream& stream;
public:
    StreamReaction(Stream& stream, const react_callback callback)
    : stream(stream), UntimedReaction(callback) {}
    void tick();
};

class TickReaction : public UntimedReaction {
public:
    TickReaction(const react_callback callback)
    : UntimedReaction(callback) {}
    void tick();
};

class ISRReaction : public Reaction {
private:
    const uint32_t pin_number;
    const int mode;
public:
    ISRReaction(uint32_t pin_number, int mode, const react_callback callback)
    : pin_number(pin_number), mode(mode), Reaction(callback) {}
    void alloc();
    void free();
    void tick();
};


///////////////////////////////////////
// Reactduino main class implementation

class Reactduino
{
    friend class Reaction;
    friend class TimedReaction;
    friend class RepeatReaction;
    friend class UntimedReaction;
    friend class ISRReaction;
public:
    Reactduino(const react_callback cb);
    void setup(void);
    void tick(void);

    // static singleton reference to the instantiated Reactduino object
    static Reactduino* app;

    // Public API
    DelayReaction* onDelay(const uint32_t t, const react_callback cb);
    RepeatReaction* onRepeat(const uint32_t t, const react_callback cb);
    StreamReaction* onAvailable(Stream& stream, const react_callback cb);
    ISRReaction* onInterrupt(const uint8_t pin_number, int mode, const react_callback cb);
    TickReaction* onTick(const react_callback cb);

private:
    const react_callback _setup;
    std::priority_queue<TimedReaction*, std::vector<TimedReaction*>, TriggerTimeCompare> timed_queue;
    std::forward_list<UntimedReaction*> untimed_list;
    std::forward_list<ISRReaction*> isr_reaction_list;
    std::forward_list<ISRReaction*> isr_pending_list;
    void tickTimed();
    void tickUntimed();
    void tickISR();
    void alloc(Reaction* re);
};

#endif
