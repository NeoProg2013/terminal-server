//  ***************************************************************************
/// @file    term-srv.h
/// @author  NeoProg
/// @brief   Terminal server 
//  ***************************************************************************
#ifndef _TERM_SRV_H_
#define _TERM_SRV_H_
#include <stdint.h>

// Settings
#define MAX_COMMAND_HISTORY_LENGTH          (3)     // Command count in history (int8_t MAX)
#define MAX_COMMAND_LENGTH                  (64)    // Command length in bytes (int16_t MAX)
#define GREETING_STRING                     ("\x1B[36mroot@hexapod-AIWM: \x1B[0m")
#define UNKNOWN_COMMAND_STRING              ("\x1B[31m - command not found\x1B[0m")


typedef struct {
    char* cmd;
    int16_t len;
    void(*handler)(const char* cmd);
} term_srv_cmd_t;


extern void term_srv_init(void(*_send_data)(const char*, int16_t), term_srv_cmd_t* _ext_cmd_list, uint8_t _ext_cmd_count);
extern void term_srv_attach(void);
extern void term_srv_detach(void);
extern void term_srv_process(char symbol);


#endif // _TERM_SRV_H_
