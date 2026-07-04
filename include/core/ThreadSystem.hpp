/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 *
 * Thread System: Handles task scheduling, thread pooling and task
 * prioritization
 */

#ifndef THREAD_SYSTEM_HPP
#define THREAD_SYSTEM_HPP

#include "Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <format>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>

// Platform-specific includes for thread naming
#if defined(__linux__) || defined(__APPLE__) || defined(_GNU_SOURCE)
#include <pthread.h>
#endif

namespace VoidLight {

// Task priority levels
enum class TaskPriority : uint8_t {
  Critical = 0, // Must execute ASAP (e.g., rendering, input handling)
  High = 1,     // Important tasks (e.g., physics, animation)
  Normal = 2,   // Default priority for most tasks
  Low = 3,      // Background tasks (e.g., asset loading)
  Idle = 4      // Only execute when nothing else is pending
};

// Task wrapper with priority information
struct PrioritizedTask {
  std::function<void()> task;
  TaskPriority priority;
  std::chrono::steady_clock::time_point enqueueTime;
  std::string description;

  // Default constructor
  PrioritizedTask()
      : priority(TaskPriority::Normal),
        enqueueTime(std::chrono::steady_clock::now()) {}

  // Constructor
  PrioritizedTask(std::function<void()> t, TaskPriority p,
                  std::string desc = "")
      : task(std::move(t)), priority(p),
        enqueueTime(std::chrono::steady_clock::now()),
        description(std::move(desc)) {}

  // Comparison operator for priority queue
  bool operator<(const PrioritizedTask &other) const {
    // Higher priority (lower enum value) comes first
    if (priority != other.priority) {
      return priority > other.priority;
    }
    // If same priority, older tasks come first (FIFO)
    return enqueueTime > other.enqueueTime;
  }
};

/**
 * @brief Thread-safe prioritized task queue using separate queues per priority
 *
 * This class provides a thread-safe queue for tasks to be executed by
 * the worker threads. Uses separate queues for each priority level to
 * reduce lock contention and improve performance.
 *
 * The queues automatically grow as needed, but can also have capacity
 * reserved in advance for better performance when submitting large
 * numbers of tasks at once.
 */
// Forward declaration for work-stealing and budget integration
struct WorkerBudget;

class TaskQueue {

public:
  /**
   * @brief Construct a new Task Queue
   *
   * @param initialCapacity Initial capacity to reserve per priority (default:
   * 256)
   * @param enableProfiling Enable detailed task profiling (default: false)
   */
  explicit TaskQueue(size_t initialCapacity = 256, bool enableProfiling = false)
      : m_desiredCapacity(initialCapacity), m_enableProfiling(enableProfiling) {

    // Initialize atomic counters and realistic capacity distribution
    for (int i = 0; i <= static_cast<int>(TaskPriority::Idle); ++i) {
      m_priorityCounts[i].count.store(0, std::memory_order_relaxed);
      m_taskStats[i] = {0, 0, 0};
    }
  }

  void push(std::function<void()> task,
            TaskPriority priority = TaskPriority::Normal,
            const std::string &description = "") {
    int const priorityIndex = static_cast<int>(priority);

    // Update last enqueue time for low-activity detection
    m_lastEnqueueTime.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);

    {
      std::unique_lock<std::mutex> lock(m_priorityMutexes[priorityIndex]);

      // Add the new task (deque handles capacity automatically)
      m_priorityQueues[priorityIndex].emplace_back(std::move(task), priority,
                                                   description);

      // Update atomic counter
      m_priorityCounts[priorityIndex].count.fetch_add(1, std::memory_order_relaxed);

      // Set bitmask bit to indicate this queue has tasks
      m_queueBitmask.fetch_or(1 << priorityIndex, std::memory_order_relaxed);

      // Update statistics
      m_taskStats[priorityIndex].enqueued++;
      m_totalTasksEnqueued.fetch_add(1, std::memory_order_relaxed);

      // If profiling is enabled and this is a high priority task, log it
      if (m_enableProfiling && priority <= TaskPriority::High &&
          !description.empty()) {
        THREADSYSTEM_INFO(std::format("High priority task enqueued: {} (Priority: {})",
                                      description, priorityIndex));
      }
    }

    // Smart notification: notify all for critical, otherwise notify one.
    std::lock_guard<std::mutex> lock(queueMutex);
    if (priority == TaskPriority::Critical) {
      condition.notify_all(); // Wake all for critical tasks to ensure immediate pickup.
    } else {
      condition.notify_one(); // Wake one for all other tasks to prevent a thundering herd.
    }
  }

