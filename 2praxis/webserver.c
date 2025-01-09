#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "data.h"
#include "http.h"
#include "util.h"

#define MAX_RESOURCES 100
#define MAX_KNOWN_HOSTS 10

struct tuple resources[MAX_RESOURCES] = {
    {"/static/foo", "Foo", sizeof "Foo" - 1},
    {"/static/bar", "Bar", sizeof "Bar" - 1},
    {"/static/baz", "Baz", sizeof "Baz" - 1}};


//LULA BEGINNT
typedef struct {
    uint16_t node_id;
    uint16_t predecessor_id;
    uint16_t successor_id;
    const char *predecessor_ip;
    const char *successor_ip;
    int predecessor_port;
    int successor_port;
    const char *self_ip;
    int self_port;
} NodeConfig;

NodeConfig config;
int udp_socket;


typedef struct {
  bool present;
  uint16_t predecessor_id;
  uint16_t id;
  char addr[16];
  uint16_t port;
} node;

node *known_hosts;

void chortonet(uint8_t *buffer, uint8_t *out) {
  for (int i = 0; i < 11; i++) {
    out[10-i] = buffer[i];  
  }

  return;
}

void nettochor(uint8_t *buffer, uint8_t *out) {
  for (int i = 0; i < 11; i++) {
    out[10-i] = buffer[i];  
  }

  return;
}

void pack_chord(uint8_t type, uint16_t hash_id, uint16_t node_id, const char* node_ip, uint16_t port, uint8_t* buffer) {
  memset(buffer, 0x0, 11);

  uint16_t network_order_hash_id = htons(hash_id);
  uint16_t network_order_node_id = htons(node_id);
  uint16_t network_order_port = htons(port);
  uint32_t network_order_ip_addr;
  inet_pton(AF_INET, node_ip, &network_order_ip_addr);

  buffer[0] = type;

  memcpy(buffer+1, &network_order_hash_id, 2);
  memcpy(buffer+3, &network_order_node_id, 2);
  memcpy(buffer+5, &network_order_ip_addr, 4);
  memcpy(buffer+9, &network_order_port, 2);

  return;
}

void send_lookup_request(uint16_t hash) { //, int udp_socket) {
    uint8_t buffer[11];
    memset(buffer, 0x0, 11);

    pack_chord(0, hash, config.node_id, config.self_ip, config.self_port, buffer);

    fprintf(stderr, "[DEBUG] sending lookup for %d\n", (uint16_t)((buffer[1] << 8) | buffer[2]));

    struct sockaddr_in successor_addr = {0};
    successor_addr.sin_family = AF_INET;
    successor_addr.sin_port = htons(config.successor_port);
    if (inet_pton(AF_INET, config.successor_ip, &successor_addr.sin_addr) != 1) {
        perror("Fehler bei der Umwandlung der Nachfolger-IP-Adresse");
        return;
    }

    // Nachricht senden
    ssize_t sent_bytes = sendto(udp_socket, buffer, 11, 0, 
                                (struct sockaddr *)&successor_addr, sizeof(successor_addr));
    if (sent_bytes == -1) {
        perror("Fehler beim Senden der Lookup-Nachricht");
    } 
}

bool correct_node(uint16_t hash, uint16_t node_id, uint16_t predecessor_id) {
    int responsible = false;

    if (predecessor_id < node_id && hash > predecessor_id && hash <= node_id) {
        responsible = true;
    } else if (predecessor_id >= node_id && (hash > predecessor_id || hash <= node_id)) {
        responsible = true;
    }

    return responsible;
}

