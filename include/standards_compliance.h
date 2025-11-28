/**
 * @file standards_compliance.h
 * @brief Terminal emulator standards compliance verification.
 *
 * This module provides comprehensive standards compliance testing and verification
 * for VT100, VT220, ANSI, and xterm compatibility standards.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#ifndef STANDARDS_COMPLIANCE_H
#define STANDARDS_COMPLIANCE_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "terminal_state.h"
#include "error_codes.h"

// Supported terminal standards
typedef enum {
    STANDARD_VT100,
    STANDARD_VT220,
    STANDARD_ANSI,
    STANDARD_XTERM,
    STANDARD_LINUX,
    STANDARD_SCREEN
} TerminalStandard;

// Compliance test result
typedef enum {
    COMPLIANCE_PASS,
    COMPLIANCE_FAIL,
    COMPLIANCE_PARTIAL,
    COMPLIANCE_NOT_TESTED
} ComplianceResult;

// Test suite category
typedef enum {
    TEST_CURSOR_CONTROL,
    TEST_SCREEN_MODES,
    TEST_COLOR_SUPPORT,
    TEST_KEYBOARD_INPUT,
    TEST_SCROLLING,
    TEST_TAB_SETS,
    TEST_CHARACTER_SETS,
    TEST_GRAPHICS,
    TEST_WINDOW_OPERATIONS,
    TEST_MOUSE_TRACKING
} TestCategory;

// Compliance test entry
typedef struct {
    char test_name[128];
    TestCategory category;
    TerminalStandard standard;
    ComplianceResult result;
    char details[256];
    uint64_t execution_time_ms;
} ComplianceTest;

// Compliance report
typedef struct {
    TerminalStandard standard;
    int total_tests;
    int passed_tests;
    int failed_tests;
    int partial_tests;
    double compliance_percentage;
    ComplianceTest* tests;
    int test_count;
    uint64_t total_execution_time;
} ComplianceReport;

/**
 * @brief Initialize standards compliance testing
 *
 * @return ErrorCode Result of initialization
 */
ErrorCode standards_compliance_init(void);

/**
 * @brief Cleanup standards compliance testing
 */
void standards_compliance_cleanup(void);

/**
 * @brief Run full compliance test suite
 *
 * @param standard Terminal standard to test against
 * @param term Pointer to terminal structure
 * @return ComplianceReport Compliance test results
 */
ComplianceReport run_compliance_tests(TerminalStandard standard, Terminal* term);

/**
 * @brief Test specific compliance category
 *
 * @param category Test category
 * @param standard Terminal standard
 * @param term Pointer to terminal structure
 * @return ComplianceResult Result of testing
 */
ComplianceResult test_compliance_category(TestCategory category, TerminalStandard standard, Terminal* term);

/**
 * @brief Test VT100 compatibility
 *
 * @param term Pointer to terminal structure
 * @return ComplianceReport VT100 compliance results
 */
ComplianceReport test_vt100_compliance(Terminal* term);

/**
 * @brief Test VT220 compatibility
 *
 * @param term Pointer to terminal structure
 * @return ComplianceReport VT220 compliance results
 */
ComplianceReport test_vt220_compliance(Terminal* term);

/**
 * @brief Test ANSI compatibility
 *
 * @param term Pointer to terminal structure
 * @return ComplianceReport ANSI compliance results
 */
ComplianceReport test_ansi_compliance(Terminal* term);

/**
 * @brief Test xterm compatibility
 *
 * @param term Pointer to terminal structure
 * @return ComplianceReport xterm compliance results
 */
ComplianceReport test_xterm_compliance(Terminal* term);

/**
 * @brief Test cursor control sequences
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_cursor_control(Terminal* term);

/**
 * @brief Test screen modes (alternate screen, origin mode, etc.)
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_screen_modes(Terminal* term);

/**
 * @brief Test color support (8-color, 16-color, 256-color, TrueColor)
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_color_support(Terminal* term);

/**
 * @brief Test keyboard input handling
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_keyboard_input(Terminal* term);

/**
 * @brief Test scrolling operations
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_scrolling(Terminal* term);

/**
 * @brief Test tab setting operations
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_tab_sets(Terminal* term);

/**
 * @brief Test character set support
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_character_sets(Terminal* term);

/**
 * @brief Test graphics support (line drawing, etc.)
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_graphics(Terminal* term);

/**
 * @brief Test window operations (resize, title, etc.)
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_window_operations(Terminal* term);

/**
 * @brief Test mouse tracking support
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_mouse_tracking(Terminal* term);

/**
 * @brief Send test sequence to terminal
 *
 * @param term Pointer to terminal structure
 * @param sequence Escape sequence to send
 * @param expected_result Expected terminal state
 * @return bool True if test passed
 */
bool send_test_sequence(Terminal* term, const char* sequence, const char* expected_result);

/**
 * @brief Verify terminal state after test
 *
 * @param term Pointer to terminal structure
 * @param expected_state Expected terminal state
 * @return bool True if state matches expected
 */
bool verify_terminal_state(Terminal* term, const char* expected_state);

/**
 * @brief Generate compliance report
 *
 * @param report Compliance report to format
 * @param output_buffer Output buffer for formatted report
 * @param buffer_size Size of output buffer
 * @return ErrorCode Result of operation
 */
ErrorCode generate_compliance_report(const ComplianceReport* report, char* output_buffer, size_t buffer_size);

/**
 * @brief Save compliance report to file
 *
 * @param report Compliance report to save
 * @param filename Output filename
 * @return ErrorCode Result of operation
 */
ErrorCode save_compliance_report(const ComplianceReport* report, const char* filename);

/**
 * @brief Load compliance report from file
 *
 * @param filename Input filename
 * @return ComplianceReport Loaded compliance report
 */
ComplianceReport load_compliance_report(const char* filename);

/**
 * @brief Compare compliance reports
 *
 * @param report1 First compliance report
 * @param report2 Second compliance report
 * @return double Similarity percentage (0.0-1.0)
 */
double compare_compliance_reports(const ComplianceReport* report1, const ComplianceReport* report2);

/**
 * @brief Get standard description
 *
 * @param standard Terminal standard
 * @return const char* Standard description
 */
const char* get_standard_description(TerminalStandard standard);

/**
 * @brief Get category description
 *
 * @param category Test category
 * @return const char* Category description
 */
const char* get_category_description(TestCategory category);

/**
 * @brief Enable/disable detailed testing
 *
 * @param enabled Whether detailed testing should be enabled
 */
void set_detailed_testing_enabled(bool enabled);

/**
 * @brief Get compliance recommendations
 *
 * @param report Compliance report
 * @param recommendations Output buffer for recommendations
 * @param buffer_size Size of recommendations buffer
 * @return ErrorCode Result of operation
 */
ErrorCode get_compliance_recommendations(const ComplianceReport* report, char* recommendations, size_t buffer_size);

/**
 * @brief Run quick compliance check
 *
 * @param term Pointer to terminal structure
 * @return bool True if basic compliance checks pass
 */
bool quick_compliance_check(Terminal* term);

/**
 * @brief Validate escape sequence parsing
 *
 * @param term Pointer to terminal structure
 * @param sequence Escape sequence to validate
 * @return bool True if sequence is parsed correctly
 */
bool validate_escape_sequence(Terminal* term, const char* sequence);

/**
 * @brief Test Unicode support
 *
 * @param term Pointer to terminal structure
 * @return ComplianceResult Test result
 */
ComplianceResult test_unicode_support(Terminal* term);

#endif // STANDARDS_COMPLIANCE_H
