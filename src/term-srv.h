//  ***************************************************************************
/// @file    term-srv.h
/// @author  NeoProg
/// @brief   Terminal server 
//  ***************************************************************************
#ifndef _TERM_SRV_H_
#define _TERM_SRV_H_
#include <stdint.h>

typedef struct {
    char* cmd;
    uint16_t len;
    void(*handler)(const char* cmd);
} term_srv_cmd_t;


extern void term_srv_init(void(*_send_data)(const char*, uint16_t), term_srv_cmd_t* _ext_cmd_list, uint8_t _ext_cmd_count);
extern void term_srv_attach(void);
extern void term_srv_detach(void);
extern void term_srv_process(char symbol);


#endif // _TERM_SRV_H_
