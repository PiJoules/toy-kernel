#ifndef SERIAL_H_
#define SERIAL_H_

namespace serial {

// Enable serial ports.
// https://wiki.osdev.org/Serial_Ports#Initialization
void Initialize();

char Read();
void Write(char c);

}  // namespace serial

#endif