  /**
   * @brief Batch enqueue multiple tasks with a single lock acquisition
   *
   * This method is highly optimized for scenarios where many tasks need to be
   * submitted at once (e.g., AI entity updates, particle batches). It reduces
   * lock contention from O(N) to O(1) by acquiring the mutex only once.
   *
   * @param tasks Vector of tasks to enqueue (will be moved from)
   * @param priority Priority level for all tasks in the batch
   * @param description Optional description prefix for debugging
   */
  void batchPush(std::vector<std::function<void()>>& tasks,
                 TaskPriority priority = TaskPriority::Normal,
                 const std::string& description = "") {
    if (tasks.empty()) {
      return;
    }

    // Update last enqueue time for low-activity detection
    m_lastEnqueueTime.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);

    int priorityIndex = static_cast<int>(priority);
    size_t batchSize = tasks.size();

    {
      std::unique_lock<std::mutex> lock(m_priorityMutexes[priorityIndex]);

      // Reserve space to avoid reallocations (deque doesn't have reserve, but this is good practice)
      for (auto& task : tasks) {
        m_priorityQueues[priorityIndex].emplace_back(std::move(task), priority, description);
      }

      // Update atomic counter once for entire batch
      m_priorityCounts[priorityIndex].count.fetch_add(batchSize, std::memory_order_relaxed);

      // Set bitmask bit to indicate this queue has tasks
      m_queueBitmask.fetch_or(1 << priorityIndex, std::memory_order_relaxed);

      // Update statistics
      m_taskStats[priorityIndex].enqueued += batchSize;
      m_totalTasksEnqueued.fetch_add(batchSize, std::memory_order_relaxed);

      // Log batch submission if profiling is enabled
      if (m_enableProfiling && !description.empty()) {
        THREADSYSTEM_INFO(std::format("Batch enqueued {} tasks: {} (Priority: {})",
                                      batchSize, description, priorityIndex));
      }
    }

    // Efficient wake strategy: minimize thundering herd while ensuring work gets picked up
    std::lock_guard<std::mutex> lock(queueMutex);
    if (priority == TaskPriority::Critical) {
      // Critical tasks need immediate attention from all workers
      condition.notify_all();
    } else if (batchSize >= 16) {
      // Large batches: wake all workers to distribute load
      condition.notify_all();
    } else {
      // Small batches: single notification reduces CPU wake overhead
      // Workers will naturally pick up remaining tasks as they complete current work
      condition.notify_one();
    }
  }

  bool pop(std::function<void()> &task) {
    std::unique_lock<std::mutex> lock(queueMutex);

    // Wait indefinitely for tasks - notify_one/notify_all will wake us instantly when tasks arrive
    condition.wait(lock, [this] {
      return stopping.load(std::memory_order_acquire) || hasAnyTasksLockFree();
    });

    if (stopping.load(std::memory_order_acquire)) {
      return false;
    }

    lock.unlock();
    return tryPopTask(task);
  }

  void stop() {
    stopping.store(true, std::memory_order_release);
    notifyAllThreads(); // Wake up all threads to exit

    // Clear queues WITHOUT holding queueMutex to avoid deadlock
    // Workers need queueMutex to check stopping flag during condition.wait()
    for (int i = 0; i <= static_cast<int>(TaskPriority::Idle); ++i) {
      std::lock_guard<std::mutex> priorityLock(m_priorityMutexes[i]);
      m_priorityQueues[i].clear();
      m_priorityCounts[i].count.store(0, std::memory_order_relaxed);
    }
    // Clear all bitmask bits
    m_queueBitmask.store(0, std::memory_order_relaxed);

    // Wake again after clearing to ensure workers see empty queues
    notifyAllThreads();
  }

  bool isEmpty() const {
    // Use atomic counters for lock-free checking
    constexpr int maxPriority = static_cast<int>(TaskPriority::Idle);
    return !std::any_of(m_priorityCounts.begin(), m_priorityCounts.begin() + maxPriority + 1,
                        [](const auto& counter) {
                          return counter.count.load(std::memory_order_relaxed) > 0;
                        });
  }

  // Directly check if stopping without acquiring lock
  bool isStopping() const { return stopping.load(std::memory_order_acquire); }

  // Reserve capacity for all priority queues to reduce memory reallocations
  void reserve(size_t capacity) {
    // Only proceed if we're actually increasing capacity
    if (capacity <= m_desiredCapacity) {
      return;
    }

    // Note: std::deque doesn't have reserve(), but we track desired capacity
    m_desiredCapacity = capacity;

    if (m_enableProfiling) {
      THREADSYSTEM_INFO(std::format("Task queue capacity manually set to {} (deques grow automatically)", capacity));
    }
  }

  // Get the current capacity of the task queue
  size_t capacity() const {
    // Return our tracked desired capacity
    return m_desiredCapacity;
  }

  // Get the current size of all task queues combined
  size_t size() const {
    constexpr int maxPriority = static_cast<int>(TaskPriority::Idle);
    return std::accumulate(m_priorityCounts.begin(), m_priorityCounts.begin() + maxPriority + 1,
                           size_t{0},
                           [](size_t sum, const auto& counter) {
                             return sum + counter.count.load(std::memory_order_relaxed);
                           });
  }

  // Enable or disable profiling
  void setProfilingEnabled(bool enabled) { m_enableProfiling = enabled; }

  // Get task statistics
  struct TaskStats {
    size_t enqueued{0};
    size_t completed{0};
    size_t totalWaitTimeMs{0};

    double getAverageWaitTimeMs() const {
      return completed > 0 ? static_cast<double>(totalWaitTimeMs) / completed
                           : 0.0;
    }
  };

  // Get statistics for a specific priority level
  TaskStats getTaskStats(TaskPriority priority) const {
    int const index = static_cast<int>(priority);
    if (index >= 0 && index <= static_cast<int>(TaskPriority::Idle)) {
      return m_taskStats[index];
    }
    return TaskStats{};
  }

  // Get total tasks processed and enqueued
  size_t getTotalTasksProcessed() const {
    return m_totalTasksProcessed.load(std::memory_order_relaxed);
  }

  size_t getTotalTasksEnqueued() const {
    return m_totalTasksEnqueued.load(std::memory_order_relaxed);
  }

  // Public accessors for condition variable and mutex
  std::condition_variable& getCondition() { return condition; }
  std::mutex& getMutex() { return queueMutex; }

  bool hasTasks() const {
    return hasAnyTasksLockFree();
  }

  // Get milliseconds since last task was enqueued (for low-activity detection)
  int64_t getTimeSinceLastEnqueue() const {
    auto now = std::chrono::steady_clock::now();
    auto lastEnqueue = m_lastEnqueueTime.load(std::memory_order_relaxed);
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEnqueue).count();
  }

