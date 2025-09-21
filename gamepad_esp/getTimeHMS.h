#ifndef _GET_TIME_HMS_H_
#define _GET_TIME_HMS_H_

String getTimeHMS(uint32_t secs) {
  byte hours = secs / 3600;
  byte minutes = (secs % 3600) / 60;
  byte seconds = secs % 60;

  char buf[9]; // "HH:MM:SS" + '\0'
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds); // 01:01:01

  return String(buf);
}

#endif
