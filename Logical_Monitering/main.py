#/home/priyanshu/Documents/GitHub/Hospital-Management-System/Logical_Monitering
import requests
import time

while True:
    url1 = "XXX"
    x = requests.get(url1)
    print(f"STATUS CODE: {x.status_code}")
    y = x.json()
    print(y)
    time.sleep(1)