private:
  // Separate deques for each priority level (O(1) pop_front, reduces lock
  // contention)
  mutable std::array<std::deque<PrioritizedTask>, 5> m_priorityQueues{};

  // Cache-line aligned mutexes to prevent false sharing
  alignas(64) mutable std::array<std::mutex, 5> m_priorityMutexes{};

  // Cache-line aligned atomic counters to prevent false sharing
  struct alignas(64) AlignedAtomic {
    std::atomic<size_t> count{0};
  };
  mutable std::array<AlignedAtomic, 5> m_priorityCounts{};

  // Bitmask tracking non-empty queues for fast skip in tryPopTask
  std::atomic<uint8_t> m_queueBitmask{0};

  mutable std::mutex queueMutex{}; // Main mutex for condition variable
  std::condition_variable condition{};
  std::atomic<bool> stopping{false};

  // Statistics tracking with cache-friendly array instead of map
  mutable std::array<TaskStats, 5> m_taskStats{};
  std::atomic<size_t> m_totalTasksProcessed{0};
  std::atomic<size_t> m_totalTasksEnqueued{0};

  size_t m_desiredCapacity{256}; // Track desired capacity ourselves
  bool m_enableProfiling{false}; // Enable detailed performance metrics

  // Track last time a task was enqueued for low-activity detection
  std::atomic<std::chrono::steady_clock::time_point> m_lastEnqueueTime{std::chrono::steady_clock::now()};

  // Lock-free check for any tasks using bitmask (O(1) single atomic load)
  bool hasAnyTasksLockFree() const {
    return m_queueBitmask.load(std::memory_order_relaxed) != 0;
  }

  // Try to pop a task without blocking
  bool tryPopTask(std::function<void()> &task) {
    // Fast-path: Check bitmask to skip empty queues
    uint8_t bitmask = m_queueBitmask.load(std::memory_order_relaxed);

    // Try to get task from highest priority queues first
    for (int priorityIndex = 0;
         priorityIndex <= static_cast<int>(TaskPriority::Idle);
         ++priorityIndex) {

      // Skip this priority level if bitmask indicates it's empty
      if (!(bitmask & (1 << priorityIndex))) {
        continue;
      }

      std::unique_lock<std::mutex> priorityLock(
          m_priorityMutexes[priorityIndex], std::try_to_lock);
      if (!priorityLock.owns_lock()) {
        continue; // Skip if we can't get the lock immediately
      }

      auto &queue = m_priorityQueues[priorityIndex];
      if (!queue.empty()) {
        // Get the oldest task from this priority level (FIFO within priority)
        PrioritizedTask prioritizedTask = std::move(queue.front());
        queue.pop_front(); // O(1) operation with deque

        // Update atomic counter
        size_t newCount = m_priorityCounts[priorityIndex].count.fetch_sub(1, std::memory_order_relaxed) - 1;

        // Clear bitmask bit if queue is now empty
        if (newCount == 0) {
          m_queueBitmask.fetch_and(~(1 << priorityIndex), std::memory_order_relaxed);
        }

        // Calculate wait time for metrics
        auto now = std::chrono::steady_clock::now();
        auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - prioritizedTask.enqueueTime)
                            .count();

        // Update statistics if profiling is enabled
        if (m_enableProfiling) {
          m_taskStats[priorityIndex].completed++;
          m_taskStats[priorityIndex].totalWaitTimeMs += waitTime;

          // Log long wait times for high priority tasks
          if (priorityIndex <= static_cast<int>(TaskPriority::High) &&
              waitTime > 100 && !prioritizedTask.description.empty()) {
            THREADSYSTEM_WARN(std::format("High priority task delayed: {} waited {}ms",
                                          prioritizedTask.description, waitTime));
          }
        }

        // Return the actual task
        task = std::move(prioritizedTask.task);
        m_totalTasksProcessed.fetch_add(1, std::memory_order_relaxed);
        return true;
      }
    }
    return false;
  }

