#include "state.h"


double CurrentState::getCurrent(){
  return m_current;
}

double CurrentState::getVoltage(){
  return m_voltage;
}

double CurrentState::getSpeed(){
  return m_speed;;
}

double CurrentState::getUsedEnergy(){
  return m_used_energy;
}

double CurrentState::getTotalEnergy(){
  return m_current;
}

double CurrentState::getLatitude() {
  return m_latitude;
}
double CurrentState::getLongitude() {
  return m_longitude;
}

void CurrentState::setTotalEnergy(double energy){
  m_total_energy = energy;
}

void CurrentState::setLocation(double latitude, double longitude, double speed) {
  m_latitude = latitude;
  m_longitude = longitude;
  m_speed = speed;
}
