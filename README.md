# RSVP Breaking News

A Pebble application that displays Reuters breaking news headlines using RSVP (Rapid Serial Visual Presentation) technique.

## Features

- Displays news headlines word by word for fast reading
- Fetches headlines from Reuters RSS feed
- Shows 5 headlines per session
- Pauses at END screen - press any button to exit
- Compatible with Aplite, Basalt, and Diorite platforms

## How it works

1. Launch the app from the Pebble menu
2. A splash screen with "REUTERS Breaking International News" is shown
3. Headlines are displayed one word at a time (~300 WPM)
4. After 5 headlines, the app shows "END" and pauses
5. Press any button to exit the app

## Building

```bash
cd /path/to/RSVP-Breaking-News
pebble build
```

## Installing

```bash
pebble install --phone YOUR_PHONE_IP
```

## Version

- Version: 1.0
- UUID: b8f4e3c2-7a19-4d5b-9e81-2f6c8a3d0e47

## Credits

News engine extracted from Din Clean watchface by CJ.
