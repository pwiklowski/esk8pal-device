#ifndef state_h
#define state_h

struct CurrentState {
  double current;
  double voltage;
  double used_energy;
  double total_energy;

  double speed;
  double latitude;
  double longitude;
};

#endif