public:
  // Wake up all waiting threads without clearing the queue
  void notifyAllThreads() {
    std::lock_guard<std::mutex> lock(queueMutex);
    condition.notify_all();
  }

};

// Thread pool for managing worker threads
// WorkerBudget-aware work-stealing queue for fair task distribution

class ThreadPool {

public:
  /**
   * @brief Construct a new Thread Pool object
   *
   * @param numThreads Number of worker threads to create
   * @param queueCapacity Capacity of the task queue (default: 256)
   * @param enableProfiling Enable detailed performance profiling (default:
   * false)
   */
  explicit ThreadPool(size_t numThreads, size_t queueCapacity = 256,
                      bool enableProfiling = false)
      : taskQueue(queueCapacity, enableProfiling) {

    // Work stealing removed for simplicity and reliability

    // Set up worker threads
    m_workers.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
      m_workers.emplace_back([this, i] {
// Set thread name for debugging (no CPU affinity - let OS scheduler optimize)
#if defined(__linux__) || defined(_GNU_SOURCE)
        // Linux: Set thread name
        std::string threadName = std::format("Worker-{}", i);
        pthread_setname_np(pthread_self(), threadName.c_str());
#elif defined(__APPLE__)
        // macOS: Set thread name
        std::string threadName = std::format("Worker-{}", i);
        pthread_setname_np(threadName.c_str());
#endif
        // Note: CPU affinity removed for better OS-level load balancing and efficiency

        // Run the worker
        workerThread(i);
      });
    }

