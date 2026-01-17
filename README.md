# FOS – FCIS Operating System Kernel (2025)

<div align="center">

![FOS](https://img.shields.io/badge/FOS-Educational%20Kernel-blue?style=for-the-badge)

An educational operating system kernel developed to explore and implement core OS concepts in a practical, low-level environment.

</div>

---

## 📌 Overview
<img src="https://github.com/user-attachments/assets/5f2c18aa-efc9-43ea-96ca-aaae7dd51757" width="50%">

FOS (Faculty of Computers and Information Science Operating System) is an academic operating system kernel developed as part of the Operating Systems course curriculum.  
The project focuses on **hands-on implementation** of core kernel components rather than theoretical simulation.

The kernel is designed to run on the **i386 (x86 32-bit)** architecture and is tested using the **Bochs emulator**.  
Throughout the project, multiple OS subsystems were implemented incrementally to gain a deep understanding of how modern operating systems manage memory, processes, and hardware interaction.

Key areas covered include:
- Virtual memory and paging
- Kernel and user space separation
- Process scheduling and context switching
- Synchronization and concurrency
- Trap, fault, and interrupt handling

---

## 🧩 Modules

The kernel is composed of several interconnected modules, each responsible for a specific operating system responsibility:

### Memory Management
- Boot-time memory initialization
- Paging and virtual memory support
- Kernel heap allocator
- User heap allocator
- Shared memory subsystem
- Working set and page tracking

### Page Replacement
- FIFO
- Clock
- Modified Clock
- Optimal (with reference tracking)

### Process & Environment Management
- User environment creation
- Process lifecycle handling
- Context switching
- Priority management

### CPU Scheduling
- Round Robin (RR)
- Priority-based Round Robin (PRIRR)

### Concurrency & Synchronization
- Kernel semaphores
- Spin locks
- Sleep locks
- Channels for inter-process communication



### Trap, Fault & Interrupt Handling
- Page fault handler
- System call interface
- Hardware interrupt handling
- Programmable Interrupt Controller (PIC)
- Kernel timer and clock

---
# My Contributions
## 🔹 Dynamic Allocator (Kernel Heap Allocation)

I implemented the **Dynamic Allocator module**, responsible for managing dynamic memory allocation inside the kernel.

- Implemented initialization of the dynamic allocator region.
- Designed block-based memory allocation with different block sizes.
- Managed free pages and free blocks using linked lists.
- Implemented allocation and deallocation of memory blocks.
- Ensured efficient reuse of memory and correct page reclamation.

This module provides flexible and efficient dynamic memory management for kernel operations.

![Project Screenshot](https://github.com/user-attachments/assets/a1a758c9-9e5b-45bc-867a-7e493b46dc17)

## 🔹 Kernel Synchronization (Locks & Semaphores) [kern/conc]

I implemented the **Kernel Synchronization module**, which provides synchronization primitives to manage concurrent execution inside the kernel.

- Implemented kernel **sleeplocks** to protect critical sections.
- Implemented **semaphores** for managing access to shared kernel resources.
- Implemented **channels** to block waiting processes.
- Ensured correct blocking and wake-up behavior for waiting processes.
- Ensured correct handling of concurrent kernel threads and processes.
- Prevented race conditions and inconsistent kernel states.
- Integrated synchronization mechanisms with the scheduler when needed.

This module ensures safe and correct kernel execution in a concurrent environment.
