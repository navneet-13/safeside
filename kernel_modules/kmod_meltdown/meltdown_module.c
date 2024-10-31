/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <linux/debugfs.h>
#include <linux/module.h>


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.2");

// This module provides the kernel side of a test for Meltdown.
//
// It holds a buffer of "secret" data in kernel memory that is never made
// (legitimately) available to userspace. Through debugfs, userspace can find
// the address and length of that buffer and can force it to be read into
// cache.
//
// Assuming debugfs is mounted in the usual place, the files are at:
//   /sys/kernel/debug/safeside_meltdown/
// And they are:
//   secret_data_address
//     virtual address of secret data, returned as "0x123"
//   secret_data_length
//     length of secret data in bytes, returned as "123"
//   secret_data_in_cache
//     when opened and read, forces secret data to be read into cache.
//
// Access is restricted to root to avoid opening any new attack surface, e.g.
// leaking kernel pointers.


inline void MemoryAndSpeculationBarrier(void) {
  // See docs/fencing.md
  asm volatile(
      "dsb sy\n"
      "isb\n"
      :
      :
      : "memory");
}

inline void FlushDataCacheLineNoBarrier(const void *address) {
  // "data cache clean and invalidate by virtual address to point of coherency"
  // https://cpu.fyi/d/047#G22.6241562
  asm volatile(
      "dc civac, %0\n"
      :
      : "r"(address)
      : "memory");
}



size_t kCacheLineBytes = 64;



// Returns the address of the first byte of the cache line *after* the one on
// which `addr` falls.
static const void* StartOfNextCacheLine(const void* addr) {
  uintptr_t addr_n = (uintptr_t)(addr);

  // Create an address on the next cache line, then mask it to round it down to
  // cache line alignment.
  uintptr_t next_n = (addr_n + kCacheLineBytes) & ~(kCacheLineBytes - 1);
  return (const void*)(next_n);
}

static void FlushFromDataCache(const void *begin, const void *end) {
  for (; begin < end; begin = StartOfNextCacheLine(begin)) {
    FlushDataCacheLineNoBarrier(begin);
  }
  MemoryAndSpeculationBarrier();
}

// The notionally secret data.

static char secret_data[] = "It's a s3kr3t!!!";

static uint32_t stack_pointer_index=0;
static uint64_t * stack_pointers; 

// Values published through debugfs.
static u64 secret_data_address = (u64)secret_data;
static u32 secret_data_length = sizeof(secret_data);

// The dentry of our debugfs folder.
static struct dentry *debugfs_dir = NULL;

static void run_again(void){

  char stack_mark = 'a';
  stack_pointers[stack_pointer_index] = (u64)&stack_mark;
  stack_pointer_index++;

  secret_data[8] = 'e';

  stack_pointer_index--;
  FlushFromDataCache(&stack_mark, (char *)stack_pointers[0]);

  return;
}

// Getter that reads secret_data into cache.
// Userspace will read "1", a sort of arbitrary value that could be taken to
// mean "yes, the secret data _is_ (now) in cache".
static int secret_data_in_cache_get(void* data, u64* out) {
  int i;
  stack_pointer_index=0;
  stack_pointers = kmalloc(1024*sizeof(char *), GFP_KERNEL);
  volatile const char *volatile_secret_data = secret_data;
  for (i = 0; i < secret_data_length; ++i) {
    (void)volatile_secret_data[i];
  }

  *out = 1;

  
  char stack_mark = 'a';
  stack_pointers[stack_pointer_index] = (u64)&stack_mark;
  stack_pointer_index++;

  run_again();

  stack_pointer_index--;
  kfree(stack_pointers);


  return 0;
}





// Custom read function to leak secret data to userspace
static ssize_t leak_secret_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    // Calculate how much data is left to read
    ssize_t remaining = secret_data_length - *offset;
    
    // If no more data to read, return 0 to signal end of file
    if (remaining <= 0)
        return 0;
    
    // Limit `len` to the remaining data size
    len = min(len, (size_t)remaining);

    // Copy `secret_data` to userspace
    if (copy_to_user(buf, secret_data + *offset, len))
        return -EFAULT;  // Return error if copy fails

    *offset += len;  // Update file offset
    return len;      // Return number of bytes read
}


static u64 leak_function_address = (u64)leak_secret_read;

DEFINE_DEBUGFS_ATTRIBUTE(fops_secret_data_in_cache, secret_data_in_cache_get,
    NULL, "%lld");

// Define file operations for `leak_secret`
static const struct file_operations fops_leak_secret = {
    .read = leak_secret_read,
};

static int __init meltdown_init(void) {
  struct dentry *child = NULL;

  debugfs_dir = debugfs_create_dir("safeside_meltdown", /*parent=*/NULL);
  if (!debugfs_dir) {
    goto out_remove;
  }
  

  debugfs_create_u32("secret_data_length", 0400, debugfs_dir,
      &secret_data_length);

  debugfs_create_x64("secret_data_address", 0400, debugfs_dir,
      &secret_data_address);

  debugfs_create_x64("leak_function_address", 0400, debugfs_dir,
      &leak_function_address);

  // Create the debugfs file that leaks `secret_data`
  debugfs_create_file("leak_secret", 0400, debugfs_dir, NULL, &fops_leak_secret);


  child = debugfs_create_file("secret_data_in_cache", 0400, debugfs_dir, NULL,
      &fops_secret_data_in_cache);
  if (!child) {
    goto out_remove;
  }
 

  
  return 0;

out_remove:
  debugfs_remove_recursive(debugfs_dir);  /* benign on NULL */
  debugfs_dir = NULL;

  return -ENODEV;
}

static void __exit meltdown_exit(void) {
  debugfs_remove_recursive(debugfs_dir);
  debugfs_dir = NULL;
  
}

module_init(meltdown_init);
module_exit(meltdown_exit);