void handle_udp(uint8_t *buffer) { //, size_t length, struct sockaddr_in *client_addr) { //, int udp_socket) {
    uint16_t hash;
    hash = (uint16_t)(buffer[1] << 8) | buffer[2];

    if (correct_node(hash, config.node_id, config.predecessor_id)) {
        // A Node atual é responsável: envie uma resposta Reply
        uint8_t reply[11];
        memset(reply, 1, 11);
        pack_chord(1, 
                  config.predecessor_id, 
                  config.node_id, 
                  config.self_ip, 
                  config.self_port, 
                  reply);

        struct sockaddr_in requester = {0};
        requester.sin_family = AF_INET;
        memcpy(&requester.sin_port, buffer + 9, 2);
        memcpy(&requester.sin_addr, buffer + 5, 4);

        sendto(udp_socket, reply, sizeof(reply), 0, (struct sockaddr *)&requester, sizeof(requester));
    } else if (correct_node(hash, config.successor_id, config.node_id)) {
        // Sucessora é responsável: envie uma resposta Reply com dados da sucessora
        uint8_t reply[11];
        pack_chord(1,
                   config.node_id,
                   config.successor_id,
                   config.successor_ip,
                   config.successor_port,
                   reply);

        struct sockaddr_in requester = {0};
        requester.sin_family = AF_INET;
        memcpy(&requester.sin_port, buffer + 9, 2);
        memcpy(&requester.sin_addr, buffer + 5, 4);

        sendto(udp_socket, reply, sizeof(reply), 0, (struct sockaddr *)&requester, sizeof(requester));

    } else {
        // Encaminhar Lookup para o sucessor
        struct sockaddr_in successor_addr;
        successor_addr.sin_family = AF_INET;
        successor_addr.sin_port = htons(config.successor_port);
        inet_pton(AF_INET, config.successor_ip, &successor_addr.sin_addr);

        sendto(udp_socket, buffer, 11, 0, (struct sockaddr *)&successor_addr, sizeof(successor_addr));
    }
}

//LULA ENDET


/**
 * Sends an HTTP reply to the client based on the received request.
 *
 * @param conn      The file descriptor of the client connection socket.
 * @param request   A pointer to the struct containing the parsed request
 * information.
 */