    if (enableProfiling) {
      THREADSYSTEM_INFO(std::format("Thread pool created with {} threads, simple queue-based threading, and profiling enabled",
                                    numThreads));
    }
  }

  ~ThreadPool() {
    // Signal all threads to stop and wake them up
    isRunning.store(false, std::memory_order_release);
    taskQueue.stop(); // This will notify all threads

    // Join all worker threads
    for (auto &worker : m_workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    THREADSYSTEM_INFO("ThreadPool shutdown completed");
  }

  /**
   * @brief Enqueue a task with specified priority
   *
   * @param task The task to execute
   * @param priority The priority level (default: Normal)
   * @param description Optional description for debugging
   */
  void enqueue(std::function<void()> task,
               TaskPriority priority = TaskPriority::Normal,
               const std::string &description = "") {
    // Simple single-queue design - all tasks go to global queue
    taskQueue.push(std::move(task), priority, description);
    // Update comprehensive statistics for all tasks
    m_totalTasksEnqueued.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * @brief Batch enqueue multiple tasks with optimized single lock acquisition
   *
   * Significantly reduces lock contention when submitting multiple tasks at once.
   * Ideal for AI updates, particle systems, and event processing batches.
   *
   * @param tasks Vector of tasks to enqueue (will be moved from)
   * @param priority Priority level for all tasks in the batch (default: Normal)
   * @param description Optional description prefix for debugging
   */
  void batchEnqueue(std::vector<std::function<void()>>& tasks,
                    TaskPriority priority = TaskPriority::Normal,
                    const std::string& description = "") {
    if (tasks.empty()) {
      return;
    }

    size_t batchSize = tasks.size();
    taskQueue.batchPush(tasks, priority, description);

    // Update comprehensive statistics for batch
    m_totalTasksEnqueued.fetch_add(batchSize, std::memory_order_relaxed);
  }

  bool busy() const {
    // Simple design - check global queue and active tasks
    if (!taskQueue.isEmpty()) {
      return true;
    }

    // Check if any worker threads are actively processing
    return m_activeTasks.load(std::memory_order_relaxed) > 0;
  }

  // Access the task queue for capacity management
  TaskQueue &getTaskQueue() { return taskQueue; }

  const TaskQueue &getTaskQueue() const { return taskQueue; }

  // Comprehensive task statistics (global + worker queues)
  size_t getTotalTasksEnqueued() const {
    return m_totalTasksEnqueued.load(std::memory_order_relaxed);
  }

  size_t getTotalTasksProcessed() const {
    return m_totalTasksProcessed.load(std::memory_order_relaxed);
  }

  /**
   * @brief Enqueue a task that returns a result with specified priority
   *
   * @param f The function to execute
   * @param priority The priority level (default: Normal)
   * @param description Optional description for debugging
   * @param args Function arguments
   * @return A future containing the result
   */
  template <class F, class... Args>
  auto enqueueWithResult(F &&f, TaskPriority priority = TaskPriority::Normal,
                         const std::string &description = "", Args &&...args)
      -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> result = task->get_future();
    enqueue([task]() { (*task)(); }, priority, description);
    return result;
  }

private:
  std::vector<std::thread> m_workers; // Thread worker pool
  TaskQueue taskQueue; // Global priority queue for high/critical tasks
  std::atomic<bool> isRunning{true};
  mutable std::atomic<size_t> m_activeTasks{0}; // Track actively running tasks
  mutable std::mutex m_mutex{}; // For thread-safe access to members

  // Simple reliable threading - unused fields removed

  // Comprehensive task statistics tracking
  std::atomic<size_t> m_totalTasksEnqueued{
      0}; // All tasks (global + worker queues)
  std::atomic<size_t> m_totalTasksProcessed{0}; // All tasks processed

  void workerThread(size_t threadIndex = 0) {
    std::function<void()> task;

    // Statistics tracking — feeds Logger calls only, negligible per-thread-lifetime cost
    auto startTime = std::chrono::steady_clock::now();
    size_t tasksProcessed = 0;
    size_t highPriorityTasks = 0;

    // Set thread as interruptible (platform-specific if needed)
    try {
      // For idle tracking and logging (scoped to try block)
      auto lastTaskTime = std::chrono::steady_clock::now();
      std::chrono::steady_clock::time_point idleStartTime;
      bool isIdle = false;
      // Minimum idle time before logging (20 seconds) - only log truly idle states
      constexpr int64_t MIN_IDLE_TIME_MS = 20000;
      constexpr int64_t MIN_IDLE_EXIT_LOG_MS = 100;

      // Main worker loop
      while (isRunning.load(std::memory_order_acquire)) {
        // Check for shutdown immediately at loop start
        if (!isRunning.load(std::memory_order_acquire)) {
          break;
        }

        // Reset gotTask at the start of each iteration
        bool gotTask = false;

        try {
          // WorkerBudget-aware task acquisition priority order:
          // 1. Global queue for high/critical priority tasks (WorkerBudget
          // engine/urgent tasks)
          if (taskQueue.pop(task)) {
            gotTask = true;
            highPriorityTasks++;
            lastTaskTime = std::chrono::steady_clock::now();
          }
          // All tasks go through single global queue - simple and reliable
        } catch (...) {
          // If any exception occurs during pop, check shutdown
          if (!isRunning.load(std::memory_order_acquire)) {
            break;
          }
          continue;
        }

        // Check for shutdown again after getting task
        if (!isRunning.load(std::memory_order_acquire)) {
          break;
        }

        if (gotTask) {
          // Exiting idle mode - log if we were previously idle for a meaningful duration
          if (isIdle) {
            auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - idleStartTime).count();
            if (idleTime >= MIN_IDLE_EXIT_LOG_MS) {
              THREADSYSTEM_INFO(std::format("Worker {} exiting idle mode (was idle for {}ms)",
                                            threadIndex, idleTime));
            }
            isIdle = false;
          }

          // Track active task count
          m_activeTasks.fetch_add(1, std::memory_order_relaxed);

          // Track execution time for profiling
          auto taskStartTime = std::chrono::steady_clock::now();

          try {
            // Execute the task
            task();
            tasksProcessed++;

            // Update comprehensive statistics
            m_totalTasksProcessed.fetch_add(1, std::memory_order_relaxed);
          } catch (const std::exception &e) {
            THREADSYSTEM_ERROR(std::format("Error in worker thread {}: {}", threadIndex, e.what()));
          } catch (...) {
            THREADSYSTEM_ERROR(std::format("Unknown error in worker thread {}", threadIndex));
          }

          // Decrement with relaxed ordering - order doesn't matter for simple
          // counting
          m_activeTasks.fetch_sub(1, std::memory_order_relaxed);

          // Track execution time
          auto taskEndTime = std::chrono::steady_clock::now();
          auto taskDuration =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  taskEndTime - taskStartTime)
                  .count();

          // Log slow tasks if they exceed 100ms (truly problematic tasks)
          if (taskDuration > 100) {
            THREADSYSTEM_WARN(std::format("Worker {} - Slow task: {}ms{}",
                                          threadIndex, taskDuration,
                                          (highPriorityTasks > 0 ? " (HIGH PRIORITY)" : "")));
          }

          // Clear task after execution to free resources
          task = nullptr;
        } else {
          // No task available - only mark as idle and log if we've been without tasks long enough
          if (!isIdle) {
            auto timeSinceLastTask = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - lastTaskTime).count();

            // Only consider it "idle" if we've been without tasks for at least MIN_IDLE_TIME_MS
            if (timeSinceLastTask >= MIN_IDLE_TIME_MS) {
              THREADSYSTEM_INFO(std::format("Worker {} entering idle mode (no tasks for {}ms)",
                                            threadIndex, timeSinceLastTask));
              isIdle = true;
              idleStartTime = std::chrono::steady_clock::now();
            }
          }
          // Worker will loop back and block in pop() until a task arrives
        }
      }
    } catch (const std::exception &e) {
      THREADSYSTEM_ERROR(std::format("Worker thread {} terminated with exception: {}",
                                     threadIndex, e.what()));
    } catch (...) {
      THREADSYSTEM_ERROR(std::format("Worker thread {} terminated with unknown exception",
                                     threadIndex));
    }

    auto endTime = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                             endTime - startTime)
                             .count();
    THREADSYSTEM_INFO(std::format("Worker {} exiting after processing {} tasks over {}ms",
                                  threadIndex, tasksProcessed, totalDuration));
  }
};

