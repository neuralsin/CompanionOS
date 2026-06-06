#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════
  GOOGLE CALENDAR — V6 CompanionOS Productivity Module

  OAuth2 integration with Google Calendar API.
  Fetches today's events, identifies current/next tasks.
═══════════════════════════════════════════════════════════
"""

import os
import json
import pickle
from datetime import datetime, timedelta, timezone

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOKEN_FILE = os.path.join(SCRIPT_DIR, 'gcal_token.pickle')
CREDENTIALS_FILE = os.path.join(SCRIPT_DIR, 'credentials.json')


class GoogleCalendar:
    """Google Calendar API integration for productivity page."""

    def __init__(self, credentials_path=None, calendar_id='primary'):
        self.credentials_path = credentials_path or CREDENTIALS_FILE
        self.calendar_id = calendar_id
        self.service = None
        self.enabled = False
        self._init_service()

    def _init_service(self):
        """Initialize Google Calendar API service with OAuth2."""
        try:
            from google.auth.transport.requests import Request
            from google_auth_oauthlib.flow import InstalledAppFlow
            from googleapiclient.discovery import build

            SCOPES = ['https://www.googleapis.com/auth/calendar.readonly']
            creds = None

            # Load saved token
            if os.path.exists(TOKEN_FILE):
                with open(TOKEN_FILE, 'rb') as token:
                    creds = pickle.load(token)

            # Refresh or get new credentials
            if creds and creds.expired and creds.refresh_token:
                try:
                    creds.refresh(Request())
                except Exception:
                    creds = None

            if not creds or not creds.valid:
                if not os.path.exists(self.credentials_path):
                    print(f"⚠️ Google Calendar: {self.credentials_path} not found.")
                    print("  Download OAuth client JSON from Google Cloud Console.")
                    return

                flow = InstalledAppFlow.from_client_secrets_file(
                    self.credentials_path, SCOPES
                )
                creds = flow.run_local_server(port=0)

                # Save token for future runs
                with open(TOKEN_FILE, 'wb') as token:
                    pickle.dump(creds, token)

            self.service = build('calendar', 'v3', credentials=creds)
            self.enabled = True
            print("✅ Google Calendar connected")

        except ImportError:
            print("⚠️ Google Calendar: Missing packages. Run:")
            print("  pip install google-auth-oauthlib google-api-python-client")
        except Exception as e:
            print(f"⚠️ Google Calendar init error: {e}")

    def get_todays_events(self):
        """Fetch today's calendar events sorted by start time."""
        if not self.enabled or not self.service:
            return []

        try:
            now = datetime.now(timezone.utc)
            start_of_day = now.replace(hour=0, minute=0, second=0, microsecond=0)
            end_of_day = start_of_day + timedelta(days=1)

            events_result = self.service.events().list(
                calendarId=self.calendar_id,
                timeMin=start_of_day.isoformat(),
                timeMax=end_of_day.isoformat(),
                singleEvents=True,
                orderBy='startTime',
                maxResults=10
            ).execute()

            events = events_result.get('items', [])
            parsed = []

            for event in events:
                start = event['start'].get('dateTime', event['start'].get('date'))
                end = event['end'].get('dateTime', event['end'].get('date'))

                # Parse datetime
                try:
                    if 'T' in start:
                        start_dt = datetime.fromisoformat(start)
                        end_dt = datetime.fromisoformat(end)
                        start_str = start_dt.strftime('%H:%M')
                        end_str = end_dt.strftime('%H:%M')
                        time_str = f"{start_str} - {end_str}"
                    else:
                        start_dt = datetime.strptime(start, '%Y-%m-%d')
                        end_dt = start_dt + timedelta(days=1)
                        time_str = "All day"
                except Exception:
                    start_dt = now
                    end_dt = now + timedelta(hours=1)
                    time_str = ""

                parsed.append({
                    'title': event.get('summary', 'No title')[:31],
                    'time': time_str,
                    'start': start_dt,
                    'end': end_dt,
                    'is_all_day': 'T' not in start
                })

            return parsed

        except Exception as e:
            print(f"Calendar fetch error: {e}")
            return []

    def get_productivity_state(self):
        """
        Get current/next tasks for ESP display.
        Returns dict ready for JSON serialization.
        """
        result = {
            'current': '',
            'current_time': '',
            'next1': '',
            'next1_time': '',
            'next2': '',
            'next2_time': '',
            'active': False,
            'progress': 0
        }

        events = self.get_todays_events()
        if not events:
            return result

        now = datetime.now(events[0]['start'].tzinfo if events[0]['start'].tzinfo else None)
        current_event = None
        upcoming = []

        for event in events:
            if event['start'] <= now < event['end']:
                current_event = event
            elif event['start'] > now:
                upcoming.append(event)

        # Current task
        if current_event:
            result['current'] = current_event['title']
            result['current_time'] = current_event['time']
            result['active'] = True

            # Calculate progress percentage
            total_duration = (current_event['end'] - current_event['start']).total_seconds()
            elapsed = (now - current_event['start']).total_seconds()
            if total_duration > 0:
                result['progress'] = min(100, int(elapsed / total_duration * 100))

        # Next tasks
        if len(upcoming) > 0:
            result['next1'] = upcoming[0]['title']
            result['next1_time'] = upcoming[0]['time'].split(' - ')[0] if ' - ' in upcoming[0]['time'] else upcoming[0]['time']
        if len(upcoming) > 1:
            result['next2'] = upcoming[1]['title']
            result['next2_time'] = upcoming[1]['time'].split(' - ')[0] if ' - ' in upcoming[1]['time'] else upcoming[1]['time']

        return result


