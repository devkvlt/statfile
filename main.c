#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <time.h>

int uptime(int *days, int *hours, int *minutes) {
    int            mib[2];
    struct timeval boottime;
    size_t         size = sizeof(boottime);
    time_t         now;
    double         uptime;

    // Set up sysctl MIB array
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;

    if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec) {
        time(&now);
        uptime   = difftime(now, boottime.tv_sec);
        *days    = (int)uptime / 86400;
        *hours   = (int)(uptime / 3600) % 24;
        *minutes = (int)(uptime / 60) % 60;

        return 0;
    } else {
        perror("Failed to retrieve boot time");
        return 1;
    }
}

int main() {
    FILE *file = fopen("uptime", "w");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    int d, h, m;
    uptime(&d, &h, &m);

    fprintf(file, "%dd %dh %dm\n", d, h, m);

    fclose(file);
    return 0;
}
