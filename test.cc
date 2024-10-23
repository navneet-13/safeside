#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <unistd.h>


#include<string.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>


#include <sys/mman.h>
// #include <fcntl.h>

// #include <cstdlib>



// #include "cache_sidechannel.h"
// #include "instr.h"
// #include "local_content.h"
// #include "meltdown_local_content.h"
// #include "utils.h"






// Signal handler that accepts a pointer to an integer
void modify_value_handler(int signum, siginfo_t *info, void *context) {
    int *ptr = (int *)info->si_value.sival_ptr;  // Retrieve the pointer passed via sigqueue
    if (ptr) {
        *ptr += 10;  // Modify the value at the pointer
        printf("Signal received. Modified value: %d\n", *ptr);
    }
    else
        printf("Signal received. Recieved NULL pointer\n");
}

int main() {
    struct sigaction act;
    int shared_value = 5;  // Variable to be modified by the signal handler
    union sigval sig_data;

    // Set up the signal handler with sigaction
    memset(&act, 0, sizeof(act));
    act.sa_sigaction = modify_value_handler;
    act.sa_flags = SA_SIGINFO;  // Enable extra information passing

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction failed");
        exit(1);
    }

    printf("Original value: %d\n", shared_value);

    // Prepare the pointer to be passed via the signal
    sig_data.sival_ptr = &shared_value;

    //  siginfo_t sig_data_struct = {.si_signo = SIGUSR1, .si_code = SI_QUEUE, .si_pid = getpid(), .si_uid = getuid(), .si_value = sig_data};
    siginfo_t sig_data_struct;
    memset(&sig_data_struct, 0, sizeof(siginfo_t));  // Clear the structure

    sig_data_struct.si_signo = SIGUSR1;     // Signal number
    sig_data_struct.si_code = SI_QUEUE;     // Code indicating it was sent via sigqueue
    sig_data_struct.si_pid = getpid();      // Sending process ID
    sig_data_struct.si_uid = getuid();      // Sending user ID
    sig_data_struct.si_value = sig_data;    // Value passed via sigqueue (union sigval)

    // Send the signal with data (pointer) using sigqueue
    // if (sigqueue(getpid(), SIGUSR1, sig_data) < 0) {
    //     perror("sigqueue failed");
    //     exit(1);
    // }
    // printf("Original value: %d\n", shared_value);


    asm volatile(
        "mov x8, %0\n"           // System call number for rt_sigqueueinfo
        "mov x0, %1\n"           // Process ID (getpid)
        "mov x1, %2\n"           // Signal number (SIGUSR1)
        "mov x2, %3\n"           // Pointer to sigval (value)
        "svc #0\n"               // Make the system call
        :: "r"(__NR_rt_sigqueueinfo), "r"(getpid()), "r"(SIGUSR1), "r"(&sig_data_struct) : "x0", "x1", "x2", "x8"
        // 
    );

    printf("Final value: %d\n", shared_value);

    return 0;
}