class LocalTaskList:
    """Fallback: read tasks from a local JSON file."""

    def __init__(self, tasks_file=None):
        self.tasks_file = tasks_file or os.path.join(SCRIPT_DIR, 'tasks.json')
        self._ensure_file()

    def _ensure_file(self):
        if not os.path.exists(self.tasks_file):
            sample = [
                {"title": "Morning Standup", "time": "09:00 - 09:30"},
                {"title": "Deep Work Block", "time": "10:00 - 12:00"},
                {"title": "Lunch Break", "time": "12:30 - 13:30"},
                {"title": "Code Review", "time": "14:00 - 15:00"},
                {"title": "Planning", "time": "16:00 - 17:00"}
            ]
            with open(self.tasks_file, 'w') as f:
                json.dump(sample, f, indent=2)

    def get_productivity_state(self):
        """Parse local task list for ESP display."""
        result = {
            'current': '',
            'current_time': '',
            'next1': '',
            'next1_time': '',
            'next2': '',
            'next2_time': '',
            'active': False,
            'progress': 0
        }

        try:
            with open(self.tasks_file) as f:
                data = json.load(f)
                tasks = data.get('tasks', data) if isinstance(data, dict) else data
        except Exception:
            return result

        if not tasks or not isinstance(tasks, list):
            return result

        now = datetime.now()
        current_hour = now.hour
        current_minute = now.minute
        now_minutes = current_hour * 60 + current_minute

        current_task = None
        upcoming = []

        for task in tasks:
            time_str = task.get('time', '')
            title = task.get('title', '')[:31]

            # Parse "HH:MM - HH:MM" format
            if ' - ' in time_str:
                try:
                    start_s, end_s = time_str.split(' - ')
                    sh, sm = map(int, start_s.strip().split(':'))
                    eh, em = map(int, end_s.strip().split(':'))
                    start_mins = sh * 60 + sm
                    end_mins = eh * 60 + em

                    if start_mins <= now_minutes < end_mins:
                        current_task = {
                            'title': title,
                            'time': time_str,
                            'start_mins': start_mins,
                            'end_mins': end_mins
                        }
                    elif start_mins > now_minutes:
                        upcoming.append({
                            'title': title,
                            'time': start_s.strip()
                        })
                except ValueError:
                    pass
            else:
                # Single time entry
                try:
                    th, tm = map(int, time_str.strip().split(':'))
                    t_mins = th * 60 + tm
                    if t_mins > now_minutes:
                        upcoming.append({'title': title, 'time': time_str})
                except ValueError:
                    pass

        if current_task:
            result['current'] = current_task['title']
            result['current_time'] = current_task['time']
            result['active'] = True
            total = current_task['end_mins'] - current_task['start_mins']
            elapsed = now_minutes - current_task['start_mins']
            if total > 0:
                result['progress'] = min(100, int(elapsed / total * 100))

        if len(upcoming) > 0:
            result['next1'] = upcoming[0]['title']
            result['next1_time'] = upcoming[0]['time']
        if len(upcoming) > 1:
            result['next2'] = upcoming[1]['title']
            result['next2_time'] = upcoming[1]['time']

        return result


# ── Standalone test ───────────────────────────────────────
if __name__ == '__main__':
    config_file = os.path.join(SCRIPT_DIR, 'config.json')
    use_google = False

    if os.path.exists(config_file):
        with open(config_file) as f:
            cfg = json.load(f)
        prod_cfg = cfg.get('productivity', {})
        creds = prod_cfg.get('google_credentials', '')
        if creds and os.path.exists(os.path.join(SCRIPT_DIR, creds)):
            use_google = True

    if use_google:
        cal = GoogleCalendar()
        print("Using Google Calendar")
    else:
        cal = LocalTaskList()
        print("Using local tasks.json (place credentials.json for Google Calendar)")

    state = cal.get_productivity_state()
    print(json.dumps(state, indent=2))
