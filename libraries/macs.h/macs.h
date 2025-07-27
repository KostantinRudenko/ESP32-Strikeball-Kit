#ifndef _MACS_H_
#define _MACS_H_
// Модуль работы с МАС адресами

//https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFiMesh/src/TypeConversionFunctions.cpp

// Сохранение МАС адреса

uint64_t macs[2][MAX_PLAYERS];
uint64_t plaer_in_[2];

const char base36Chars[36] PROGMEM = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};


String uint8ArrayToHexString(const uint8_t *uint8Array, const uint32_t arrayLength) {
  // Convert the contents of a uint8_t array to a String in HEX format. The resulting String starts from index 0 of the array.
  // All array elements will be padded with zeroes to ensure they are converted to 2 String characters each.
  // 
  // param uint8Array The array to make into a HEX String.
  // param arrayLength The size of uint8Array, in bytes.
  // return Normally a String containing the HEX representation of the uint8Array. An empty String if the memory allocation for the String failed.
  String hexString;
  if (!hexString.reserve(2 * arrayLength))   // Each uint8_t will become two characters (00 to FF)
  {
      return emptyString;
  }

  for (uint32_t i = 0; i < arrayLength; ++i)
  {
      hexString += (char)pgm_read_byte(base36Chars + (uint8Array[i] >> 4));
      hexString += (char)pgm_read_byte(base36Chars + uint8Array[i] % 16);
  }

  return hexString;
}


uint64_t macToUint64(const uint8_t *macArray) {
  // Takes a uint8_t array and converts the first 6 bytes to a uint64_t. Assumes index 0 of the array contains MSB.
  // param: macArray A uint8_t array with the mac address to convert to a uint64_t. Should be 6 bytes in total.
  // return: A uint64_t representation of the mac.
  uint64_t result = (uint64_t)macArray[0] << 40 | (uint64_t)macArray[1] << 32 | (uint64_t)macArray[2] << 24 | 
                    (uint64_t)macArray[3] << 16 | (uint64_t)macArray[4] << 8 | (uint64_t)macArray[5];
  return result;
}


String uint64ToString(uint64_t number, const uint8_t base = 16) {
  // Note that using base 10 instead of 16 increases conversion time by roughly a factor of 5, due to unfavourable 64-bit arithmetic.
  // Note that using a base higher than 16 increases likelihood of randomly generating SSID strings containing controversial words.
  // 
  // param number The number to convert to a string with radix "base".
  // param base The radix to convert "number" into. Must be between 2 and 36.
  // return A string of "number" encoded in radix "base".

  assert(2 <= base && base <= 36);
  
  String result;
  
  if(base == 16)
  {
    do {
      result += (char)pgm_read_byte(base36Chars + number % base);
      number >>= 4; // We could write number /= 16; and the compiler would optimize it to a shift, but the explicit shift notation makes it clearer where the speed-up comes from.
    } while ( number );
  }
  else
  {
    do {
      result += (char)pgm_read_byte(base36Chars + number % base);
      number /= base;
    } while ( number );
  }
  
  std::reverse( result.begin(), result.end() );
  // при необходимости добаляем лидирующие нули
  if (base == 16)
  { 
    if (result.length() < 12)
    {
      uint8_t i = 12 - result.length();
      while (i--) result = "0" + result;
    }
  }
  return result;
}


String macToString(const uint8_t *mac) { 
  // Takes a uint8_t array and converts the first 6 bytes to a hexadecimal string.
  // param: mac A uint8_t array with the mac address to convert to a string. Should be 6 bytes in total.
  // return: A hexadecimal string representation of the mac. 
  return uint8ArrayToHexString(mac, 6);
}


void clearMACsInRAM() {
  // обнуление MAC всех игроков команд   
  for (uint8_t team = 0; team < 2; team++)
    for(uint8_t player = 0; player < MAX_PLAYERS; player++)
      macs[team][player] = 0;
}


void readMACsFromFlash() {
  // Чтение МАС всех игроков из Flash
  uint8_t team;
  uint8_t player_no;
  char player_name[4] = {' ', ' ', ' ', 0};

  // очищаем MAC всех игроков команд   
  for (team = 0; team < 2; team++)
  {
    if (team == RED)
      player_name[0]= 'R';
    else
      player_name[0] = 'B';

    for(player_no = 0; player_no < MAX_PLAYERS; player_no++)
    {
      player_name[1] = '0' + player_no / 10;
      player_name[2] = '0' + player_no % 10;
      macs[team][player_no] = preferences.getULong64(player_name, 0);
    }
  }
}

#endif    // _MACS_H_