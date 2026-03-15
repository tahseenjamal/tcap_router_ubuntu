
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

# TCAP Router

A lightweight **TCAP routing / load-balancing framework** built on top of the **Osmocom SIGTRAN stack**.
This project focuses on routing **TCAP transactions based on OTID hashing** to backend TCAP application servers.

The goal is to eventually evolve this into a **carrier-grade TCAP router for SIGTRAN networks**.

---

# Environment

Tested on:

| Component    | Version          |
| ------------ | ---------------- |
| OS           | Ubuntu 24.04 LTS |
| GCC          | 13.x             |
| Kernel       | 6.x              |
| Architecture | x86_64           |

---

# High Level Architecture

```
Telco Network
      │
      │ M3UA / SCCP / TCAP
      ▼
┌─────────────────────┐
│ TCAP Router         │
│                     │
│  OTID Hash          │
│  Worker Pool        │
│  Message Dispatch   │
└─────────┬───────────┘
          │
          │ TCP
          ▼
 ┌─────────────┐
 │ Backend #1  │
 ├─────────────┤
 │ Backend #2  │
 ├─────────────┤
 │ Backend #3  │
 └─────────────┘
```

---

# Required Libraries

The router depends on the **Osmocom SIGTRAN stack**.

Required projects:

| Library             | Purpose                    |
| ------------------- | -------------------------- |
| libosmocore         | Core utilities and logging |
| libosmo-netif       | Networking helpers         |
| libosmo-abis        | Osmocom support library    |
| libosmo-sccp        | SCCP stack                 |
| libosmo-sigtran     | M3UA / SUA                 |
| libosmo-m3ua        | M3UA protocol              |
| osmo-stp (optional) | SIGTRAN transfer point     |

---

# Install Base Dependencies

```
sudo apt update

sudo apt install -y \
git \
build-essential \
autoconf \
automake \
libtool \
pkg-config \
libtalloc-dev \
libgnutls28-dev \
libpcap-dev \
libmnl-dev \
libsctp-dev \
libdbus-1-dev \
libsystemd-dev
```

---

# Directory Layout

Recommended build workspace:

```
~/sigtran-build
```

Create directory:

```
mkdir -p ~/sigtran-build
cd ~/sigtran-build
```

---

# Build Osmocom Libraries From Source

Build order is **very important**.

---

# 1. libosmocore

Clone:

```
git clone https://gitea.osmocom.org/osmocom/libosmocore.git
cd libosmocore
```

Build:

```
autoreconf -fi
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

# 2. libosmo-netif

```
cd ~/sigtran-build
git clone https://gitea.osmocom.org/osmocom/libosmo-netif.git
cd libosmo-netif
```

Build:

```
autoreconf -fi
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

# 3. libosmo-abis

```
cd ~/sigtran-build
git clone https://gitea.osmocom.org/osmocom/libosmo-abis.git
cd libosmo-abis
```

Build:

```
autoreconf -fi
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

# 4. libosmo-sccp

```
cd ~/sigtran-build
git clone https://gitea.osmocom.org/osmocom/libosmo-sccp.git
cd libosmo-sccp
```

Build:

```
autoreconf -fi
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

# 5. libosmo-sigtran

```
cd ~/sigtran-build
git clone https://gitea.osmocom.org/osmocom/libosmo-sigtran.git
cd libosmo-sigtran
```

Build:

```
autoreconf -fi
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

# Optional: osmo-stp

If you want to run a **SIGTRAN STP for testing**:

```
cd ~/sigtran-build
git clone https://gitea.osmocom.org/osmocom/osmo-stp.git
cd osmo-stp
```

Build:

```
autoreconf -fi
./configure
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

# Verify Installation

Check installed libraries:

```
ldconfig -p | grep osmo
```

Expected output should show:

```
libosmocore.so
libosmo-sccp.so
libosmo-sigtran.so
```

---

# Build TCAP Router

Clone your repository:

```
git clone https://github.com/tahseenjamal/tcap_router_ubuntu.git
cd tcap_router_ubuntu
```

Compile:

```
gcc -o tcap-router \
main.c \
sigtran/sigtran_stack.c \
core/worker_pool.c \
backend/backend_pool.c \
-losmocore \
-losmo-sccp \
-losmo-sigtran \
-lpthread
```

---

# Run

```
./tcap-router
```

Expected output:

```
Initializing SS7 + SCCP stack
TCAP Router started
Workers initialized
```

---

# Testing

You can simulate TCAP traffic using:

* osmo-stp
* ss7box
* custom TCAP generators

Packet capture:

```
sudo tcpdump -i any port 2905 -w sigtran.pcap
```

---

# Current Features

| Feature                   | Status      |
| ------------------------- | ----------- |
| TCAP OTID hashing         | Implemented |
| Worker thread pool        | Implemented |
| Backend connection pool   | Implemented |
| SCCP stack initialization | Implemented |
| TCAP transaction routing  | Implemented |
| SIGTRAN M3UA handling     | Partial     |
| Health checks             | Pending     |
| Dynamic backend discovery | Pending     |
| Metrics / Prometheus      | Pending     |
| Carrier-grade failover    | Pending     |

---

# Future Work

Planned improvements:

* TCAP dialogue tracking
* backend failover
* congestion control
* SCTP multi-homing
* statistics and monitoring
* carrier-grade SIGTRAN routing

---

# License

MIT License

---

# Author

Tahseen Jamal

