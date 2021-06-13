#ifndef _LOG_H
#define _LOG_H

#define DEBUG(fmt, ...) printf("[DEBUG] " fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) printf("[WARN] " fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...) printf("[ERROR] " fmt, ##__VA_ARGS__)

#endif  // _LOG_H
