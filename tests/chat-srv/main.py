#!/usr/bin/env python3
import sys
import socket
import threading
import traceback
import time
import binascii
SERVER_PORT = 8881
PACKET_MTU = 1024
SERVER_UP = False

MODE_TCP = 0
MODE_UDP = 1


class Server:
    def __init__(self):
        try:
            global SERVER_UP
            # Create a socket object
            self.running = False

            self.sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            # self.sock_tcp.settimeout(300)
            # self.sock_udp.settimeout(300)
            # Reserve a port for your service. Default + instance num
            self.clients_tcp = {}
            self.clients_udp = {}
            self.threads = []
            self.sock_tcp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock_udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock_tcp.bind(('', SERVER_PORT))
            self.sock_udp.bind(('', SERVER_PORT))
            SERVER_UP = True
            Client.server = self
            self.tcp_thread = threading.Thread(target=self.run_tcp)
            self.udp_thread = threading.Thread(target=self.run_udp)
            self.tcp_thread.start()
            print(f"server running on port TCP:{SERVER_PORT}")
            self.udp_thread.start()
            print(f"server running on port UDP:{SERVER_PORT}")
        except:
            print(traceback.format_exc(limit=None, chain=True))
            return
        self.fromserver()
        exit(0)

    def fromserver(self):
        global SERVER_UP
        while SERVER_UP:
            try:
                emitthis = input("")
                if (emitthis == "/stop"):
                    print(f"Server shutting down in 5s. Goodbye!")
                    for conn in self.clients_tcp:
                        try:
                            conn.send(
                                bytes(f"Server shutting down in 5s. Goodbye!\0", 'utf-8'))
                        except BrokenPipeError:
                            pass
                    for addr in self.clients_udp:
                        try:
                            self.sock_udp.sendto(
                                bytes(f"Server shutting down in 5s. Goodbye!\0", "utf-8"), self.clients_udp[addr])
                        except BrokenPipeError:
                            pass
                    time.sleep(5)
                    for conn in self.clients_tcp:
                        conn.close()
                    time.sleep(1)
                    SERVER_UP = False
                    break
                else:
                    self.broadcast(f"[server] {emitthis}")
            except:
                print(traceback.format_exc(limit=None, chain=True))
                pass
        self.sock_tcp.close()
        self.sock_udp.close()
        print("Server closed!")
        return

    def run_tcp(self):
        global SERVER_UP
        self.sock_tcp.listen(1)
        while SERVER_UP:
            try:
                conn, addr = self.sock_tcp.accept()
                self.broadcast(f"new client from tcp:{addr}")
                self.sock_udp.sendto(
                    bytes(f"udp test ping\0", 'utf-8'), addr)
                self.clients_tcp[conn] = client = Client(conn, addr)
                thread = threading.Thread(target=client.handle_connection)
                self.threads.append(thread)
                thread.start()
            except:
                print(traceback.format_exc(limit=None, chain=True))
                return
            time.sleep(0.002)
        self.sock_tcp.close()

    def run_udp(self):
        global SERVER_UP
        while SERVER_UP:
            try:
                data, addr = self.sock_udp.recvfrom(
                    PACKET_MTU)  # buffer size is 1024 bytes
                if not addr[0] in self.clients_udp:
                    self.clients_udp[addr[0]] = addr
                    self.broadcast(f"new client from udp:{addr}")
                self.broadcast(data.decode('utf-8'))
            except:
                print(traceback.format_exc(limit=None, chain=True))
                return
            time.sleep(0.002)
        self.sock_udp.close()

    def broadcast(self, data):
        print(f"{data}")
        for conn in self.clients_tcp:
            conn.send(bytes(f"{data}\0", 'utf-8'))
        for addr in self.clients_udp:
            self.sock_udp.sendto(
                bytes(f"{data}\0", 'utf-8'), self.clients_udp[addr])


class ClientDisconnectErr(Exception):
    pass


class Client:
    server = None

    def __init__(self, conn, addr):
        self.conn = conn
        self.ip = addr[0]
        self.port = addr[1]
        return

    def handle_connection(self):
        global SERVER_UP
        self.conn.send(bytes(f"Welcome to the lwIP private beta!\0", 'utf-8'))
        self.conn.send(bytes(
            f"You have connected in TCP mode. Pressing [Mode] on your device should toggle TCP/UDP protocols.\0", 'utf-8'))
        self.data_stream = b''
        self.data_size = 0
        self.type = MODE_TCP
        self.conn.settimeout(300)
        while SERVER_UP:
            try:
                data = self.conn.recv(PACKET_MTU)
                if not data:
                    raise ClientDisconnectErr(
                        f"{self.ip}:{self.port} disconnected!")

                Client.server.broadcast(data.decode('utf-8'))

            except ClientDisconnectErr as e:
                self.conn.close()
                del Client.server.clients_tcp[self.conn]
                Client.server.broadcast(e)
                return

            except:
                print(traceback.format_exc(limit=None, chain=True))
                return


server = Server()
