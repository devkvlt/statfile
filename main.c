#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <time.h>

int main() {
    int mib[2];
    struct timeval boottime;
    size_t size = sizeof(boottime);
    time_t now;
    double uptime;

    // Set up sysctl MIB array
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;

    if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec) {
        time(&now);
        uptime = difftime(now, boottime.tv_sec);

        int days = (int)uptime / 86400;
        int hours = (int)(uptime / 3600) % 24;
        int minutes = (int)(uptime / 60) % 60;

        printf("%dd %dh %dm\n", days, hours, minutes);
    } else {
        printf("Failed to retrieve boot time\n");
        return 1;
    }

    return 0;
}
