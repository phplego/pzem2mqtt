#include <PZEM004Tv30.h>
#include "Queue.h"

class MeasureService{
    private:

        // Interval in milliseconds of the data reading
        unsigned int        interval            = 1000;     // 1 second by default
        unsigned long       lastUpdateTime      = 0;

        PZEM004Tv30 *       pzem;

        float               power;
        float               voltage;
        float               current;
        float               energy;
        float               frequency;
        float               powerFactor;

        Queue<10>           powerQueue;


    public:
        // Is the first measurement done
        bool                    ready               = false;    


    private:
        void doMeasurement()
        {
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

    public:
        MeasureService()
        {
            //instance    = this;
            pzem        = new PZEM004Tv30((uint8_t)0 /*D3*/, (uint8_t)2 /*D4*/);
        }

        void init()
        {
            this->doMeasurement();
            ready = true;
        }

        void loop()
        {
            if(!ready) return;

            unsigned long now = millis();

            // Take a measurement at a fixed time (interval milliseconds)
            if ( !lastUpdateTime || now - lastUpdateTime > interval ) { 
                this->doMeasurement();
            }

            lastUpdateTime = now;
        }

        float getPower(){
            return this->powerQueue.average();
        }
        float getVoltage(){
            return this->voltage;
        }
        float getCurrent(){
            return this->current;
        }
        float getEnergy(){
            return this->energy;
        }
        float getFrequency(){
            return this->frequency;
        }
        float getPowerFactor(){
            return this->powerFactor;
        }
};