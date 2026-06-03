/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE ThreadSystemTests
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "core/ThreadSystem.hpp"

// Global fixture for test setup and cleanup
struct ThreadTestFixture {
    ThreadTestFixture() {
        if (!VoidLight::ThreadSystem::Instance().init(4096)) {
            throw std::runtime_error("ThreadSystem::init() failed");
        }
    }

    ~ThreadTestFixture() {
        VoidLight::ThreadSystem::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(ThreadTestFixture);

BOOST_AUTO_TEST_CASE(TestThreadPoolInitialization) {
    // Check that the thread system is initialized
    BOOST_CHECK(!VoidLight::ThreadSystem::Instance().isShutdown());

    // Check that the thread count is reasonable (at least 1)
    unsigned int threadCount = VoidLight::ThreadSystem::Instance().getThreadCount();
    BOOST_CHECK_GT(threadCount, 0);

    // Output the thread count for information
    std::cout << "Thread system initialized with " << threadCount << " threads." << std::endl;

    // Check that the queue capacity is set to the test value (4096)
    BOOST_CHECK_EQUAL(VoidLight::ThreadSystem::Instance().getQueueCapacity(), 4096);
}

BOOST_AUTO_TEST_CASE(TestSimpleTaskExecution) {
    // Create an atomic flag to be set by the task
    std::atomic<bool> taskExecuted{false};

    // Submit a simple task that sets the flag
    VoidLight::ThreadSystem::Instance().enqueueTask([&taskExecuted]() {
        taskExecuted = true;
    });

    // Wait for a longer time to allow the task to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check that the task was executed
    BOOST_CHECK(taskExecuted);
}

BOOST_AUTO_TEST_CASE(TestTaskWithResult) {
    // Submit a task that returns a value
    auto future = VoidLight::ThreadSystem::Instance().enqueueTaskWithResult([]() -> int {
        return 42;
    });

    // Wait for the result and check it
    int result = future.get();
    BOOST_CHECK_EQUAL(result, 42);
}

BOOST_AUTO_TEST_CASE(TestTaskPriorities) {
    // This test verifies that we can submit tasks with different priorities
    // We skip checking execution order as it depends on thread scheduling

    // Counter to track completed tasks
    std::atomic<int> tasksCompleted{0};

    // Create tasks with different priorities
    auto lowPriorityTask = [&tasksCompleted]() {
        tasksCompleted.fetch_add(1);
    };

    auto normalPriorityTask = [&tasksCompleted]() {
        tasksCompleted.fetch_add(1);
    };

    auto highPriorityTask = [&tasksCompleted]() {
        tasksCompleted.fetch_add(1);
    };

    auto criticalPriorityTask = [&tasksCompleted]() {
        tasksCompleted.fetch_add(1);
    };

    // Submit tasks with different priorities
    VoidLight::ThreadSystem::Instance().enqueueTask(
        lowPriorityTask,
        VoidLight::TaskPriority::Low,
        "Low priority task"
    );

    VoidLight::ThreadSystem::Instance().enqueueTask(
        normalPriorityTask,
        VoidLight::TaskPriority::Normal,
        "Normal priority task"
    );

    VoidLight::ThreadSystem::Instance().enqueueTask(
        highPriorityTask,
        VoidLight::TaskPriority::High,
        "High priority task"
    );

    VoidLight::ThreadSystem::Instance().enqueueTask(
        criticalPriorityTask,
        VoidLight::TaskPriority::Critical,
        "Critical priority task"
    );

    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify that all tasks were processed
    BOOST_CHECK_EQUAL(tasksCompleted.load(), 4);
}

BOOST_AUTO_TEST_CASE(TestMultipleTasks) {
    // Number of tasks to create
    const int numTasks = 512;
    // Atomic counter to be incremented by each task
    std::atomic<int> counter{0};

    // Submit multiple tasks
    for (int i = 0; i < numTasks; ++i) {
        VoidLight::ThreadSystem::Instance().enqueueTask(
            [&counter]() {
                // Simulate some work with reduced sleep time
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                counter++;
            },
            VoidLight::TaskPriority::Normal,
            "Counter increment task"
        );
    }

    // Wait for all tasks to complete with proper timeout calculation
    // With 21 threads and reduced work, allow more time for queue processing
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // Check that all tasks were executed
    BOOST_CHECK_EQUAL(counter, numTasks);
}

BOOST_AUTO_TEST_CASE(TestConcurrentTaskResults) {
    // Number of tasks to create
    const int numTasks = 50;

    // Vector to store futures for each task
    std::vector<std::future<int>> futures;

    // Submit tasks that each return their index
    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
            [i]() -> int {
                // Simulate varying work times
                std::this_thread::sleep_for(std::chrono::milliseconds(i % 10));
                return i;
            },
            VoidLight::TaskPriority::Normal,
            "Return index task " + std::to_string(i)
        ));
    }

    // Collect results and verify
    std::unordered_set<int> results;
    for (auto& future : futures) {
        results.insert(future.get());
    }

    // Check that we got all expected values
    BOOST_CHECK_EQUAL(results.size(), numTasks);
    for (int i = 0; i < numTasks; ++i) {
        BOOST_CHECK(results.find(i) != results.end());
    }
}

