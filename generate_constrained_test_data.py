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
    "Isabelle",
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
# Create popularity tiers to force scarcity
popular_activities = [
    "Robotics Workshop",  # Very popular
    "3D Printing Lab",  # Very popular
    "Coding Bootcamp",  # Very popular
]

medium_activities = [
    "Drone Programming",  # Medium popularity
    "CAD Design Studio",  # Medium popularity
    "Solar Car Racing",  # Medium popularity
]

unpopular_activities = [
    "Bridge Building Challenge",  # Less popular
    "Rocket Design",  # Less popular
    "Circuit Building",  # Less popular
    "Environmental Engineering",  # Less popular
]

all_activities = popular_activities + medium_activities + unpopular_activities

pathways = ["CS", "Engineering", "Entrepreneurship"]
teachers = last_names[20:25]
days = ["A", "B"]
presents = ["Yes", "No", "Maybe"]

# Generate 100 students with biased preferences
students = []
used_names = set()

for i in range(800):
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
    teacher = random.choice(teachers)
    day = random.choice(days)
    present = random.choice(presents)
    pathway = random.choice(pathways)

    # Bias the selection toward popular activities
    # 70% of students have at least 2 popular activities in their top 3
    choice_pool = []
    if random.random() < 0.70:
        # Popular-biased student
        choice_pool = popular_activities * 3 + medium_activities + unpopular_activities
    else:
        # More diverse preferences
        choice_pool = all_activities

    # Random 3 choices (without replacement from the biased pool)
    choices = []
    available = list(choice_pool)
    for _ in range(3):
        if available:
            choice = random.choice(available)
            choices.append(choice)
            # Remove all instances of this choice to avoid duplicates
            available = [a for a in available if a != choice]

    # Pad with random choices if needed
    while len(choices) < 3:
        remaining = [a for a in all_activities if a not in choices]
        if remaining:
            choices.append(random.choice(remaining))
        else:
            break

    students.append([student_id, first, last, pathway, teacher, day, present] + choices)

# Write to CSV
with open("constrained_test_data.csv", "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(
        [
            "Student ID",
            "First Name",
            "Last Name",
            "Pathway",
            "Teacher",
            "Day",
            "Present",
            "Choice 1",
            "Choice 2",
            "Choice 3",
        ]
    )
    writer.writerows(students)

# Analyze the demand
demand = {activity: 0 for activity in all_activities}
for student in students:
    for choice in student[7:10]:  # Choices are in columns 7,8,9
        if choice:
            demand[choice] += 1

print(f"✅ Generated constrained_test_data.csv with {len(students)} students")
print(f"\n📊 Activity Demand Analysis (for capacity 10):")
print(f"   {'Activity':<30} {'Total Demand':<15} {'Capacity':<10} {'Overflow'}")
print(f"   {'-'*30} {'-'*15} {'-'*10} {'-'*10}")

capacity = 10
for activity in sorted(all_activities, key=lambda x: demand[x], reverse=True):
    overflow = max(0, demand[activity] - capacity)
    status = "⚠️ OVERSUBSCRIBED" if overflow > 0 else "✓ OK"
    print(
        f"   {activity:<30} {demand[activity]:<15} {capacity:<10} {overflow:>3}  {status}"
    )

total_demand = sum(demand.values())
total_capacity = len(all_activities) * capacity
print(f"\n   Total demand: {total_demand}")
print(f"   Total capacity: {total_capacity}")
print(
    f"   Students guaranteed fallback: ~{max(0, total_demand - total_capacity)} if uniform distribution"
)

print(
    f"\n💡 Recommendation: Set capacity to 10 to force many students into non-preferred activities!"
)
print(
    f"   Popular activities will overflow, forcing ~{sum(max(0, d - capacity) for d in demand.values())} preference violations"
)
