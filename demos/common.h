#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "tester.h"

// https://github.com/daveti/rowhammer-test/blob/master/rowhammer_test.cc

//daveti: get the cache line mask once
uint64_t cl_mask;
static inline void get_cl_mask(void)
{
  asm("mrs x3, ctr_el0\n\t"
	"lsr x3, x3, #16\n\t"
	"and x3, x3, #0xf\n\t"
	"mov x2, #4\n\t"
	"lsl x2, x2, x3\n\t"           /* cache line size */
	/* x2 <- minimal cache line size in cache system */
	"sub %[res], x2, #1\n\t"
	: [res] "=r" (cl_mask)
	:
	: "x2", "x3");
};


inline __attribute__((always_inline)) void serialized_flush(void *ptr) {
    asm volatile("dc civac, %0; isb sy; dsb ish" :: "r" (ptr));
}

inline __attribute__((always_inline)) uint64_t time_load(void *ptr) {
    uint64_t before, after;
    asm volatile("isb sy; mrs %0, PMCCNTR_EL0; isb sy" : "=r" (before));
    *(volatile char *)ptr;
    asm volatile("dsb ish; isb sy; mrs %0, PMCCNTR_EL0; isb sy" : "=r" (after));
    return after - before;
}

/* Flush a cache block of address "addr" */
extern inline __attribute__((always_inline))
void clflush(uint64_t* addr)
{
  asm volatile("mov x0, %[addr]\n\t"
		"bic x0, x0, %[mask]\n\t"
		"dc civac, x0\n\t"      /* clean & invalidate data or unified cache */
 		"dsb sy\n\t"
 		"isb sy\n\t"
		:
		: [addr] "r" (addr), [mask] "r" (cl_mask)
		: "x0", "memory");
}

extern inline __attribute__((always_inline))
void clflush_u(uint64_t* addr)
{
  asm volatile("mov x0, %[addr]\n\t"
		"bic x0, x0, %[mask]\n\t"
		"dc cvau, x0\n\t"      /* clean data at L1 */
 		"isb sy\n\t"
		:
		: [addr] "r" (addr), [mask] "r" (cl_mask)
		: "x0", "memory");
}

/* Load address "addr" */
extern inline __attribute__((always_inline))
void maccess(uint64_t* addr)
{
  asm volatile("ldr x0, [%0]\n\t"
	        "dsb sy\n\t"
	        "isb sy\n\t"
	      : /*output*/
	      : /*input*/ "r"(addr)
	      : /*clobbers*/ "x0");
  
  volatile size_t unused = 0;
  for(size_t spin = 0; spin < 100; spin++) {
	  unused += spin;
	  unused *= spin;
  }
  return;
}

/* Load address "addr" */
extern inline __attribute__((always_inline))
uint64_t maccess_nospin_out(uint64_t* addr)
{
  uint64_t out;
  asm volatile("ldr %0, [%1]\n\t"
	        "dsb sy\n\t"
	        "isb sy\n\t"
	      : /*output*/ "=r"(out)
	      : /*input*/ "r"(addr)
	      : /*clobbers*/ );
  return out;
}

/* Loads addr and measure the access time */
extern inline __attribute__((always_inline))
uint64_t maccess_t(uint64_t* addr)
{
  uint64_t cycles;
  asm volatile("mov x0, %0\n\t"
              "dsb sy\n\t"
              "isb sy\n\t"
              "mrs x1, CNTVCT_EL0\n\t"
              "ldr x0, [x0]\n\t"
              "dsb sy\n\t"
              "isb sy\n\t"
              "mrs x2, CNTVCT_EL0\n\t"
              "sub %1, x2, x1\n\t"
              "dsb sy\n\t"
              "isb sy\n\t"
	      : /*output*/ "=r"(cycles)
	      : /*input*/  "r"(addr)
	      : /*clobbers*/ "x0", "x1", "x2");
  return cycles;
}

/* Loads addr and measure the access time */
extern inline __attribute__((always_inline))
uint64_t maccess_t_pmu(uint64_t* addr)
{
  uint64_t cycles;
  asm volatile("mov x0, %0\n\t"
              "dsb sy\n\t"
              "isb sy\n\t"
              "mrs x1, PMCCNTR_EL0\n\t"
              "ldr x0, [x0]\n\t"
              "dsb sy\n\t"
              "isb sy\n\t"
              "mrs x2, PMCCNTR_EL0\n\t"
              "sub %1, x2, x1\n\t"
              "dsb sy\n\t"
              "isb sy\n\t"
	      : /*output*/ "=r"(cycles)
	      : /*input*/  "r"(addr)
	      : /*clobbers*/ "x0", "x1", "x2");
  return cycles;
}


// Sattolo's https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#Sattolo's_algorithm & SE
// https://stackoverflow.com/questions/6127503/shuffle-array-in-c
void cycle(uint64_t *array, size_t n) {
	size_t i = n;
	while(i > 1) {
		i = i - 1;
		size_t j = rand() % i;
		uint64_t t = array[j];
		array[j] = array[i];
		array[i] = t;
	}
}

// https://stackoverflow.com/questions/6127503/shuffle-array-in-c
/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
// void shuffle(uint64_t **array, size_t n)
// {
//     if (n > 1) 
//     {
//         size_t i;
//         for (i = 0; i < n - 1; i++) 
//         {
//           size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
//           uint64_t* t = array[j];
//           array[j] = array[i];
//           array[i] = t;
//         }
//     }
// }

