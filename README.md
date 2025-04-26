# Throttr

<p align="center"><a href="https://throttr.cl" target="_blank"><img src="./throttr.png" alt="Throttr"></a></p>

<p align="center">
<a href="https://github.com/throttr/throttr/actions/workflows/build.yml"><img src="https://github.com/throttr/throttr/actions/workflows/build.yml/badge.svg" alt="Build"></a>
<a href="https://codecov.io/gh/throttr/throttr"><img src="https://codecov.io/gh/throttr/throttr/graph/badge.svg?token=QCWYBNCJ0T" alt="Coverage"></a>
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=alert_status" alt="Quality Gate"></a>
</p>

<p align="center">
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=bugs" alt="Bugs"></a>
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=vulnerabilities" alt="Vulnerabilities"></a>
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=code_smells" alt="Code Smells"></a>
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=duplicated_lines_density" alt="Duplicated Lines"></a>
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=sqale_index" alt="Technical Debt"></a>
</p>

<p align="center">
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=reliability_rating" alt="Reliability"></a>
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=security_rating" alt="Security"></a>
<a href="https://sonarcloud.io/project/overview?id=throttr_throttr"><img src="https://sonarcloud.io/api/project_badges/measure?project=throttr_throttr&metric=sqale_rating" alt="Maintainability"></a>
</p>

## What is?

**Throttr** is a high-performance TCP server designed to manage and control request rates at the network level.

It implements a custom binary protocol where each client specifies:

Their IP address and port.

- The maximum number of allowed requests.
- The time window (TTL) to consume them.
- The URL associated with the rate limit.

Upon receiving a request, **Throttr**:

- Parses the binary payload efficiently, avoiding unnecessary memory copies.
- Tracks the available request quota and time-to-live per (IP, port, URL) combination.
- Responds with a compact binary structure indicating whether the request is allowed, how many requests are left, and the remaining TTL in milliseconds.

If the TTL expires, **Throttr** automatically resets the state, treating the next request as a fresh, independent cycle.

**Throttr** is engineered to be:

- Minimal.
- Extremely fast.
- Fully asynchronous (non-blocking).
- Sovereign, with no external dependencies beyond Boost.Asio.

It is intended for those who need absolute control over traffic flows, without relying on heavyweight frameworks or third-party infrastructures.

## About Protocol

**Throttr** defines a minimal, efficient binary protocol for communication between clients and the server.

### Request Format

Each client sends a binary payload composed of:

| Field          | Type                      | Size     | Description                                       |
|:---------------|:--------------------------|:---------|:--------------------------------------------------|
| `ip_version`   | `uint8_t`                 | 1 byte   | IP version (4 for IPv4, 6 for IPv6).              |
| `ip`           | `std::array<uint8_t, 16>` | 16 bytes | IP address (IPv4 is stored in the first 4 bytes). |
| `port`         | `uint16_t`                | 2 bytes  | Client port in network byte order.                |
| `max_requests` | `uint32_t`                | 4 bytes  | Maximum number of requests allowed.               |
| `ttl`          | `uint32_t`                | 4 bytes  | Time-to-live window in milliseconds.              |
| `size`         | `uint8_t`                 | 1 byte   | Length of the URL payload.                        |
| `url`          | `char[size_]`             | N bytes  | Target URL associated with the rate limit.        |

After the fixed-size header, the client sends the URL as a simple UTF-8 string of `size_` bytes.

### Response Format

The server responds with a compact 13-byte binary structure:

| Field                | Type      | Size    | Description                                                   |
|:---------------------|:----------|:--------|:--------------------------------------------------------------|
| `can`                | `uint8_t` | 1 byte  | 1 if the request is allowed, 0 otherwise.                     |
| `available_requests` | `int32_t` | 4 bytes | Remaining number of allowed requests.                         |
| `ttl_remaining`      | `int64_t` | 8 bytes | Time left before the current limit expires (in milliseconds). |

### State Management

- When a request is received, Throttr constructs a **unique key** based on the client's IP address, port, and URL.
- It checks the internal state:
    - If the key does not exist, it **creates a new record** using the `max_requests_` and `ttl_` provided.
    - If the key exists:
        - If the TTL has expired, the old record is **deleted**, and a **new cycle** starts with the new parameters.
        - If the TTL is still valid, Throttr decrements the available requests and updates the remaining TTL.
- If there are no available requests left or the TTL has expired, the response indicates that the request is **not allowed**.

All operations are designed to be **fully asynchronous and lock-free** to maximize throughput and minimize latency.

### Design Goals

- **Zero-copy parsing**: Avoid unnecessary memory allocations.
- **Minimal wire protocol**: Small, fixed headers and simple payloads.
- **Statelessness beyond TTL**: Once the TTL expires, the record is discarded.
- **Deterministic behavior**: Every request deterministically maps to an allowed/denied outcome based solely on the protocol rules.

### License

This software has been built by **Ian Torres** and released using [GNU Affero General Public License](./LICENSE).

We can talk, send me something to [iantorres@outlook.com](mailto:iantorres@outlook.com).
