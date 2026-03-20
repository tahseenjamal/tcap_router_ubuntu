# TCAP Router (Dialog-Aware Load Balancer with M3UA Backend)

## 🚀 Overview

This project implements a **high-performance TCAP dialog-aware load balancer in C**.

It is designed to run **behind Osmocom STP (`osmo-stp`)** and in front of **M3UA-based backend applications (via an adaptor layer)**.

The router provides:

* TCAP dialog stickiness
* Backend load balancing
* SCCP Calling Party GT rewriting
* High-throughput concurrent processing

---

## 🧠 Architecture

```
        SS7 Network
             │
        (SIGTRAN / M3UA)
             │
        ┌─────────────┐
        │  osmo-stp   │
        └─────────────┘
             │
         SCCP/TCAP
             │
    ┌───────────────────┐
    │   TCAP Router     │
    │ (this project)    │
    └───────────────────┘
             │
        M3UA Adaptor
             │
    ┌───────────────────┐
    │ Backend Apps      │
    │ (M3UA Clients)    │
    └───────────────────┘
```

---

## 🎯 Responsibilities

### TCAP Router

* Extract TCAP OTID / DTID
* Maintain dialog → backend mapping
* Perform load balancing for BEGIN dialogs
* Ensure dialog stickiness for CONTINUE/END
* Rewrite SCCP Calling Party GT
* Restore original GT on return path

---

### M3UA Adaptor

* Converts SCCP/TCAP ↔ M3UA
* Handles SCTP associations
* Manages ASP/AS state
* Provides clean interface to backend apps

---

### Backend Applications

* Speak **M3UA protocol**
* Handle business logic (MAP, USSD, SMS, etc.)
* Stateless per message (router ensures session consistency)

---

## ⚙️ Core Features

| Feature           | Description                             |
| ----------------- | --------------------------------------- |
| Dialog Stickiness | Same TCAP dialog routed to same backend |
| Load Balancing    | Round-robin for new dialogs             |
| GT Rewrite        | Replace calling GT with router identity |
| GT Restore        | Restore original GT on response         |
| Worker Pool       | Parallel processing with dialog hashing |
| Lock Striping     | High-performance transaction table      |
| Message Pool      | Reduced memory allocations              |
| Epoll-based IO    | Efficient backend handling              |

---

## 🔄 Message Flow

### 1. STP → Router

* SCCP message received
* TCAP parsed
* If BEGIN:

  * Select backend
  * Store OTID → backend mapping
  * Rewrite Calling GT → SELF_GT
* If CONTINUE/END:

  * Lookup backend via DTID
  * Restore original GT

---

### 2. Router → Backend (via M3UA Adaptor)

* SCCP/TCAP forwarded
* Adaptor converts to M3UA
* Backend processes request

---

### 3. Backend → Router

* Response received via adaptor
* TCAP parsed
* Lookup dialog mapping
* Restore original GT
* Forward to STP

---

## 🧵 Concurrency Model

* Fixed worker pool (`MAX_WORKERS`)
* Dialog-based hashing:

```
worker = (OTID or DTID) % MAX_WORKERS
```

### Guarantees

* In-order processing per dialog
* No cross-thread contention
* High throughput under load

---

## 📦 Components

| File                       | Purpose                     |
| -------------------------- | --------------------------- |
| `main.c`                   | Entry point                 |
| `sigtran/sigtran_stack.c`  | SCCP stack integration      |
| `router/router.c`          | Core routing logic          |
| `router/tcap_parser.c`     | TCAP parsing                |
| `router/sccp_gt.c`         | GT extraction/rewrite       |
| `core/backend_server.c`    | Backend connection handling |
| `core/worker_pool.c`       | Worker pool                 |
| `core/transaction_table.c` | Dialog mapping              |
| `core/msg_pool.c`          | Message memory pool         |

---

## 🔧 Build

### Requirements

* gcc
* libosmocore
* libosmo-sigtran
* libsctp

### Build

```bash
make
```

### Clean

```bash
make clean
```

---

## ▶️ Run

```bash
./tcap-router
```

Backend connections are accepted on:

```
Port: 2906
```

---

## ⚠️ Assumptions

* SCCP messages are primarily UDT/XUDT
* M3UA adaptor handles full SIGTRAN responsibilities
* Backend apps are stable M3UA clients

---

## 🚧 Limitations (Current Version)

* Partial SCCP support (no full segmentation handling)
* Limited TCAP parsing (basic tags only)
* No backend health checks
* No congestion control
* No observability (metrics/logging limited)

---

## 🧭 Future Enhancements

* Prometheus metrics (TPS, latency, queue depth)
* Backend health-aware routing
* Weighted load balancing
* Full SCCP (XUDT, segmentation)
* Full TCAP ASN.1 parsing
* High availability (active-active routers)

---

## 💡 Use Cases

* USSD platforms
* MAP / HLR / HSS services
* SMSC routing
* SS7 service scaling
* SS7 ↔ Diameter interworking

---

## 🧪 Testing

Recommended validation:

* Simulate TCAP load
* Verify:

  * Dialog stickiness
  * GT rewrite correctness
  * Backend routing consistency
  * Failure handling

---

## 📌 Summary

This project provides:

> A high-performance TCAP routing layer between SS7 (via STP) and M3UA-based backend systems.

It cleanly separates:

* SS7 signaling layer (handled by STP + adaptor)
* Application logic (handled by backend systems)

---

## 🧑‍💻 Author

Tahseen Jamal

