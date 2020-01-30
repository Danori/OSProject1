#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

/**
 * Note that 3.7 GHz for the clock speed is an average of the clock speeds seen on
 * the c4 machines, it may not be the exact clock speed the program is executed at.
 * May cause slight deviation in actual time taken for the context switches.
 */
#define NUM_SYSCALLS 1000000
#define CLOCK_SPEED  3700000000
#define SEC_TO_MICROSEC 1000000

typedef unsigned long long clockCycles;

/**
 * Function using rdtsc to return the clock cycle counter at the time of the function call.
 * See the report for where this function is referenced from, with the permission from
 * Dr. Iamnitchi since the function involves x86 assembly.
 */
static __inline__ clockCycles getClockCycles()
{
    unsigned int a, d;
    asm("cpuid");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));

    return (((clockCycles)a) | (((clockCycles)d) << 32));
}

int main()
{
    struct timeval start, end;
    char buffer[0];
    clockCycles ccStart, ccFinish, ccTaken, ccSum = 0;
    int fd, i;

    // Attempt to open a file.
    fd = open("temp.txt", O_CREAT | O_RDONLY, 0400);

    // If file failed to open, print error, exit program.
    if (fd == -1) {
        printf("Error: Failed to create temp.txt.\n");

        return 1;
    }

    for (i = 0; i < NUM_SYSCALLS; i++) {
        // Perform a 0 byte read syscall on the opened file, record clock cycles before and after.
        ccStart = getClockCycles();
        read(fd, buffer, 0);
        ccFinish = getClockCycles();

        // Determine the clock cycles taken, add to the sum of clock cycles.
        ccTaken = ccFinish - ccStart;
        ccSum += ccTaken;
    }

    // Once all the system calls are complete, determine the time per system call.
    double totalTimeTaken = (double)ccSum / CLOCK_SPEED;
    double timePerSyscall = totalTimeTaken / NUM_SYSCALLS;

    // Final output.
    printf("SYSTEM_CALL ------\n");
    printf("Average time taken for %d system calls: %lf microseconds.\n", NUM_SYSCALLS, timePerSyscall * SEC_TO_MICROSEC);
    printf("------------------\n");

    close(fd);

    return 0;
}