### v4.0.11

- Fixed early access without full size check.

### v4.0.10

- Broken read sequence fixed.

### v4.0.9

- Now throttr has been fully tested against Typescript SDK including batching mode.
- Read and write ops uses now a better synchronized algorithm.
- This version can be considered as **staging** but **not recommended** to be used **on production** environments, yet.

### v4.0.8

- Now update TTL makes effects on the schedule side.

### v4.0.7

- No delay added.

### v4.0.6

- Now we have beauty logs for enums.
- TTL type issue fixed.

### v4.0.5

- Pipeline fixed.
- Debug binaries are now published.

### v4.0.4

- Now we support responses on batch.

### v4.0.3

- Now we have prints on connected/disconnected and read/write operations (debug-only).

### v4.0.2

- Now we have fmt to print requests, responses, garbage collector and schedule activity (debug-only).

### v4.0.1

- Protocol throttr 4.0.1 implemented.
- Now we have containers for better-fit use cases. This limit TTL and Quota.
  - UINT8 to 255.
  - UINT16 to 65,535.
  - UINT32 to 4,294,967,295.
  - UINT64 to 18,446,744,073,709,551,615.

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

### v2.1.2

- Connections are not closed by unhandled cases.
- Pipeline added.

### v2.1.1

- Threads variable recovered.

### v2.1.0

- [Protocol](https://github.com/throttr/protocol) is now a external dependency.

### v2.0.1

- Static link added by default.
- Container now uses `FROM scratch`.

### v2.0.0

- Redesigned protocol with Consumer ID and Resource ID.
- Added Query, Update and Purge requests.
- Response is now 18 bytes (was 13) and 1 byte for no payload requests.
- 100% Unit Test Coverage.
- Zero Sonar issues.

### v1.0.0

- Minimum viable product released.