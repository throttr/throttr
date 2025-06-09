### v5.0.5

- Now the program options are refactorized (macro usage reduced).
- INFO request missing current timestamp issue fixed.
- Now we have less dependency to connection class and will be used as connection<T>, being T different socket type.
- Now all the transport protocol is inside transport.hpp.
- Now tests doesn't use macros everywhere.
- Now we have event request type to be used on the future event bus.
- Now we have agnostic transport layer.
- Now we have the maximum optimization via -Ofast.
- Now we remove the architecture native instructions and can be enabled via compilation flags.
- Now we have agent connection to the master.
- Now we can broadcast to all connections using PUBLISH *.
- Now we have better testing.
- Now we have unix and tcp connection living in the same context.
- Now we have EVENT request type.
- Now we have several news program parameters.

### v5.0.4

- Now we have AMD64/ARM64 packages.
- Now we have big-endian support on this project side.

### v5.0.3

- Now entry containers are more thread safe.
- Now SET operation over existing key updates the buffer, ttl and ttl type, ideally to make HOT SWAP.
- Now Buffer and Counters are Thread Safe.
- Now we support unix sockets.
- Now we have the project tested using matrix over compilation flags.

### v5.0.2

- Connection now have a message container. This reduces the number of allocations to 1 during the entire life of the connection.
- Now we don't use resize and write_buffer so there are no option to overwrite buffer.

### v5.0.1

- Server now uses protocol v7.0.1 which removes pragma packs.

### v5.0.0

- Server now support protocol v7.0.0.
- Server now doesn't have any visible UB.
- Now we have multiple pipelines building natively on different architectures.
- Now we have LIST, INFO, STAT, STATS, SUBSCRIBE, UNSUBSCRIBE, PUBLISH, CONNECTIONS, CONNECTION, CHANNELS, CHANNEL, WHOAMI requests.

### v4.0.17

- Shared pointer has been removed from container.
- Container now doesn't use expired ordered index.
- Operation are designed to be more stable.
- Now we avoid to copy elements on query and get.
- Process has been tested against multiple 20m insert and queries tests.

### v4.0.16

- Now server can resist huge amount of concurrent operations without SIGSEGV.
- Entry was deprecated, and we now uses Request entry from protocol.hpp.
- Now we have more stable write operation.
- Intrusive pointers has been rolled back.
- Local tests proves 20m requests on 8544ms. 2.34 millions RPS (with existing key).

### v4.0.15

- Now we uses shared_ptr on stored elements.
- Now server can resist 2.5 millions request per seconds using pipelined requests (non-existing key).
- Now we have a more optimized algorithm.

### v4.0.14

- Now we have a security mechanism to avoid UPDATE quota operations to raw. 

### v4.0.13

- Now we have a security mechanism for cross operations (GET to counter or QUERY to raw).

### v4.0.12

- Now throttr server uses protocol 5.0.0 and that's complete compatible with protocol 4.0.1 (doesn't break SDK's).
- Now we have GET and SET requests.
- Now the expiration mechanism is more simple and safe for concurrent operations.
- Now the container is more specialized so:
  - Read operations now only can be done on non-expired keys.
  - Expired keys are marked using container API modify method.
  - Only entries marked as expired will be removed.
- Now the get message size function is more simple to understand.

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