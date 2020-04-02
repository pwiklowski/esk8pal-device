#ifndef state_h
#define state_h

class CurrentState {
  double m_current;
  double m_voltage;
  double m_speed;
  double m_used_energy;
  double m_total_energy;

  double m_latitude;
  double m_longitude;

public:
  double getCurrent();
  double getVoltage();
  double getSpeed();
  double getLatitude();
  double getLongitude();
  double getUsedEnergy();
  double getTotalEnergy();
  void setTotalEnergy(double energy);

  void setLocation(double latitude, double longitude, double speed);
};

#endif