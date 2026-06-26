/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#define BOOST_TEST_MODULE SaveManagerTests
#include <boost/test/unit_test.hpp>
#include "utils/BinarySerializer.hpp"
#include "mocks/MockPlayer.hpp"
#include "managers/EntityDataManager.hpp"
#include <filesystem>
#include <sstream>
#include <thread>

// Global fixture for test setup and cleanup
struct TestFixture {
    TestFixture() {
        if (!EntityDataManager::Instance().init()) {
            throw std::runtime_error("EntityDataManager::init() failed");
        }
        // Ensure the relative output directory exists before any test writes to
        // it. Without this the suite is order/cwd-dependent: saveToFile() fails
        // when tests/test_data/ is absent (fresh build tree, ctest working dir,
        // or after cleanup), making tests flaky. std::filesystem is portable
        // (forward slashes resolve correctly on Windows too).
        std::error_code ec;
        std::filesystem::create_directories("tests/test_data", ec);
        if (ec) {
            throw std::runtime_error("Failed to create tests/test_data directory: " + ec.message());
        }
    }

    ~TestFixture() {
        // Clean up any leftover test files
        try {
            std::filesystem::remove("tests/test_data/test_mockplayer.dat");
            std::filesystem::remove("tests/test_data/test_slot_1.dat");
            std::filesystem::remove("tests/test_data/valid_test.dat");
            std::filesystem::remove("tests/test_data/integration_test.dat");
            std::filesystem::remove("tests/test_data/test_vector.dat");
            std::filesystem::remove("tests/test_data/test_writer.dat");
            std::filesystem::remove("tests/test_data/test_primitives.dat");
            std::filesystem::remove("tests/test_data/test_vector_int.dat");
            std::filesystem::remove("tests/test_data/test_vector_float.dat");
        } catch (...) {}

        EntityDataManager::Instance().clean();
    }
};

BOOST_GLOBAL_FIXTURE(TestFixture);

BOOST_AUTO_TEST_CASE(TestSaveAndLoad) {
    // Test BinarySerializer directly with MockPlayer (which implements ISerializable)
    MockPlayer player;
    player.setTestPosition(123.0f, 456.0f);
    player.setTestTextureID("test_texture");
    player.setTestState("running");

    // Save using BinarySerializer convenience function
    bool saveResult = BinarySerial::saveToFile("tests/test_data/test_mockplayer.dat", player);
    BOOST_CHECK(saveResult);

    // Create a new player with different state
    MockPlayer loadedPlayer;
    loadedPlayer.setTestPosition(0.0f, 0.0f);
    loadedPlayer.setTestTextureID("different_texture");
    loadedPlayer.setTestState("idle");

    // Load using BinarySerializer convenience function
    bool loadResult = BinarySerial::loadFromFile("tests/test_data/test_mockplayer.dat", loadedPlayer);
    BOOST_CHECK(loadResult);

    // Check that the state was loaded correctly
    BOOST_CHECK_CLOSE(loadedPlayer.getPosition().getX(), 123.0f, 0.001f);
    BOOST_CHECK_CLOSE(loadedPlayer.getPosition().getY(), 456.0f, 0.001f);
    BOOST_CHECK_EQUAL(loadedPlayer.getCurrentStateName(), "running");
    BOOST_CHECK_EQUAL(loadedPlayer.getTextureID(), "test_texture");

    // Clean up
    std::filesystem::remove("tests/test_data/test_mockplayer.dat");
}

