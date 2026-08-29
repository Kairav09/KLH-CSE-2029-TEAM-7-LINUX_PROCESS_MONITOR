#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t p1, p2;
    pid_t finished;

    p1 = fork();
    if (p1 == 0) {
        printf("Child 1 started. PID: %d\n", getpid());
        sleep(2);
        printf("Child 1 finished.\n");
        return 0;
    }

    p2 = fork();
    if (p2 == 0) {
        printf("Child 2 started. PID: %d\n", getpid());
        sleep(1);
        printf("Child 2 finished.\n");
        return 0;
    }

    finished = wait(NULL);
    printf("wait() finished child PID: %d\n", finished);
    waitpid(p1, NULL, 0);
    printf("waitpid() finished child PID: %d\n", p1);

    return 0;
}