void send_reply(int conn, struct request *request) {

    // Create a buffer to hold the HTTP reply
    char buffer[HTTP_MAX_SIZE];
    char *reply = buffer;
    size_t offset = 0;

    //LULA BEGINNT
    uint16_t hash = pseudo_hash((const unsigned char *)request->uri, strlen(request->uri));

    size_t resource_length = 0;
    const char *resource = NULL;

    fprintf(stderr, "[DEBUG: %d] received %s for %s with hash %d\n", config.node_id, request->method, request->uri, hash);

    fprintf(stderr, "[DEBUG: %d] first known host is %d with id %d and IP %s\n", config.node_id, known_hosts[0].present, known_hosts[0].id, known_hosts[0].addr);


    if (correct_node(hash, config.node_id, config.predecessor_id) == 0) {
      // successor responsible
      if (correct_node(hash, config.successor_id, config.node_id) == 1) {
        offset = sprintf(reply, "HTTP/1.1 303 See Other\r\nLocation: http://%s:%d%s\r\nContent-Length: 0\r\n\r\n", config.successor_ip, config.successor_port, request->uri);
      } else { //if (known_hash != config.node_id) {
        bool host_found = false;

        for (int i = 0; i < MAX_KNOWN_HOSTS; i++) {
          if (correct_node(hash, known_hosts[i].id, known_hosts[i].predecessor_id) && known_hosts[i].present == true) {
            offset = sprintf(reply, "HTTP/1.1 303 See Other\r\nLocation: http://%s:%d%s\r\nContent-Length: 0\r\n\r\n", known_hosts[i].addr, known_hosts[i].port, request->uri);
            host_found = true;
            break;
          }
        }

        if (!host_found) {
          fprintf(stderr, "[DEBUG] Host not found");
          offset = sprintf(reply, "HTTP/1.1 503 Service Unavailable\r\nRetry-After: 1\r\nContent-Length: 0\r\n\r\n");
          send_lookup_request(hash); //, udp_socket);
          //sleep(1);
        }
       
      }

    } else {
    if (strcmp(request->method, "GET") == 0) {
        resource = get(request->uri, resources, MAX_RESOURCES, &resource_length);
        if (resource) {
            size_t payload_offset = sprintf(reply, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n\r\n", resource_length);
            memcpy(reply + payload_offset, resource, resource_length);
            offset = payload_offset + resource_length;
            //offset = strlen(reply);

        } else {
            reply = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            offset = strlen(reply);
        }
    } else if (strcmp(request->method, "PUT") == 0) {
        // Try to set the requested resource with the given payload in the
        // 'resources' array.
        if (set(request->uri, request->payload, request->payload_length,
                resources, MAX_RESOURCES)) {
            reply = "HTTP/1.1 204 No Content\r\n\r\n";
        } else {
            reply = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
        }
        offset = strlen(reply);
    } else if (strcmp(request->method, "DELETE") == 0) {
        // Try to delete the requested resource from the 'resources' array
        if (delete (request->uri, resources, MAX_RESOURCES)) {
            reply = "HTTP/1.1 204 No Content\r\n\r\n";
        } else {
            reply = "HTTP/1.1 404 Not Found\r\n\r\n";
        }
        offset = strlen(reply);
    } else {
        reply = "HTTP/1.1 501 Method Not Supported\r\n\r\n";
        offset = strlen(reply);
    }
    }

    // Send the reply back to the client
    if (send(conn, reply, offset, 0) == -1) {
        perror("send");
        close(conn);
    }
}

/**
 * Processes an incoming packet from the client.
 *
 * @param conn The socket descriptor representing the connection to the client.
 * @param buffer A pointer to the incoming packet's buffer.
 * @param n The size of the incoming packet.
 *
 * @return Returns the number of bytes processed from the packet.
 *         If the packet is successfully processed and a reply is sent, the
 * return value indicates the number of bytes processed. If the packet is
 * malformed or an error occurs during processing, the return value is -1.
 *
 */
ssize_t process_packet(int conn, char *buffer, size_t n) {
    struct request request = {
        .method = NULL, .uri = NULL, .payload = NULL, .payload_length = -1};
    ssize_t bytes_processed = parse_request(buffer, n, &request);

    if (bytes_processed > 0) {
        send_reply(conn, &request);

        // Check the "Connection" header in the request to determine if the
        // connection should be kept alive or closed.
        const string connection_header = get_header(&request, "Connection");
        if (connection_header && strcmp(connection_header, "close")) {
            return -1;
        }
    } else if (bytes_processed == -1) {
        // If the request is malformed or an error occurs during processing,
        // send a 400 Bad Request response to the client.
        fprintf(stderr, "[DEBUG: %d] %s\n", config.node_id, buffer);
        const string bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(conn, bad_request, strlen(bad_request), 0);
        printf("Received malformed request, terminating connection.\n");
        close(conn);
        return -1;
    }

    return bytes_processed;
}

/**
 * Sets up the connection state for a new socket connection.
 *
 * @param state A pointer to the connection_state structure to be initialized.
 * @param sock The socket descriptor representing the new connection.
 *
 */
static void connection_setup(struct connection_state *state, int sock) {
    // Set the socket descriptor for the new connection in the connection_state
    // structure.
    state->sock = sock;

    // Set the 'end' pointer of the state to the beginning of the buffer.
    state->end = state->buffer;

    // Clear the buffer by filling it with zeros to avoid any stale data.
    memset(state->buffer, 0, HTTP_MAX_SIZE);
}

/**
 * Discards the front of a buffer
 *
 * @param buffer A pointer to the buffer to be modified.
 * @param discard The number of bytes to drop from the front of the buffer.
 * @param keep The number of bytes that should be kept after the discarded
 * bytes.
 *
 * @return Returns a pointer to the first unused byte in the buffer after the
 * discard.
 * @example buffer_discard(ABCDEF0000, 4, 2):
 *          ABCDEF0000 ->  EFCDEF0000 -> EF00000000, returns pointer to first 0.
 */
char *buffer_discard(char *buffer, size_t discard, size_t keep) {
    memmove(buffer, buffer + discard, keep);
    memset(buffer + keep, 0, discard); // invalidate buffer
    return buffer + keep;
}

/**
 * Handles incoming connections and processes data received over the socket.
 *
 * @param state A pointer to the connection_state structure containing the
 * connection state.
 * @return Returns true if the connection and data processing were successful,
 * false otherwise. If an error occurs while receiving data from the socket, the
 * function exits the program.
 */
//bool handle_connection(struct connection_state *state) {
bool handle_connection(int sock, struct connection_state *state) {
    // Calculate the pointer to the end of the buffer to avoid buffer overflow
    const char *buffer_end = state->buffer + HTTP_MAX_SIZE;

    // Check if an error occurred while receiving data from the socket

    ssize_t bytes_read = recv(sock, state->end, buffer_end - state->end, 0);
    if (bytes_read == -1) {
        perror("recv");
        close(sock);
        return false;
        //exit(EXIT_FAILURE);
    } else if (bytes_read == 0) {
        return false;
    }

    char *window_start = state->buffer;
    char *window_end = state->end + bytes_read;

    ssize_t bytes_processed = 0;
    while ((bytes_processed = process_packet(sock, window_start,
                                             window_end - window_start)) > 0) {
        window_start += bytes_processed;
    }

    if (bytes_processed == -1) {
        return false;
    }

    state->end = buffer_discard(state->buffer, window_start - state->buffer,
                                window_end - window_start);
    return true;
}

/**
 * Derives a sockaddr_in structure from the provided host and port information.
 *
 * @param host The host (IP address or hostname) to be resolved into a network
 * address.
 * @param port The port number to be converted into network byte order.
 *
 * @return A sockaddr_in structure representing the network address derived from
 * the host and port.
 */
static struct sockaddr_in derive_sockaddr(const char *host, const char *port) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
    };
    struct addrinfo *result_info;

    // Resolve the host (IP address or hostname) into a list of possible
    // addresses.
    int returncode = getaddrinfo(host, port, &hints, &result_info);
    if (returncode) {
        fprintf(stderr, "Error parsing host/port");
        exit(EXIT_FAILURE);
    }

    // Copy the sockaddr_in structure from the first address in the list
    struct sockaddr_in result = *((struct sockaddr_in *)result_info->ai_addr);

    // Free the allocated memory for the result_info
    freeaddrinfo(result_info);
    return result;
}

