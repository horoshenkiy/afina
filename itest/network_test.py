import socket
import unittest
import time
from concurrent.futures import ThreadPoolExecutor


class IntegrationTest(unittest.TestCase):

    def testSimple(self):
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('localhost', 8080))

        client.send(b"set foo 0 0 6\r\nfoobar\r\n")
        res = client.recv(4096)
        self.assertEqual(res, b'STORED\r\n')

        client.send(b"get foo\r\n")
        res = client.recv(4096)
        self.assertEqual(res, b"VALUE foo 0 6\r\nfoobar\r\nEND\r\n")

        client.close()

    def testMultipleSetInTP(self):

        def task(el):
            client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client.connect(('localhost', 8080))

            # set command
            set_command = "set foo{} 0 0 6\r\nfoobar\r\n".format(el)
            client.send(set_command.encode('utf-8'))

            res = client.recv(4096)
            self.assertEqual(res, b'STORED\r\n')

            # get command
            get_command = "get foo{}\r\n".format(el)
            client.send(get_command.encode('utf-8'))
            res = client.recv(4096)

            wait_result = "VALUE foo{} 0 6\r\nfoobar\r\nEND\r\n".format(el)
            self.assertEqual(res, wait_result.encode('utf-8'))

            client.close()

        tp = ThreadPoolExecutor(max_workers=32)

        res = []
        for i in range(100):
            res.append(tp.submit(task, i))

        [el.result() for el in res]

    def testMultipleCommands(self):

        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('localhost', 8080))

        client.send(
            b"set foo_mt 0 0 3\r\nwtf\r\n"
            b"set bar_mt 0 0 3\r\nzzz\r\n"
            b"get foo_mt bar_mt\r\n"
        )

        time.sleep(0.001)

        res = client.recv(4096)
        self.assertEqual(
            res,
            b"STORED\r\n"
            b"STORED\r\n"
            b"VALUE foo_mt 0 3\r\nwtf\r\n"
            b"VALUE bar_mt 0 3\r\nzzz\r\n"
            b"END\r\n"
        )

        client.close()

    def testAdd(self):
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('localhost', 8080))

        client.send(b"add test 0 0 6\r\nfoobar\r\n")
        client.recv(4096)

        client.send(b"add test 0 0 6\r\nfoobar\r\n")
        res = client.recv(4096)
        self.assertEqual(res, b'NOT_STORED\r\n')

        client.close()

    def testDirty(self):
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('localhost', 8080))

        client.send(b"blablabla 0 0 0\r\n")
        res = client.recv(4096)
        self.assertEqual(res, b'CLIENT_ERROR Unknown command name\r\n')

        client.close()

    def testDirtyGet(self):
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('localhost', 8080))

        client.send(b"get var\r\n")
        res = client.recv(4096)
        self.assertEqual(res, b'END\r\n')

        client.close()

    def testPartCommand(self):
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('localhost', 8080))

        client.send(b"set foo 0 ")
        client.send(b"0 3\r")
        client.send(b"\nwtf\r\n")
        res = client.recv(4096)
        self.assertEqual(res, b'STORED\r\n')

        client.close()


if __name__ == '__main__':
    unittest.main()