BOOST_AUTO_TEST_CASE(TestSlotOperations) {
    // Test BinarySerializer with multiple files (simulating slot behavior)
    MockPlayer player;
    player.setTestPosition(123.0f, 456.0f);

    // Save to "slot 1" (just a different filename)
    bool saveResult = BinarySerial::saveToFile("tests/test_data/test_slot_1.dat", player);
    BOOST_CHECK(saveResult);

    // Check if the file exists
    BOOST_CHECK(std::filesystem::exists("tests/test_data/test_slot_1.dat"));

    // Load from "slot 1"
    MockPlayer loadedPlayer;
    bool loadResult = BinarySerial::loadFromFile("tests/test_data/test_slot_1.dat", loadedPlayer);
    BOOST_CHECK(loadResult);

    // Check slot data was loaded correctly
    BOOST_CHECK_CLOSE(loadedPlayer.getPosition().getX(), 123.0f, 0.001f);

    // Delete the file
    bool deleteResult = std::filesystem::remove("tests/test_data/test_slot_1.dat");
    BOOST_CHECK(deleteResult);

    // Check that the file no longer exists
    BOOST_CHECK(!std::filesystem::exists("tests/test_data/test_slot_1.dat"));
}

BOOST_AUTO_TEST_CASE(TestErrorHandling) {
    // Create a test player
    MockPlayer player;

    // Try to load a non-existent file
    bool loadResult = BinarySerial::loadFromFile("non_existent.dat", player);
    BOOST_CHECK(!loadResult);

    // Test invalid file access (try to save to invalid path)
    bool saveResult = BinarySerial::saveToFile("/invalid/path/test.dat", player);
    BOOST_CHECK(!saveResult);  // Should fail for invalid path

    // Test valid save and file existence validation
    saveResult = BinarySerial::saveToFile("tests/test_data/valid_test.dat", player);
    BOOST_CHECK(saveResult);

    // Test file existence
    BOOST_CHECK(std::filesystem::exists("tests/test_data/valid_test.dat"));
    BOOST_CHECK(!std::filesystem::exists("non_existent.dat"));

    // Clean up
    std::filesystem::remove("tests/test_data/valid_test.dat");

    std::cout << "TestErrorHandling completed successfully" << std::endl;
}

BOOST_AUTO_TEST_CASE(TestNewSerializationSystem) {
    // Test the new serialization system directly
    std::cout << "Testing new fast serialization system..." << std::endl;

    // Test Vector2D serialization
    Vector2D originalPos(123.456f, 789.012f);
    Vector2D loadedPos;

    // Test convenience functions
    bool saveSuccess = BinarySerial::saveToFile("tests/test_data/test_vector.dat", originalPos);
    BOOST_CHECK(saveSuccess);

    bool loadSuccess = BinarySerial::loadFromFile("tests/test_data/test_vector.dat", loadedPos);
    BOOST_CHECK(loadSuccess);

    BOOST_CHECK_CLOSE(loadedPos.getX(), originalPos.getX(), 0.001f);
    BOOST_CHECK_CLOSE(loadedPos.getY(), originalPos.getY(), 0.001f);

    // Test Writer/Reader-based serialization
    {
        auto writer = BinarySerial::Writer::createFileWriter("tests/test_data/test_writer.dat");
        BOOST_CHECK(writer != nullptr);
        if (writer) {
            bool result = writer->writeSerializable(originalPos);
            BOOST_CHECK(result);
            BOOST_CHECK(writer->good());
        }
    }

    {
        Vector2D writerLoadedPos;
        auto reader = BinarySerial::Reader::createFileReader("tests/test_data/test_writer.dat");
        BOOST_CHECK(reader != nullptr);
        if (reader) {
            bool result = reader->readSerializable(writerLoadedPos);
            BOOST_CHECK(result);
            BOOST_CHECK(reader->good());
            BOOST_CHECK_CLOSE(writerLoadedPos.getX(), originalPos.getX(), 0.001f);
            BOOST_CHECK_CLOSE(writerLoadedPos.getY(), originalPos.getY(), 0.001f);
        }
    }

    std::cout << "New serialization system tests completed successfully" << std::endl;
}

