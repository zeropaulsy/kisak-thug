/* Winsock names on POSIX sockets, for the shared PC networking code (Gel/Net). */
#ifndef KISAK_WIN32_NET_H
#define KISAK_WIN32_NET_H
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int SOCKET;
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#define closesocket(s) close(s)
#define ioctlsocket(s, cmd, argp) ioctl((s), (cmd), (argp))
#define WSAGetLastError() errno
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEADDRINUSE EADDRINUSE
#define WSACleanup() 0
#define WSAEINPROGRESS EINPROGRESS
#define WSAEALREADY EALREADY
#define WSAECONNRESET ECONNRESET
#define WSAEISCONN EISCONN
#endif
