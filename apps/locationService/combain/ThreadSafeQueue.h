#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

// A threadsafe-queue.
template <class T> class ThreadSafeQueue
{
public:
    ThreadSafeQueue(void)
        : q() , m() , c()
    {}

    ~ThreadSafeQueue(void)
    {}

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(this->m);
        this->q.push(t);
        this->c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(this->m);
        this->c.wait(lock, [this]{ return !this->q.empty(); });
        while(this->q.empty())
        {
            // release lock as long as the wait and reaquire it afterwards.
            this->c.wait(lock);
        }
        T val = this->q.front();
        this->q.pop();
        return val;
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};

#endif // THREAD_SAFE_QUEUE_H
