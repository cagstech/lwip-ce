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


class Server:
    def __init__(self):
        try:
            global SERVER_UP
            # Create a socket object
            self.running = False
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(None)
            # Reserve a port for your service. Default + instance num
            self.clients = {}
            self.threads = []
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind(('', SERVER_PORT))
            SERVER_UP = True
            self.main_thread = threading.Thread(target=self.run)
            self.main_thread.start()
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
                    for conn in self.clients:
                        conn.send(
                            bytes(f"Server shutting down in 5s. Goodbye!\0", 'utf-8'))
                    time.sleep(5)
                    for conn in self.clients:
                        conn.close()
                    time.sleep(1)
                    SERVER_UP = False
                    break
                else:
                    self.broadcast(f"[server] {emitthis}")
            except:
                print(traceback.format_exc(limit=None, chain=True))
                pass
        print("Server closed!")
        return

    def run(self):
        global SERVER_UP
        print(f"server running on port {SERVER_PORT}")
        Client.server = self
        self.sock.listen(1)
        while SERVER_UP:
            try:
                conn, addr = self.sock.accept()
                self.broadcast(f"new client from {addr}")
                self.clients[conn] = client = Client(conn, addr)
                thread = threading.Thread(target=client.handle_connection)
                self.threads.append(thread)
                thread.start()
            except:
                print(traceback.format_exc(limit=None, chain=True))
                return
            time.sleep(0.002)
        self.sock.close()

    def broadcast(self, data):
        print(f"{data}")
        for conn in self.clients:
            conn.send(bytes(f"{data}\0", 'utf-8'))


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
        self.conn.send(bytes(f"welcome to the lwIP private beta!\0", 'utf-8'))
        self.data_stream = b''
        self.data_size = 0
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
                del Client.server.clients[self.conn]
                Client.server.broadcast(e)
                return

            except:
                print(traceback.format_exc(limit=None, chain=True))
                return


server = Server()
