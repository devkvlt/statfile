#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/vm_statistics.h>
#include <math.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

const int PATH_STRLEN    = 256; // "/Users/some_really_long_username"
const int TIME_STRLEN    = 10;  // "00:00:00"
const int DATE_STRLEN    = 11;  // "31/12/9999"
const int BATTERY_STRLEN = 6;   // "1,100"
const int UPTIME_STRLEN  = 10;  // "999,23,59"
const int RAM_STRLEN     = 4;   // "100"
const int NETWORK_STRLEN = 9;   // "1023.9,4"

const int DEFAULT_INTERVAL = 1; // 1 second

char statfile_path[PATH_STRLEN];
char pidfile_path[PATH_STRLEN];
int  update_interval = DEFAULT_INTERVAL;

// uptime retrieves the number of days and hours the system has been up and
// stores the values in the provided pointers. It returns 0 on success and -1 on
// error.
int uptime(int *days, int *hours) {
    int            mib[2];
    struct timeval boottime;
    size_t         size = sizeof(boottime);
    time_t         now;

    // Set up sysctl MIB array
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;

    if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 && boottime.tv_sec) {
        time(&now);
        double diff = difftime(now, boottime.tv_sec);
        *days       = (int)diff / 86400;
        *hours      = (int)(diff / 3600) % 24;
        return 0;
    }

    perror("Failed to retrieve boot time");
    return -1;
}

// battery retrieves the battery status and capacity and stores the values in
// the provided pointers. A status value of 1 means "charging" and a value of 0
// means "not charging". It returns 0 on success and -1 on error.
int battery(int *is_charging, int *capacity) {
    CFTypeRef       info    = IOPSCopyPowerSourcesInfo();
    CFArrayRef      sources = IOPSCopyPowerSourcesList(info);
    CFDictionaryRef source;
    CFBooleanRef    cf_is_charging;
    CFNumberRef     cf_current_cap;

    if (CFArrayGetCount(sources) == 0) {
        perror("No power sources found");
        return -1;
    }

    // We assume there is only one source
    source = CFArrayGetValueAtIndex(sources, 0);

    cf_is_charging = (CFBooleanRef)CFDictionaryGetValue(source, CFSTR(kIOPSIsChargingKey));
    if (cf_is_charging != NULL && CFBooleanGetValue(cf_is_charging)) {
        *is_charging = 1;
    } else {
        *is_charging = 0;
    }

    cf_current_cap = (CFNumberRef)CFDictionaryGetValue(source, CFSTR(kIOPSCurrentCapacityKey));
    CFNumberGetValue(cf_current_cap, kCFNumberIntType, capacity);

    CFRelease(sources);
    CFRelease(info);

    return 0;
}

// set_value_and_unit converts a size (in bytes) into a human-friendly format
// (e.g. 13.5 KB) and stores the values in the provided pointers. A unit value
// of 0 represents byte, 1 -> KB, 2 -> MB, etc. It returns 0 on success and -1
// on error.
void set_value_and_unit(int size, float *value, int *unit) {
    if (size != 0) {
        int i  = (int)floor(log(size) / log(1024));
        *unit  = i;
        *value = (float)(size / pow(1024, i));
    }
}

// network retrieves the number of bytes sent and received over the network and
// stores the values in the provided pointers. It returns 0 on success and -1 if
// an error occurs.
int network(u_int32_t *sent, u_int32_t *received) {
    struct ifaddrs *ifaddr;
    // Get a linked list of network interfaces and their associated addresses
    if (getifaddrs(&ifaddr) == -1) {
        perror("Failed to get network interface information");
        return -1;
    }

    u_int32_t bytes_sent     = 0;
    u_int32_t bytes_received = 0;

    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        // Skip interfaces with no address
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        // Get the interface data and add its sent and received byte counts
        struct if_data *ifd = (struct if_data *)ifa->ifa_data;
        if (ifd != NULL) {
            bytes_sent += ifd->ifi_obytes;
            bytes_received += ifd->ifi_ibytes;
        }
    }

    *sent     = bytes_sent;
    *received = bytes_received;

    freeifaddrs(ifaddr);

    return 0;
}

