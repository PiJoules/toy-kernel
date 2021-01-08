This is a minimalist libc that contains some implementations that can be shared
between user and kernel space. Not everything can be shared (malloc in userspace
vs kernel as an example), but some things are ring-independent or hermetic and
can be used anywhere (like memcpy).
