/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#ifndef RESOURCEPATH_HPP
#define RESOURCEPATH_HPP

#include <string>
#include <vector>

namespace VoidLight {

/**
 * ResourcePath - Resolves resource paths across different execution contexts.
 *
 * Handles path resolution for:
 * - macOS app bundles (Contents/Resources/)
 * - Direct execution from project directory
 * - Future modding support via search path priorities
 *
 * THREAD-SAFETY NOTE:
 * ResourcePath::init() must be called exactly once during single-threaded
 * initialization (before any game threads start). Once initialized, all
 * methods are thread-safe for reading. The static state is immutable after
 * init() completes.
 *
 * Usage:
 *   ResourcePath::init();  // Call once at startup (single-threaded)
 *   std::string path = ResourcePath::resolve("res/img/icon.png");  // Thread-safe reads
 */
class ResourcePath {
public:
    /**
     * Initialize the resource path system.
     * Detects execution context (bundle vs direct) and sets up base paths.
     * Must be called once before any resolve() calls.
     *
     * THREAD-SAFETY: Must be called from main thread before ThreadSystem starts.
     * After initialization, all methods are safe for concurrent reads.
     */
    static void init();

    /**
     * Resolve a relative resource path to an absolute path.
     * Searches all registered paths in priority order.
     *
     * @param relativePath Path relative to resource root (e.g., "res/img/icon.png")
     * @return Absolute path to the resource, or the original path if not found
     */
    static std::string resolve(const std::string& relativePath);

    /**
     * Check if a resource exists at the given relative path.
     *
     * @param relativePath Path relative to resource root
     * @return true if resource exists in any search path
     */
    static bool exists(const std::string& relativePath);

    /**
     * Add a search path for resource resolution.
     * Higher priority paths are searched first.
     *
     * @param path Absolute path to add as a search location
     * @param priority Higher values = searched first (default: 0)
     */
    static void addSearchPath(const std::string& path, int priority = 0);

    /**
     * Remove a previously added search path.
     *
     * @param path The path to remove
     */
    static void removeSearchPath(const std::string& path);

    /**
     * Get the base resource path (primary search path).
     * Useful for managers that need the root directory.
     *
     * @return The highest priority search path, or empty if not initialized
     */
    static std::string getBasePath();

    /**
     * Check if running from a macOS app bundle.
     *
     * @return true if executing from within an .app bundle
     */
    static bool isRunningFromBundle();

private:
    struct SearchPath {
        std::string path{};
        int priority{0};
    };

    static std::vector<SearchPath> s_searchPaths;
    static bool s_initialized;
    static bool s_isBundle;

    static void detectExecutionContext();
    static std::string getExecutablePath();
};

} // namespace VoidLight

#endif // RESOURCEPATH_HPP
