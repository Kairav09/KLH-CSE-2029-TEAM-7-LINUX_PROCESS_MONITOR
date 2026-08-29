#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Process PID: %d\n", getpid());
    printf("Process is Running.\n");
    printf("Process entering Waiting State for 30 seconds.\n");
    sleep(30);
    printf("Process is Running again.\n");
    printf("Process Terminated.\n");
    return 0;
}