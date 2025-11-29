from locust import HttpUser, task, between, constant
import random
import re


# 🧪 Stress Test: поступове збільшення навантаження
class StressUser(HttpUser):
    wait_time = between(1, 3)  # пауза між запитами

    @task
    def search_basic(self):
        # простий пошуковий запит
        self.client.get("/?q=this")

    @task
    def search_with_page(self):
        # запит із параметром сторінки
        self.client.get("/?q=hello&page=1")


# ⚡ Spike Test: різке збільшення навантаження
class SpikeUser(HttpUser):
    wait_time = constant(1)  # без паузи, щоб створити сплеск

    @task
    def search_spike(self):
        # складніший запит з пробілами
        self.client.get("/?q=this+is+a&page=2")

    @task
    def search_file(self):
        # симуляція відкриття конкретного файлу
        self.client.get("/?q=this+film&file=test_neg_5271_2.txt")

class SearchUser(HttpUser):
    wait_time = between(1, 3)

    @task
    def search_and_open_random_file(self):
        query = random.choice(["movie", "film", "review"])
        
        # 1. Виконати пошук
        response = self.client.get(f"/?q={query}")
        
        if response.status_code == 200:
            html = response.text
            
            # 2. Витягнути всі посилання на файли
            files = re.findall(r'href=[\'"](/\?q=[^&]+&file=[^\'"]+)', html)
            
            if files:
                # 3. Випадково вибрати одне
                chosen = random.choice(files)
                
                # 4. Перейти за цим посиланням
                self.client.get(chosen)
