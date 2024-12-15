# MapReduce Library Implementation
This project implements a MapReduce framework in C, using a thread pool to execute mapping and reducing tasks concurrently. It utilizes several synchronization primitives for thread safety and organizes data into partitions for efficient processing. Below is an outline of the key components, synchronization primitives, partition management, and testing procedures used in this implementation.

To use: `import "mapreduce.h"`

Example usage presented in `distwc.c`

## Synchronization Primitives
My implementation relies on several synchronization primitives to manage concurrency and ensure thread-safe operations.

### Mutex Locks:
I used this within partitions (pthread_mutex_t lock) to serialize access to each partition's data. This prevents race conditions when multiple threads attempt to read or write data concurrently.
I used this within the thread pool structure to manage access to the job queue and synchronize operations between threads.

### Condition Variables:
I have two of these in my implementation. 
- jobAvailable: Used in the thread pool to notify threads when a new job is added to the queue.
- allIdle: Signals when all threads are idle and no jobs remain in the queue, useful for shutting down the pool safely.

Using these primitives together provides fine-grained control over critical sections within both the partition and thread pool components, allowing for safe concurrent processing.

## Partition Implementation
I implemented partitions using a linked list of key-value pairs, organized within a structure to support synchronized access. Each partition is managed independently and contains the following key elements:

### Linked List
Stores key-value pairs (KeyValueNode) for efficient insertion and retrieval.
### Partition Lock
Each partition has an independent mutex lock (pthread_mutex_t lock) to control access during insertions and modifications.
### Partitioning Logic 
A custom hash function (MR_Partitioner) assigns keys to partitions based on a hashed index, balancing load across partitions.
When a Mapper function emits a <key, value> pair, the key is hashed to determine the appropriate partition. The mutex is so that only one thread can modify a partition at any given time, preventing inconsistencies.

## Testing and Verification
To ensure the functionality and thread-safety of the MapReduce framework, I tested each component, such as the MR_Emit and MR_GetNext functions, individually to confirm proper functionality in isolation. I did this by writing unit tests in a seperate test.c file.
I tested thread pool to make sure jobs are processed in a first-come-first-serve manner and that idle threads signal completion appropriately.

The tests helped troubleshoot and confirmed that key-value pairs are consistently stored in the correct partitions and that access to partitions is serialized as expected.

I tracked memory allocation and deallocation to avoid leaks, especially within the thread pool and partition structures.

## File Overview
- mapreduce.c: Contains the core MapReduce functionality, including partitioning, mapping, reducing, and emitting key-value pairs.
- threadpool.c: Implements the thread pool, managing the queue of jobs and synchronizing threads.
- mapreduce.h and threadpool.h: Header files defining the structures and function prototypes used across the implementation.

