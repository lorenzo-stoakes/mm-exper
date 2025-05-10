# mm experiments

This is a collection of experiments and tools revolving around the linux memory
management subsystem. Used to assist in my work writing [The Linux Memory
Manager](https://linuxmemory.org), a book describing the linux memory management
subsystem.

I've also used it for playing around with ideas, problems, reproducing bugs, etc.

This is a bit of a mess, but generally random/ has a bunch of little experiments
in, read-pageflags has a whole thing for using /proc/$pid/pagemap and friends
for reading back flags, tools/ has some userland helper tools stuff,
userfaultfd/ has some uffd example code, repros/ has a bunch of repros of
previous bugs etc. etc.

Licensed under the GPLv2.
