import socket
import random
import time
from locust import User, task, constant

class TcpUser(User):
    # мінімальні паузи між запитами → більше RPS
    wait_time = constant(0.1)

    def connect(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("localhost", 8080))
        return s

    @task
    def search_phrase(self):
        phrase = random.choice([
            "hello", "film", "review",
            "this is a", "this is a classic film", "best movie"
        ])
        s = None
        start_time = time.time()
        try:
            s = self.connect()
            s.sendall((phrase + "\n").encode("utf-8"))
            response = s.recv(4096).decode("utf-8", errors="ignore")

            total_time = int((time.time() - start_time) * 1000)
            self.environment.events.request.fire(
                request_type="tcp",
                name="search_phrase",
                response_time=total_time,
                response_length=len(response),
                exception=None,
            )
        except Exception as e:
            total_time = int((time.time() - start_time) * 1000)
            self.environment.events.request.fire(
                request_type="tcp",
                name="search_phrase",
                response_time=total_time,
                response_length=0,
                exception=e,
            )
        finally:
            if s:
                s.close()

    @task
    def ping(self):
        """Простий ping-запит для перевірки доступності сервера"""
        s = None
        start_time = time.time()
        try:
            s = self.connect()
            s.sendall(b"PING\n")
            response = s.recv(1024).decode("utf-8", errors="ignore")

            total_time = int((time.time() - start_time) * 1000)
            self.environment.events.request.fire(
                request_type="tcp",
                name="ping",
                response_time=total_time,
                response_length=len(response),
                exception=None,
            )
        except Exception as e:
            total_time = int((time.time() - start_time) * 1000)
            self.environment.events.request.fire(
                request_type="tcp",
                name="ping",
                response_time=total_time,
                response_length=0,
                exception=e,
            )
        finally:
            if s:
                s.close()
