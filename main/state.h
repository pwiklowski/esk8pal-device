#ifndef state_h
#define state_h


typedef union {
  double value;
  char bytes[sizeof(double)];
} DoubleCharacteristic;



struct CurrentState {
  DoubleCharacteristic current;
  DoubleCharacteristic voltage;
  DoubleCharacteristic used_energy;
  DoubleCharacteristic total_energy;

  DoubleCharacteristic speed;
  DoubleCharacteristic latitude;
  DoubleCharacteristic longitude;

  uint8_t day;
  uint8_t month;
  uint16_t year;
  uint32_t time;
};

#endif