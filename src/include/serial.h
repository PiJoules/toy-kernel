#ifndef SERIAL_H_
#define SERIAL_H_

namespace serial {

// Enable serial ports.
// https://wiki.osdev.org/Serial_Ports#Initialization
void Initialize();

char Read();
void Put(char c);
void Write(const char *str);

}  // namespace serial

#endif