//LULA BEGINNT
void setup_udp_socket(struct sockaddr_in addr) {
    const int disable = 1;
    const int enable = 1;

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == -1) {
        perror("UDP-Socket");
        exit(EXIT_FAILURE);
    }

    if (fcntl(udp_socket, F_SETFL, O_NONBLOCK) == -1) {
      perror("fcntl");
      exit(EXIT_FAILURE);
    }

    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
      perror("setsockopt failed");
      close(udp_socket);
      exit(EXIT_FAILURE);
    }

    if (bind(udp_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Bind UDP-Socket");
        close(udp_socket);
        fprintf(stderr, "DEBUG: Bind UDP-Socket: Address already in use\r\nBind failed with errno=%d\n", errno);
        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "DEBUG: UDP-Socket erfolgreich gebunden an %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    }

    //return udp_socket;
}
//LULA ENDET

/**
 * Sets up a TCP server socket and binds it to the provided sockaddr_in address.
 *
 * @param addr The sockaddr_in structure representing the IP address and port of
 * the server.
 *
 * @return The file descriptor of the created TCP server socket.
 */
static int setup_server_socket(struct sockaddr_in addr) {
    const int enable = 1;
    const int backlog = 1;

    // Create a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("TCP-Socket");
        exit(EXIT_FAILURE);
    }

    // Avoid dead lock on connections that are dropped after poll returns but
    // before accept is called
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR socket option to allow reuse of local addresses
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) ==
        -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind socket to the provided address
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Bind TCP-Socket");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Start listening on the socket with maximum backlog of 1 pending
    // connection
    if (listen(sock, backlog)) {
        perror("Listen TCP-Socket");
        exit(EXIT_FAILURE);
    }

    return sock;
}

void get_environment() {
  config.predecessor_id = (getenv("PRED_ID")) ? (uint16_t)atoi(getenv("PRED_ID")) : 0;
  config.successor_id = (getenv("SUCC_ID")) ? (uint16_t)atoi(getenv("SUCC_ID")) : 0;

  config.predecessor_ip = (getenv("PRED_IP")) ? getenv("PRED_IP") : "";
  config.successor_ip = (getenv("SUCC_IP")) ? getenv("SUCC_IP") : "";

  config.predecessor_port = (getenv("PRED_PORT")) ? atoi(getenv("PRED_PORT")) : 0;
  config.successor_port = (getenv("SUCC_PORT")) ? atoi(getenv("SUCC_PORT")) : 0;
}

/**
 *  The program expects 3; otherwise, it returns EXIT_FAILURE.
 *
 *  Call as:
 *
 *  ./build/webserver self.ip self.port
 */
