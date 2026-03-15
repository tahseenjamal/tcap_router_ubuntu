# TCAP Router (Dialog-Aware Load Balancer)

## Overview

This project implements a **high-performance TCAP dialog load balancer written in C**.

It is designed to run **behind Osmocom STP (`osmo-stp`)**, which handles the full **SIGTRAN protocol stack**:

* SCTP
* M3UA
* SCCP routing
* SS7 network management

This router focuses **only on TCAP dialog routing**, enabling scalable backend clusters for telecom services such as:

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
      │  SCTP / M3UA
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

| Component       | Responsibility                   |
| --------------- | -------------------------------- |
| **osmo-stp**    | SIGTRAN stack (SCTP, M3UA, SCCP) |
| **TCAP Router** | Dialog-aware TCAP load balancing |
| **Backends**    | Telecom application logic        |

This separation keeps the router **lightweight, deterministic, and scalable**.

---

# TCAP Dialog Routing

TCAP dialogs use three primary message types:

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

Routing logic:

```
BEGIN
   → choose backend (round robin)
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

Advantages:

* consistent dialog processing
* CPU cache locality
* lock-free worker queues
* no cross-worker synchronization

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

Registers the router as an SCCP user in the Osmocom stack.

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
* worker affinity based on dialog ID
* atomic queue pointers
* drop counter for overflow protection

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
* automatic dialog garbage collection
* configurable timeout (`TX_TIMEOUT`)

Expired dialogs are removed automatically.

---

# TCAP Routing

File:

```
router/router.c
```

Pipeline:

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

The parser includes **pointer validation to prevent malformed packet crashes**.

---

# Backend Pool

File:

```
core/backend_pool.c
```

Maintains persistent connections to backend TCAP servers.

Features:

* round-robin load balancing
* atomic backend state
* connection failure detection
* automatic reconnect thread

Example backend configuration:

```
127.0.0.1:4000
127.0.0.1:4001
```

Backend failure flow:

```
send() failure
     ↓
backend marked inactive
     ↓
health thread reconnects
```

---

# Performance

Expected throughput on modern hardware:

| CPU      | Throughput |
| -------- | ---------- |
| 4 cores  | ~80k TPS   |
| 8 cores  | ~150k TPS  |
| 16 cores | ~300k TPS  |

With kernel tuning:

```
>500k TCAP TPS
```

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

| Library         | Purpose        |
| --------------- | -------------- |
| libosmocore     | core utilities |
| libosmo-sccp    | SCCP stack     |
| libosmo-sigtran | M3UA / SIGTRAN |

Optional:

| Component | Purpose             |
| --------- | ------------------- |
| osmo-stp  | SS7 STP for testing |

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

---

# Run

```
./tcap-router
```

Expected startup output:

```
Starting TCAP Router
Initializing SS7 + SCCP stack
SCCP stack initialized
Backend 0 connected
Backend 1 connected
```

---

# Testing

Capture traffic:

```
sudo tcpdump -i any port 2905 -w sigtran.pcap
```

Test sources:

* osmo-stp
* SS7 simulators
* TCAP traffic generators

---

# Current Features

| Feature                  | Status      |
| ------------------------ | ----------- |
| Dialog-aware routing     | Implemented |
| Worker thread pool       | Implemented |
| Lock-free worker queues  | Implemented |
| Transaction table        | Implemented |
| Backend reconnect logic  | Implemented |
| Atomic backend state     | Implemented |
| SCCP validation          | Implemented |
| Dialog garbage collector | Implemented |
| Metrics counters         | Partial     |

---

# Future Improvements

Possible next steps:

* Prometheus metrics
* backend configuration file
* dynamic backend discovery
* SCTP multi-homing
* congestion control
* SS7 statistics interface