BOOST_AUTO_TEST_CASE(TestTasksWithExceptions) {
    // Submit a task that throws an exception
    auto future = VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
        []() -> int {
            throw std::runtime_error("Test exception");
            return 0; // Never reached
        },
        VoidLight::TaskPriority::Normal,
        "Exception-throwing task"
    );

    // Check that the exception is properly propagated
    BOOST_CHECK_THROW(future.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(TestConcurrencyIsolation) {
    // Create a shared resource
    int sharedValue = 0;
    std::mutex mutex;

    // Submit tasks that access the shared resource with proper synchronization
    const int numTasks = 100;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
            [&sharedValue, &mutex]() -> void {
                // Properly lock the shared resource
                std::lock_guard<std::mutex> lock(mutex);
                sharedValue++;
            },
            VoidLight::TaskPriority::Normal,
            "Synchronized increment task"
        ));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }

    // Verify the result
    BOOST_CHECK_EQUAL(sharedValue, numTasks);
}

BOOST_AUTO_TEST_CASE(TestBusyFlag) {
    // Check initial busy state - should typically be false before we add work
    bool initialBusy = VoidLight::ThreadSystem::Instance().isBusy();
    BOOST_CHECK(!initialBusy);

    // Submit a long-running task
    VoidLight::ThreadSystem::Instance().enqueueTask(
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        },
        VoidLight::TaskPriority::Normal,
        "Long-running task"
    );

    // Check if system reports as busy
    bool busyDuringTask = VoidLight::ThreadSystem::Instance().isBusy();

    // Wait for task to complete with a longer timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Check if system reports as not busy after task completion
    bool busyAfterTask = VoidLight::ThreadSystem::Instance().isBusy();

    // Check that the busy flag was set correctly
    // Note: These checks might be flaky due to timing, but should generally work
    BOOST_CHECK(busyDuringTask);
    BOOST_CHECK(!busyAfterTask);
}

BOOST_AUTO_TEST_CASE(TestNestedTasks) {
    // Atomic counter to track task execution
    std::atomic<int> counter{0};

    // Submit a task that itself submits another task
    VoidLight::ThreadSystem::Instance().enqueueTask(
        [&counter]() {
            counter++;

            // Submit a nested task
            VoidLight::ThreadSystem::Instance().enqueueTask(
                [&counter]() {
                    counter++;
                },
                VoidLight::TaskPriority::High,
                "Nested task"
            );
        },
        VoidLight::TaskPriority::Normal,
        "Parent task"
    );

    // Wait for both tasks to complete with longer timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Check that both tasks were executed
    BOOST_CHECK_EQUAL(counter, 2);
}

BOOST_AUTO_TEST_CASE(TestLoadBalancing) {
    // Number of tasks to create
    const int numTasks = 200;

    // Keep track of which thread executed each task
    std::vector<unsigned long> threadIds(numTasks);
    std::mutex idMutex;

    // Submit tasks that record their thread ID
    std::vector<std::future<void>> futures;
    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
            [i, &threadIds, &idMutex]() -> void {
                // Get the current thread ID
                unsigned long threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());

                // Record which thread executed this task
                {
                    std::lock_guard<std::mutex> lock(idMutex);
                    threadIds[i] = threadId;
                }

                // Small amount of work
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            },
            VoidLight::TaskPriority::Normal,
            "Thread ID recording task " + std::to_string(i)
        ));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }

    // Count the number of unique thread IDs used
    std::unordered_set<unsigned long> uniqueThreads;
    for (auto id : threadIds) {
        uniqueThreads.insert(id);
    }

    // Ensure that multiple threads were used (should use at least 2 threads)
    unsigned int threadCount = VoidLight::ThreadSystem::Instance().getThreadCount();
    // In case of single-threaded system, test is still valid
    unsigned int minExpectedThreads = (threadCount > 1) ? 2 : 1;

    BOOST_CHECK_GE(uniqueThreads.size(), minExpectedThreads);
    std::cout << "Tasks were executed on " << uniqueThreads.size() << " different threads." << std::endl;
}

BOOST_AUTO_TEST_CASE(TestQueueCapacityReservation) {
    // Test that we can reserve capacity in the queue
    size_t initialCapacity = VoidLight::ThreadSystem::Instance().getQueueCapacity();
    size_t newCapacity = initialCapacity * 2;

    // Reserve more capacity
    bool success = VoidLight::ThreadSystem::Instance().reserveQueueCapacity(newCapacity);

    // Check that reservation succeeded
    BOOST_CHECK(success);

    // Check that capacity was increased
    BOOST_CHECK_GE(VoidLight::ThreadSystem::Instance().getQueueCapacity(), newCapacity);
}

