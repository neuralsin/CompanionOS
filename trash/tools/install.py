

#!/usr/bin/env python3
"""
Automated Setup Scripts for CompanionOS
Creates Local Environment config and isolates placeholders correctly.
"""
import os
import sys

def main():
    print("Welcome to the CompanionOS System Installer!")
    print("This tool dynamically builds your .env secure environment.\n")
    
    env_content = []
    
    # Prompt for real overrides to drop placeholders entirely out of dev lifecycle
    spotify_id = input("Enter Spotify Client ID (Press Enter to skip for now): ").strip()
    if spotify_id:
        env_content.append(f"SPOTIFY_CLIENT_ID={spotify_id}")
    
    spotify_secret = input("Enter Spotify Client Secret (Press Enter to skip for now): ").strip()
    if spotify_secret:
        env_content.append(f"SPOTIFY_CLIENT_SECRET={spotify_secret}")
        
    musixmatch = input("Enter Musixmatch API Key (Press Enter to skip for now): ").strip()
    if musixmatch:
        env_content.append(f"MUSIXMATCH_KEY={musixmatch}")
        
    github_user = input("Enter GitHub Username (Press Enter to skip for now): ").strip()
    if github_user:
        env_content.append(f"GITHUB_USERNAME={github_user}")
        
    github_token = input("Enter GitHub Token (Press Enter to skip for now): ").strip()
    if github_token:
        env_content.append(f"GITHUB_TOKEN={github_token}")
        
    env_path = os.path.join(os.path.dirname(__file__), "..", "python", ".env")
    
    if env_content:
        with open(env_path, "w") as f:
            f.write("\n".join(env_content) + "\n")
        print(f"\nCreated {env_path} securely! Placeholders bypassed.")
    else:
        print("\nNo credentials entered. You must update config.json or .env manually before startup.")
        
    print("\nNext steps:")
    print("1. cd ../python")
    print("2. python -m venv venv")
    print("3. venv/Scripts/activate (or source venv/bin/activate on Mac/Linux)")
    print("4. pip install -r requirements.txt")
    print("5. python companion_controller.py")

if __name__ == "__main__":
    main()
