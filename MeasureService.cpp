#include "MeasureService.h"

MeasureService* MeasureService::instance;

MeasureService::MeasureService()
{
    instance    = this;
    pzem        = new PZEM004Tv30((uint8_t)0 /*D3*/, (uint8_t)2 /*D4*/);
}

void MeasureService::doMeasurement()
{
    Serial.println("MeasureService::doMeasurement()"); 
    this->power       = pzem->power();
    
    if(!isnan(this->power)){
        powerQueue.add(this->power);
    }

    this->voltage     = pzem->voltage();
    this->current     = pzem->current();
    this->energy      = pzem->energy();
    this->frequency   = pzem->frequency();
    this->powerFactor = pzem->pf();

    Serial.print("Power (avg_10): "); Serial.print(powerQueue.average()); Serial.println("W");
    Serial.print("Voltage: "); Serial.print(voltage); Serial.println("V");
    Serial.print("Current: "); Serial.print(current); Serial.println("A");
    Serial.print("Energy: "); Serial.print(energy,3); Serial.println("kWh");
    Serial.print("Frequency: "); Serial.print(frequency, 1); Serial.println("Hz");
    Serial.print("PF: "); Serial.println(powerFactor);
    Serial.println();              
}


void MeasureService::init()
{
    this->doMeasurement();
    ready = true;
}

void MeasureService::loop()
{
    if(!ready) return;

    unsigned long now = millis();

    // Take a measurement at a fixed time (interval milliseconds)
    if ( !lastUpdateTime || now - lastUpdateTime > interval ) { 
        this->doMeasurement();
        lastUpdateTime = now;
    }

}

float MeasureService::getPower(){
    return this->powerQueue.average();
}
float MeasureService::getVoltage(){
    return this->voltage;
}
float MeasureService::getCurrent(){
    return this->current;
}
float MeasureService::getEnergy(){
    return this->energy;
}
float MeasureService::getFrequency(){
    return this->frequency;
}
float MeasureService::getPowerFactor(){
    return this->powerFactor;
}
