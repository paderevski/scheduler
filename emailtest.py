import os
import smtplib
from email.mime.text import MIMEText


def load_env(path: str = ".env") -> None:
    if not os.path.exists(path):
        return
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            if "=" not in stripped:
                continue
            key, value = stripped.split("=", 1)
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            os.environ.setdefault(key, value)


load_env()

# --- Config ---
SENDER = "emailpatrick74@gmail.com"
GMAIL_APP_PASSWORD = os.getenv("GMAIL_APP_PASSWORD", "")
RECIPIENT = "emailpatrick74@gmail.com"

# --- Build message ---
msg = MIMEText("This is a test email sent from Python via Gmail SMTP.")
msg["Subject"] = "SMTP Test"
msg["From"] = SENDER
msg["To"] = RECIPIENT

# --- Send ---
if not GMAIL_APP_PASSWORD:
    raise RuntimeError("GMAIL_APP_PASSWORD not set. Add it to .env or export it.")

try:
    with smtplib.SMTP("smtp.gmail.com", 587) as server:
        server.starttls()
        server.login(SENDER, GMAIL_APP_PASSWORD)
        server.send_message(msg)
    print("✓ Email sent successfully!")
except Exception as e:
    print(f"✗ Failed: {e}")
