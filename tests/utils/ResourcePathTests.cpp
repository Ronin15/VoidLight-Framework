/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#define BOOST_TEST_MODULE ResourcePathTests
#include <boost/test/unit_test.hpp>

#include "utils/ResourcePath.hpp"

#include <filesystem>
#include <fstream>

using VoidLight::ResourcePath;

namespace fs = std::filesystem;

namespace {

struct ResourcePathFixture {
    ResourcePathFixture() {
        ResourcePath::init();
    }

    ~ResourcePathFixture() {
        if (!m_highPath.empty()) {
            ResourcePath::removeSearchPath(m_highPath);
        }
        if (!m_lowPath.empty()) {
            ResourcePath::removeSearchPath(m_lowPath);
        }

        std::error_code ec;
        if (!m_tempRoot.empty()) {
            fs::remove_all(m_tempRoot, ec);
        }
    }

    fs::path makeTempRoot() {
        if (m_tempRoot.empty()) {
            m_tempRoot = fs::temp_directory_path() / "hammer_resource_path_tests";
            std::error_code ec;
            fs::remove_all(m_tempRoot, ec);
            fs::create_directories(m_tempRoot / "high" / "res" / "img");
            fs::create_directories(m_tempRoot / "low" / "res" / "img");

            std::ofstream(m_tempRoot / "high" / "res" / "img" / "icon.png") << "high";
            std::ofstream(m_tempRoot / "low" / "res" / "img" / "icon.png") << "low";

            m_highPath = (m_tempRoot / "high").string();
            m_lowPath = (m_tempRoot / "low").string();
        }
        return m_tempRoot;
    }

    fs::path m_tempRoot;
    std::string m_highPath;
    std::string m_lowPath;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(ResourcePathTests, ResourcePathFixture)

BOOST_AUTO_TEST_CASE(TestSearchPathPriorityControlsResolution) {
    const fs::path tempRoot = makeTempRoot();

    ResourcePath::addSearchPath(m_lowPath, 100);
    ResourcePath::addSearchPath(m_highPath, 200);

    const std::string resolved = ResourcePath::resolve("res/img/icon.png");
    BOOST_CHECK_EQUAL(resolved, (tempRoot / "high" / "res" / "img" / "icon.png").string());
    BOOST_CHECK(ResourcePath::exists("res/img/icon.png"));
}

BOOST_AUTO_TEST_CASE(TestRemoveSearchPathFallsBackToNextCandidate) {
    const fs::path tempRoot = makeTempRoot();

    ResourcePath::addSearchPath(m_lowPath, 100);
    ResourcePath::addSearchPath(m_highPath, 200);

    ResourcePath::removeSearchPath(m_highPath);
    m_highPath.clear();

    const std::string resolved = ResourcePath::resolve("res/img/icon.png");
    BOOST_CHECK_EQUAL(resolved, (tempRoot / "low" / "res" / "img" / "icon.png").string());
}

BOOST_AUTO_TEST_SUITE_END()
