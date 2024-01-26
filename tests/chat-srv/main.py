#!/usr/bin/env python3

import socket
import threading
import traceback
import time
import binascii
SERVER_PORT = 8881
PACKET_MTU = 1024


class Server:
    def __init__(self):
        try:
            # Create a socket object
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(None)
            # Reserve a port for your service. Default + instance num
            self.clients = {}
            self.threads = []
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind(('', SERVER_PORT))
            self.main_thread = threading.Thread(target=self.run)
            self.main_thread.start()
            self.running = True
        except:
            print(traceback.format_exc(limit=None, chain=True))
            return

    def run(self):
        print(f"server running on port {SERVER_PORT}")
        Client.server = self
        while self.running:
            try:
                self.sock.listen(1)
                conn, addr = self.sock.accept()
                self.clients[conn] = client = Client(conn, addr)
                thread = threading.Thread(target=client.handle_connection)
                self.threads.append(thread)
                thread.start()
            except:
                print(traceback.format_exc(limit=None, chain=True))
                return
            time.sleep(0.002)

    def broadcast(self, data):
        print(f"{data}\n")


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
        server.broadcast(f"new client from {self.ip}:{self.port}")
        self.data_stream = b''
        self.data_size = 0
        self.conn.settimeout(300)
        while Client.server.running:
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
server.run()
