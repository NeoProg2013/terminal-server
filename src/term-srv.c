//  ***************************************************************************
/// @file    term-srv.c
/// @author  NeoProg
//  ***************************************************************************
#include "term-srv.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define ESCAPE_SEQUENCES_COUNT              (12)
#define ESCAPE_SEQUENCES_MAX_LENGTH         (10)


typedef struct {
    char cmd[MAX_COMMAND_LENGTH];
    int16_t len;
} cmd_info_t;


static void esc_return_handler(const char*);
static void esc_backspace_handler(const char*);
static void esc_del_handler(const char*);
static void esc_up_handler(const char*);
static void esc_down_handler(const char*);
static void esc_left_handler(const char*);
static void esc_right_handler(const char*);
static void esc_home_handler(const char*);
static void esc_end_handler(const char*);
static void esc_tab_handler(const char*);


static term_srv_cmd_t esc_seq_list[ESCAPE_SEQUENCES_COUNT] = {
    { .cmd = "\x0D",    .len = 1, .handler = esc_return_handler,    },
    { .cmd = "\x0A",    .len = 1, .handler = esc_return_handler,    },
    { .cmd = "\x7F",    .len = 1, .handler = esc_backspace_handler, },
    { .cmd = "\x08",    .len = 1, .handler = esc_backspace_handler, },
    { .cmd = "\x09",    .len = 1, .handler = esc_tab_handler,       },
    { .cmd = "\x1B[3~", .len = 4, .handler = esc_del_handler,       },
    { .cmd = "\x1B[A",  .len = 3, .handler = esc_up_handler,        },
    { .cmd = "\x1B[B",  .len = 3, .handler = esc_down_handler,      },
    { .cmd = "\x1B[D",  .len = 3, .handler = esc_left_handler,      },
    { .cmd = "\x1B[C",  .len = 3, .handler = esc_right_handler,     },
    { .cmd = "\x1B[4~", .len = 4, .handler = esc_end_handler,       },
    { .cmd = "\x1B[1~", .len = 4, .handler = esc_home_handler,      }
};

static void(*send_data)(const char* data, int16_t) = NULL;
static term_srv_cmd_t* ext_cmd_list = NULL;
static uint8_t ext_cmd_count = 0;

static int16_t cursor_pos = 0;
static cmd_info_t current_cmd = {0};
static char esc_seq_buffer[ESCAPE_SEQUENCES_MAX_LENGTH] = {0};
static int16_t esc_seq_length = 0;

static cmd_info_t history_elements[MAX_COMMAND_HISTORY_LENGTH] = {0};
static cmd_info_t* history[MAX_COMMAND_HISTORY_LENGTH] = {0};
static int8_t history_len = 0;
static int8_t history_pos = -1;


static inline void save_cursor_pos() { send_data("\x1B[s", 3); }
static inline void load_cursor_pos() { send_data("\x1B[u", 3); }


//  ***************************************************************************
/// @brief  CLI core initialization
/// @param  none
/// @return none
//  ***************************************************************************
void term_srv_init(void(*_send_data)(const char*, int16_t), term_srv_cmd_t* _ext_cmd_list, uint8_t _ext_cmd_count) {
    send_data = _send_data;
    ext_cmd_list = _ext_cmd_list;
    ext_cmd_count = _ext_cmd_count;
    term_srv_detach();
}

//  ***************************************************************************
/// @brief  Call when client is connected
//  ***************************************************************************
void term_srv_attach(void) {
    send_data("\r\n", 2);
    send_data(GREETING_STRING, strlen(GREETING_STRING));
}

//  ***************************************************************************
/// @brief  Call when client is disconnected
//  ***************************************************************************
void term_srv_detach(void) {
    cursor_pos = 0;
    memset(&current_cmd, 0, sizeof(current_cmd));
    
    esc_seq_length = 0;
    memset(esc_seq_buffer, 0, sizeof(esc_seq_buffer));
    
    memset(history_elements, 0, sizeof(history_elements));
    for (int16_t i = 0; i < MAX_COMMAND_HISTORY_LENGTH; ++i) {
        history[i] = &history_elements[i];
    }
    history_len = 0;
    history_pos = 0;
}

