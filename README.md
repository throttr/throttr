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

**Throttr** is a high-performance TCP server designed to deal with [in-memory data objects](https://en.wikipedia.org/wiki/In-memory_database) and [rate limiting](https://en.wikipedia.org/wiki/Rate_limiting) operations with minimal latency.

It implements a custom binary protocol where each client specifies:

- The key. 
- The maximum allowed quota.
- The time window (TTL) and type (ns/ms/s).
- The action: INSERT/SET, QUERY/GET, UPDATE or PURGE.

Upon receiving a request, **Throttr**:

- Parses the binary payload efficiently, zero-copy.
- Manages:
  - Quotas/Buffers and TTL.
- Responds compactly whether the operation succeeded and the current state.

**Throttr** is engineered to be:

- Minimal.
- Extremely fast.
- Fully asynchronous (non-blocking).
- Sovereign, with no external dependencies beyond Boost.Asio.

## 📜 About Protocol

The full specification of the Throttr protocol — including request formats, field types, and usage rules — has been moved to a dedicated repository.

👉 See: https://github.com/throttr/protocol

## Running the distributed binaries

👉 Download your favorite flavour on : https://github.com/throttr/throttr/releases

> See the following sections to understand difference between debug and release or value sizes ...

```
unzip throttr-Debug-UINT16.zip
./throttr --port=9000 --threads=4
```

## 🐳 Running as Container

Pull and run the latest release:

```bash
docker run -p 9000:9000 ghcr.io/throttr/throttr:4.0.17-release-uint16
```

You can get the debug version (contains debugging output)

```bash
docker run -p 9000:9000 ghcr.io/throttr/throttr:4.0.17-debug-uint16
```

Environment variables can also be passed to customize the behavior:

```bash
docker run -e THREADS=4 -p 9000:9000 ghcr.io/throttr/throttr:4.0.17-release-uint16
```

## ¿Debug or Release?

> Which I should use?
>
> If you're on development environments, debug will be helpful to see what is going on behind the scene but not recommended for benchmarks as it uses I/O.

## ¿uint8, uint16, uint32 or uint64?


> Those are unsigned integers (non-negative) sizes. 
> 
> ¿Which is better for me?
> 
> 
> You should analyse your rates and stored data size...
>
> If you have rates like 60 usages per minute. uint8 fit perfects to you.
>
> In other hand, if your scale is in bytes, and you're tracking petabytes ... uint64 is for you ...
> 
> In terms of SET/GET operations, that limit the size of the stored buffer.
> 
> Use the following reference:

| Type   | Max        |
|--------|------------|
| uint8  | 255        |
| uint16 | 65535      |
| uint32 | 4294967295 |
| uint64 | 2^64 - 1   |

> ¿What is the impact if I choose one of them?
> 
> Fields like TTL and Quota will increase the memory and bandwidth usage per transaction.
> 
> It means 1, 3, 5 and 7 bytes of difference per field impacted. In an example:
> 
> INSERT operation use, at least, 3 bytes plus two dynamic fields sizes (Quota and TTL) and I'm not considering the `key` field ... 
> 
> If you use uint8, you'll use 5 bytes but if you use the biggest one (uint64), you'll use 19 bytes only for fixed size fields.

### 📝 Changelog

👉 Moved: [CHANGELOG.md](./CHANGELOG.md)

### ⚖️ License

This software has been built by **Ian Torres** and released using [GNU Affero General Public License](./LICENSE).

We can talk, send me something to [iantorres@outlook.com](mailto:iantorres@outlook.com).
