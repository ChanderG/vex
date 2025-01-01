# vex

Warning: Early stage WIP.

VTerm based Terminal Emulator for X.

A (toy) terminal emulator built on top of:
+ Eduterm: https://www.uninformativ.de/blog/postings/2018-02-24/0/POSTING-en.html
+ Libvterm (used to embed terminals in Neovim and Emacs)

While Eduterm starts us off, actually using libvterm is quite difficult due to the lack of documentation. I have pieced together a bit by reading the source code of libvterm and emacs-libvterm, but am not done yet. When I do have more information, this terminal will be "complete" and I will also document the usage of libvterm clearly here or on my website as blog entries.

## Install

Dependencies include X11, Xft etc.

We also need `libvterm` (statically compiled in):
1. Get the repo from here https://www.leonerd.org.uk/code/libvterm/ into a folder called `deps/libvterm`.
2. Run `make` in `deps/libvterm`.
3. `make` in the top level.

## References

X11 Clipboard:
- https://github.com/exebook/x11clipboard
- https://github.com/ColleagueRiley/Clipboard-Copy-Paste
- https://handmade.network/forums/articles/t/8544-implementing_copy_paste_in_x11
