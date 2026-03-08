import json
import random

# Sample data pools
first_names = ["Aarav", "Priya", "Rahul", "Ananya", "Vikram", "Neha", "Rohan", "Sneha", "Aditya", "Pooja", "Karan",
               "Kavya", "Arjun", "Meera", "Siddharth"]
last_names = ["Sharma", "Patel", "Verma", "Desai", "Singh", "Gupta", "Kumar", "Iyer", "Reddy", "Joshi", "Nair", "Das"]
conditions = ["Cardiac Arrhythmia", "Asthma Exacerbation", "Hypertension", "Post-operative Monitoring", "COPD",
              "Pneumonia", "Sepsis Observation", "Hypoglycemia"]
statuses = ["Stable", "Warning", "Critical"]


def generate_patient_data(num_patients):
    database = {"patients": {}}

    for i in range(1, num_patients + 1):
        patient_id = f"P{i:03d}"
        name = f"{random.choice(first_names)} {random.choice(last_names)}"

        # Simulating ESP32 sensor readings
        heart_rate = random.randint(60, 130)
        spo2 = random.randint(85, 100)
        temp = round(random.uniform(36.0, 39.5), 1)

        # Determine status based on readings to make it realistic
        if spo2 < 90 or heart_rate > 120 or temp > 39.0:
            status = "Critical"
        elif spo2 < 95 or heart_rate > 100 or temp > 38.0:
            status = "Warning"
        else:
            status = "Stable"

        database["patients"][patient_id] = {
            "name": name,
            "age": random.randint(18, 85),
            "condition": random.choice(conditions),
            "current_vitals": {
                "heart_rate_bpm": heart_rate,
                "spo2_percent": spo2,
                "temperature_c": temp
            },
            "status": status
        }

    return database


# Generate 100 patients
patient_db = generate_patient_data(100)

# Save to JSON file
with open("patients_data.json", "w") as outfile:
    json.dump(patient_db, outfile, indent=4)

print("Successfully generated patients_data.json with 100 records!")