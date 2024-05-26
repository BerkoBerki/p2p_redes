execve("./tracker", ["./tracker"], 0x7ffc77f2d090 /* 47 vars */) = 0
brk(NULL)                               = 0x56e15d39f000
arch_prctl(0x3001 /* ARCH_??? */, 0x7ffe58aac6e0) = -1 EINVAL (Invalid argument)
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x702c236d4000
access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
newfstatat(3, "", {st_mode=S_IFREG|0644, st_size=69611, ...}, AT_EMPTY_PATH) = 0
mmap(NULL, 69611, PROT_READ, MAP_PRIVATE, 3, 0) = 0x702c236c3000
close(3)                                = 0
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libcrypto.so.3", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832
newfstatat(3, "", {st_mode=S_IFREG|0644, st_size=4455728, ...}, AT_EMPTY_PATH) = 0
mmap(NULL, 4469952, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x702c23200000
mmap(0x702c232b2000, 2486272, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0xb2000) = 0x702c232b2000
mmap(0x702c23511000, 860160, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x311000) = 0x702c23511000
mmap(0x702c235e3000, 385024, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x3e2000) = 0x702c235e3000
mmap(0x702c23641000, 9408, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x702c23641000
close(3)                                = 0
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libstdc++.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832
newfstatat(3, "", {st_mode=S_IFREG|0644, st_size=2260296, ...}, AT_EMPTY_PATH) = 0
mmap(NULL, 2275520, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x702c22e00000
mprotect(0x702c22e9a000, 1576960, PROT_NONE) = 0
mmap(0x702c22e9a000, 1118208, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x9a000) = 0x702c22e9a000
mmap(0x702c22fab000, 454656, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1ab000) = 0x702c22fab000
mmap(0x702c2301b000, 57344, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x21a000) = 0x702c2301b000
mmap(0x702c23029000, 10432, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x702c23029000
close(3)                                = 0
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libgcc_s.so.1", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832
newfstatat(3, "", {st_mode=S_IFREG|0644, st_size=125488, ...}, AT_EMPTY_PATH) = 0
mmap(NULL, 127720, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x702c236a3000
mmap(0x702c236a6000, 94208, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x3000) = 0x702c236a6000
mmap(0x702c236bd000, 16384, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1a000) = 0x702c236bd000
mmap(0x702c236c1000, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1d000) = 0x702c236c1000
close(3)                                = 0
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0P\237\2\0\0\0\0\0"..., 832) = 832
pread64(3, "\6\0\0\0\4\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0"..., 784, 64) = 784
pread64(3, "\4\0\0\0 \0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0"..., 48, 848) = 48
pread64(3, "\4\0\0\0\24\0\0\0\3\0\0\0GNU\0\226 \25\252\235\23<l\274\3731\3540\5\226\327"..., 68, 896) = 68
newfstatat(3, "", {st_mode=S_IFREG|0755, st_size=2220400, ...}, AT_EMPTY_PATH) = 0
pread64(3, "\6\0\0\0\4\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0"..., 784, 64) = 784
mmap(NULL, 2264656, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x702c22a00000
mprotect(0x702c22a28000, 2023424, PROT_NONE) = 0
mmap(0x702c22a28000, 1658880, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x28000) = 0x702c22a28000
mmap(0x702c22bbd000, 360448, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1bd000) = 0x702c22bbd000
mmap(0x702c22c16000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x215000) = 0x702c22c16000
mmap(0x702c22c1c000, 52816, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x702c22c1c000
close(3)                                = 0
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libm.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832
newfstatat(3, "", {st_mode=S_IFREG|0644, st_size=940560, ...}, AT_EMPTY_PATH) = 0
mmap(NULL, 942344, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x702c23119000
mmap(0x702c23127000, 507904, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0xe000) = 0x702c23127000
mmap(0x702c231a3000, 372736, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x8a000) = 0x702c231a3000
mmap(0x702c231fe000, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0xe4000) = 0x702c231fe000
close(3)                                = 0
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x702c236a1000
mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x702c2369e000
arch_prctl(ARCH_SET_FS, 0x702c2369e740) = 0
set_tid_address(0x702c2369ea10)         = 248324
set_robust_list(0x702c2369ea20, 24)     = 0
rseq(0x702c2369f0e0, 0x20, 0, 0x53053053) = 0
mprotect(0x702c22c16000, 16384, PROT_READ) = 0
mprotect(0x702c231fe000, 4096, PROT_READ) = 0
mprotect(0x702c236c1000, 4096, PROT_READ) = 0
mprotect(0x702c2301b000, 45056, PROT_READ) = 0
mprotect(0x702c235e3000, 372736, PROT_READ) = 0
mprotect(0x56e15d110000, 4096, PROT_READ) = 0
mprotect(0x702c2370e000, 8192, PROT_READ) = 0
prlimit64(0, RLIMIT_STACK, NULL, {rlim_cur=8192*1024, rlim_max=RLIM64_INFINITY}) = 0
munmap(0x702c236c3000, 69611)           = 0
getrandom("\x19\x33\x0b\xd0\x40\x79\xf5\x38", 8, GRND_NONBLOCK) = 8
brk(NULL)                               = 0x56e15d39f000
brk(0x56e15d3c0000)                     = 0x56e15d3c0000
futex(0x702c2302977c, FUTEX_WAKE_PRIVATE, 2147483647) = 0
socket(AF_INET, SOCK_STREAM, IPPROTO_IP) = 3
setsockopt(3, SOL_SOCKET, SO_REUSEADDR, [1], 4) = 0
bind(3, {sa_family=AF_INET, sin_port=htons(8888), sin_addr=inet_addr("0.0.0.0")}, 16) = 0
newfstatat(1, "", {st_mode=S_IFCHR|0620, st_rdev=makedev(0x88, 0x2), ...}, AT_EMPTY_PATH) = 0
write(1, "Red creada en el puerto 8888. \n", 31) = 31
listen(3, 6)                            = 0
write(1, "Esperando peers ...\n", 20)   = 20
pselect6(4, [3], NULL, NULL, NULL, NULL) = 1 (in [3])
accept(3, {sa_family=AF_INET, sin_port=htons(46320), sin_addr=inet_addr("127.0.0.1")}, [16]) = 4
read(4, "jero", 1024)                   = 4
write(4, "ok", 2)                       = 2
read(4, "6000\n", 20)                   = 5
write(1, "jero se ha conectado a la red. S"..., 70) = 70
pselect6(5, [3 4], NULL, NULL, NULL, NULL) = 1 (in [4])
read(4, "pieces", 1024)                 = 6
write(1, "HOLA\n", 5)                   = 5
recvfrom(4, 0x7ffe58814710, 4, 0, NULL, NULL) = ? ERESTARTSYS (To be restarted if SA_RESTART is set)
--- SIGINT {si_signo=SIGINT, si_code=SI_KERNEL} ---
+++ killed by SIGINT +++
