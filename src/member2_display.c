/*
 * member2_display.c
 * -------------------------------------------------------
 * MEMBER 2's PART A -- sorting and displaying the process table.
 *
 * JAVA NOTE: qsort() is C's version of Collections.sort() /
 * Arrays.sort() with a Comparator. Instead of passing a
 * Comparator object, you pass a plain function pointer (the
 * "compare" function below) that qsort() calls internally.
 * -------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include "process.h"

/*
 * Compare function for qsort(): sort by cpu_percent DESCENDING
 * (highest CPU usage first, like `top` does by default).
 *
 * qsort() requires this exact signature: two "const void *"
 * pointers, return negative/0/positive. We cast them back to
 * "const struct Process *" to actually compare the fields.
 */
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
 */
void display_table(struct Process procs[], int count) {
    printf("\n%-8s %-6s %-6s %-10s %-8s\n",
           "PID", "PPID", "STATE", "MEM(kB)", "CPU%");
    printf("--------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%-8d %-6d %-6c %-10ld %-8.2f\n",
               procs[i].pid, procs[i].ppid, procs[i].state,
               procs[i].mem_kb, procs[i].cpu_percent);
    }
    printf("--------------------------------------------\n");
    printf("Total processes: %d\n", count);
}
