#ifndef SIMTEMP_H
#define SIMTEMP_H

/*Structure for data interchange between device and user space*/

typedef struct simtemp_sample {
    uint64_t timestamp_ns;
    int temp_mC;
    int sampling_ms;
    unsigned short NEW_SAMPLE       :1;
    unsigned short LOW_TEMP_ALERT   :1;
    unsigned short HIGH_TEMP_ALERT  :1;
    unsigned short                  :13;  
} simtemp_sample;

#endif //SIMTEMP_H
