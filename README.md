# 🚀 Throttr

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

## ❓ What is 

**Throttr** is a high-performance TCP server designed to manage and control request rates at the network level with minimal latency.

It implements a custom binary protocol where each client specifies:

- The key. 
- The maximum allowed quota.
- The time window (TTL) and type (ns/ms/s).
- The action: Insert, Query, Update or Purge.

Upon receiving a request, **Throttr**:

- Parses the binary payload efficiently, zero-copy.
- Manages quotas and expirations.
- Responds compactly whether the operation succeeded and the current state.

**Throttr** is engineered to be:

- Minimal.
- Extremely fast.
- Fully asynchronous (non-blocking).
- Sovereign, with no external dependencies beyond Boost.Asio.

## 📜 About Protocol

The full specification of the Throttr binary protocol — including request formats, field types, and usage rules — has been moved to a dedicated repository.

👉 See: https://github.com/throttr/protocol

## 🐳 Running as Container

Pull the latest release:

```bash
docker pull ghcr.io/throttr/throttr:4.0.1-release-UINT16
```

Run it

```bash
docker run -p 9000:9000 ghcr.io/throttr/throttr:4.0.1-release-UINT16
```

Environment variables can also be passed to customize the behavior:

```bash
docker run -e THREADS=4 -p 9000:9000 ghcr.io/throttr/throttr:4.0.1-release-UINT16
```

### 📝 Changelog

### v4.0.1

- Protocol throttr 4.0.1 implemented.
- Now we have containers for better-fit use cases. This limit TTL and Quota.
  - UINT8 to 255.
  - UINT16 to 65'535.
  - UINT32 to 4,294,967,295.
  - UINT64 to 18,446,744,073,709,551,615.

> Which I should use?
> 
> You should analyse your rates... 
> 
> If you have rates like 60 usages per minute. UINT8 fit perfects to you.
> 
> In other hand, if your scale is in bytes, and you're tracking petabytes ... UINT64 is for you ... 
> 
> It's just maths. 

### v4.0.0

- Protocol throttr 4.0.0 implemented.
- Now uses 1 byte for status based requests plus 5 bytes on query.
- Now uses `uint16_t` instead of `uint64_t`. This reduces 6 of 8 bytes on `TTL` and `Quota`.
- Now we have `microseconds`, `minutes` and `hours`. More can be added but need to be tested.
- Now update request will respond `0x00` if the `Value` to be reduced is more than the `Quota`.
- Now `consumer_id` and `resource_id` has been merged to `key`. This is 1 byte reduction.

### v3.0.0

- Protocol throttr 3.0.0 implemented.
- Custom allocation implemented.
- Zero-copy on read and write.
- Zero string usages.

### v2.1.3

- Requests now can be sent on batch.

#### v2.1.2

- Connections are not closed by unhandled cases.
- Pipeline added.

#### v2.1.1

- Threads variable recovered.

#### v2.1.0

- [Protocol](https://github.com/throttr/protocol) is now a external dependency.

#### v2.0.1

- Static link added by default.
- Container now uses `FROM scratch`.

#### v2.0.0

- Redesigned protocol with Consumer ID and Resource ID.
- Added Query, Update and Purge requests.
- Response is now 18 bytes (was 13) and 1 byte for no payload requests.
- 100% Unit Test Coverage.
- Zero Sonar issues.

#### v1.0.0

- Minimum viable product released.

### ⚖️ License

This software has been built by **Ian Torres** and released using [GNU Affero General Public License](./LICENSE).

We can talk, send me something to [iantorres@outlook.com](mailto:iantorres@outlook.com).
