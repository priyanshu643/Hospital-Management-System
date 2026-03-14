#/home/priyanshu/Documents/GitHub/Hospital-Management-System/Logical_Monitering
#importing libraries
import requests
import time
# opening the loop because we want the status of the patient
while True:
    # adding the API for making connection between the ESP32 and the server
    url1 = "XXX" # <------- Add the ULR of the API here
    url2 = "XXX" # <------- Add the ULR of the API here
    # x is a verible defined for the request sent to the API server
    x = requests.get(url1)
    # printing the status code of the API
    print(f"STATUS CODE: {x.status_code}")
    # y is a verible defined for the response received from the API server in the json format
    y = x.json()
    #printing the response received from the API server
    print(y)
    if type(y) == dict:
        # creating a for loop to iterate through the response received from the API server, because the response is in the form {"id": {"message": "status"}}
        for firebase_id, inner_dict in y.items():
            # This will print the status of the patient
            print(inner_dict['message'])
            # This condition will check that whether if there is any integer
            if int(inner_dict['message']):
                # this condition will check the status of the patient and fid out that if the patient is safe, warning or danger
                if 1 <= int(inner_dict['message']) <= 3:
                    print("safe")
                    # This condition is to define that the condition is safe
                    payload = {"message": "safe"}
                    requests.post(url2, json=payload)
                elif 3 < int(inner_dict['message']) <= 5:
                    print("warning!")
                    # This condition is to define that the condition is warning
                    payload = {"message": "warning"}
                    requests.post(url2, json=payload)
                else:
                    print("danger! invalid input")
                    # This condition is to define that the condition is danger
                    payload = {"message": "invalid input"}
                    requests.post(url2, json=payload)
            else:
                print("invalid input")
                # This condition is to define that the condition is danger
                payload = {"message": "invalid input"}
                requests.post(url2, json=payload)
    else:
        print("danger! API lost")
        # This condition is to define that the condition is danger
        payload = {"message": "API_lost"}
        requests.post(url2, json=payload)
    # This will delete the data from the API server so it recheck the status of the patient from the old data
    requests.delete(url1)
    # adding a sleep time of 1 second so it doesn't spam the server
    time.sleep(1)
