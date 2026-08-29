#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        printf("Child process ending.\n");
        return 0;
    } else if (pid > 0) {
        printf("Parent sleeping. Child is now a zombie.\n");
        sleep(20);
    }
    return 0;
}