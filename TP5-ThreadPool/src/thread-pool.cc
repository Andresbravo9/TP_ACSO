/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
#include <vector>

using namespace std;

ThreadPool::ThreadPool(size_t numThreads) 
    : wts(numThreads), 
      done(false),
      tasksInQueue(0),
      availableWorkers(numThreads),
      pending_tasks(0)
{
    for (size_t i = 0; i < numThreads; i++) {
        wts[i].available = true;
        wts[i].workReady = new Semaphore(0);
        wts[i].ts = thread([this, i] { worker(i); });
    }
    dt = thread([this] { dispatcher(); });
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    {
        lock_guard<mutex> lock(wait_mutex);
        pending_tasks++;
    }

    {
        lock_guard<mutex> lock(queueLock);
        taskQueue.push(thunk);
    }
    
    tasksInQueue.signal();
}

void ThreadPool::wait() {
    unique_lock<mutex> lock(wait_mutex);
    wait_cond.wait(lock, [this] { return pending_tasks == 0; });
}

ThreadPool::~ThreadPool() {
    wait();

    done = true;

    tasksInQueue.signal(); 
    for (size_t i = 0; i < wts.size(); ++i) {
        availableWorkers.signal();
        wts[i].workReady->signal();
    }

    if (dt.joinable()) dt.join();
    for (auto& worker : wts) {
        if (worker.ts.joinable()) worker.ts.join();
        delete worker.workReady;
    }
}

void ThreadPool::dispatcher() {
    while (!done) {
        tasksInQueue.wait();
        if (done) break;

        availableWorkers.wait();
        if (done) break;

        function<void(void)> task;
        {
            lock_guard<mutex> lock(queueLock);
            if (taskQueue.empty()) {
                 availableWorkers.signal();
                 continue;
            }
            task = taskQueue.front();
            taskQueue.pop();
        }

        for (size_t i = 0; i < wts.size(); i++) {
            wts[i].workerMutex.lock();
            if (wts[i].available) {
                wts[i].available = false;
                wts[i].thunk = task;
                wts[i].workerMutex.unlock();
                wts[i].workReady->signal();
                break; 
            }
            wts[i].workerMutex.unlock();
        }
    }
}

void ThreadPool::worker(int id) {
    while (!done) {
        wts[id].workReady->wait();
        if (done) break;

        wts[id].thunk();
        
        {
            lock_guard<mutex> lock(wts[id].workerMutex);
            wts[id].available = true;
        }
        availableWorkers.signal();

        {
            lock_guard<mutex> lock(wait_mutex);
            pending_tasks--;
            if (pending_tasks == 0) {
                wait_cond.notify_all();
            }
        }
    }
}
