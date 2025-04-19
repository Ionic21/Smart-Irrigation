import os
import json
import time
from dotenv import load_dotenv
from groq import Groq

# ===== Load API Key =====
load_dotenv()
groq_api_key = os.getenv("GROQ_API_KEY")
client = Groq(api_key=groq_api_key)

# ===== Cache Setup =====
CACHE_FILE = "crop_cache.json"
REQUEST_LIMIT = 5  # Limit requests per crop
cache = {}

# Load cache if exists
if os.path.exists(CACHE_FILE):
    with open(CACHE_FILE, "r") as f:
        cache = json.load(f)


def save_cache():
    with open(CACHE_FILE, "w") as f:
        json.dump(cache, f, indent=4)


# ===== Crop Data Fetch Function =====
def get_crop_info_from_groq(crop_name):
    crop_name = crop_name.lower()

    if crop_name in cache:
        print(f"Using cached data for {crop_name}")
        cache[crop_name]["request_count"] += 1
        save_cache()
        return cache[crop_name]["info"]

    # Generate prompt
    prompt = f"""
    Provide optimal growing conditions for the crop "{crop_name}" strictly as a JSON object.
    Only include:
    - Ideal Temperature Range (°C)
    - Soil Moisture Level (Low/Moderate/High)
    - Ideal Humidity Range (%)
    - Irrigation Tips (as a list)

    Example Output:
    {{
      "temperature": "15-25°C",
      "soil_moisture": "Moderate",
      "humidity": "60-80%",
      "irrigation_tips": [
        "Water early in the morning.",
        "Avoid over-irrigation."
      ]
    }}
    Strictly return only the JSON object with no extra text or explanations.
    """
    
    try:
        response = client.chat.completions.create(
            model="llama3-70b-8192",  # Use the right Groq model
            messages=[
                {"role": "system", "content": "You are a helpful assistant providing agricultural advice."},
                {"role": "user", "content": prompt}
            ],
            max_tokens=200,
            temperature=0.3
        )

        # Extract response
        crop_info = response.choices[0].message.content.strip()
        print("Fetched from Groq:", crop_info)

        # Try parsing the response as JSON
        try:
            crop_info_json = json.loads(crop_info)
            print("Parsed Crop Info:", json.dumps(crop_info_json, indent=4))

            # Cache response
            cache[crop_name] = {
                "info": crop_info_json,
                "request_count": cache.get(crop_name, {}).get("request_count", 0) + 1,
                "timestamp": time.time()
            }
            save_cache()

            return crop_info_json

        except json.JSONDecodeError:
            print("❌ Failed to parse JSON. Raw Response:", crop_info)
            return None

    except Exception as e:
        print("Error fetching crop info:", e)
        return None