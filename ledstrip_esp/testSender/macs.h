#ifndef _MACS_H_
#define _MACS_H_

String MacByteArrayToString(uint8_t mac[6]) {
  char macStr[18] = { 0 };
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}


String MacByteArrayToStringWithoutColumn(uint8_t mac[6]) {
  char macStr[13] = { 0 };
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}


bool MacStringToByteArray(String strMAC, uint8_t mac[6]) {
  //----------------------------------------------------------------------------+
  //                       Convert МАС format                                   |
  //         from <strMAC> = "58:BF:25:8B:DD:2C"  to  <mac[6]>                  |
  //  return true, if MAC format is valid                                       |
  //----------------------------------------------------------------------------+
  return (sscanf(strMAC.c_str(), "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX", mac, mac+1, mac+2, mac+3, mac+4, mac+5) == 6);
}

#endif