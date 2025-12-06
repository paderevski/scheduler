import csv
import random

# Pool of first and last names
first_names = [
    "Emma",
    "Liam",
    "Olivia",
    "Noah",
    "Ava",
    "Ethan",
    "Sophia",
    "Mason",
    "Isabella",
    "William",
    "Mia",
    "James",
    "Charlotte",
    "Benjamin",
    "Amelia",
    "Lucas",
    "Harper",
    "Henry",
    "Evelyn",
    "Alexander",
    "Abigail",
    "Michael",
    "Emily",
    "Daniel",
    "Elizabeth",
    "Matthew",
    "Sofia",
    "Jackson",
    "Avery",
    "Sebastian",
    "Ella",
    "Jack",
    "Scarlett",
    "Aiden",
    "Grace",
    "Owen",
    "Chloe",
    "Samuel",
    "Victoria",
    "David",
    "Riley",
    "Joseph",
    "Aria",
    "Carter",
    "Lily",
    "Wyatt",
    "Aubrey",
    "John",
    "Zoey",
    "Luke",
    "Penelope",
    "Dylan",
    "Nora",
    "Jayden",
    "Hannah",
    "Gabriel",
    "Lillian",
    "Anthony",
    "Addison",
    "Isaac",
    "Eleanor",
    "Grayson",
    "Natalie",
    "Julian",
    "Luna",
    "Levi",
    "Savannah",
    "Christopher",
    "Brooklyn",
    "Joshua",
    "Leah",
    "Andrew",
    "Zoe",
    "Lincoln",
    "Stella",
    "Mateo",
    "Hazel",
    "Ryan",
    "Ellie",
    "Jaxon",
    "Paisley",
    "Nathan",
    "Audrey",
    "Aaron",
    "Skylar",
    "Isaiah",
    "Violet",
    "Thomas",
    "Claire",
    "Charles",
    "Bella",
    "Caleb",
    "Aurora",
    "Josiah",
    "Lucy",
    "Christian",
    "Anna",
    "Hunter",
    "Caroline",
    "Eli",
]

last_names = [
    "Smith",
    "Johnson",
    "Williams",
    "Brown",
    "Jones",
    "Garcia",
    "Miller",
    "Davis",
    "Rodriguez",
    "Martinez",
    "Hernandez",
    "Lopez",
    "Gonzalez",
    "Wilson",
    "Anderson",
    "Thomas",
    "Taylor",
    "Moore",
    "Jackson",
    "Martin",
    "Lee",
    "Perez",
    "Thompson",
    "White",
    "Harris",
    "Sanchez",
    "Clark",
    "Ramirez",
    "Lewis",
    "Robinson",
    "Walker",
    "Young",
    "Allen",
    "King",
    "Wright",
    "Scott",
    "Torres",
    "Nguyen",
    "Hill",
    "Flores",
    "Green",
    "Adams",
    "Nelson",
    "Baker",
    "Hall",
    "Rivera",
    "Campbell",
    "Mitchell",
    "Carter",
    "Roberts",
]

# 10 Engineering Week activities
activities = [
    "Robotics Workshop",
    "3D Printing Lab",
    "Bridge Building Challenge",
    "Coding Bootcamp",
    "Rocket Design",
    "Circuit Building",
    "Environmental Engineering",
    "Drone Programming",
    "CAD Design Studio",
    "Solar Car Racing",
]

# Generate 100 students
students = []
used_names = set()

for i in range(100):
    # Generate unique student name
    while True:
        first = random.choice(first_names)
        last = random.choice(last_names)
        full_name = f"{first} {last}"
        if full_name not in used_names:
            used_names.add(full_name)
            break

    # Student ID
    student_id = f"S{1000 + i}"

    # Random 3 choices (without replacement)
    choices = random.sample(activities, 3)

    students.append([student_id, full_name, choices[0], choices[1], choices[2]])

# Write to CSV
with open("test_data.csv", "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(["Student ID", "Student Name", "Choice 1", "Choice 2", "Choice 3"])
    writer.writerows(students)

print(f"✅ Generated test_data.csv with {len(students)} students")
print(f"   Activities: {', '.join(activities)}")
print(f"\nFirst 5 rows:")
for i, student in enumerate(students[:5], 1):
    print(
        f"   {i}. {student[0]:6s} {student[1]:20s} → {student[2]}, {student[3]}, {student[4]}"
    )
