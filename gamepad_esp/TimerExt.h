#ifndef _TIMEREXT_H_
#define _TIMEREXT_H_

class TimerExt
{
public:
    TimerExt(bool up) {
      _up = up;
      _ms = 0;
      _prev = 0;
      _mult = 1.0f;
      _timer_running = false;
    }

    void SetDir(bool up) { _up = up; }
    bool GetDir() const { return _up; }

    void SetTime(uint32_t ms) { _ms = ms; }
    uint32_t GetTime() const { return _ms; }

    void SetMult(float mult) { _mult = mult; }
    float GetMult() const { return _mult; }

    void Start() { _prev = xTaskGetTickCount(); _timer_running = true; }
    void Stop() { _prev = xTaskGetTickCount(); _timer_running = false; }

    bool isRunning() const { return _timer_running; }

    void Tick() {
        auto now = xTaskGetTickCount();
        auto elapsed = _timer_running ? (now - _prev) * _mult : 0;

        if (_up)
        {
          _ms += elapsed;
        }
        else
        {
          _ms = elapsed < _ms ? _ms - elapsed : 0;  
        }
        _prev = now;
    }

    uint32_t Secs() { return GetTime() / 1000; }

private:
    uint32_t _ms;
    uint32_t _prev;
    float _mult;
    bool _timer_running;
    bool _up;             // напраление счета
};

#endif // _TIMEREXT_H_