BOOST_AUTO_TEST_CASE(TestBinaryWriterReader) {
    std::cout << "Testing binary writer/reader..." << std::endl;

    // Test basic data types with Writer/Reader
    {
        auto writer = BinarySerial::Writer::createFileWriter("tests/test_data/test_primitives.dat");
        BOOST_CHECK(writer != nullptr);
        if (writer) {
            BOOST_CHECK(writer->write(42));
            BOOST_CHECK(writer->write(3.14f));
            BOOST_CHECK(writer->write(true));
            BOOST_CHECK(writer->writeString("test string"));
            BOOST_CHECK(writer->good());
        }
    }

    // Load the primitives
    {
        auto reader = BinarySerial::Reader::createFileReader("tests/test_data/test_primitives.dat");
        BOOST_CHECK(reader != nullptr);
        if (reader) {
            int intVal;
            float floatVal;
            bool boolVal;
            std::string stringVal;

            BOOST_CHECK(reader->read(intVal));
            BOOST_CHECK(reader->read(floatVal));
            BOOST_CHECK(reader->read(boolVal));
            BOOST_CHECK(reader->readString(stringVal));
            BOOST_CHECK(reader->good());

            BOOST_CHECK_EQUAL(intVal, 42);
            BOOST_CHECK_CLOSE(floatVal, 3.14f, 0.001f);
            BOOST_CHECK_EQUAL(boolVal, true);
            BOOST_CHECK_EQUAL(stringVal, "test string");
        }
    }

    std::cout << "Binary writer/reader tests completed successfully" << std::endl;
}

BOOST_AUTO_TEST_CASE(TestBorrowedStreamWriterReader) {
    std::cout << "Testing borrowed stream writer/reader..." << std::endl;

    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    {
        BinarySerial::Writer writer(stream);
        BOOST_CHECK(writer.write(99));
        BOOST_CHECK(writer.writeString("borrowed stream"));
        BOOST_CHECK(writer.good());
    }

    stream.seekg(0);

    {
        BinarySerial::Reader reader(stream);
        int intVal = 0;
        std::string stringVal;

        BOOST_CHECK(reader.read(intVal));
        BOOST_CHECK(reader.readString(stringVal));
        BOOST_CHECK(reader.good());
        BOOST_CHECK_EQUAL(intVal, 99);
        BOOST_CHECK_EQUAL(stringVal, "borrowed stream");
    }

    std::cout << "Borrowed stream writer/reader tests completed successfully"
              << std::endl;
}

BOOST_AUTO_TEST_CASE(TestVectorSerialization) {
    std::cout << "Testing vector serialization..." << std::endl;

    // Test vector<int> serialization
    {
        std::vector<int> originalScores = {100, 250, 300, 175, 400};

        // Write the vector and ensure the writer is destroyed to flush the stream
        {
            auto writer = BinarySerial::Writer::createFileWriter("tests/test_data/test_vector_int.dat");
            BOOST_CHECK(writer != nullptr);
            if (writer) {
                BOOST_CHECK(writer->writeVector(originalScores));
                BOOST_CHECK(writer->good());
            }
        } // Writer is destroyed here, ensuring stream is closed and flushed

        // Now read the vector
        std::vector<int> loadedScores;
        auto reader = BinarySerial::Reader::createFileReader("tests/test_data/test_vector_int.dat");
        BOOST_CHECK(reader != nullptr);
        if (reader) {
            BOOST_CHECK(reader->readVector(loadedScores));
            BOOST_CHECK(reader->good());
            BOOST_CHECK_EQUAL(loadedScores.size(), originalScores.size());
            for (size_t i = 0; i < originalScores.size(); ++i) {
                BOOST_CHECK_EQUAL(loadedScores[i], originalScores[i]);
            }
        }
    }

    // Test vector<float> serialization
    {
        std::vector<float> originalValues = {1.1f, 2.2f, 3.3f, 4.4f};
        auto writer = BinarySerial::Writer::createFileWriter("tests/test_data/test_vector_float.dat");
        BOOST_CHECK(writer != nullptr);
        if (writer) {
            BOOST_CHECK(writer->writeVector(originalValues));
            writer->flush();
        }

        std::vector<float> loadedValues;
        auto reader = BinarySerial::Reader::createFileReader("tests/test_data/test_vector_float.dat");
        BOOST_CHECK(reader != nullptr);
        if (reader) {
            BOOST_CHECK(reader->readVector(loadedValues));
            BOOST_CHECK_EQUAL(loadedValues.size(), originalValues.size());
            for (size_t i = 0; i < originalValues.size(); ++i) {
                BOOST_CHECK_CLOSE(loadedValues[i], originalValues[i], 0.001f);
            }
        }
    }

    std::cout << "Vector serialization tests completed successfully" << std::endl;
}