BOOST_AUTO_TEST_CASE(TestTaskStats) {
    // Create and submit some tasks
    const int numTasks = 50;
    std::vector<std::future<void>> futures;

    // Get initial task stats
    size_t initialEnqueued = VoidLight::ThreadSystem::Instance().getTotalTasksEnqueued();
    size_t initialProcessed = VoidLight::ThreadSystem::Instance().getTotalTasksProcessed();

    // Submit tasks
    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
            []() -> void {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            },
            VoidLight::TaskPriority::Normal,
            "Stats test task"
        ));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }

    // Ensure worker threads have updated processed counters before sampling
    for (int i = 0; i < 20 && VoidLight::ThreadSystem::Instance().isBusy(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Check that task statistics were updated
    size_t finalEnqueued = VoidLight::ThreadSystem::Instance().getTotalTasksEnqueued();
    size_t finalProcessed = VoidLight::ThreadSystem::Instance().getTotalTasksProcessed();

    BOOST_CHECK_GE(finalEnqueued, initialEnqueued + numTasks);
    BOOST_CHECK_GE(finalProcessed, initialProcessed + numTasks);
}

BOOST_AUTO_TEST_CASE(TestQueueOverflowProtection) {
    // Test submitting tasks close to queue capacity (4096)
    const size_t testTaskCount = 3500; // Safe threshold below capacity
    std::vector<std::future<float>> futures;

    std::cout << "Testing queue capacity with " << testTaskCount << " tasks..." << std::endl;

    // Submit many tasks quickly to test queue limits
    for (size_t i = 0; i < testTaskCount; ++i) {
        futures.push_back(VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
            []() -> float {
                // Small work to simulate AI batch processing
                float temp = 0.0f;
                for (int j = 0; j < 10; ++j) {
                    temp += std::sqrt(static_cast<float>(j + 1));
                }

                return temp;
            },
            VoidLight::TaskPriority::Normal,
            "Load test task"
        ));

        // Check queue size periodically
        if (i % 500 == 0) {
            size_t currentQueueSize = VoidLight::ThreadSystem::Instance().getQueueSize();
            std::cout << "Queue size at " << i << " tasks: " << currentQueueSize << std::endl;

            // Verify we're not approaching dangerous levels
            BOOST_CHECK_LT(currentQueueSize, 4000);
        }
    }

    // Wait for all tasks to complete and consume the computed work
    double totalWork = 0.0;
    for (auto& future : futures) {
        totalWork += future.get();
    }
    BOOST_CHECK_GT(totalWork, 0.0);

    // Final queue size should be manageable
    size_t finalQueueSize = VoidLight::ThreadSystem::Instance().getQueueSize();
    std::cout << "Final queue size: " << finalQueueSize << std::endl;
    BOOST_CHECK_LT(finalQueueSize, 100); // Should drain quickly
}



BOOST_AUTO_TEST_CASE(TestBurstTaskSubmission) {
    // Simulate burst conditions like what AIManager might create
    std::cout << "Testing burst task submission patterns..." << std::endl;

    const int burstSize = 100;
    const int burstCount = 10;

    for (int burst = 0; burst < burstCount; ++burst) {
        size_t queueBefore = VoidLight::ThreadSystem::Instance().getQueueSize();

        // Submit a burst of tasks quickly (simulating AI batch submission)
        std::vector<std::future<void>> burstTasks;
        for (int i = 0; i < burstSize; ++i) {
            burstTasks.push_back(VoidLight::ThreadSystem::Instance().enqueueTaskWithResult(
                []() -> void {
                    // Simulate AI processing work
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                },
                VoidLight::TaskPriority::Normal,
                "Burst task"
            ));
        }

        size_t queueAfter = VoidLight::ThreadSystem::Instance().getQueueSize();
        size_t queueGrowth = queueAfter - queueBefore;

        std::cout << "Burst " << burst << ": Added " << queueGrowth << " tasks, queue size: " << queueAfter << std::endl;

        // Verify queue doesn't overflow
        BOOST_CHECK_LT(queueAfter, 4000);

        // Wait for burst to complete before next burst
        for (auto& task : burstTasks) {
            task.wait();
        }

        // Brief pause between bursts
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Burst testing completed successfully" << std::endl;
}

BOOST_AUTO_TEST_CASE(TestThreadSystemReinitialization) {
    // Clean up the current thread system
    VoidLight::ThreadSystem::Instance().clean();

    // Verify it's shut down
    BOOST_CHECK(VoidLight::ThreadSystem::Instance().isShutdown());

    // Try to re-initialize with custom settings
    unsigned int customThreads = 2;
    size_t customCapacity = 1024;

    // This should fail because the system was already shut down
    bool initSuccess = VoidLight::ThreadSystem::Instance().init(customCapacity, customThreads);

    // The init should fail since we already cleaned up
    BOOST_CHECK(!initSuccess);

    // Thread system should still be in shutdown state
    BOOST_CHECK(VoidLight::ThreadSystem::Instance().isShutdown());
}
