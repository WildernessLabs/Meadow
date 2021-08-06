/*
 * console.h
 *
 *  Created on: 1 Aug 2021
 *      Author: ioann
 */
//#include "main.h"
#include "usart.h"
#include "log_msg.h"

#ifndef CONSOLE_H_
#define CONSOLE_H_

void LogConsole(char* string, uint16_t size);
void PrintVersion(void);
void PrintOtaFlags(void);

#endif /* INC_CONSOLE_H_ */
