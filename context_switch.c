#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>

/**
 * Note that 3.7 GHz for the clock speed is an average of the clock speeds seen on
 * the c4 machines, it may not be the exact clock speed the program is executed at.
 * May cause slight deviation in actual time taken for the context switches.
 */
#define NUM_CONTEXT_SWITCHES 1000000
#define CLOCK_SPEED  3700000000
#define SEC_TO_MICROSEC 1000000
#define READ 0
#define WRITE 1

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
    char readBuffer[8];
    char writeBuffer[] = "a";
    int pipe1[2], pipe2[2], timePipe[2];
    pid_t pid;
    cpu_set_t cpuSet;
    clockCycles ccStart, ccFinish, ccTaken, ccSum = 0;

    /**
     * Check if any pipes failed to be opened.
     * The first two pipes are to force the context switches,
     * while the third is to communicate the clock cycles between parent
     * and child processes.
     */
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1 || pipe(timePipe) == -1) {
        printf("Error: Failed to open pipes.\n");
        return 1;
    }

    // Set the cpuSet for this process to the 0th core.
    CPU_ZERO(&cpuSet);
    CPU_SET(0, &cpuSet);

    // Move the process to the 0th core if theyre not already executing on it.
    if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuSet) == -1) {
        printf("Error: Failed to set processes to CPU core 0.\n");
        return 1;
    }

    // Fork a child process, note it will also be bound to the 0th core.
    pid = fork();

    if (pid < 0) {
        printf("Error: Failed to fork child process.\n");
        return 1;
    }
    // Parent process
    else if (pid > 0) {
        int i;
        for (i = 0; i < NUM_CONTEXT_SWITCHES; i++) {
            // Write to the first pipe, so the child process may read from it.
            write(pipe1[WRITE], writeBuffer, strlen(writeBuffer) + 1);

            // Record the clock cycle number right before context switch is forced.
            ccStart = getClockCycles();

            // Force the context switch, as the parent process waits on a write on pipe2 from the child process.
            read(pipe2[READ], readBuffer, sizeof(readBuffer));

            // Once execution returns to the parent process, record the clock cycle number the context switch finished at in the child process.
            read(timePipe[READ], &ccFinish, sizeof(clockCycles));

            // Determine the clock cycles taken, add to the total sum of clock cycles.
            ccTaken = ccFinish - ccStart;
            ccSum += ccTaken;
        }

        // Once all the context switches are complete, determine the time per context switch.
        double totalTimeTaken = (double)ccSum / CLOCK_SPEED;
        double timePerContextSwitch = totalTimeTaken / NUM_CONTEXT_SWITCHES;

        // Final output.
        printf("CONTEXT_SWITCH ---\n");
        printf("Average time taken for %d context switches: %lf microseconds.\n", NUM_CONTEXT_SWITCHES, (timePerContextSwitch * SEC_TO_MICROSEC));
        printf("------------------\n");
    }
    // Child process
    else {
        int i;
        for (i = 0; i < NUM_CONTEXT_SWITCHES; i++) {
            // Read the write from the parent process, and in subsequent iterations force context switch back to parent process.
            read(pipe1[READ], readBuffer, sizeof(readBuffer));

            // Write to the parent process to allow it to complete its next iteration.
            write(pipe2[WRITE], writeBuffer, strlen(writeBuffer) + 1);

            // Record the clock cycle number once the context switch is finished.
            ccFinish = getClockCycles();

            // Send the time the context switch finished to the paent process.
            write(timePipe[WRITE], &ccFinish, sizeof(clockCycles));
        }
    }

    // Close the pipes.
    close(pipe1[READ]);
    close(pipe1[WRITE]);
    close(pipe2[READ]);
    close(pipe2[WRITE]);
    close(timePipe[READ]);
    close(timePipe[WRITE]);
    
    return 0;
}