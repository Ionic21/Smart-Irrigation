import openai
import os
from dotenv import load_dotenv

# Load API Key
load_dotenv()
api_key = os.getenv("OPENAI_API_KEY")

# Test simple call
try:
    openai_client = openai.Client(api_key=api_key)
    response = openai_client.models.list()
    print("API Key Works! Models:", [m.id for m in response.data])
except Exception as e:
    print("Error:", e)
