# TCAP Router (Dialog-Aware Load Balancer)

## Overview

This project implements a **high-performance TCAP dialog load balancer written in C**.

It is designed to run **behind Osmocom STP (`osmo-stp`)**, which handles the full **SIGTRAN protocol stack**:

* SCTP
* M3UA
* SCCP routing
* SS7 network management

The router itself focuses **only on TCAP dialog routing**, enabling scalable backend clusters for telecom services such as:

* USSD platforms
* MAP / HLR / HSS services
* SMS routing systems
* CAMEL / IN services
* SS7 ↔ Diameter gateways

The router guarantees **dialog stickiness**, meaning all TCAP messages belonging to the same dialog are always routed to the same backend application.

---

# Architecture

```
SS7 Network
      │
      │ SCTP / M3UA
      ▼
+-------------+
|  osmo-stp   |
| (SIGTRAN)   |
+-------------+
      │
      │ SCCP primitives
      ▼
+----------------------+
|  TCAP Router         |
|                      |
|  - TCAP parsing      |
|  - dialog hashing    |
|  - worker dispatch   |
|  - backend routing   |
+----------------------+
      │
      │ SCTP
      ▼
+--------------+   +--------------+
| Backend TCAP |   | Backend TCAP |
| Application  |   | Application  |
+--------------+   +--------------+
```

---

# Key Design Principles

## Separation of Responsibilities

| Component       | Responsibility                     |
| --------------- | ---------------------------------- |
| **osmo-stp**    | SIGTRAN stack (SCTP + M3UA + SCCP) |
| **TCAP Router** | Dialog-aware TCAP load balancing   |
| **Backends**    | Telecom application logic          |

This keeps the router **lightweight, deterministic, and scalable**.

---

# TCAP Dialog Routing

TCAP dialogs contain the following message types:

| Message  | Tag    |
| -------- | ------ |
| Begin    | `0x62` |
| Continue | `0x65` |
| End      | `0x64` |

The router extracts:

```
OTID – Originating Transaction ID
DTID – Destination Transaction ID
```

Routing algorithm:

```
BEGIN
   → choose backend (round-robin)
   → store OTID → backend mapping

CONTINUE / END
   → lookup DTID
   → route to same backend
```

This guarantees **dialog affinity**.

---

# Worker Affinity

Dialogs are distributed across worker threads using:

```
worker = (OTID ^ DTID) % MAX_WORKERS
```

Benefits:

* CPU cache locality
* lock-free worker queues
* consistent dialog handling
* minimal synchronization

---

# Project Structure

```
tcap_router/
│
├── main.c
├── config.h
│
├── sigtran/
│   ├── sigtran_stack.c
│   └── sigtran_stack.h
│
├── router/
│   ├── router.c
│   └── router.h
│
├── core/
│   ├── worker_pool.c
│   ├── worker_pool.h
│   ├── backend_pool.c
│   ├── backend_pool.h
│   ├── transaction_table.c
│   └── transaction_table.h
```

---

# Core Components

## SIGTRAN Integration

File:

```
sigtran/sigtran_stack.c
```

Registers the router as an SCCP user inside the Osmocom stack:

```
osmo_sccp_user_bind(sccp, "tcap-router", sccp_prim_cb, 146);
```

Incoming flow:

```
SCCP primitive
   ↓
TCAP parser
   ↓
worker_enqueue()
```

---

# Worker Pool

File:

```
core/worker_pool.c
```

Features:

* lock-free ring buffers
* one queue per worker
* dialog-based worker affinity
* atomic queue pointers
* queue drop counter

Queue overflow increments:

```
queue_drops
```

instead of silently losing packets.

---

# Transaction Table

File:

```
core/transaction_table.c
```

Maintains dialog stickiness:

```
OTID → backend
```

Features:

* hash table with **262,144 buckets**
* per-bucket locking
* dialog garbage collector thread
* configurable timeout (`TX_TIMEOUT`)

Expired dialogs are automatically removed.

---

# TCAP Routing

File:

```
router/router.c
```

Processing pipeline:

```
SCCP message
   ↓
extract SCCP user data
   ↓
parse TCAP message
   ↓
determine dialog type
   ↓
route to backend
```

Supported SCCP message types:

```
UDT  (0x09)
XUDT (0x11)
LUDT (0x13)
```

Pointer validation prevents malformed SCCP packets from crashing the router.

---

# Backend Pool

File:

```
core/backend_pool.c
```

Maintains persistent connections to backend TCAP servers.

Features:

* round-robin backend selection
* atomic backend state
* connection failure detection
* automatic reconnect thread

Example backend configuration:

```
127.0.0.1:4000
127.0.0.1:4001
```

Failure handling:

```
send() failure
     ↓
backend marked inactive
     ↓
health thread reconnects
```

---

# Performance

Expected performance on modern servers:

| CPU      | TCAP Dialog TPS |
| -------- | --------------- |
| 4 cores  | ~80k            |
| 8 cores  | ~150k           |
| 16 cores | ~300k           |

With kernel tuning and optimized backends:

```
>500k TCAP messages/sec
```

Actual limits are often **determined by osmo-stp throughput**.

---

# Environment

Tested on:

| Component    | Version      |
| ------------ | ------------ |
| OS           | Ubuntu 24.04 |
| GCC          | 13+          |
| Kernel       | 6.x          |
| Architecture | x86_64       |

---

# Dependencies

Required Osmocom libraries:

| Library         | Purpose           |
| --------------- | ----------------- |
| libosmocore     | core utilities    |
| libosmo-sccp    | SCCP stack        |
| libosmo-sigtran | SIGTRAN protocols |

Install base dependencies:

```
sudo apt update

sudo apt install -y \
build-essential \
libsctp-dev \
pkg-config \
libosmocore-dev \
libosmo-sccp-dev \
libosmo-sigtran-dev \
osmo-stp
```

---

# Build

Clone repository:

```
git clone https://github.com/tahseenjamal/tcap_router_ubuntu.git
cd tcap_router_ubuntu
```

Compile:

```
make
```

Binary produced:

```
tcap-router
```

---

# Running the Router

Start the router:

```
./tcap-router
```

Expected output:

```
Starting TCAP Router
Initializing SS7 + SCCP stack
SCCP stack initialized
Backend 0 connected
Backend 1 connected
```

---

# Configuring osmo-stp

The router receives SCCP traffic via **SSN 146**.

Install STP:

```
sudo apt install osmo-stp
```

Create configuration:

```
sudo nano /etc/osmocom/osmo-stp.cfg
```

Example configuration:

```
log stderr
 logging level debug

cs7 instance 0
 point-code 0.0.1

 asp asp1 2905 0 m3ua
  remote-ip 127.0.0.1
  role asp

 as as1 m3ua
  asp asp1
  routing-key 1 0.0.0

sccp

 sccp-address local
  point-code 0.0.1
  ssn 146

 sccp-routing
  route on ssn 146
   destination local
```

This tells the STP:

```
All SCCP traffic for SSN 146 → deliver locally
```

Your router registers the same SSN internally:

```
osmo_sccp_user_bind(..., 146);
```

---

# Starting osmo-stp

Run:

```
sudo osmo-stp -c /etc/osmocom/osmo-stp.cfg
```

Expected log:

```
SCCP initialized
M3UA stack initialized
```

---

# Traffic Flow

```
SS7 Network
    │
    │ M3UA
    ▼
osmo-stp
    │
    │ SCCP SSN 146
    ▼
TCAP Router
    │
    ▼
Backend TCAP Servers
```

---

# Testing

Capture traffic:

```
sudo tcpdump -i any port 2905 -w sigtran.pcap
```

Possible test sources:

* osmo-stp
* SS7 simulators
* custom TCAP generators

---

# Current Features

| Feature                  | Status      |
| ------------------------ | ----------- |
| Dialog-aware routing     | Implemented |
| Worker thread pool       | Implemented |
| Lock-free queues         | Implemented |
| Transaction table        | Implemented |
| Backend reconnect logic  | Implemented |
| Atomic backend state     | Implemented |
| SCCP validation          | Implemented |
| Dialog garbage collector | Implemented |

---

# Future Improvements

Planned enhancements:

* Prometheus metrics
* dynamic backend configuration
* SCTP multi-homing
* congestion control
* cluster deployment
* SS7 statistics interface

---

# License

MIT License

---

# Author

**Tahseen Jamal**
