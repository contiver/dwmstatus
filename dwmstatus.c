#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>

#define TEMP1_INPUT "/sys/class/hwmon/hwmon1/device/temp1_input"
#define BAT_ENERGY_FULL "/sys/class/power_supply/BAT0/energy_full"
#define BAT_ENERGY_NOW "/sys/class/power_supply/BAT0/energy_now"
#define BAT_STATUS "/sys/class/power_supply/BAT0/status"
#define BAT_VOLTAGE_NOW "/sys/class/power_supply/BAT0/voltage_now"
#define TIMEZONE "America/Buenos_Aires"
#define DATE_TIME_FMT "%a %b %d  %H:%M"
#define UPDATE_INTERVAL 4

static Display *dpy;

char *
smprintf(char *fmt, ...){
    va_list fmtargs;
    char *ret;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    if( (ret = malloc(++len)) == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    va_start(fmtargs, fmt);
    vsnprintf(ret, len, fmt, fmtargs);
    va_end(fmtargs);

    return ret;
}

void
settz(char *tzname){
    setenv("TZ", tzname, 1);
}

void
setstatus(char *str){
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

/*
 * Copies the battery status into the bs array.
 * Note it must have at least size for 12 chars for the "Discharging\0" string
 */
void
batteryStatus(char bs[]){
    FILE *file;

    if( (file = fopen(BAT_STATUS, "r")) == NULL ){
        fprintf(stderr, "Error opening status.\n");
        strcpy(bs, "error");
        return;
    }
    fscanf(file, "%s", bs);
    fclose(file);
}

int
batteryPercentage(void){
    FILE *file;
    int energy_now, energy_full, voltage_now;

    if( (file = fopen(BAT_ENERGY_NOW, "r")) == NULL){
        fprintf(stderr, "Error opening energy_now.\n");
        return -1;
    }
    fscanf(file, "%d", &energy_now);
    fclose(file);

    
    if( (file = fopen(BAT_ENERGY_FULL, "r")) == NULL){
        fprintf(stderr, "Error opening energy_full.\n");
        return -1;
    }
    fscanf(file, "%d", &energy_full);
    fclose(file);
    
    if( (file = fopen(BAT_VOLTAGE_NOW, "r")) == NULL){
        fprintf(stderr, "Error opening voltage_now.\n");
        return -1;
    }
    fscanf(file, "%d", &voltage_now);
    fclose(file);

    return ((float)energy_now * 1000 / (float)voltage_now) * 100 /
                ((float)energy_full * 1000 / (float)voltage_now);
}

void
mktimes(char dateTime[129]){
    time_t tim;
    struct tm *timtm;

    settz(TIMEZONE);
    tim = time(NULL);
    if( (timtm = localtime(&tim)) == NULL){
        perror("localtime");
        exit(EXIT_FAILURE);
    }

    if(!strftime(dateTime, 128, DATE_TIME_FMT, timtm)){
        fprintf(stderr, "strftime == 0\n");
        exit(EXIT_FAILURE);
    }
}

int
getTemperature(void){
    FILE *file;
    int temp1_input;

    if( (file = fopen(TEMP1_INPUT, "r")) == NULL){
        fprintf(stderr, "Error opening temp1_input.\n");
        return -1;
    }
    fscanf(file, "%d", &temp1_input);
    fclose(file);
    return  temp1_input/1000;
}

int
main(void){
    char bstatus[12], dateTime[129];
    char *status = NULL;
    int bp, temp;

    if(!(dpy = XOpenDisplay(NULL))){
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    for(;;sleep(UPDATE_INTERVAL)){
        temp = getTemperature();
        bp = batteryPercentage();
        mktimes(dateTime);
        batteryStatus(bstatus);

        status = smprintf("%s %d%%  |  T: %d  |  %s",
                          bstatus, bp, temp, dateTime);
        setstatus(status);
        free(status);
    }

    XCloseDisplay(dpy);
    return 0;
}
