# Imported the libraries
import requests
import time
# Assinging the url to the API server
url = "XXX" # <--- Add the url here
url1 = f"{url}.json"
url2 = "XXX" # <--- Add the url here

# creating the function to check the status of the patient
def patient_status():
    x = requests.get(url1)
    y = x.json()
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
            requests.delete(f"{url}/{firebase_id}.json")
    else:
        pass
    # This will delete the data from the API server so it rechecks the status of the patient from the old data

    return
#Opening a loop to check the status of the patient
while True:

    # Using try and except to check the network glitch
    try:
        # Calling the function to check the status of the patient
        patient_status()
    # catching the exception
    except requests.exceptions.RequestException as e:
        print(f"Network glitch! The internet dropped: {e}")
        print("Waiting a few seconds before trying again...")
        # sleeping for 1 second for printing the error message
        time.sleep(1)
    # sleeping for 1 second so that the fire base wouldn't get banned
    time.sleep(1)