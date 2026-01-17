# FOS – FCIS Operating System Kernel (2025)

<div align="center">

![FOS](https://img.shields.io/badge/FOS-Educational%20Kernel-blue?style=for-the-badge)

An educational operating system kernel developed to explore and implement core OS concepts in a practical, low-level environment.

</div>

---

## 📌 Overview

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

