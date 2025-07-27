#ifndef _SHOW_TIME_HMS_H_
#define _SHOW_TIME_HMS_H_

template <typename Stream>
void showTimeHMS(Stream &s, uint32_t secs)
{
  byte hours = secs / 3600;
  byte minutes = secs % 3600 / 60;
  byte seconds = secs % 60;

  if (hours < 10)
    s.print('0');
  s.print(hours);
  s.print(':');
  if (minutes < 10)
    s.print('0');
  s.print(minutes);
  s.print(':');
  if (seconds < 10)
    s.print('0');
  s.print(seconds);
}

#endif
