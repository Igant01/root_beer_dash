#ifndef LIGHTS_H
#define LIGHTS_H

class cluster_lights{

    public:
        cluster_lights();
        void on();
        void off();
        void set(int brightness);
        void backlight_on();
        void backlight_off();
        void backlight_set(int brightness);
        void gaugelight_on();
        void gaugelight_off();
        void gaugelight_set(int brightness);

};

#endif