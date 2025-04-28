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

**Throttr** is a high-performance TCP server designed to manage and control request rates at the network level with minimal latency.

It implements a custom binary protocol where each client specifies:

- Their Consumer ID.
- Their Resource ID.
- The maximum allowed quota.
- The time window (TTL) and type (ns/ms/s).
- The action: Insert, Query or Update.

Upon receiving a request, **Throttr**:

- Parses the binary payload efficiently, zero-copy.
- Manages quotas and expirations.
- Responds compactly whether the operation succeeded and the current state.

**Throttr** is engineered to be:

- Minimal.
- Extremely fast.
- Fully asynchronous (non-blocking).
- Sovereign, with no external dependencies beyond Boost.Asio.

## About Protocol

**Throttr** defines a minimal, efficient binary protocol based on three main request types:

- Insert Request
- Query Request
- Update Request

### Insert Request Format

| Field              | Type       | Size    | Description                        |
|:-------------------|:-----------|:--------|:-----------------------------------|
| `request_type`     | `uint8_t`  | 1 byte  | Always 0x01 for insert.            |
| `quota`            | `uint64_t` | 8 bytes | Maximum number of allowed actions. |
| `usage`            | `uint64_t` | 8 bytes | Initial usage.                     |
| `ttl_type`         | `uint8_t`  | 1 byte  | 0 = ns, 1 = ms, 2 = s.             |
| `ttl`              | `uint64_t` | 8 bytes | Time to live value.                |
| `consumer_id_size` | `uint8_t`  | 1 byte  | Size of Consumer ID.               |
| `resource_id_size` | `uint8_t`  | 1 byte  | Size of Resource ID.               |
| `consumer_id`      | `char[N]`  | N bytes | Consumer identifier.               |
| `resource_id`      | `char[M]`  | M bytes | Resource identifier.               |

### Query and Purge Request Format

| Field              | Type      | Size    | Description                       |
|:-------------------|:----------|:--------|:----------------------------------|
| `request_type`     | `uint8_t` | 1 byte  | 0x02 for query and 0x04 on purge. |
| `consumer_id_size` | `uint8_t` | 1 byte  | Size of Consumer ID.              |
| `resource_id_size` | `uint8_t` | 1 byte  | Size of Resource ID.              |
| `consumer_id`      | `char[N]` | N bytes | Consumer identifier.              |
| `resource_id`      | `char[M]` | M bytes | Resource identifier.              |

### Update Request Format

| Field              | Type       | Size    | Description                             |
|:-------------------|:-----------|:--------|:----------------------------------------|
| `request_type`     | `uint8_t`  | 1 byte  | Always 0x03 for update.                 |
| `attribute`        | `uint8_t`  | 1 byte  | 0 = quota, 1 = ttl.                     |
| `change`           | `uint8_t`  | 1 byte  | 0 = patch, 1 = increase, 2 = decrease.  |
| `value`            | `uint64_t` | 8 bytes | Value to apply according to the change. |
| `consumer_id_size` | `uint8_t`  | 1 byte  | Size of Consumer ID.                    |
| `resource_id_size` | `uint8_t`  | 1 byte  | Size of Resource ID.                    |
| `consumer_id`      | `char[N]`  | N bytes | Consumer identifier.                    |
| `resource_id`      | `char[M]`  | M bytes | Resource identifier.                    |

### Response Format

Server responds always with 18 bytes for Insert and Query but 1 byte when you're Updating or Purging:

| Field             | Type       | Size    | Description                                  |
|:------------------|:-----------|:--------|:---------------------------------------------|
| `can`             | `uint8_t`  | 1 byte  | 1 if successful, 0 otherwise.                |
| `quota_remaining` | `uint64_t` | 8 bytes | Remaining available quota.                   |
| `ttl_remaining`   | `int64_t`  | 8 bytes | Remaining TTL (ns/ms/s according to config). |

## Running as Container

Pull the latest release:

```bash
docker pull ghcr.io/throttr/throttr:2.0.0-release
```

Run it

```bash
docker run -p 9000:9000 ghcr.io/throttr/throttr:2.0.0-release
```

Environment variables can also be passed to customize the behavior:

```bash
docker run -e THREADS=4 -p 9000:9000 ghcr.io/throttr/throttr:2.0.0-release
```

### Changelog

#### v2.0.0

- Redesigned protocol with Consumer ID and Resource ID.
- Added Query, Update and Purge requests.
- Response is now 18 bytes (was 13) and 1 byte for no payload requests.
- 100% Unit Test Coverage.
- Zero Sonar issues.

#### v1.0.0

- Minimum viable product released.

### License

This software has been built by **Ian Torres** and released using [GNU Affero General Public License](./LICENSE).

We can talk, send me something to [iantorres@outlook.com](mailto:iantorres@outlook.com).
