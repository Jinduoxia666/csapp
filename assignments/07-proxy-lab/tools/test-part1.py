#!/usr/bin/env python3
"""本机回环网络集成测试：验证转发、并发和缓存，不依赖公网。"""

import contextlib
import concurrent.futures
import http.server
import pathlib
import queue
import socket
import struct
import subprocess
import tempfile
import threading
import time
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
BINARY = bytes(range(256)) * 1024
REQUESTS = queue.Queue()


class Origin(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        REQUESTS.put((self.path, self.request_version, list(self.headers.items())))
        if self.path == "/reset":
            self.connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                       struct.pack("ii", 1, 0))
            self.close_connection = True
            return
        if self.path == "/chunked":
            self.connection.sendall(
                b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
                b"Connection: close\r\n\r\n3\r\na\x00b\r\n0\r\n\r\n")
            return
        if self.path == "/cache-truncated":
            self.connection.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 10\r\n\r\nshort")
            return
        if self.path == "/cache-badchunk":
            self.connection.sendall(b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n")
            return
        body = BINARY if self.path == "/binary" else self.path.encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Type", "application/octet-stream")
        self.end_headers()
        with contextlib.suppress(BrokenPipeError, ConnectionResetError):
            self.wfile.write(body)


class ProxyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.origin = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Origin)
        cls.origin.daemon_threads = True
        cls.worker = threading.Thread(target=cls.origin.serve_forever, daemon=True)
        cls.worker.start()
        cls.origin_port = cls.origin.server_port
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            cls.proxy_port = reserve.getsockname()[1]
        cls.log = tempfile.TemporaryFile()
        cls.proxy = subprocess.Popen([str(ROOT / "proxy"), str(cls.proxy_port)],
                                     stdout=cls.log, stderr=cls.log)
        for _ in range(100):
            if cls.proxy.poll() is not None:
                raise RuntimeError("代理未能启动")
            try:
                with socket.create_connection(("127.0.0.1", cls.proxy_port), .1):
                    return
            except OSError:
                time.sleep(.02)
        raise RuntimeError("等待代理监听超时")

    @classmethod
    def tearDownClass(cls):
        cls.proxy.terminate()
        cls.proxy.wait(timeout=3)
        cls.log.close()
        cls.origin.shutdown()
        cls.origin.server_close()
        cls.worker.join()

    def setUp(self):
        while not REQUESTS.empty():
            REQUESTS.get_nowait()

    def exchange(self, request):
        with socket.create_connection(("127.0.0.1", self.proxy_port), 3) as client:
            client.sendall(request)
            client.shutdown(socket.SHUT_WR)
            result = bytearray()
            while True:
                try:
                    block = client.recv(65536)
                except ConnectionResetError:
                    break
                if not block:
                    return bytes(result)
                result.extend(block)
            return bytes(result)

    def get(self, path="/hello", headers=b"", version="HTTP/1.1"):
        return self.exchange(
            f"GET http://127.0.0.1:{self.origin_port}{path} {version}\r\n".encode()
            + headers + b"\r\n")

    def assert_healthy(self):
        self.assertIsNone(self.proxy.poll())
        self.assertEqual(self.get().split(b"\r\n\r\n", 1)[1], b"/hello")

    def test_request_rewrite_and_headers(self):
        response = self.get("/hello?q=2&x=3", b"Host: custom.example:1234\r\n"
                            b"User-Agent: old\r\nConnection: keep-alive\r\n"
                            b"Proxy-Connection: keep-alive\r\nX-Test: preserved\r\n")
        self.assertTrue(response.startswith(b"HTTP/1.0 200"))
        path, version, headers = REQUESTS.get(timeout=1)
        self.assertEqual((path, version), ("/hello?q=2&x=3", "HTTP/1.0"))
        values = {}
        for name, value in headers:
            values.setdefault(name.lower(), []).append(value)
        self.assertEqual(values["host"], ["custom.example:1234"])
        self.assertEqual(values["connection"], ["close"])
        self.assertEqual(values["proxy-connection"], ["close"])
        self.assertEqual(values["x-test"], ["preserved"])
        self.assertEqual(values["user-agent"], [
            "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3"])

    def test_missing_host_and_empty_path(self):
        self.get("")
        path, version, headers = REQUESTS.get(timeout=1)
        self.assertEqual(path, "/")
        self.assertIn(("Host", f"127.0.0.1:{self.origin_port}"), headers)

    def test_query_without_slash(self):
        self.assertEqual(self.get("?a=1").split(b"\r\n\r\n", 1)[1], b"/?a=1")

    def test_binary_larger_than_cache_object_limit(self):
        self.assertEqual(self.get("/binary").split(b"\r\n\r\n", 1)[1], BINARY)

    def test_chunked_passthrough(self):
        self.assertEqual(self.get("/chunked"),
                         b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
                         b"Connection: close\r\n\r\n3\r\na\x00b\r\n0\r\n\r\n")

    def test_http10_and_case_insensitive_headers(self):
        self.get(headers=b"hOsT: keep.example\r\ncOnNeCtIoN: keep-alive\r\n",
                 version="HTTP/1.0")
        _, _, headers = REQUESTS.get(timeout=1)
        self.assertIn(("hOsT", "keep.example"), headers)
        self.assertNotIn(("cOnNeCtIoN", "keep-alive"), headers)

    def test_malformed_requests_and_recovery(self):
        requests = [
            b"bad\r\n\r\n",
            b"GET http://localhost:99999/ HTTP/1.1\r\n\r\n",
            b"GET http://localhost/ HTTP/1.1 EXTRA\r\n\r\n",
            b"GET http://localhost/ HTTP/1.1\r\nBad Header: x\r\n\r\n",
            b"GET http://localhost/ HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n",
            b"GET http://localhost/ HTTP/1.1\r\nContent-Length: 5\r\n\r\n",
            b"GET http://localhost/ HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",
            b"GET http://local\x00host/ HTTP/1.1\r\n\r\n",
            b"GET http://localhost/ HTTP/1.1\r\nUnfinished",
            b"GET http://localhost/" + b"a" * 9000 + b" HTTP/1.1\r\n\r\n",
        ]
        for request in requests:
            with self.subTest(request=request[:80]):
                self.assertTrue(self.exchange(request).startswith(b"HTTP/1.0 400"))
                self.assert_healthy()

    def test_unsupported_method(self):
        self.assertTrue(self.exchange(b"POST http://localhost/ HTTP/1.1\r\n\r\n")
                        .startswith(b"HTTP/1.0 501"))
        self.assert_healthy()

    def test_header_limit(self):
        response = self.get(headers=(b"X-Test: " + b"a" * 1000 + b"\r\n") * 70)
        self.assertTrue(response.startswith(b"HTTP/1.0 431"))
        self.assert_healthy()

    def test_refused_connection(self):
        with socket.socket() as unused:
            unused.bind(("127.0.0.1", 0))
            port = unused.getsockname()[1]
        # 先释放端口；macOS 上只绑定而不监听的 socket 可能导致连接等待。
        response = self.exchange(f"GET http://127.0.0.1:{port}/ HTTP/1.0\r\n\r\n".encode())
        self.assertTrue(response.startswith(b"HTTP/1.0 502"))
        self.assert_healthy()

    def test_peer_disconnects(self):
        self.get("/reset")
        self.assert_healthy()
        with socket.create_connection(("127.0.0.1", self.proxy_port), 3) as client:
            client.sendall(f"GET http://127.0.0.1:{self.origin_port}/binary HTTP/1.0\r\n\r\n".encode())
            client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        self.assert_healthy()

    def test_default_port(self):
        try:
            origin = http.server.ThreadingHTTPServer(("127.0.0.1", 80), Origin)
        except OSError as error:
            self.skipTest(f"无法绑定测试所需的 80 端口：{error}")
        worker = threading.Thread(target=origin.serve_forever, daemon=True)
        worker.start()
        try:
            response = self.exchange(b"GET http://127.0.0.1/default HTTP/1.0\r\n\r\n")
            self.assertEqual(response.split(b"\r\n\r\n", 1)[1], b"/default")
        finally:
            origin.shutdown()
            origin.server_close()
            worker.join()

    def test_url_boundaries(self):
        # 在不占用特权端口的情况下验证默认 80 端口及 URL 边界。
        with tempfile.TemporaryDirectory() as directory:
            executable = pathlib.Path(directory) / "test-url"
            subprocess.run(["cc", "-g", "-Wall", str(ROOT / "tools/test-url.c"),
                            str(ROOT / "csapp.o"), str(ROOT / "cache.o"), "-lpthread", "-o", str(executable)],
                           check=True)
            subprocess.run([str(executable)], check=True)

    def test_cache_rules_and_concurrency(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = pathlib.Path(directory) / "test-cache"
            subprocess.run(["cc", "-g", "-Wall", "-fsanitize=address,undefined",
                            str(ROOT / "tools/test-cache.c"), "-lpthread",
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True, timeout=30)

    def test_cache_hit_after_origin_stops(self):
        # 独立源服务器关闭后，缓存仍须提供字节完全相同的响应。
        origin = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Origin)
        worker = threading.Thread(target=origin.serve_forever, daemon=True)
        worker.start()
        request = f"GET http://127.0.0.1:{origin.server_port}/cached HTTP/1.0\r\n\r\n".encode()
        try:
            first = self.exchange(request)
            self.assertTrue(first.startswith(b"HTTP/1.0 200"))
        finally:
            origin.shutdown()
            origin.server_close()
            worker.join()
        self.assertEqual(self.exchange(request), first)

    def test_cache_oversized_and_truncated_bypass(self):
        for path in ("/binary", "/cache-truncated", "/cache-badchunk"):
            with self.subTest(path=path):
                self.get(path)
                self.get(path)
                seen = [REQUESTS.get(timeout=1)[0] for _ in range(2)]
                self.assertEqual(seen, [path, path])

    def test_cache_complete_chunked_response(self):
        headers = b"X-Test: cache-chunked\r\n"
        first = self.get("/chunked", headers)
        self.assertEqual(REQUESTS.get(timeout=1)[0], "/chunked")
        self.assertEqual(self.get("/chunked", headers), first)
        self.assertTrue(REQUESTS.empty())

    def test_cache_distinguishes_headers_and_query(self):
        for path, host in (("/cache-key?q=1", "one.example"),
                           ("/cache-key?q=1", "two.example"),
                           ("/cache-key?q=2", "one.example")):
            headers = f"Host: {host}\r\n".encode()
            first = self.get(path, headers)
            self.assertEqual(REQUESTS.get(timeout=1)[0], path)
            self.assertEqual(self.get(path, headers), first)
            self.assertTrue(REQUESTS.empty())

    def test_concurrent_cache_hits(self):
        expected = self.get("/cache-many")
        self.assertEqual(REQUESTS.get(timeout=1)[0], "/cache-many")
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            responses = list(pool.map(lambda _: self.get("/cache-many"), range(128)))
        self.assertTrue(all(response == expected for response in responses))
        self.assertTrue(REQUESTS.empty())

    def test_blocked_upstream_does_not_block_other_clients(self):
        with socket.socket() as blocked:
            blocked.bind(("127.0.0.1", 0))
            blocked.listen(1)
            blocked.settimeout(3)
            port = blocked.getsockname()[1]
            with socket.create_connection(("127.0.0.1", self.proxy_port), 3) as slow:
                slow.sendall(f"GET http://127.0.0.1:{port}/slow HTTP/1.0\r\n\r\n".encode())
                upstream, _ = blocked.accept()
                with upstream:
                    upstream.settimeout(3)
                    request = bytearray()
                    while not request.endswith(b"\r\n\r\n"):
                        data = upstream.recv(8192)
                        self.assertTrue(data)
                        request.extend(data)
                    # 已确认代理在等上游响应；此时另一条请求必须能完成。
                    self.assert_healthy()
                    upstream.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 4\r\n\r\nslow")
                    upstream.shutdown(socket.SHUT_WR)
                    response = bytearray()
                    while data := slow.recv(8192):
                        response.extend(data)
                    self.assertTrue(response.endswith(b"\r\n\r\nslow"))

    def test_incomplete_client_does_not_block_other_clients(self):
        with socket.create_connection(("127.0.0.1", self.proxy_port), 3) as slow:
            slow.sendall(f"GET http://127.0.0.1:{self.origin_port}/slow HTTP/1.1\r\n".encode())
            # 请求头尚未结束，该连接不能妨碍其他请求。
            self.assert_healthy()
            slow.sendall(b"\r\n")
            response = bytearray()
            while data := slow.recv(8192):
                response.extend(data)
            self.assertTrue(response.endswith(b"\r\n\r\n/slow"))

    def test_concurrent_requests_keep_their_own_connections(self):
        def fetch(index):
            path = f"/client-{index}?value={index * 17}"
            response = self.get(path)
            self.assertTrue(response.startswith(b"HTTP/1.0 200"))
            self.assertEqual(response.split(b"\r\n\r\n", 1)[1], path.encode())
        # 多轮复用客户端线程，检查描述符复用后仍没有串线或误关闭。
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(fetch, range(128)))
        self.assert_healthy()

    def test_official_tiny_files(self):
        subprocess.run(["make", "-C", str(ROOT / "tiny")], check=True,
                       stdout=subprocess.DEVNULL)
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            port = reserve.getsockname()[1]
        with tempfile.TemporaryFile() as log:
            tiny = subprocess.Popen(["./tiny", str(port)], cwd=ROOT / "tiny",
                                    stdout=log, stderr=log)
            try:
                for attempt in range(100):
                    if tiny.poll() is not None:
                        self.fail("Tiny 启动失败")
                    try:
                        with socket.create_connection(("127.0.0.1", port), .1):
                            break
                    except OSError:
                        if attempt == 99:
                            self.fail("Tiny 启动超时")
                        time.sleep(.02)
                # 使用官方 Basic 评测的五个文件，逐字节比较响应体。
                for name in ("home.html", "csapp.c", "tiny.c", "godzilla.jpg", "tiny"):
                    with self.subTest(file=name):
                        response = self.exchange(
                            f"GET http://127.0.0.1:{port}/{name} HTTP/1.1\r\n\r\n".encode())
                        self.assertTrue(response.startswith(b"HTTP/1.0 200"))
                        self.assertEqual(response.split(b"\r\n\r\n", 1)[1],
                                         (ROOT / "tiny" / name).read_bytes())
            finally:
                tiny.terminate()
                tiny.wait(timeout=3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