// Singleton Thread System Manager
class ThreadSystem {

public:
  // Task queue settings
  static constexpr size_t DEFAULT_QUEUE_CAPACITY = 4096;

  // Timeout settings
  static constexpr int DEFAULT_SHUTDOWN_TIMEOUT_MS = 5000; // 5 seconds
  static constexpr int DEFAULT_TASK_TIMEOUT_MS = 30000;    // 30 seconds

  static ThreadSystem &Instance() {
    static ThreadSystem instance;
    return instance;
  }

  /**
   * @brief Check if the ThreadSystem has been initialized
   * @return True if the ThreadSystem has been initialized, false otherwise
   */
  static bool Exists() {
    return !Instance().m_isShutdown.load(std::memory_order_acquire);
  }

  void clean() {
    THREADSYSTEM_INFO("ThreadSystem resources cleaned!");

    // Set shutdown flag first so any new accesses will be rejected
    m_isShutdown.store(true, std::memory_order_release);
    // Ensure visibility across all threads
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (m_threadPool) {
      // First signal the pool to stop accepting new tasks
      // We don't actually need to wait for pending tasks to complete here
      m_threadPool->getTaskQueue().notifyAllThreads();

      // Allow a very brief delay for threads to notice shutdown signal
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      // Log the number of pending tasks
      size_t pendingTasks = m_threadPool->getTaskQueue().size();
      if (pendingTasks > 0) {
        THREADSYSTEM_INFO(std::format("Canceling {} pending tasks during shutdown...",
                                      pendingTasks));
      }

      // Reset the thread pool - this will trigger its destructor
      // which handles thread cleanup gracefully
      m_threadPool.reset();

      // Add a small delay to allow any final thread messages to print
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      THREADSYSTEM_INFO("Thread pool successfully shut down");
    }
  }

  ~ThreadSystem() {
    if (!m_isShutdown) {
      clean();
    }
  }

  /**
   * @brief Initialize the ThreadSystem
   *
   * This method initializes the thread pool with an optimal number of worker
   * threads based on the hardware and a default task queue capacity.
   * After initialization, the task queue can grow dynamically as needed.
   *
   * @param queueCapacity Initial capacity for the task queue (default: 1024)
   * @param customThreadCount Optional parameter to specify exact thread count
   * (0 for auto-detect)
   * @param enableProfiling Enable detailed task profiling (default: false)
   * @return true if initialization succeeded, false otherwise
   */
  [[nodiscard]] bool init(size_t queueCapacity = DEFAULT_QUEUE_CAPACITY,
            unsigned int customThreadCount = 0, bool enableProfiling = false) {
    // If already shutdown, don't allow re-initialization
    if (m_isShutdown.load(std::memory_order_acquire)) {
      if (m_enableDebugLogging) {
        THREADSYSTEM_WARN(
            "ThreadSystem already shut down, ignoring init request");
      }
      return false;
    }

    // Set queue capacity
    m_queueCapacity = queueCapacity;
    m_enableProfiling = enableProfiling;

    // Determine optimal thread count based on hardware
    //
    // HARDWARE CONCURRENCY - 1 PATTERN:
    // ThreadSystem allocates (hardware_concurrency - 1) workers to reserve one core
    // for the main rendering thread, which performs active work every frame:
    //   - Swapchain presentation / frame pacing
    //   - SDL_PollEvent (event polling)
    //   - Double-buffer coordination (swapBuffers)
    //
    // This prevents CPU oversubscription and context switching overhead that would
    // cause inconsistent frame times. The main thread is NOT idle - it needs dedicated
    // CPU resources for real-time rendering at 60 FPS.
    //
    // Thread allocation breakdown (8-core example):
    //   - Main thread: 1 (main loop/rendering/events, NOT in worker pool)
    //   - ThreadSystem workers: 7 (hardware_concurrency - 1)
    //   - All 7 workers available for managers via WorkerBudget
    //
    // Minimum worker count is 1 (not 0) even on single-core systems.
    //
    // COORDINATION WITH WORKERBUDGET:
    // WorkerBudget::calculateWorkerBudget() receives this worker count and
    // distributes all workers to managers. See WorkerBudget.hpp.
    if (customThreadCount > 0) {
      m_numThreads = customThreadCount;
    } else {
      unsigned int hardwareThreads = std::thread::hardware_concurrency();
      // Reserve one core for main rendering thread; minimum 1 worker for update thread
      m_numThreads = (hardwareThreads > 1) ? (hardwareThreads - 1) : 1;
    }

    // Create thread pool with profiling if enabled
    try {
      m_threadPool = std::make_unique<ThreadPool>(m_numThreads, m_queueCapacity,
                                                  m_enableProfiling);

      THREADSYSTEM_INFO(std::format("ThreadSystem initialized with {} worker threads{}",
                                    m_numThreads,
                                    (m_enableProfiling ? " (profiling enabled)" : "")));

      return m_threadPool != nullptr;
    } catch (const std::exception &e) {
      THREADSYSTEM_ERROR(std::format("Failed to initialize ThreadSystem: {}",
                                     e.what()));
      return false;
    }
  }