void enable_pmu() {
	printf("enabling PMU registers\n");
	int device = open("/dev/tester", O_RDONLY);
	uint64_t val = 0xdeadbeef;
	
	ioctl(device, IOCTL_PMUSERENR_EL0, &val);
	printf("PMUSERENR_EL0: %lx\n", val);

	val = 1 | (1<<2);
	printf("write PMUSERENR_EL0: %lx\n", val);
	ioctl(device, IOCTL_PMUSERENR_EL0_WRITE, &val);
	ioctl(device, IOCTL_PMUSERENR_EL0, &val);
	printf("PMUSERENR_EL0: %lx\n", val);

	// enable
	asm volatile("isb sy\n\tmrs %0, PMCR_EL0\nisb sy\n\t" : "=r" (val));
	printf("PMCR_EL0: %lx\n", val);
	val &= 0xffffffffffffff80;
	val |= 1;
	printf("write PMCR_EL0: %lx\n", val);
	asm volatile("isb sy\n\tmsr PMCR_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMCR_EL0\nisb sy\n\t" : "=r" (val));
	printf("PMCR_EL0: %lx\n", val);

	// set events
	asm volatile("isb sy\n\tmrs %0, PMCNTENSET_EL0\nisb sy\n\t" : "=r" (val));
	printf("PMCNTENSET_EL0: %lx\n", val);
	val = (1<<31) | (1<<0) | (1<<1) | (1<<2) | (1<<3);
	asm volatile("isb sy\n\tmsr PMCNTENSET_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMCNTENSET_EL0\nisb sy\n\t" : "=r" (val));
	printf("PMCNTENSET_EL0: %lx\n", val);

	printf("setting events up\n");
	val = 0;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 0x42; // L1D_CACHE_REFILL_RD
	asm volatile("isb sy\n\tmsr PMXEVTYPER_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 1;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 0x52; // CACHE_RD_REFILL (L2)
	asm volatile("isb sy\n\tmsr PMXEVTYPER_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 2;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 0x2A; // L3D_CACHE_REFILL
	asm volatile("isb sy\n\tmsr PMXEVTYPER_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 3;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 0x44; // L1D_CACHE_REFILL_INNER
	asm volatile("isb sy\n\tmsr PMXEVTYPER_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 4;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	val = 0x45; // L1D_CACHE_REFILL_OUTER
	asm volatile("isb sy\n\tmsr PMXEVTYPER_EL0, %0\nisb sy\n\t" :: "r" (val));
	

	// reset
	asm volatile("isb sy\n\tmrs %0, PMCR_EL0\nisb sy\n\t" : "=r" (val));
	printf("PMCR_EL0: %lx\n", val);
	val &= 0xffffffffffffff80;
	val |= 1 | (1<<1); // reset
	printf("write PMCR_EL0: %lx\n", val);
	asm volatile("isb sy\n\tmsr PMCR_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMCR_EL0\nisb sy\n\t" : "=r" (val));
	printf("PMCR_EL0: %lx\n", val);


	close(device);
}


extern inline __attribute__((always_inline))
uint64_t read_l1d_refills() {
	uint64_t val = 0;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMXEVCNTR_EL0\nisb sy\n\t" : "=r" (val));
	return val;
}


extern inline __attribute__((always_inline))
uint64_t read_l2d_refills() {
	uint64_t val = 1;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMXEVCNTR_EL0\nisb sy\n\t" : "=r" (val));
	return val;
}

extern inline __attribute__((always_inline))
uint64_t read_l1d_refills_inner() {
	uint64_t val = 3;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMXEVCNTR_EL0\nisb sy\n\t" : "=r" (val));
	return val;
}

extern inline __attribute__((always_inline))
uint64_t read_l1d_refills_outer() {
	uint64_t val = 4;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMXEVCNTR_EL0\nisb sy\n\t" : "=r" (val));
	return val;
}

extern inline __attribute__((always_inline))
uint64_t read_l3d_refills() {
	uint64_t val = 2;
	asm volatile("isb sy\n\tmsr PMSELR_EL0, %0\nisb sy\n\t" :: "r" (val));
	asm volatile("isb sy\n\tmrs %0, PMXEVCNTR_EL0\nisb sy\n\t" : "=r" (val));
	return val;
}

extern inline __attribute__((always_inline))
uint64_t maccess_t_pmu_l2(uint64_t* addr)
{
	uint64_t start = read_l2d_refills();
	maccess(addr);
	return read_l2d_refills() - start;
}

extern inline __attribute__((always_inline))
uint64_t maccess_t_pmu_l3(uint64_t* addr)
{
	uint64_t start = read_l3d_refills();
	maccess(addr);
	return read_l3d_refills() - start;
}

extern inline __attribute__((always_inline))
uint64_t maccess_t_pmu_l1(uint64_t* addr)
{
	uint64_t start = read_l1d_refills();
	maccess(addr);
	return read_l1d_refills() - start;
}

extern inline __attribute__((always_inline))
uint64_t maccess_t_pmu_l1_inner(uint64_t* addr)
{
	uint64_t start = read_l1d_refills_inner();
	maccess(addr);
	return read_l1d_refills_inner() - start;
}

extern inline __attribute__((always_inline))
uint64_t maccess_t_pmu_l1_outer(uint64_t* addr)
{
	uint64_t start = read_l1d_refills_outer();
	maccess(addr);
	return read_l1d_refills_outer() - start;
}

int open_kernel_device() {
	return open("/dev/tester", O_RDONLY);
}

extern inline __attribute__((always_inline))
void clear_cache_l1(int device) {
	ioctl(device, IOCTL_CLEAR_CACHE_L1, 0);
}

extern inline __attribute__((always_inline))
void clear_cache_l2(int device) {
	ioctl(device, IOCTL_CLEAR_CACHE_L2, 0);
}

extern inline __attribute__((always_inline))
void clear_cache_l3(int device) {
	ioctl(device, IOCTL_CLEAR_CACHE_L3, 0);
}

#endif
