#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        printf("Child process ending.\n");
        return 0;
    } else if (pid > 0) {
        wait(NULL);
        printf("Parent collected child process.\n");
    }
    return 0;
}