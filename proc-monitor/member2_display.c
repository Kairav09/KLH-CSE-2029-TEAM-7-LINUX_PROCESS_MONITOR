/*
 * member2_display.c
 * -------------------------------------------------------
 * MEMBER 2's PART A -- sorting and displaying the process table.
 *
 * JAVA NOTE: qsort() is C's version of Collections.sort() /
 * Arrays.sort() with a Comparator. Instead of a Comparator object,
 * you pass a plain function pointer that qsort() calls internally.
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include "process.h"

/* Sort by cpu_percent DESCENDING (highest CPU first, like `top`). */
static int compare_cpu_desc(const void *a, const void *b) {
    const struct Process *pa = (const struct Process *)a;
    const struct Process *pb = (const struct Process *)b;

    if (pa->cpu_percent < pb->cpu_percent) return 1;   // a goes after b
    if (pa->cpu_percent > pb->cpu_percent) return -1;  // a goes before b
    return 0;
}

void sort_by_cpu_desc(struct Process procs[], int count) {
    qsort(procs, count, sizeof(struct Process), compare_cpu_desc);
}

/*
 * Print the process table, like a mini `top` snapshot.
 *
 * CTRL column = can YOU pause/kill this process?
 *   YES  - you own it (or you are root)
 *   NO   - owned by another user; Linux will refuse the signal
 *   KERN - kernel thread; the kernel ignores SIGSTOP/SIGKILL for it
 *   INIT - PID 1; the kernel protects it
 *
 * If only_controllable is non-zero, rows with CTRL != YES are hidden.
 */
void display_table(struct Process procs[], int count, int only_controllable) {
    printf("\n%-7s %-7s %-10s %-5s %-10s %-8s %-5s %s\n",
           "PID", "PPID", "USER", "STATE", "MEM(kB)", "CPU%", "CTRL", "NAME");
    printf("-----------------------------------------------------------------------\n");

    int shown = 0;
    for (int i = 0; i < count; i++) {
        enum ControlStatus cs = control_status(&procs[i]);
        if (only_controllable && cs != CTRL_YES) continue;

        printf("%-7d %-7d %-10.10s %-5c %-10ld %-8.2f %-5s %s\n",
               procs[i].pid, procs[i].ppid, procs[i].owner, procs[i].state,
               procs[i].mem_kb, procs[i].cpu_percent,
               control_label(cs), procs[i].name);
        shown++;
    }
    printf("-----------------------------------------------------------------------\n");
    if (only_controllable) {
        printf("Showing %d of %d processes (filter ON: only ones you can control)\n",
               shown, count);
    } else {
        printf("Total processes: %d\n", count);
    }
    printf("CTRL: YES = you can control it | NO = owned by another user (needs sudo)\n"
           "      KERN = kernel thread | INIT = PID 1 (kernel-protected)\n");
}