//  ***************************************************************************
/// @brief  Process received symbol
/// @param  symbol: received symbol
/// @return none
//  ***************************************************************************
void term_srv_process(char symbol) {
    if (current_cmd.len + 1 >= MAX_COMMAND_LENGTH) {
        return; // Incoming buffer is overflow
    }
    
    if (esc_seq_length == 0) {
        // Check escape signature
        if (symbol <= 0x1F || symbol == 0x7F) {
            esc_seq_buffer[esc_seq_length++] = symbol;
        }
        // Print symbol if escape sequence signature is not found
        if (esc_seq_length == 0) {
            if (cursor_pos < current_cmd.len) {
                memmove(&current_cmd.cmd[cursor_pos + 1], &current_cmd.cmd[cursor_pos], current_cmd.len - cursor_pos); // Offset symbols after cursor
                save_cursor_pos();
                send_data(&current_cmd.cmd[cursor_pos], current_cmd.len - cursor_pos + 1);
                load_cursor_pos();
            }
            current_cmd.cmd[cursor_pos++] = symbol;
            ++current_cmd.len;
            send_data(&symbol, 1); 
        }
    } else {
        esc_seq_buffer[esc_seq_length++] = symbol;
    }
    
    // Process escape sequence
    if (esc_seq_length != 0) {
        int8_t possible_esc_seq_count = 0;
        int8_t possible_esc_idx = 0;
        for (int8_t i = 0; i < ESCAPE_SEQUENCES_COUNT; ++i) {
            if (esc_seq_length <= esc_seq_list[i].len && memcmp(esc_seq_buffer, esc_seq_list[i].cmd, esc_seq_length) == 0) {
                ++possible_esc_seq_count;
                possible_esc_idx = i;
            }
        }
        
        switch (possible_esc_seq_count) {
        case 0: // No sequence - display all symbols
            for (int8_t i = 0; i < esc_seq_length && current_cmd.len + 1 < MAX_COMMAND_LENGTH; ++i) {
                if (esc_seq_buffer[i] <= 0x1F || esc_seq_buffer[i] == 0x7F) {
                    esc_seq_buffer[i] = '?';
                }
                current_cmd.cmd[cursor_pos + i] = esc_seq_buffer[i];
                ++current_cmd.len;
            }
            send_data(&current_cmd.cmd[cursor_pos], esc_seq_length);
            cursor_pos += esc_seq_length;
            esc_seq_length = 0;
            break;
        
        case 1: // We found one possible sequence - check size and call handler
            if (esc_seq_list[possible_esc_idx].len == esc_seq_length) {
                esc_seq_length = 0;
                esc_seq_list[possible_esc_idx].handler(NULL);
            }
            break;
        
        default: // We found few possible sequences
            break;
        }
    }
}



//  ***************************************************************************
/// @brief  Process '\r' and '\n' escapes
/// @param  none
/// @return none
//  ***************************************************************************
static void esc_return_handler(const char* cmd) {
    send_data("\r\n", 2);
    
    // Add command into history
    if (current_cmd.len) {
        if (history_len >= MAX_COMMAND_HISTORY_LENGTH) {
            // If history is overflow -- offset all items to begin by 1 position, first item move to end
            void* temp_addr = history[0];
            for (int16_t i = 0; i < MAX_COMMAND_HISTORY_LENGTH - 1; ++i) {
                history[i] = history[i + 1];
            }
            history[MAX_COMMAND_HISTORY_LENGTH - 1] = temp_addr;
            --history_len;
        }
        memcpy(history[history_len++], &current_cmd, sizeof(current_cmd));
        history_pos = history_len;
    }
    
    // Calc command length without args
    void* addr = memchr(current_cmd.cmd, ' ', current_cmd.len);
    int16_t cmd_len = current_cmd.len;
    if (addr != NULL) {
        cmd_len = (uint32_t)addr - (uint32_t)current_cmd.cmd;
    }
    
    // Search command
    bool is_find = false;
    for (int16_t i = 0; i < ext_cmd_count; ++i) {
        if (cmd_len == ext_cmd_list[i].len && memcmp(ext_cmd_list[i].cmd, current_cmd.cmd, cmd_len) == 0) {
            ext_cmd_list[i].handler(current_cmd.cmd);
            send_data("\r\n", 2);
            is_find = true;
            break;
        }
    }
    if (!is_find && cmd_len) {
        send_data(current_cmd.cmd, cmd_len);
        send_data(UNKNOWN_COMMAND_STRING, strlen(UNKNOWN_COMMAND_STRING));
        send_data("\r\n", 2);
    }
    send_data(GREETING_STRING, strlen(GREETING_STRING));

    // Clear buffer to new command
    memset(&current_cmd, 0, sizeof(current_cmd));
    cursor_pos = 0;
}

//  ***************************************************************************
/// @brief  Process BACKSPACE escape
//  ***************************************************************************
static void esc_backspace_handler(const char* cmd) {
    if (cursor_pos > 0) {
        memmove(&current_cmd.cmd[cursor_pos - 1], &current_cmd.cmd[cursor_pos], current_cmd.len - cursor_pos); // Remove symbol from buffer
        current_cmd.cmd[current_cmd.len - 1] = 0; // Clear last symbol
        --current_cmd.len;
        --cursor_pos;

        send_data("\x7F", 1); // Remove symbol
        save_cursor_pos();
        send_data(&current_cmd.cmd[cursor_pos], current_cmd.len - cursor_pos); // Replace old symbols
        send_data(" ", 1);    // Hide last symbol
        load_cursor_pos();
    }
}

