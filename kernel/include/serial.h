#ifndef SERIAL_H_
#define SERIAL_H_

namespace serial {

// Enable serial ports.
// https://wiki.osdev.org/Serial_Ports#Initialization
void Initialize();

// Attempt to read a character from serial. If we were successfully able to read
// a character, return true and store the character in `c`.
bool TryRead(char &c);

// Attempt to write a character to serial. If we were successfully able to write
// a character, return true.
bool TryPut(char c);

// Variants of Read and Put that will wait before reading/writing a valid
// character. These should only be run in circumstances where we know interrupts
// are enabled. Otherwise, it's possible to be stuck waiting for serial,
// preventing other interrupts (like the timer) from trigerring.
char AtomicRead();
void AtomicPut(char c);
void AtomicWrite(const char *str);

}  // namespace serial

#endif
