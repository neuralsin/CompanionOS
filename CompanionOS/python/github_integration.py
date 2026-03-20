"""
github_integration.py
Handles fetching remote parameters from GitHub safely, handling placeholders explicitly.
"""
import requests

class GitHubIntegration:
    def __init__(self, username, token=None):
        self.username = username
        self.token = token
        self.enabled = False
        
        if "yourusername" in self.username:
            print("GitHub: Placeholder username detected. Skipping.")
            return

        self.enabled = True

    def get_profile(self):
        if not self.enabled:
            return None
        
        try:
            headers = {}
            if self.token and not "ghp_abc123" in self.token:
                headers['Authorization'] = f'token {self.token}'
            
            url = f"https://api.github.com/users/{self.username}"
            response = requests.get(url, headers=headers, timeout=5)
            
            if response.status_code != 200:
                return None
                
            data = response.json()
            return {
                'username': data.get('login'),
                'name': data.get('name', ''),
                'repos': data.get('public_repos', 0),
                'followers': data.get('followers', 0),
                'following': data.get('following', 0),
                'avatar_url': data.get('avatar_url')
            }
        except Exception as e:
            print(f"GitHub Error: {e}")
            return None
