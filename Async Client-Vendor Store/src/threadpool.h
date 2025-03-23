#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

class ThreadPool {
public:
    ThreadPool(int numThreads);
    ~ThreadPool();

    template <typename Function, typename... Args>
    void enqueue(Function&& f, Args&&... args);

private:
    // Threads in the pool
    std::vector<std::thread> workers;
    
    // Task queue
    std::queue<std::function<void()>> tasks;
    
    // Synchronization
    std::mutex queueMutex;
    std::condition_variable condition;
    
    // Flags for stopping threads
    bool stop;
};

ThreadPool::ThreadPool(int numThreads) : stop(false) {
    for (int i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });
                    
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        worker.join();
    }
}

template <typename Function, typename... Args>
void ThreadPool::enqueue(Function&& f, Args&&... args) {
    auto task = std::make_shared<std::function<void()>>(
        std::bind(std::forward<Function>(f), std::forward<Args>(args)...)
    );
    
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        tasks.emplace([task]() {
            (*task)();
        });
    }
    
    condition.notify_one();
}

#endif

// // Example usage:
// void TaskFunction(int id) {
//     std::cout << "Task " << id << " is running in thread " << std::this_thread::get_id() << std::endl;
// }

// int main() {
//     ThreadPool pool(4); // Create a thread pool with 4 threads
    
//     for (int i = 0; i < 8; ++i) {
//         pool.enqueue(TaskFunction, i);
//     }
    
//     // Sleep to allow the tasks to complete
//     std::this_thread::sleep_for(std::chrono::seconds(2));
    
//     return 0;
// }
