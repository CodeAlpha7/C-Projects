# Multi-threading and gRPC
In any system we have a fixed number of pre-determined threads and effectively utilizing these threads to execute multiple concurrent tasks is handled by the threadpool. Pending tasks wait in queue until a thread is freed an is ready to take up the task.

We implement our own threadpool based on the gRPC store requirements. 

how does a thread work in a threadpool?
we implement a threadpool class in which:
1. we initialize a fixed number of workers

2. When a new task arrives - it is enqueued and the following steps are triggered:
 - check if a thread is free, in the threadpool.
 - if yes, remove from queue and perform the task.

[1]

## BASIC CONSTRUCTS:
1. A vector of threads
2. A queue of tasks
3. multiple threads access the same resource, so have to protect them by using a mutex. These locks are resource agnostic.
4. threads can be put to sleep when not in use and then later woken up by implementing a conditional variable.
5. destroy threads

## MAIN LOGIC:
2 key components: thread workers and queue.

in the store implementation, clients can make requests to the server either synchronously or asynchronously. 
 - synchronous RPC when a client sends a request and blocks until it receives a response
 - asynchronous RPC when client sends a request and receives a callback when the response arrives.
But we mainly do it asynchronously.

The server on the other hand is designed to work asynchronously only. The server can process multiple client requests concurrently using a thread pool and asynchronously handle requests to vendors. This allows the server to be more efficient and handle multiple clients without blocking.


## IMPLEMENTATION:
### 1. Store.cc
 - We implement a traditional gRPC server using asynchronous service methods. 
 - Client requests handled using async gRPC calls.
 - Vendor communication = async gRPC - allows concurrent requests and non-blocking behavior. Simple threadpool used for these concurrent RPC calls.
 - loads vendor server addresses from a file, processes queries async and produces responses to client.

#### Key enablers:
1. **Asynchronous gRPC commmunication:** used between store and vendors. The QueryVendor method in the VendorClient class makes an RPC call to the vendor and retrieves the bid.

2. **Threadpool:** Used in StoreServiceImpl class to process incoming requests asynchronously. For each vendor, equeue task into threadpool. Make RPC call to vendor and store response in the bids vector.

3. **Mutex:** bidsMutex is used to ensure thread safety when updating the bids vector.

4. **Error Handling:** When RPC call fails, set bid to -1. Done in the QueryVendor method. Helps identify failed requests.


### 2. threadpool.h

#### Key enablers:
1. **Thread Creation:** Creating specified number of worker threads in Threapool constructor.

2. **Task Queue:** Contains a queue of tasks that need to be executed by the worker threads. Add new tasks to queue using enqueue function which also notifies one of the sleeping threads.

3. **Mutex:** To access the task queue in a thread-safe manner, the queueMutex has been used. Crucial for multi-threading.

4. **Condition Variable:** This puts threads to sleep when there are no tasks and wakes them up when new tasks are added.

5. **Destructor:** Sets stop flag to notify all flags and joins all of them to ensure all threads finish their work before exiting.


### OTHER APPROACHES:
1. Async gRPC server using async bidirectional streaming - more event-driven. This method uses CallData for each client request and handles them concurrently using gRPC completion queues. (more efficient and scalable handling of concurrent requests).

## Synchronous and Asynchronous gRPC
In this section, we take the time to express how synchronous and asynchronous gRPC calls work in our code. We have implemented both pieces of code and it is no surprise that synchronous calls are much simpler. So, when we call:

~~~
Status status = stub_->QueryVendor(&context, request, &reply);
~~~

This method sends the request to the vendor service and waits for a reply. So, it is a blocking call which waits for the RPC to finish and return its result before it continues execution. For this sake of this project's requirements, we do not want this.

Instead, we want to use the asynchronous gRPC APIs provided by the gRPC library which can typically be more complex to use than their synchronous counterparts. Here, we instead use the following method call:

~~~
CompletionQueue cq;
Status status;

unique_ptr<ClientAsyncResponseReader<VendorReply>> rpc(
stub_->PrepareAsyncQueryVendor(&context, request, &cq)
);

rpc->StartCall();

rpc->Finish(&reply, &status, (void*)1);

void* got_tag;
bool ok = false;

GPR_ASSERT(cq.Next(&got_tag, &ok));
~~~

Here, we are using the **PrepareAsyncQueryVendor** method of the gRPC stub to initiate an asynchronous RPC call. We then call **StartCall** to start the RPC, and **Finish** to request the result when it’s ready. Also, we are using a **CompletionQueue** to manage the asynchronous RPCs. We call **Next** on the completion queue to wait for the next result to be available.



## REFERENCES:
[1] https://www.educba.com/c-plus-plus-thread-pool/ \
[2] https://www.youtube.com/watch?v=6re5U82KwbY (Author: Zen Sepiol)