  /**
   * @brief Enqueue a task for execution by the thread pool
   *
   * This method adds a task to the thread pool's queue for execution.
   * The task will be executed by one of the worker threads as soon as
   * a thread becomes available. Tasks are executed in approximately
   * the order they are submitted.
   *
   * @param task The task to execute
   * @param priority The priority level for the task
   * @param description Optional description for debugging and monitoring
   */
  void enqueueTask(std::function<void()> task,
                   TaskPriority priority = TaskPriority::Normal,
                   const std::string &description = "") {
    // If shutdown or no thread pool, silently reject the task (for tests)
    if (m_isShutdown.load(std::memory_order_acquire) || !m_threadPool) {
      if (m_enableDebugLogging) {
        THREADSYSTEM_DEBUG(
            "Ignoring task after shutdown" +
            (description.empty() ? "" : " (" + description + ")"));
      }
      return;
    }

    // If debug logging is enabled and we have a description, log it
    if (!description.empty() && m_enableDebugLogging) {
      THREADSYSTEM_DEBUG(std::format("Enqueuing task: {}", description));
    }

    m_threadPool->enqueue(std::move(task), priority, description);
  }

  /**
   * @brief Batch enqueue multiple tasks with optimized performance
   *
   * This method is highly optimized for submitting multiple tasks at once,
   * reducing lock contention from O(N) to O(1). Use this when submitting
   * batches of tasks from AI updates, particle systems, or event processing.
   *
   * Example usage:
   *   std::vector<std::function<void()>> tasks;
   *   for (auto& entity : entities) {
   *     tasks.push_back([&entity](){ processEntity(entity); });
   *   }
   *   ThreadSystem::Instance().batchEnqueueTasks(tasks, TaskPriority::Normal, "AI Batch");
   *
   * @param tasks Vector of tasks to enqueue (will be moved from)
   * @param priority Priority level for all tasks in the batch (default: Normal)
   * @param description Optional description prefix for debugging and monitoring
   */
  void batchEnqueueTasks(std::vector<std::function<void()>>& tasks,
                         TaskPriority priority = TaskPriority::Normal,
                         const std::string& description = "") {
    // If shutdown or no thread pool, silently reject the tasks
    if (m_isShutdown.load(std::memory_order_acquire) || !m_threadPool) {
      if (m_enableDebugLogging) {
        THREADSYSTEM_DEBUG(std::format("Ignoring batch of {} tasks after shutdown{}",
                                       tasks.size(),
                                       (description.empty() ? "" : " (" + description + ")")));
      }
      return;
    }

    if (tasks.empty()) {
      return;
    }

    // If debug logging is enabled, log the batch submission
    if (m_enableDebugLogging && !description.empty()) {
      THREADSYSTEM_DEBUG(std::format("Batch enqueuing {} tasks: {}",
                                     tasks.size(), description));
    }

    m_threadPool->batchEnqueue(tasks, priority, description);
  }

