# A Loadable Kernel Module for Efficient Rewiring

Rewiring is a technique for mapping virtual pages arbitrarily to physical pages. In the [original](http://www.vldb.org/pvldb/vol9/p768-schuhknecht.pdf) paper, rewiring is implemented with mmap. Thus, rewiring can be used from the user-space without much preparation.

However, mmap-based rewiring has several drawbacks, that have already been discussed in literature:

* In the worst case, the kernel allocates one `vm_area_struct` (~ 300 bytes) for one page, leading to massive kernel space consumption. Additionally, on most systems, the `vm.max_map_count` kernel variable limits the number of `vm_area_struct` objects per default to 65330
* Rewiring may lead to many costly calls of the mmap syscall, impacting performance.
* As mmap does not populate the pagetable, many additional page faults occur, also hurting performance.

This project implements rewiring efficiently through a Loadable Kernel Module for Linux. It solves all three mentioned problems by introducing a non-linear mapping in the kernel module. This mapping can be manipulated using ioctl system calls. Still, no kernel recompilation is required, just compile the kernel module for your kernel version (5.3.* works, others might work) and insert it at runtime. 

## Project Structure

### Loadable Kernel Module

* `communication.h`: Defines types for the ioctl interface. Also included in e.g. the C++ Library.
* `global_state.h` + `global_state.c`: Implements a global state object, that manages physical page allocation. Implements lookup for abstract 'page ids' and returns the corresponding physical page address.
* `local_state.c` + `local_state.h`: Implements a local state object: Stores mapping from virtual pages to abstract page ids.
* `rewiring-lkm.c`: Main program. Reacts to open/mmap/unmap/close/ioctl operations and handles page faults.

### C++ Library
Additionally, this project also contains a C++ header-only library for
simplifying rewiring, located under `lib`. The library detects if the developed kernel module
is running. If not, it automatically switches to a mmap-based
implementation.

### Benchmarks
* `bench/alltoone.cpp`: Map `N` virtual pages to 1 physical page and iterate over all pages for the first time. Shows best-case speedup over mmap-based approach
* `bench/deque.cpp` Compares a rewired deque implementation using the kernel module with the same implementation using the mmap-approach and additionally the `std::deque` implementation. The rewired deque implementation is located under `bench/util/deque.h`

## Building and Loading the Kernel Module
### 1. Install kernel header files:

On Debian/Ubuntu like systems the following command should be enough
```
sudo apt install search linux-headers-$(uname -r)
```
### 2. Add udev rule

Create this file `/etc/udev/rules.d/99-rewiring.rules` with this line:
```
KERNEL=="rewiring", SUBSYSTEM=="rewiring", MODE="0666"
```
This allows every user to open and use the later created device file under `/dev/rewiring`

### 3. Build with Makefile
Change into the `module` directory of this repository.
Then execute `make` to build the module.
### 4. Load the module
**You should always check the code yourself before inserting an unknown kernel module. In this case, the code base is actually small enough, to really do this!**

Now, a `rewiring.ko` file should exist. If not, recheck the build process.
Insert the module into the kernel:
```
sudo insmod rewiring.ko
```
### 5. Verify 
To verify that everything worked, check with ls
```
$ ls -la /dev/rewiring
crw-rw-rw- 1 root root 236, 0 Apr  5 19:37 /dev/rewiring
```
Additionally, the kernel module writes a log message on loading.
This message should appear when using e.g. `dmesg`
```
$ dmesg
... 
[...] REWIRING_LKM: Initializing
...
```
### 6. Use it
### 7. Unload the module
On the next reboot, the module will automatically disappear from the kernel. If you want to remove it earlier execute:
```
sudo rmmod rewiring
```