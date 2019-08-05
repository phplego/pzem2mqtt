#pragma once

#include <PZEM004Tv30.h>
#include "Queue.h"

class MeasureService{
    private:

        // Interval in milliseconds of the data reading
        unsigned long       interval            = 1000;     // 1 second by default
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
        static MeasureService* instance;
        // Is the first measurement done
        bool                    ready               = false;    


    private:
        void doMeasurement();              

    public:
        MeasureService();

        void init();

        void loop();

        float getPower();
        float getVoltage();
        float getCurrent();
        float getEnergy();
        float getFrequency();
        float getPowerFactor();
};

