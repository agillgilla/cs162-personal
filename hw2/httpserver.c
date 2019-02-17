#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;


/*
 * Helper function that concatenates an arbitrary number of char arrays and
 * returns the resulting array and sets the size_t length to the result length.
 */
char *super_strcat(size_t *length, int numStrs, ...) {
  va_list valist;
  
  size_t result_len = 0;

  // Init valist
  va_start(valist, numStrs);

  // Iterate through char* args
  for (int i = 0; i < numStrs; i++) {
    char *curStr = va_arg(valist, char*);
    result_len += strlen(curStr);
  }

  // De-allocate memory
  va_end(valist);

  char *result = malloc(result_len + 1);

  va_start(valist, numStrs);

  strcpy(result, va_arg(valist, char*));

  for (int i = 1; i < numStrs; i++) {
    char *curStr = va_arg(valist, char*);
    strcat(result, curStr);
  }

  *length = result_len + 1;

  return result;
}


/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {

  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */

  struct http_request *request = http_request_parse(fd);

  char *filename;

  if (strcmp(request->path, "/") != 0) {
    filename = malloc(strlen(server_files_directory) + strlen(request->path) + 1);
    if (filename == NULL) {
      http_start_response(fd, 200);
      http_send_header(fd, "Content-Type", "text/html");
      http_end_headers(fd);
      http_send_string(fd,
          "<center>"
          "<h1>Error allocating memory for request.</h1>"
          "</center>");
      return;
    }
    strcpy(filename, server_files_directory);
    strcat(filename, request->path);
  } else {
    filename = server_files_directory;
  }

  struct stat statbuf;
  int file_exists = stat(filename, &statbuf);
  if (file_exists == 0) {
    if (S_ISREG(statbuf.st_mode)) {
      // Requested file is a regular file
      char buf[1024];
      int in_fd = open(filename, O_RDONLY);
      
      if (in_fd != -1) {
        // Opened regular file successfully
        char lenBuf[256];
        size_t len = statbuf.st_size;
      
        snprintf(lenBuf, sizeof(lenBuf), "%zu", len);
        
        http_start_response(fd, 200);
        http_send_header(fd, "Content-Type", http_get_mime_type(filename));
        http_send_header(fd, "Content-Length", lenBuf);
        http_end_headers(fd);

        size_t read_len;
        while ((read_len = read(in_fd, buf, sizeof(buf))) != 0){
          http_send_data(fd, buf, read_len);
        }
        close(in_fd);

        return;
      } else {
        // File failed to open
        char lenBuf[256];
        size_t len;
        char *response = super_strcat(&len, 1, "<h1>Unable to open file.</h1>");
        snprintf(lenBuf, sizeof(lenBuf), "%zu", len);

        http_start_response(fd, 200);
        http_send_header(fd, "Content-Type", "text/html");
        http_send_header(fd, "Content-Length", lenBuf);
        http_end_headers(fd);
        http_send_string(fd, response);

        return;
      }
    } else if (S_ISDIR(statbuf.st_mode)) {
      // Requested file is a directory
      char *index_filename = "/index.html";
      char *index_path = malloc(strlen(filename) + strlen(index_filename) + 1);
      strcpy(index_path, filename);
      strcat(index_path, index_filename);

      struct stat statbuf_index;
      int index_exists = stat(index_path, &statbuf_index);

      if (index_exists == 0) {
        // An index.html exists in the requested directory, serve it
        char buf[1024];
        int in_fd = open(index_path, O_RDONLY);
        
        if (in_fd != -1) {
          // Opened regular file successfully
          char lenBuf[256];
          size_t len = statbuf_index.st_size;
        
          snprintf(lenBuf, sizeof(lenBuf), "%zu", len);
          
          http_start_response(fd, 200);
          http_send_header(fd, "Content-Type", "text/html");
          http_send_header(fd, "Content-Length", lenBuf);
          http_end_headers(fd);

          size_t read_len;
          while ((read_len = read(in_fd, buf, sizeof(buf))) != 0){
            http_send_data(fd, buf, read_len);
          }
          close(in_fd);

          return;
        } else {
          // File failed to open
          char lenBuf[256];
          size_t len;
          char *response = super_strcat(&len, 1, "<h1>Unable to open file.</h1>");
          snprintf(lenBuf, sizeof(lenBuf), "%zu", len);

          http_start_response(fd, 200);
          http_send_header(fd, "Content-Type", "text/html");
          http_send_header(fd, "Content-Length", lenBuf);
          http_end_headers(fd);
          
          http_send_string(fd, response);
          return;
        }
      } else {
        // There is no index.html in the requested directory, list the files inside it
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(filename)) != NULL) {
          // Send HTTP headers
          http_start_response(fd, 200);
          http_send_header(fd, "Content-Type", "text/html");
          http_end_headers(fd);

          // Get all files inside the directory and print links
          while ((ent = readdir(dir)) != NULL) {
            // Don't print current (.) and parent (..) directory links
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
              http_send_string(fd, "<a href=\"");
              http_send_string(fd, ent->d_name);
              http_send_string(fd, "\">");
              http_send_string(fd, ent->d_name);
              http_send_string(fd, "</a><br>");
            }
          }
          closedir(dir);

          http_send_string(fd, "<a href=\"../\">Parent directory</a>");
          return;
        } else {
          http_start_response(fd, 404);
          http_send_header(fd, "Content-Type", "text/html");
          http_end_headers(fd);
          http_send_string(fd,
              "<center>"
              "<h1>Error opening directory.</h1>"
              "</center>");
          return;
        }
      }
    } else {
      // Not a file or a directory, error
      http_start_response(fd, 404);
      http_send_header(fd, "Content-type", "text/html");
      http_end_headers(fd);
      http_send_string(fd, 
                "<center>"
                "<h1>Error finding resource: ");
      http_send_string(fd, filename);
      http_send_string(fd,
                ". Not a file or directory</h1>"
                "</center>");
      return;
    }
  } else {
    // File doesn't exist
    http_start_response(fd, 404);
    http_send_header(fd, "Content-type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, 
              "<center>"
              "<h1>Error finding file: ");
    http_send_string(fd, filename);
    http_send_string(fd,
              ". File doesn't exist</h1>"
              "</center>");
    return;
  }
          

  /*http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", "text/html");
  http_end_headers(fd);
  http_send_string(fd,
      "<center>"
      "<h1>Welcome to httpserver!</h1>"
      "<hr>"
      "<p>Nothing's here yet.</p>"
      "</center>");*/
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */

  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client_socket_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    return;

  }

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
}

typedef void (*callback)(int);

typedef struct threadargs
{
    callback request_handler;
} threadargs;

void *thread_func(void *request_handler) {
  
  threadargs *ta = request_handler;

  while (1) {
    //pthread_t tid = pthread_self();
    //printf("Thread ID: %d - Blocked on wq_pop\n", (int) tid);
    // wq_pop blocks on no work objects in queue
    int client_fd = wq_pop(&work_queue);

    //printf("Thread ID: %d - I am FREE!\n", (int) tid);
    // Handle the request on the returned client socket
    ta->request_handler(client_fd);

    close(client_fd);
  }
  return NULL;
}



void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  wq_init(&work_queue);

  

  pthread_t threads[num_threads];
  // Loop to create num_threads threads
  for (int i = 0; i < num_threads; i++) {
    threadargs *ta = (threadargs*) malloc(sizeof(threadargs));
    ta->request_handler = request_handler;
    //pthread_t curr_thread = (pthread_t) malloc(sizeof(pthread_t));
    pthread_create(&threads[i], NULL, thread_func, ta);
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    // TODO: Change me?
    wq_push(&work_queue, client_socket_number);

    /*
    request_handler(client_socket_number);
    close(client_socket_number);
    */

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
