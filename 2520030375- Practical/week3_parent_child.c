#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        printf("Fork failed.\n");
        return 1;
    }

    if (pid == 0) {
        printf("Child Process\n");
        printf("Child PID: %d\n", getpid());
        printf("Parent PID: %d\n", getppid());
        printf("Child State: Running\n");
        sleep(5);
        printf("Child State: Terminated\n");
        exit(0);
    } else {
        printf("Parent Process\n");
        printf("Parent PID: %d\n", getpid());
        printf("Child PID: %d\n", pid);
        printf("Parent State: Waiting\n");
        wait(NULL);
        printf("Parent State: Running\n");
        printf("Child process terminated\n");
    }
    return 0;
}