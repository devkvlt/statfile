#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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

void start() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork daemon process");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process: write daemon PID to file and exit
        FILE *pidfile = fopen("pid", "w");
        if (pidfile == NULL) {
            perror("Failed to create pid file");
            exit(EXIT_FAILURE);
        }
        fprintf(pidfile, "%d\n", pid);
        fclose(pidfile);
        printf("Daemon started with PID %d\n", pid);
        exit(EXIT_SUCCESS);
    } else {
        // Child process: run daemon
        umask(0);             // Set file permissions to allow read/write access for all users
        if (setsid() == -1) { // Create new session and process group
            perror("Failed to create new session");
            exit(EXIT_FAILURE);
        }
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        while (1) {
            FILE *statfile = fopen("uptime", "w");
            if (statfile == NULL) {
                perror("Failed to open uptime file");
                exit(EXIT_FAILURE);
            }

            int d, h, m;
            uptime(&d, &h, &m);

            fprintf(statfile, "%dd %dh %dm\n", d, h, m);

            fclose(statfile);

            sleep(1);
        }
    }
}

void stop() {
    FILE *pidfile = fopen("pid", "r");
    if (pidfile == NULL) {
        perror("Failed to open pid file");
        exit(EXIT_FAILURE);
    }
    pid_t pid;
    fscanf(pidfile, "%d", &pid);
    fclose(pidfile);
    if (kill(pid, SIGTERM) == -1) {
        perror("Failed to send SIGTERM signal to daemon process\n");
        exit(EXIT_FAILURE);
    }
    if (remove("pid") == -1) {
        perror("Failed to remove pid file");
        exit(EXIT_FAILURE);
    }
    printf("Daemon stopped\n");
}

int main(int argc, char *argv[]) {
    if (strcmp(argv[1], "start") == 0) {
        start();
    } else if (strcmp(argv[1], "stop") == 0) {
        stop();
    } else {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