  /**
   * @brief Enqueue a task that returns a result with priority
   *
   * This method adds a task to the thread pool and returns a future that
   * can be used to retrieve the result. The task will be executed by one
   * of the worker threads according to its priority level.
   *
   * @param f The function to execute
   * @param priority Priority level for the task (default: Normal)
   * @param description Optional description for debugging and monitoring
   * @param args The arguments to pass to the function
   * @return A future that will contain the result of the function call
   * @throws std::runtime_error if the ThreadSystem is shut down
   */
  template <class F, class... Args>
  auto
  enqueueTaskWithResult(F &&f, TaskPriority priority = TaskPriority::Normal,
                        const std::string &description = "", Args &&...args)
      -> std::future<typename std::invoke_result<F, Args...>::type> {
    // If shutdown or no thread pool, return a future with default value (for
    // tests)
    if (m_isShutdown.load(std::memory_order_acquire) || !m_threadPool) {
      // Create a promise/future pair with a default-constructed result
      using ResultType = typename std::invoke_result<F, Args...>::type;
      std::promise<ResultType> promise;

      if (m_enableDebugLogging) {
        THREADSYSTEM_DEBUG(
            "Returning default value for task after shutdown" +
            (description.empty() ? "" : " (" + description + ")"));
      }

      // Set the result using default construction if possible
      try {
        if constexpr (std::is_void_v<ResultType>) {
          promise.set_value();
        } else if constexpr (std::is_default_constructible_v<ResultType>) {
          promise.set_value(ResultType{});
        } else if constexpr (std::is_pointer_v<ResultType>) {
          promise.set_value(nullptr);
        } else {
          // For types like unique_ptr that can't be default constructed
          // Set an exception instead
          promise.set_exception(std::make_exception_ptr(std::runtime_error(
              "ThreadSystem shutdown: Cannot create default value")));
        }
      } catch (...) {
        promise.set_exception(std::current_exception());
      }

      return promise.get_future();
    }

    try {
      return m_threadPool->enqueueWithResult(std::forward<F>(f), priority,
                                             description,
                                             std::forward<Args>(args)...);
    } catch (const std::exception &e) {
      THREADSYSTEM_ERROR(std::format("Error enqueueing task: {}", e.what()));
      throw;
    }
  }

  bool isBusy() const {
    // If shutdown or no thread pool, not busy anymore
    if (m_isShutdown.load(std::memory_order_acquire) || !m_threadPool) {
      return false;
    }

    // Lock for thread safety when accessing mp_threadPool
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_threadPool->busy();
  }

  unsigned int getThreadCount() const { return m_numThreads; }

  bool isShutdown() const {
    return m_isShutdown.load(std::memory_order_acquire);
  }

  // Get the current task queue capacity
  size_t getQueueCapacity() const {
    if (m_threadPool) {
      return m_threadPool->getTaskQueue().capacity();
    }
    return m_queueCapacity;
  }

  // Get the current number of tasks in the queue
  size_t getQueueSize() const {
    if (m_threadPool) {
      return m_threadPool->getTaskQueue().size();
    }
    return 0;
  }

  /**
   * @brief Reserve capacity for the task queue
   *
   * NOTE: In most cases, you should NOT need to call this method directly.
   * The ThreadSystem is designed to manage its own capacity internally,
   * and will automatically grow as needed. This method is provided
   * primarily for specialized use cases where you know in advance
   * exactly how many tasks will be submitted.
   *
   * @param capacity The new capacity to reserve
   * @return true if capacity was reserved, false if ThreadSystem is shut down
   */
  bool reserveQueueCapacity(size_t capacity) {
    if (m_isShutdown.load(std::memory_order_acquire) || !m_threadPool) {
      return false;
    }
    m_threadPool->getTaskQueue().reserve(capacity);
    return true;
  }

  // Get comprehensive task statistics (global + worker queues)
  size_t getTotalTasksProcessed() const {
    if (m_threadPool) {
      return m_threadPool->getTotalTasksProcessed();
    }
    return 0;
  }

  size_t getTotalTasksEnqueued() const {
    if (m_threadPool) {
      return m_threadPool->getTotalTasksEnqueued();
    }
    return 0;
  }

  // Enable or disable debug logging
  void setDebugLogging(bool enable) { m_enableDebugLogging = enable; }

  bool isDebugLoggingEnabled() const { return m_enableDebugLogging; }

private:
  std::unique_ptr<ThreadPool> m_threadPool{nullptr};
  unsigned int m_numThreads{};
  size_t m_queueCapacity{DEFAULT_QUEUE_CAPACITY};
  std::atomic<bool> m_isShutdown{false}; // Flag to indicate shutdown status
  mutable std::mutex m_mutex{};          // For thread-safe access to members
  bool m_enableDebugLogging{false};      // Flag to control debug logging
  bool m_enableProfiling{false}; // Flag for detailed performance metrics

  // Prevent copying and assignment
  ThreadSystem(const ThreadSystem &) = delete;
  ThreadSystem &operator=(const ThreadSystem &) = delete;

  ThreadSystem() = default;
};

} // namespace VoidLight

#endif // THREAD_SYSTEM_HPP
