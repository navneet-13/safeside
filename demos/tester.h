// Thanks: https://tldp.org/LDP/lkmpg/2.4/html/x856.html

#ifndef TESTER_H
#define TESTER_H

#include <linux/ioctl.h>

#define MAJOR_NUM 100

#define IOCTL_CurrentEL _IOR(MAJOR_NUM, 0, uint64_t*)
#define IOCTL_CPUECTLR_EL1 _IOR(MAJOR_NUM, 1, uint64_t*)
#define IOCTL_PMUSERENR_EL0 _IOR(MAJOR_NUM, 2, uint64_t*)
// #define IOCTL_CPUECTLR_EL1_WRITE _IOW(MAJOR_NUM, 301, uint64_t*)
#define IOCTL_PMUSERENR_EL0_WRITE _IOW(MAJOR_NUM, 3, uint64_t*)
#define IOCTL_PMCR_EL0 _IOR(MAJOR_NUM, 4, uint64_t*)
#define IOCTL_PMCR_EL0_WRITE _IOW(MAJOR_NUM, 5, uint64_t*)
#define IOCTL_ACTLR_EL2 _IOR(MAJOR_NUM, 5, uint64_t*)

#define IOCTL_CLEAR_CACHE_L1 _IOW(MAJOR_NUM, 101, uint64_t)
#define IOCTL_CLEAR_CACHE_L2 _IOW(MAJOR_NUM, 102, uint64_t)
#define IOCTL_CLEAR_CACHE_L3 _IOW(MAJOR_NUM, 103, uint64_t)
#define DEVICE_FILE_NAME "tester"

#endif
