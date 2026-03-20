# TCAP Router (Dialog-Aware Load Balancer)

## 🚀 Overview

This project implements a **high-performance TCAP dialog-aware load balancer in C**.

It is designed to run:

* **Behind Osmocom STP (`osmo-stp`)**
* **In front of an M3UA adaptor layer**
* Which connects to backend applications

---

## 🧠 Architecture

```
        SS7 Network
             │
      (MTP3 / SIGTRAN)
             │
        ┌─────────────┐
        │  osmo-stp   │
        └─────────────┘
             │
          SCCP
             │
          TCAP
             │
    ┌───────────────────┐
    │   TCAP Router     │
    │ (this project)    │
    └───────────────────┘
             │
        SCCP / TCAP
             │
      ┌────────────────┐
      │  M3UA Adaptor  │
      └────────────────┘
             │
          M3UA/SCTP
             │
    ┌───────────────────┐
    │ Backend Apps      │
    │ (M3UA Clients)    │
    └───────────────────┘
```

---

## ⚠️ Protocol Separation (Important)

* **TCAP Router handles:**

  * SCCP
  * TCAP

* **M3UA Adaptor handles:**

  * M3UA
  * SCTP
  * ASP/AS state

> TCAP itself does NOT speak M3UA.
> The adaptor performs protocol translation between SCCP/TCAP and M3UA.

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

* Encapsulates SCCP into M3UA
* Manages SCTP associations
* Handles ASP/AS state machine
* Interfaces with backend applications

---

### Backend Applications

* Speak **M3UA protocol**
* Handle business logic (MAP, USSD, SMS, etc.)
* Remain stateless (router ensures dialog affinity)

---

## ⚙️ Core Features

| Feature           | Description                                    |
| ----------------- | ---------------------------------------------- |
| Dialog Stickiness | Same TCAP dialog always routed to same backend |
| Load Balancing    | Round-robin for new dialogs                    |
| GT Rewrite        | Replace calling GT with router identity        |
| GT Restore        | Restore original GT on response                |
| Worker Pool       | Parallel processing using dialog hashing       |
| Lock Striping     | Efficient transaction table                    |
| Message Pool      | Reduced allocations                            |
| Epoll-based IO    | Efficient backend handling                     |

---

## 🔄 Message Flow

### 1. STP → Router

* SCCP message received
* TCAP parsed

**BEGIN:**

* Select backend
* Store OTID → backend mapping
* Rewrite Calling GT

**CONTINUE / END:**

* Lookup backend using DTID
* Restore original GT

---

### 2. Router → M3UA Adaptor

* SCCP/TCAP forwarded
* Adaptor converts to M3UA
* Sent over SCTP

---

### 3. Backend → Router

* M3UA → SCCP conversion by adaptor
* Router receives SCCP/TCAP
* Lookup dialog mapping
* Restore GT
* Forward to STP

---

## 🧵 Concurrency Model

* Fixed worker pool (`MAX_WORKERS`)
* Dialog hashing:

```
worker = (OTID or DTID) % MAX_WORKERS
```

### Guarantees

* In-order processing per dialog
* No cross-thread locking
* High throughput

---

## 📦 Components

| File                       | Purpose                     |
| -------------------------- | --------------------------- |
| `main.c`                   | Entry point                 |
| `sigtran/sigtran_stack.c`  | SCCP stack integration      |
| `router/router.c`          | Routing logic               |
| `router/tcap_parser.c`     | TCAP parsing                |
| `router/sccp_gt.c`         | GT handling                 |
| `core/backend_server.c`    | Backend connection handling |
| `core/worker_pool.c`       | Worker threads              |
| `core/transaction_table.c` | Dialog mapping              |
| `core/msg_pool.c`          | Memory pool                 |

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

Backend connections on:

```
Port: 2906
```

---

## ⚠️ Assumptions

* SCCP messages are UDT/XUDT (basic support)
* M3UA adaptor handles full SIGTRAN stack
* Backend apps are stable M3UA clients

---

## 🚧 Limitations

* No full SCCP segmentation support
* Limited TCAP parsing (basic tags)
* No backend health checks
* No congestion control
* Limited observability

---

## 🧭 Future Enhancements

* Metrics (Prometheus)
* Backend health-aware routing
* Weighted load balancing
* Full SCCP support (XUDT, segmentation)
* Full TCAP ASN.1 parsing
* HA deployment (active-active)

---

## 💡 Use Cases

* USSD platforms
* MAP/HLR/HSS routing
* SMSC routing
* SS7 service scaling
* Telecom middleware

---

## 📌 Summary

This project provides:

> A high-performance TCAP routing layer between SS7 signaling (via STP) and M3UA-based backend systems.

It cleanly separates:

* **Signaling transport (M3UA/SCTP)** → handled by adaptor
* **Application routing (TCAP)** → handled by this router

---

## 🧑‍💻 Author

Tahseen Jamal

