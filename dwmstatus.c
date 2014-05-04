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
#define BAT_POWER_NOW   "/sys/class/power_supply/BAT0/power_now"
#define AC_ONLINE       "/sys/class/power_supply/AC0/online"
#define TIMEZONE        "America/Buenos_Aires"
#define DATE_TIME_FMT   "%a %d %b  %H:%M"
#define UPDATE_INTERVAL 3
#define BL_BUF          8       // Enough for "(xx:xx)\0"
#define DT_BUF          129
#define NET_BUF         129
#define ST_BUF          300
#define AC_BUF          4

void
get_ac_status(char *bs){
    FILE *file;
    int ac_online;

    if( (file = fopen(AC_ONLINE, "r")) == NULL ){
        strcpy(bs, "¿?");
        return;
    }
    fscanf(file, "%d", &ac_online);
    fclose(file);
    if(ac_online) strcpy(bs, "Ac");
    else strcpy(bs, "Bat");
}

void
get_battery_percentage(int *bp){
    FILE *file;
    float energy_now, energy_full;

    if( (file = fopen(BAT_ENERGY_NOW, "r")) == NULL ){
        *bp = -1;
        return;
    }
    fscanf(file, "%f", &energy_now);
    fclose(file);
    
    if( (file = fopen(BAT_ENERGY_FULL, "r")) == NULL ){
        *bp = -2;
        return;
    }
    fscanf(file, "%f", &energy_full);
    fclose(file);
    *bp = (100*energy_now)/energy_full; 
}

void
get_battery_left(char *bl){
    FILE *fp1, *fp2;
    float fval, fval2;
    int aux;

    fp1 = fopen(BAT_ENERGY_NOW, "r");
    fp2 = fopen(BAT_POWER_NOW, "r");
    if(fp1 == NULL || fp2 == NULL){
        strcpy(bl, "(¿?)");
        return;
    }
    fscanf(fp1, "%f", &fval);
    fscanf(fp2, "%f", &fval2);
    fclose(fp1);
    fclose(fp2);
    
    fval2 = fval / fval2;
    aux = fval2;
    fval = (fval2 - aux)*60;
    
    sprintf(bl, "(%.0f:%02.0f)", fval, fval2);
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

    if( (file = fopen(TEMP1_INPUT, "r")) == NULL ){
        *temp = -1;
        return;
    }
    fscanf(file, "%2d", temp);
    fclose(file);
}

int 
is_up(char *device){
    FILE *fp;
    char fn[32], state[5];

    snprintf(fn, sizeof(fn), "/sys/class/net/%s/operstate", device);
    if( (fp = fopen(fn, "r")) == NULL) return 0;
    fscanf(fp, "%s", state);
    fclose(fp);
    if(strcmp(state, "up") == 0) return 1;
    return 0;
}

void
get_network(char *buf){
    int sockfd, qual = 0;
    char ssid[IW_ESSID_MAX_SIZE + 1] = "N/A";
    struct iwreq wreq;
    struct iw_statistics stats;

    if( is_up(WIRELESS_DEVICE) ){
        memset(&wreq, 0, sizeof(struct iwreq));
        sprintf(wreq.ifr_name, WIRELESS_DEVICE);
        if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) != -1 ){
            wreq.u.essid.pointer = ssid;
            wreq.u.essid.length = sizeof(ssid);
            ioctl(sockfd, SIOCGIWESSID, &wreq);
            wreq.u.data.pointer = (caddr_t)&stats;
            wreq.u.data.length = sizeof(struct iw_statistics);
            wreq.u.data.flags = 1;
            if( !ioctl(sockfd, SIOCGIWSTATS, &wreq) )
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
    Display *disp;
    char net[NET_BUF], status[ST_BUF], bl[BL_BUF],
         ac[AC_BUF], datetime[DT_BUF];
    int bp = -1, temp = -1;

    if( (disp = XOpenDisplay(NULL)) == NULL ){
        fprintf(stderr, "error: dwmstatus could not open display.\n");
        return 1;
    }

    for(;;sleep(UPDATE_INTERVAL)){
        get_network(net);
        get_temperature(&temp);
        get_ac_status(ac);
        get_battery_percentage(&bp);
        get_battery_left(bl);
        get_datetime(datetime);

        snprintf(status, ST_BUF, "%s     %s %d%% %s    T: %d     %s",
                                net, ac, bp, bl, temp, datetime);
        XStoreName(disp, DefaultRootWindow(disp), status);
        XSync(disp, False);
    }

    XCloseDisplay(disp);
    return 0;
}
