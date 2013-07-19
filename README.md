readfile_mulitprocess
=====================

description:
  it is process file, use child process line and parent only read
  
compile:
  gcc -g -o test readfile_via_multprocess.c
  
use:
 ./test -c[child num] -f[read file]