//  ***************************************************************************
/// @brief  Process DELETE escapes
//  ***************************************************************************
static void esc_del_handler(const char* cmd) {
    if (cursor_pos < current_cmd.len) {
        memmove(&current_cmd.cmd[cursor_pos], &current_cmd.cmd[cursor_pos + 1], current_cmd.len - cursor_pos); // Remove symbol from buffer
        current_cmd.cmd[current_cmd.len - 1] = 0; // Clear last symbol
        --current_cmd.len;

        save_cursor_pos();
        send_data(&current_cmd.cmd[cursor_pos], current_cmd.len - cursor_pos);
        send_data(" ", 1); // Hide last symbol
        load_cursor_pos();
    }
}

//  ***************************************************************************
/// @brief  Process ARROW_UP escape
//  ***************************************************************************
static void esc_up_handler(const char* cmd) {
    if (history_pos - 1 < 0) {
        return;
    }
    --history_pos;

    // Calculate diff between current command and command from history
    int16_t remainder = current_cmd.len - history[history_pos]->len;

    // Move cursor to begin of command
    while (cursor_pos) {
      send_data("\x1B[D", 3);
      --cursor_pos;
    }

    // Print new command
    memcpy(&current_cmd, history[history_pos], sizeof(current_cmd));
    send_data(current_cmd.cmd, current_cmd.len);
    cursor_pos += current_cmd.len;

    // Clear others symbols
    save_cursor_pos();
    for (int16_t i = 0; i < remainder; ++i) {
        send_data(" ", 1);
    }
    load_cursor_pos();
}

//  ***************************************************************************
/// @brief  Process ARROW_DOWN escape
//  ***************************************************************************
static void esc_down_handler(const char* cmd) {
    if (history_pos + 1 > history_len) {
        return;
    }
    ++history_pos;

    int16_t remainder = 0;
    if (history_pos < history_len) {

       // Calculate diff between current command and command from history
       remainder = current_cmd.len - history[history_pos]->len;

       // Move cursor to begin of command
       while (cursor_pos) {
           send_data("\x1B[D", 3);
           --cursor_pos;
       }

       // Print new command
       memcpy(&current_cmd, history[history_pos], sizeof(current_cmd));
       send_data(current_cmd.cmd, current_cmd.len);
       cursor_pos += current_cmd.len;
    } 
    else {
       remainder = current_cmd.len;

       // Move cursor to begin of command
       while (cursor_pos) {
           send_data("\x1B[D", 3);
           --cursor_pos;
       }

       memset(&current_cmd, 0, sizeof(current_cmd));
    }

    // Clear others symbols
    save_cursor_pos();
    for (int16_t i = 0; i < remainder; ++i) {
        send_data(" ", 1);
    }
    load_cursor_pos();
}

//  ***************************************************************************
/// @brief  Process ARROW_LEFT escape
//  ***************************************************************************
static void esc_left_handler(const char* cmd) {
    if (cursor_pos > 0) {
        send_data("\x1B[D", 3);
        --cursor_pos;
    }
}

//  ***************************************************************************
/// @brief  Process ARROW_RIGHT escape
//  ***************************************************************************
static void esc_right_handler(const char* cmd) {
    if (cursor_pos < current_cmd.len) {
        send_data("\x1B[C", 3);
        ++cursor_pos;
    }
}

//  ***************************************************************************
/// @brief  Process HOME escape
//  ***************************************************************************
static void esc_home_handler(const char* cmd) {
    while (cursor_pos > 0) {
        send_data("\x1B[D", 3);
        --cursor_pos;
    }
}

//  ***************************************************************************
/// @brief  Process END escape
//  ***************************************************************************
static void esc_end_handler(const char* cmd) {
    while (cursor_pos < current_cmd.len) {
        send_data("\x1B[C", 3);
        ++cursor_pos;
    }
}

//  ***************************************************************************
/// @brief  Process DELETE escapes
//  ***************************************************************************
static void esc_tab_handler(const char* cmd) {
    int8_t possible_cmd_count = 0;
    int8_t possible_cmd_idx = 0;
    for (int16_t i = 0; i < ext_cmd_count; ++i) {
        if (current_cmd.len <= ext_cmd_list[i].len && memcmp(ext_cmd_list[i].cmd, current_cmd.cmd, current_cmd.len) == 0) {
            ++possible_cmd_count;
            possible_cmd_idx = i;
        }
    }
    if (possible_cmd_count == 1) {
        int16_t remainder = ext_cmd_list[possible_cmd_idx].len - current_cmd.len;
        if (remainder != 0) {
            memcpy(&current_cmd.cmd[cursor_pos], &ext_cmd_list[possible_cmd_idx].cmd[cursor_pos], remainder);
            current_cmd.len += remainder;
            send_data(&current_cmd.cmd[cursor_pos], remainder);
            cursor_pos += remainder;
        }
    }
}
