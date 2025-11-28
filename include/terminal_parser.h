/**
 * @file terminal_parser.h
 * @brief ANSI/VT100 escape sequence parser for terminal emulation.
 * 
 * Handles parsing and processing of ANSI and VT100 escape sequences.
 */

#ifndef TERMINAL_PARSER_H
#define TERMINAL_PARSER_H

#include "terminal_state.h"

/**
 * @brief Process input data and update terminal state
 * @param term Terminal state
 * @param buf Input buffer
 * @param len Length of input buffer
 * @return true on success, false on error
 */
bool terminal_parser_process_input(Terminal* term, const char* buf, size_t len);

/**
 * @brief Handle a regular character
 * @param term Terminal state
 * @param c Character to process
 */
void terminal_parser_put_char(Terminal* term, uint32_t c);

/**
 * @brief Handle CSI (Control Sequence Introducer) command
 * @param term Terminal state
 * @param command CSI command character
 */
void terminal_parser_handle_csi(Terminal* term, char command);

/**
 * @brief Handle OSC (Operating System Command) sequence
 * @param term Terminal state
 */
void terminal_parser_handle_osc(Terminal* term);

/**
 * @brief Handle SGR (Select Graphic Rendition) parameters
 * @param term Terminal state
 */
void terminal_parser_handle_sgr(Terminal* term);

/**
 * @brief Reset parser state
 * @param term Terminal state
 */
void terminal_parser_reset(Terminal* term);

#endif // TERMINAL_PARSER_H
