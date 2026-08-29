#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    char cmd[100];
    printf("Enter a command: ");
    scanf("%99s", cmd);
    char *args[] = {cmd, NULL};
    pid_t pid = fork();

    if (pid == 0) {
        printf("Child PID: %d, Parent PID: %d\n", getpid(), getppid());
        execvp(args[0], args);
        printf("Command execution failed.\n");
    } else if (pid > 0) {
        printf("Parent PID: %d, Child PID: %d\n", getpid(), pid);
        wait(NULL);
        printf("Child process has finished executing.\n");
    } else {
        printf("Fork failed.\n");
    }
    return 0;
}