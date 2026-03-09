#/home/priyanshu/Documents/GitHub/Hospital-Management-System/Logical_Monitering
#importing libraries
import requests
import time
# opening the loop because we want the status of the patient
while True:
    # adding the API for making connection between the ESP32 and the server
    url1 = "XXX" # <------- Add the ULR of the API here
    # x is a verible defined for the request sent to the API server
    x = requests.get(url1)
    # printing the status code of the API
    print(f"STATUS CODE: {x.status_code}")
    # y is a verible defined for the response received from the API server in the json format
    y = x.json()
    #printing the response received from the API server
    print(y)
    # adding a sleep time of 1 second so it doesn't spam the server
    time.sleep(1)
