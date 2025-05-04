# 🧾 Formal Narrative of the Throttr System Architecture

## 1. General Description

Throttr is a high-performance binary protocol server operating over TCP. It is designed to accept multiple concatenated requests in a single transmission, process them independently, and respond with a consolidated buffer per connection. The architecture is modeled after an industrial processing line, with clearly defined functional roles to ensure high efficiency, overload resilience, and adaptability to variable input flows.

## 2. Operational Model

### 2.1. Receptionist (asynchronous socket reader)

- Always active and ready to receive data.
- Upon receiving incoming data from the client, it appends it to the shared per-connection buffer (buffer_).
- It does not interpret the data. Instead, it stores it and signals the processor (try_process_next()).
- The size of this input buffer defines the physical capacity of the session. If it becomes full, reading is paused—letting the operating system apply natural backpressure.

### 2.2. Dispatcher (segmentation and request decoder)

- Triggered when new data is available.
- Consumes the entire accumulated buffer and segments it into discrete request messages using get_message_size().
- For each segment, it reconstructs a request structure (request_insert, request_query, etc.) using from_buffer() and delegates it to the state component.
- Currently synchronous by design but architecturally prepared for decoupled scaling.

### 2.3. Machine (the state component)

- Processes each request deterministically, producing one binary response per input.
- Each response is enqueued into a per-connection write queue (write_queue_).
- When the first response is enqueued and no write operation is ongoing (writing_ == false), a flag is raised to initiate output.

### 2.4. Opportunistic Worker (response emitter)

- Fetches all queued responses, concatenates them into a single buffer, and transmits it using boost::asio::async_write.

- Upon completion:
  - If more responses are queued, it continues writing. 
  - If the queue is empty, it resets the writing_ flag and returns to idle.

### 3. Critical Considerations and Proposed Enhancements

#### 3.1. Input Buffer Capacity

- Should not be limited arbitrarily.
- The system should behave according to the physical memory of the host.
- Reading should pause when the input buffer is full, allowing for TCP-level backpressure.

#### 3.2. Parser Saturation

- Not a practical concern due to the current low-overhead parser (direct memory cast + 2 memcpy operations).
- Future scaling could delegate the state logic to distributed nodes or specialized backend services.

#### 3.3. Write Worker Synchronization

- An atomic flag or strand must ensure that only one write operation is active per connection.
- The "bulk write" model should be preserved to reduce syscall frequency and improve throughput.

#### 3.4. Response Ordering and Identification

- To eliminate the invisible contention of ordered responses, each request must carry a request_id.
- This request_id will be a uint32_t value included in both request and response headers.
- If global uniqueness or traceability is needed, uint64_t can be adopted. 
- The protocol structure will be updated to include this field in a common base header.

### 4. Proposed Base Header Structure (pending implementation)

```cpp
#pragma pack(push, 1)
struct protocol_header {
    uint8_t request_type;
    uint32_t request_id; // Unique identifier for the message
};
#pragma pack(pop)
```

### 5. Final Notes

- This architecture allows horizontal scaling, burst tolerance, and both ordered and asynchronous interaction depending on configuration.
- The system does not defend against implementer misconfiguration: behavior under saturation is deterministic, not protective.
- Each component is designed to react to events, decouple operations, and minimize structural contention.