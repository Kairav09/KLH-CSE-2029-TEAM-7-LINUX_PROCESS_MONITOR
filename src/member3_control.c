/*
 * member3_control.c
 * -------------------------------------------------------
 * MEMBER 3's PART A -- controlling processes via signals.
 *
 * JAVA NOTE: there's no equivalent of Unix signals in plain Java.
 * Closest mental model: kill() is like calling a method on another
 * running program from outside it, except the "method" is just a
 * numbered signal (SIGSTOP, SIGCONT, SIGKILL) and the target
 * process decides how (or whether) to react to it -- the OS just
 * delivers it.
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "process.h"

/*
 * Each function returns 0 on success, -1 on failure (e.g. no such
 * PID, or not permitted -- typically because it belongs to another
 * user and we're not root).
 */

int pause_process(int pid) {
    if (kill(pid, SIGSTOP) == -1) {
        fprintf(stderr, "Could not pause PID %d: %s\n", pid, strerror(errno));
        return -1;
    }
    printf("PID %d paused (SIGSTOP).\n", pid);
    return 0;
}

int resume_process(int pid) {
    if (kill(pid, SIGCONT) == -1) {
        fprintf(stderr, "Could not resume PID %d: %s\n", pid, strerror(errno));
        return -1;
    }
    printf("PID %d resumed (SIGCONT).\n", pid);
    return 0;
}

int kill_process(int pid) {
    if (kill(pid, SIGKILL) == -1) {
        fprintf(stderr, "Could not kill PID %d: %s\n", pid, strerror(errno));
        return -1;
    }
    printf("PID %d killed (SIGKILL).\n", pid);
    return 0;
}
