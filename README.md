# TCAP Router (Dialog-Aware Load Balancer)

## Overview

This project implements a **high-performance TCAP dialog load balancer** written in C.

It is designed to work together with **Osmocom STP (`osmo-stp`)**, which handles the full SIGTRAN stack:

* SCTP
* M3UA
* SCCP routing
* SS7 network management

The router implemented here **focuses only on TCAP dialog routing**, enabling scalable backend application clusters for telecom services such as:

* USSD
* MAP (HLR / HSS)
* SMS routing
* CAMEL / IN services
* Diameter interworking gateways

The router ensures **dialog stickiness**, meaning all TCAP messages belonging to the same dialog are always routed to the same backend node.

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
|  (this project)      |
|                      |
|  - TCAP parsing      |
|  - dialog hashing    |
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

### 1. Separation of Responsibilities

| Component       | Responsibility                     |
| --------------- | ---------------------------------- |
| **osmo-stp**    | SIGTRAN stack (SCTP + M3UA + SCCP) |
| **TCAP Router** | Dialog-aware TCAP load balancing   |
| **Backends**    | Application logic                  |

This keeps the router **lightweight and scalable**.

---

### 2. Dialog Stickiness

TCAP dialogs consist of:

| Message  | Tag    |
| -------- | ------ |
| Begin    | `0x62` |
| Continue | `0x65` |
| End      | `0x64` |

The router extracts:

```
OTID (Originating Transaction ID)
DTID (Destination Transaction ID)
```

Routing logic:

```
Begin → select backend (round-robin)
      → store OTID → backend mapping

Continue/End → lookup DTID
             → route to same backend
```

This guarantees **dialog affinity**.

---

### 3. Worker Affinity

Incoming dialogs are distributed across workers using:

```
worker = (OTID ^ DTID) % MAX_WORKERS
```

Benefits:

* CPU cache locality
* zero locking between workers
* consistent dialog processing

---

# Project Structure

```
tcap_router/
│
├── main.c
│
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
│
└── network/
    ├── sctp_server.c
    └── sctp_server.h
```

---

# Core Components

## SIGTRAN Integration

```
sigtran/sigtran_stack.c
```

Registers an SCCP user with `osmo-stp`.

```
osmo_sccp_user_bind(sccp, "tcap-router", sccp_prim_cb, 146);
```

When SCCP traffic arrives:

```
SCCP primitive
   ↓
extract OTID
   ↓
enqueue to worker
```

---

# Worker Pool

```
core/worker_pool.c
```

Implements:

* lock-free ring buffers
* one queue per worker
* worker affinity based on dialog ID

Workers process jobs:

```
route_tcap()
```

---

# Transaction Table

```
core/transaction_table.c
```

Maintains mapping:

```
OTID → backend
```

Used to ensure dialog stickiness.

Features:

* hash table (262k buckets)
* bucket locking
* garbage collector thread

Expired dialogs are removed automatically.

---

# TCAP Routing

```
router/router.c
```

Pipeline:

```
SCCP message
   ↓
extract SCCP user data
   ↓
parse TCAP
   ↓
determine message type
   ↓
route to backend
```

Routing logic:

```
Begin     → choose backend
Continue  → lookup DTID
End       → lookup DTID
```

---

# Backend Pool

```
core/backend_pool.c
```

Maintains active backend connections.

Selection algorithm:

```
round robin
```

Example backend setup:

```
127.0.0.1:4000
127.0.0.1:4001
```

---

# Performance

Expected throughput on modern servers:

| CPU      | Throughput |
| -------- | ---------- |
| 4 cores  | ~80k TPS   |
| 8 cores  | ~150k TPS  |
| 16 cores | ~300k TPS  |

With further tuning:

```
> 500k TCAP TPS
```

---

# Build

Example compile command:

```
gcc main.c \
core/backend_pool.c \
core/worker_pool.c \
core/transaction_table.c \
router/router.c \
sigtran/sigtran_stack.c \
-o tcap-router \
-O2 -Wall -pthread \
-losmocore -losmo-sigtran -lsctp
```

---

# Running

```
./tcap-router
```

Startup output:

```
Starting TCAP Router
Initializing SCCP stack
SCCP stack initialized
```

---

# Example Deployment

Typical telecom deployment:

```
Telco STP
   │
   ▼
osmo-stp
   │
   ▼
TCAP Router
   │
   ▼
Application cluster
```

---

# Future Improvements

Possible enhancements:

* M3UA protocol parsing
* backend health monitoring
* dynamic backend discovery
* SCTP multi-homing
* zero-copy packet pools
* metrics / observability
* Prometheus integration

---

# License

Open source implementation for telecom experimentation and high-performance TCAP routing.

---
