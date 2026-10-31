/*
 * member3_control.c
 * -------------------------------------------------------
 * MEMBER 3's PART A -- controlling processes via signals, and
 *                       explaining WHY a process can't be controlled.
 *
 * JAVA NOTE: there's no equivalent of Unix signals in plain Java.
 * kill() is like calling a method on another running program from
 * outside it, except the "method" is a numbered signal (SIGSTOP,
 * SIGCONT, SIGKILL) and the OS decides whether it's allowed.
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "process.h"

/* ===================================================================
 * Can we control this process? (checked BEFORE sending any signal)
 * =================================================================== */

/*
 * Linux rules for sending a signal:
 *   - You may signal a process only if you own it (same user ID), or
 *     you are root. Otherwise kill() fails with EPERM.
 *   - Kernel threads have no user-space code; the kernel ignores
 *     SIGSTOP/SIGKILL for them, even from root.
 *   - PID 1 (init) is protected: SIGSTOP/SIGKILL sent to it from inside
 *     the system are ignored, even from root.
 */
enum ControlStatus control_status(const struct Process *p) {
    if (p->pid == 1) return CTRL_INIT;
    if (p->is_kernel_thread) return CTRL_KERNEL;

    uid_t me = geteuid();
    if (me == 0 || p->uid == me) return CTRL_YES;
    return CTRL_NO_OWNER;
}

const char *control_label(enum ControlStatus s) {
    switch (s) {
        case CTRL_YES:      return "YES";
        case CTRL_NO_OWNER: return "NO";
        case CTRL_KERNEL:   return "KERN";
        case CTRL_INIT:     return "INIT";
    }
    return "?";
}

static const char *my_username(void) {
    static char name[USER_LEN];
    struct passwd *pw = getpwuid(geteuid());
    if (pw != NULL) {
        snprintf(name, sizeof(name), "%s", pw->pw_name);
    } else {
        snprintf(name, sizeof(name), "uid %u", (unsigned)geteuid());
    }
    return name;
}

/* Write a human-readable reason into buf for why p can't be controlled. */
void explain_control(const struct Process *p, char *buf, size_t buf_size) {
    switch (control_status(p)) {
        case CTRL_INIT:
            snprintf(buf, buf_size,
                "PID %d (%s) is the init process. The kernel protects it: "
                "SIGSTOP/SIGKILL sent to it are ignored, even by root.",
                p->pid, p->name);
            break;
        case CTRL_KERNEL:
            snprintf(buf, buf_size,
                "PID %d (%s) is a kernel thread. It has no user-space code, "
                "and the kernel ignores SIGSTOP/SIGKILL for it, so it can't "
                "be paused or killed (even by root).",
                p->pid, p->name);
            break;
        case CTRL_NO_OWNER:
            snprintf(buf, buf_size,
                "PID %d (%s) belongs to '%s', but you are '%s'. Linux only "
                "lets a user signal their own processes (root can signal "
                "anyone). Re-run with sudo to control it.",
                p->pid, p->name, p->owner, my_username());
            break;
        case CTRL_YES:
            snprintf(buf, buf_size, "PID %d (%s) can be controlled.",
                     p->pid, p->name);
            break;
    }
}

/* ===================================================================
 * Sending signals
 * Each returns 0 on success, -1 on failure (e.g. no such PID).
 * =================================================================== */

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

/* ===================================================================
 * Test-process helpers: lets the demo create its own pausable process
 * (a child of this program, so we own it and signals are permitted).
 * =================================================================== */
#define MAX_TEST_PROCS 32
static pid_t test_pids[MAX_TEST_PROCS];
static int test_count = 0;

/*
 * fork() a child that burns CPU (`yes > /dev/null`), so it shows up near
 * the top of the table at ~100% CPU. Returns the child's PID, or -1.
 *
 * JAVA NOTE: fork() has no Java equivalent. It clones the running program;
 * it returns TWICE -- 0 in the new child, the child's PID in the parent.
 */
pid_t create_test_process(void) {
    if (test_count >= MAX_TEST_PROCS) {
        fprintf(stderr, "Too many test processes created.\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // ---- child: silence output, then become `yes` ----
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
        }
        execlp("yes", "yes", (char *)NULL);
        for (;;) { }   // fallback busy loop if `yes` isn't installed
    }

    // ---- parent ----
    test_pids[test_count++] = pid;
    return pid;
}

/*
 * A killed child lingers as a zombie (state 'Z') until its parent calls
 * wait(). WNOHANG = don't block if nothing has exited.
 */
void reap_children(void) {
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
}

/* Kill every test process we created, so none keep burning CPU after exit. */
void cleanup_test_processes(void) {
    for (int i = 0; i < test_count; i++) {
        kill(test_pids[i], SIGKILL);
        waitpid(test_pids[i], NULL, 0);   // SIGKILL can't be ignored, so this won't hang
    }
}
