#define _BSD_SOURCE
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/wireless.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define WIRED_DEVICE    "enp4s0"
#define WIRELESS_DEVICE "wlp3s0"
#define TEMP1_INPUT     "/sys/class/hwmon/hwmon1/device/temp1_input"
#define BAT_ENERGY_FULL "/sys/class/power_supply/BAT0/energy_full"
#define BAT_ENERGY_NOW  "/sys/class/power_supply/BAT0/energy_now"
#define BAT_STATUS      "/sys/class/power_supply/BAT0/status"
#define BAT_VOLTAGE_NOW "/sys/class/power_supply/BAT0/voltage_now"
#define TIMEZONE        "America/Buenos_Aires"
#define DATE_TIME_FMT   "%a %d  %H:%M"
#define UPDATE_INTERVAL 3
#define DT_BUF          129
#define NET_BUF         129
#define ST_BUF          300
#define BS_BUF          12 

static Display *dpy;

/*
 * Copies the battery status into the bs array.
 * Note it must have at least size for 12 chars for the "Discharging\0" string
 */
void
get_battery_status(char *bs){
    FILE *file;

    if( (file = fopen(BAT_STATUS, "r")) == NULL ){
        fprintf(stderr, "Error opening status.\n");
        strcpy(bs, "error");
        return;
    }
    fscanf(file, "%s", bs);
    fclose(file);
}

void
get_battery_percentage(int *bp){
    FILE *file;
    int energy_now, energy_full, voltage_now;

    if( (file = fopen(BAT_ENERGY_NOW, "r")) == NULL){
        fprintf(stderr, "Error opening energy_now.\n");
        return;
    }
    fscanf(file, "%d", &energy_now);
    fclose(file);

    
    if( (file = fopen(BAT_ENERGY_FULL, "r")) == NULL){
        fprintf(stderr, "Error opening energy_full.\n");
        return;
    }
    fscanf(file, "%d", &energy_full);
    fclose(file);
    
    if( (file = fopen(BAT_VOLTAGE_NOW, "r")) == NULL){
        fprintf(stderr, "Error opening voltage_now.\n");
        return;
    }
    fscanf(file, "%d", &voltage_now);
    fclose(file);

    *bp = ((float)energy_now * 1000 / (float)voltage_now) * 100 /
                ((float)energy_full * 1000 / (float)voltage_now);
}

void
get_datetime(char *dtp){
    time_t t;

    time(&t);
    strftime(dtp, DT_BUF-1, DATE_TIME_FMT, localtime(&t));
}

void
get_temperature(int *temp){
    FILE *file;
    int temp1_input;

    if( (file = fopen(TEMP1_INPUT, "r")) == NULL){
        fprintf(stderr, "Error opening temp1_input.\n");
        return;
    }
    fscanf(file, "%d", &temp1_input);
    fclose(file);
    *temp = temp1_input/1000;
}


int 
is_up(char *device){
    FILE *fp;
    char fn[32], state[5];

    snprintf(fn, sizeof(fn), "/sys/class/net/%s/operstate", device);
    fp = fopen(fn, "r");
    if(fp == NULL)
        return 0;
    fscanf(fp, "%s", state);
    fclose(fp);
    if(strcmp(state, "up") == 0)
        return 1;
    return 0;
}

void
get_network(char *buf){
    int sockfd, qual = 0;
    char ssid[IW_ESSID_MAX_SIZE + 1] = "N/A";
    struct iwreq wreq;
    struct iw_statistics stats;

    if(is_up(WIRELESS_DEVICE)){
        memset(&wreq, 0, sizeof(struct iwreq));
        sprintf(wreq.ifr_name, WIRELESS_DEVICE);
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd != -1) {
            wreq.u.essid.pointer = ssid;
            wreq.u.essid.length = sizeof(ssid);
            if(!ioctl(sockfd, SIOCGIWESSID, &wreq))
                ssid[0] = toupper(ssid[0]);

            wreq.u.data.pointer = (caddr_t) &stats;
            wreq.u.data.length = sizeof(struct iw_statistics);
            wreq.u.data.flags = 1;
            if(!ioctl(sockfd, SIOCGIWSTATS, &wreq))
                qual = stats.qual.qual;
        }
        snprintf(buf, NET_BUF, "%s %d/70", ssid, qual);
        close(sockfd);

    }else if(is_up(WIRED_DEVICE))
        snprintf(buf, NET_BUF, "Eth On");
    else
        snprintf(buf, NET_BUF, "No Internet");
}

int
main(void){
    char net[NET_BUF], status[ST_BUF], bstatus[BS_BUF], datetime[DT_BUF];
    int bp = -1, temp = -1;

    if( (dpy = XOpenDisplay(NULL)) == NULL ){
        fprintf(stderr, "error: dwmstatus could not open display.\n");
        return 1;
    }

    for(;;sleep(UPDATE_INTERVAL)){
        get_network(net);
        get_temperature(&temp);
        get_battery_percentage(&bp);
        get_datetime(datetime);
        get_battery_status(bstatus);

        snprintf(status, ST_BUF, "%s  |  %s %d%%  |  T: %d  |  %s",
                                net, bstatus, bp, temp, datetime);
        XStoreName(dpy, DefaultRootWindow(dpy), status);
        XSync(dpy, False);
    }

    XCloseDisplay(dpy);
    return 0;
}
