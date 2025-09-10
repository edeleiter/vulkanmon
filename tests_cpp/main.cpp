/*
 * VulkanMon C++ Unit Tests - Main Runner
 * 
 * Catch2-based unit testing for VulkanMon Phase 3 classes.
 * 
 * This file provides the main entry point for all C++ unit tests.
 * Using Catch2WithMain to automatically generate the main function.
 * 
 * Following our core principles:
 * - "Simple is Powerful" - Clean test structure and readable assertions
 * - "Test, Test, Test" - Comprehensive coverage of critical functionality
 * - "Document Often" - Clear test names that document expected behavior
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

// Note: Using Catch2::Catch2WithMain means we don't need to define main()
// Catch2 will automatically generate it for us

// Any global test setup can be done here using Catch2 listeners
// For now, we rely on the default Catch2 behavior