int main(int argc, char **argv) {
    if (argc < 2) {
      fprintf(stderr, "You must supply an IP and a Port\n");
      return EXIT_FAILURE;
    }

    memset(&config, 0, sizeof(config));

    config.self_ip = argv[1];
    config.self_port = atoi(argv[2]);

    struct sockaddr_in addr = derive_sockaddr(config.self_ip, argv[2]);

    int server_socket = setup_server_socket(addr);
    setup_udp_socket(addr);

    printf("UDP-Socket läuft auf %s:%s\n", config.self_ip, argv[2]);


    if (argc == 3) {
      printf("[Warning] No Node id was given");
      config.node_id = 0;
    } else {
      config.node_id = atoi(argv[3]);
    }
   
    get_environment();
    printf("Node ID: %u\n", config.node_id);
    printf("Predecessor: ID=%u, IP=%s, Port=%d\n", config.predecessor_id, config.predecessor_ip, config.predecessor_port);
    printf("Successor: ID=%u, IP=%s, Port=%d\n", config.successor_id, config.successor_ip, config.successor_port);
    //LULA ENDET
    int MAX_CLIENTS = 1;
    struct pollfd sockets[MAX_CLIENTS+2]; 
    memset(sockets, 0x0, sizeof(struct pollfd) * (MAX_CLIENTS+2));

    sockets[0].fd = server_socket;
    sockets[0].events = POLLIN;
    sockets[1].fd = udp_socket;
    sockets[1].events = POLLIN;

    known_hosts = (node *)malloc(sizeof(node)*MAX_KNOWN_HOSTS);

    int kh_head = 0;

    struct connection_state state = {0};

    while (true) {
        int poll_res = poll(sockets, MAX_CLIENTS+2, -1);

        if (poll_res > 0) {
          // check for new connection on tcp
          if (sockets[0].revents & POLLIN) {
            int client_sock = accept(server_socket, NULL, NULL);

            connection_setup(&state, client_sock);
            // socket to poll
            for (int i = 2; i < MAX_CLIENTS+2; i++) {
              if (sockets[i].fd == 0) {
                sockets[i].fd = client_sock;
                sockets[i].events = POLLIN;
                break;
              }
            }
          }

          // check for data on UDP
          if (sockets[1].revents & POLLIN) {
            fprintf(stderr, "[DEBUG] Starting to handle UDP\n");

            uint8_t buffer[11];
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            ssize_t received = recvfrom(udp_socket, buffer, sizeof(buffer), 0,
                                           (struct sockaddr *)&client_addr, &addr_len);
            if (received < 11) {
              perror("Fehler beim Empfangen über UDP\n");
              continue;
            }

            //handle received data
            uint8_t flag = (uint8_t)buffer[0];
            uint16_t hash = (uint16_t)((buffer[1] << 8) | buffer[2]);

            if (flag == 1) { 
              fprintf(stderr, "[DEBUG: %d] Got Reply. head at %d\n", config.node_id, kh_head);
              char addr[16];      

              sprintf(addr, "%d.%d.%d.%d", (uint8_t)buffer[5], (uint8_t)buffer[6], (uint8_t)buffer[7], (uint8_t)buffer[8]);

              node new_node;
              new_node.present = true;
              memcpy(new_node.addr, addr, 16);
              new_node.id = (uint16_t)((buffer[3] << 8) | buffer[4]);
              new_node.predecessor_id = (uint16_t)((buffer[1] << 8) | buffer[2]);
              new_node.port = (uint16_t)((buffer[9] << 8) | buffer[10]);

              memcpy(&(known_hosts[kh_head]), &new_node, sizeof(node));

              fprintf(stderr, "[DEBUG: %d] Added Node %d with predecessor %d\n", config.node_id, known_hosts[kh_head].id, known_hosts[kh_head].predecessor_id);
              kh_head++;
              kh_head = kh_head % MAX_KNOWN_HOSTS;
            } else  if (flag == 0) {
              handle_udp(buffer);
            } else {
              fprintf(stderr, "[Warning] Unknonwn DHT Flag\n");
            }

            sockets[1].revents = 0;
          }

          // check remainging
          for (int i = 2; i < MAX_CLIENTS+2; i++) {
            if (sockets[i].fd > 0 && sockets[i].revents & POLLIN) {
              bool cont = handle_connection(sockets[i].fd, &state);

              if (!cont) {
                sockets[0].events = POLLIN;
                sockets[i].fd = 0;
                sockets[i].events = 0;
              }
            }
          }
        }
     }
    
    close(udp_socket); //(LULA)
    return EXIT_FAILURE;
}