BOOST_AUTO_TEST_CASE(TestPerformanceComparison) {
    std::cout << "Testing serialization performance..." << std::endl;

    // Create test data
    Vector2D testPos(123.456f, 789.012f);
    std::string testString = "Performance test string with some content";
    std::vector<int> testVector(1000, 42);  // 1000 integers

    // Time the new serialization system
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {  // 100 iterations
        std::string filename = "tests/test_data/perf_test_" + std::to_string(i) + ".dat";
        
        {
            // Scope the writer to ensure proper cleanup
            auto writer = BinarySerial::Writer::createFileWriter(filename);
            if (writer) {
                writer->writeSerializable(testPos);
                writer->writeString(testString);
                writer->writeVector(testVector);
                writer->flush();  // Ensure data is written
            }
        } // Writer destructor called here, releasing file handle

        // Small delay on Windows to allow file handle cleanup
        #ifdef _WIN32
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        #endif

        // Clean up with error checking
        try {
            if (std::filesystem::exists(filename)) {
                std::filesystem::remove(filename);
            }
        } catch (const std::exception& e) {
            // On Windows, file may still be locked briefly - this is expected
            #ifndef _WIN32
            std::cout << "Warning: Failed to remove file " << filename << ": " << e.what() << std::endl;
            #endif
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "New serialization system: 100 operations took "
              << duration.count() << " microseconds" << std::endl;

    // Basic performance check - should complete in reasonable time
    // Windows file system operations are slower due to file locking
    #ifdef _WIN32
    BOOST_CHECK(duration.count() < 2000000);  // Less than 2 seconds for 100 operations on Windows
    #else
    BOOST_CHECK(duration.count() < 100000);   // Less than 100ms for 100 operations on Unix
    #endif

    std::cout << "Performance test completed successfully" << std::endl;
}

BOOST_AUTO_TEST_CASE(TestBinarySerializerIntegration) {
    // Test that BinarySerializer works properly with complex objects
    MockPlayer player;
    player.setTestPosition(999.0f, 888.0f);
    player.setTestTextureID("integration_test");
    player.setTestState("serializer_test");

    // Test save with BinarySerializer
    bool saveResult = BinarySerial::saveToFile("tests/test_data/integration_test.dat", player);
    BOOST_CHECK(saveResult);

    // Test load with BinarySerializer
    MockPlayer loadedPlayer;
    bool loadResult = BinarySerial::loadFromFile("tests/test_data/integration_test.dat", loadedPlayer);
    BOOST_CHECK(loadResult);

    // Verify data integrity through BinarySerializer
    BOOST_CHECK_CLOSE(loadedPlayer.getPosition().getX(), 999.0f, 0.001f);
    BOOST_CHECK_CLOSE(loadedPlayer.getPosition().getY(), 888.0f, 0.001f);
    BOOST_CHECK_EQUAL(loadedPlayer.getTextureID(), "integration_test");
    BOOST_CHECK_EQUAL(loadedPlayer.getCurrentStateName(), "serializer_test");

    // Test file existence functionality
    BOOST_CHECK(std::filesystem::exists("tests/test_data/integration_test.dat"));

    // Clean up
    std::filesystem::remove("tests/test_data/integration_test.dat");
    BOOST_CHECK(!std::filesystem::exists("tests/test_data/integration_test.dat"));

    std::cout << "BinarySerializer integration test completed successfully" << std::endl;
}