// start starts the program. It forks a child process to run the daemon, writes
// the daemon's PID to a pidfile for later reference and continuously writes the
// various retrived stats to the statfile.
void start() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Failed to fork daemon process");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process: write daemon PID to file and exit
        FILE *pidfile = fopen(pidfile_path, "w");
        if (pidfile == NULL) {
            perror("Failed to create pidfile");
            exit(EXIT_FAILURE);
        }

        fprintf(pidfile, "%d\n", pid);
        fclose(pidfile);

        printf("Daemon started with PID %d\n", pid);
        exit(EXIT_SUCCESS);
    } else {
        // Child process: run daemon
        umask(0); // Allow read/write access for all users

        if (setsid() == -1) { // Create new session and process group
            perror("Failed to create new session");
            exit(EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Initialize network traffic before entering the loop
        u_int32_t prev_sent;
        u_int32_t prev_received;
        u_int32_t curr_sent;
        u_int32_t curr_received;
        network(&prev_sent, &prev_received);

        while (1) {
            // Open statfile
            FILE *statfile = fopen(statfile_path, "w");
            if (statfile == NULL) {
                perror("Failed to open statfile");
                exit(EXIT_FAILURE);
            }

            /*****************
             * Date and time *
             *****************/
            time_t     now      = time(NULL);
            struct tm *timeinfo = localtime(&now);

            char date_str[DATE_STRLEN];
            char time_str[TIME_STRLEN];

            strftime(date_str, DATE_STRLEN, "%d/%m/%Y", timeinfo);
            strftime(time_str, TIME_STRLEN, "%H:%M", timeinfo);

            /***********
             * Battery *
             ***********/
            int  is_charging;
            int  capacity;
            char battery_str[BATTERY_STRLEN];

            battery(&is_charging, &capacity);
            sprintf(battery_str, "%d,%d", is_charging, capacity);

            /**********
             * Uptime *
             **********/
            int  days;
            int  hours;
            char uptime_str[UPTIME_STRLEN];

            uptime(&days, &hours);
            sprintf(uptime_str, "%d,%d", days, hours);

            /*******
             * RAM *
             *******/
            mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
            vm_statistics_data_t   vmstat;

            if (KERN_SUCCESS != host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmstat, &count)) {
                perror("Failed to retrieve RAM info");
                exit(EXIT_FAILURE);
            }

            double total = vmstat.wire_count + vmstat.active_count + vmstat.inactive_count + vmstat.free_count;
            int    used  = (int)(100 * (vmstat.active_count + vmstat.inactive_count) / total);

            char ram_str[RAM_STRLEN];
            sprintf(ram_str, "%d", used);

            /***********
             * Network *
             ***********/
            network(&curr_sent, &curr_received);
            int sent     = (int)(curr_sent - prev_sent) / update_interval;
            int received = (int)(curr_received - prev_received) / update_interval;

            float sent_value     = 0;
            float received_value = 0;
            int   sent_unit      = 0;
            int   received_unit  = 0;

            set_value_and_unit(sent, &sent_value, &sent_unit);
            set_value_and_unit(received, &received_value, &received_unit);

            char sent_str[NETWORK_STRLEN];
            char received_str[NETWORK_STRLEN];

            if (sent_unit == 0 || sent_unit == 1) {
                sprintf(sent_str, "%.0f,%d", sent_value, sent_unit);
            } else {
                sprintf(sent_str, "%.1f,%d", sent_value, sent_unit);
            }

            if (received_unit == 0 || received_unit == 1) {
                sprintf(received_str, "%.0f,%d", received_value, received_unit);
            } else {
                sprintf(received_str, "%.1f,%d", received_value, received_unit);
            }

            // Update prev before next iteration
            prev_sent     = curr_sent;
            prev_received = curr_received;

            // Write to statfile
            fprintf(
                statfile, "%s|%s|%s|%s|%s|%s|%s", sent_str, received_str, uptime_str, battery_str, date_str, time_str,
                ram_str
            );

            fclose(statfile); // TODO: figure out if I should get this out of
                              // the loop
            sleep(update_interval);
        }
    }
}

// stop stops the program by killing the process whose PID is in pidfile.
void stop() {
    FILE *pid_file = fopen(pidfile_path, "r");
    if (pid_file == NULL) {
        perror("Failed to open pidfile");
        exit(EXIT_FAILURE);
    }

    pid_t pid;
    fscanf(pid_file, "%d", &pid);
    fclose(pid_file);
    if (kill(pid, SIGTERM) == -1) {
        perror("Failed to send SIGTERM signal to daemon process");
        exit(EXIT_FAILURE);
    }
    if (remove(pidfile_path) == -1) {
        perror("Failed to remove pid file");
        exit(EXIT_FAILURE);
    }
    printf("Daemon stopped\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2 && argc != 4) {
        printf("Usage: statfile start [-s update_interval]\n");
        printf("       statfile stop\n");
        return 1;
    }

    const char *home = getenv("HOME");
    if (home == NULL) {
        perror("Unable to retrieve home directory path");
        exit(EXIT_FAILURE);
    }

    snprintf(statfile_path, PATH_STRLEN, "%s/.statfile", home);
    snprintf(pidfile_path, PATH_STRLEN, "%s/.statfile.pid", home);

    if (strcmp(argv[1], "start") == 0) {
        if (argc == 4 && strcmp(argv[2], "-s") == 0) {
            update_interval = atoi(argv[3]);
        }
        start();
    } else if (strcmp(argv[1], "stop") == 0) {
        stop();
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
