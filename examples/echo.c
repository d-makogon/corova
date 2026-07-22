#include <arpa/inet.h>
#include <libloom/coroutines.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 20

#define USE_COROUTINES

#ifdef USE_COROUTINES
typedef struct {
  struct sockaddr_in client_addr;
  int client_fd;
} ClientContext;

volatile bool stop = false;

void echo_client_routine(void *client_context_p) {
  ClientContext *client_context = client_context_p;

  char peer_ip[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &client_context->client_addr.sin_addr, peer_ip,
                 INET_ADDRSTRLEN)) {
    perror("inet_ntop failed");
    goto ret;
  }

  printf("Accepted connection from %s:%d\n", peer_ip,
         ntohs(client_context->client_addr.sin_port));

  char buf[BUF_SIZE];
  ssize_t total_read_n = 0;

  while (1) {
    coroutine_wait_fd(client_context->client_fd, CORO_WAIT_READ);
    ssize_t read_n = read(client_context->client_fd, buf + total_read_n,
                          sizeof(buf) - total_read_n - 1);
    if (read_n < 0) {
      perror("read failed");
      goto ret;
    }

    total_read_n += read_n;
    if (read_n == 0 || total_read_n == BUF_SIZE - 1)
      break;
  }
  buf[total_read_n] = '\0';

  if ((unsigned)total_read_n >= strlen("stop") &&
      strncmp(buf, "stop", strlen("stop")) == 0) {
    printf("Accepted 'stop' command\n");
    stop = true;
    goto ret;
  }

  ssize_t total_write_n = 0;
  while (total_write_n < total_read_n) {
    coroutine_wait_fd(client_context->client_fd, CORO_WAIT_WRITE);
    ssize_t write_n = write(client_context->client_fd, buf, total_read_n);
    if (write_n < 0) {
      perror("write failed");
      goto ret;
    }
    total_write_n += write_n;
  }

ret:
  close(client_context->client_fd);
  free(client_context);
}

void echo_server_routine(void *) {
  int server_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket failed");
    return;
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    perror("setsockopt failed");
    return;
  }

  struct sockaddr_in listen_addr = {0};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(8080);
  listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(server_fd, (void *)&listen_addr, sizeof(listen_addr)) == -1) {
    perror("bind failed");
    return;
  }

  if (listen(server_fd, /*backlog=*/69) == -1) {
    perror("listen failed");
    return;
  }

  char ip_str[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &listen_addr.sin_addr, ip_str, INET_ADDRSTRLEN)) {
    perror("inet_ntop failed");
    return;
  }
  printf("Listening on %s:%d\n", ip_str, ntohs(listen_addr.sin_port));

  while (1) {
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);

    printf("Waiting for a client...\n");
    coroutine_wait_fd(server_fd, CORO_WAIT_READ);
    printf("Accepting a client...\n");

    int client_fd = accept(server_fd, (void *)&client_addr, &client_addr_len);
    if (client_fd < 0) {
      perror("accept failed");
      return;
    }

    char peer_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &client_addr.sin_addr, peer_ip, INET_ADDRSTRLEN)) {
      perror("inet_ntop failed");
      return;
    }

    ClientContext *client_context = calloc(1, sizeof(*client_context));
    client_context->client_fd = client_fd;
    client_context->client_addr = client_addr;
    coroutine_go(echo_client_routine, client_context);
  }
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  coroutine_init();
  coroutine_go(echo_server_routine, NULL);
  coroutines_run();
  return 0;
}
// Leaving the trivial single thread blocking implementation for reference.
#else
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int server_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket failed");
    return 1;
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    perror("setsockopt failed");
    return 4;
  }

  struct sockaddr_in listen_addr = {0};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(8080);
  listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(server_fd, (void *)&listen_addr, sizeof(listen_addr)) == -1) {
    perror("bind failed");
    return 2;
  }

  if (listen(server_fd, /*backlog=*/69) == -1) {
    perror("listen failed");
    return 6;
  }

  char ip_str[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &listen_addr.sin_addr, ip_str, INET_ADDRSTRLEN)) {
    perror("inet_ntop failed");
    return 3;
  }
  printf("Listening on %s:%d\n", ip_str, ntohs(listen_addr.sin_port));

  while (1) {
    struct pollfd server_pfd = {0};
    server_pfd.fd = server_fd;
    server_pfd.events = POLLIN;
    while (1) {
      int ret = poll(&server_pfd, 1, -1);
      printf("poll: %d\n", ret);
      if (ret > 0)
        break;
      if (ret == -1) {
        perror("poll failed");
        return 9;
      }
    }

    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (void *)&client_addr, &client_addr_len);
    if (client_fd < 0) {
      perror("accept failed");
      return 5;
    }

    char peer_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &client_addr.sin_addr, peer_ip, INET_ADDRSTRLEN)) {
      perror("inet_ntop failed");
      return 3;
    }

    printf("Accepted connection from %s:%d\n", peer_ip,
           ntohs(client_addr.sin_port));

    char buf[BUF_SIZE];
    ssize_t total_read_n = 0;

    while (1) {
      ssize_t read_n =
          read(client_fd, buf + total_read_n, sizeof(buf) - total_read_n - 1);
      if (read_n < 0) {
        perror("read failed");
        return 7;
      }

      total_read_n += read_n;
      if (read_n == 0 || total_read_n == BUF_SIZE - 1)
        break;
    }
    buf[total_read_n] = '\0';

    if ((unsigned)total_read_n >= strlen("stop") &&
        strncmp(buf, "stop", strlen("stop")) == 0) {
      printf("Stopping server...\n");
      break;
    }

    ssize_t total_write_n = 0;
    while (total_write_n < total_read_n) {
      ssize_t write_n = write(client_fd, buf, total_read_n);
      if (write_n < 0) {
        perror("write failed");
        return 8;
      }
      total_write_n += write_n;
    }
    close(client_fd);
  }
  close(server_fd);
  return 0;
}
#endif // #if 0
