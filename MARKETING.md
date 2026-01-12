# Marketing Materials for Pebble RSVP News Reader

---

## 🏷️ PROJECT NAME SUGGESTIONS

Here are several name options, ranked by relevance to the RSVP speed-reading concept:

### Top Picks

1. **FlashRead News** - Captures the rapid word-by-word display perfectly
2. **WordStream** - Evokes the flow of words across the tiny screen
3. **GlanceNews** - Implies quick reading at a glance
4. **Wrist Headlines** - Direct, descriptive, Pebble-specific
5. **SpeedRead RSS** - Technical but clear
6. **PebbleStream** - Platform + technique combined

### Alternative Names

- QuickFeed
- NewsFlash RSVP
- Rapid Reader
- FlowRead
- GlimpseNews
- OneWord News

**My recommendation: "FlashRead News"** - It's catchy, describes exactly what the app does (flash words one at a time), and works well for marketing. Short, memorable, and unique.

---

## 📱 APP STORE DESCRIPTION

### Short Version (for character-limited stores)

> **FlashRead News** - Read RSS news on your Pebble using RSVP speed-reading. Words appear one at a time at your chosen pace. Select from 6 pre-configured feeds (BBC, NY Times, NPR, Guardian, Le Monde, Reuters) or add your own. Adjustable reading speed (100-600 WPM). Minimal, open source.

### Medium Version

> **FlashRead News** brings RSS feeds to your Pebble using RSVP (Rapid Serial Visual Presentation) - a proven speed-reading technique that displays words one at a time, centered on the optimal recognition point.
>
> **Features:**
> - 6 pre-configured news sources (BBC, NY Times, NPR, Guardian, Le Monde, Reuters)
> - Add unlimited custom RSS feeds via phone configuration
> - Adjustable reading speed from 100 to 600 WPM
> - Read headlines, then dive into full article descriptions
> - Optional backlight during reading
> - Navigate between articles with physical buttons
>
> Perfect for the small Pebble screen. Free and open source.

### Long Version (detailed)

> **FlashRead News** is an RSS news reader for Pebble smartwatches that uses RSVP (Rapid Serial Visual Presentation) - a scientifically-backed speed-reading technique where words are displayed one at a time, precisely centered on the optimal recognition point (ORP).
>
> Unlike traditional scrolling text that's nearly impossible to read on a tiny screen, RSVP lets you read at natural speed without moving your eyes. Words flow past your fixed gaze at your chosen pace.
>
> **What you get:**
> - Select from 6 quality news sources: BBC World, NY Times, NPR News, The Guardian, Le Monde, Reuters
> - Add your own RSS feeds through the phone configuration page
> - Adjustable reading speed from 100 to 600 words per minute
> - Smart delays: longer pauses on punctuation (periods, commas) for natural reading rhythm
> - Two reading modes: scan headlines quickly, then select to read the full article description
> - Keep backlight on while reading (optional)
> - Simple physical button navigation
>
> Free, open source, no tracking, no ads.
>
> GitHub: [repository URL]

---

## 📝 REDDIT POST (r/pebble)

### Title Options:

1. "I built an RSVP speed-reading news app for Pebble - words appear one at a time, perfect for tiny screens"
2. "FlashRead News - an open source RSS reader using speed-reading technique for Pebble"
3. "Made a news reader that displays one word at a time - finally readable text on my Pebble"

### Post Body:

---

**Title:** I built an RSVP speed-reading news app for Pebble - words appear one at a time, perfect for tiny screens

Hey everyone!

I wanted to share a project I've been working on. Like many of you, I've always wished I could catch up on news from my Pebble, but let's be honest - reading scrolling text on that tiny screen is painful. Your eyes jump around, you lose your place, and it's just not practical.

So I built **FlashRead News** - an RSS reader that uses RSVP (Rapid Serial Visual Presentation). If you haven't heard of it, it's a speed-reading technique where words appear one at a time, centered on the screen. Instead of your eyes scanning across text, the text comes to you. It sounds weird at first, but once you try it, it just clicks.

**How it works:**

When you launch the app, you pick a news source from a menu. I've pre-configured 6 feeds (BBC, NY Times, NPR, Guardian, Le Monde, Reuters), but you can add your own RSS feeds through the phone settings. Once you select a source, headlines start appearing word by word.

The reading feels surprisingly natural because I implemented the Spritz algorithm - it calculates the "optimal recognition point" of each word (usually around the 2nd or 3rd letter) and centers THAT letter on screen, making your brain process words faster. The app also adds smart pauses: longer breaks after periods and commas, so you naturally sense sentence boundaries without seeing punctuation flash by.

**I thought it would be nice to:**

- Let you adjust the reading speed. The default is 400 words per minute, but you can go anywhere from 100 to 600 WPM. I personally read headlines at 400 and slow down to 250 for articles.

- Give you the option to keep the backlight on while reading. Nothing worse than the screen dimming mid-sentence.

- Add a simple way to navigate between articles. Up/Down buttons let you skip to the next headline. When you find something interesting, press Select to read the article description.

- Support international feeds. The app handles French accents and special characters properly, so Le Monde works just as well as BBC.

**The visual layout:**

I kept it minimal - black background, white text, with two small guide lines above and below the word area. There's a small indicator showing which letter is the pivot point. At the bottom, there are subtle hints for button functions that change based on context.

**What's next:**

This is fully open source and I'd love feedback. Currently thinking about:
- Storing more than just the description (full article content would need a proxy service)
- Offline caching of headlines
- Custom accent colors for color Pebbles

[**Screenshots/GIF will go here**]

**Download:** Available on the Pebble app store as "FlashRead News"
**Source code:** GitHub link

Let me know what you think! Happy to answer questions about the implementation or take feature requests.

---

## 🎬 VISUAL CONTENT - ASSETS

### Core Screenshots (in `marketing/` folder):

| File | Description | Resolution |
|------|-------------|------------|
| `01_feed_selection.png` | Feed selection menu - choose your news source | 144×168 |
| `02_headline_display.png` | RSVP headline reading - title word by word | 144×168 |
| `03_page_indicator.png` | Page/position indicator - track your progress | 144×168 |
| `04_article_reading.png` | Article content - full description reading | 144×168 |
| `05_phone_settings.jpg` | Phone configuration page - customize feeds & speed | 1080×2002 |

### Scaled Versions (3× - for better visibility):

| File | Description |
|------|-------------|
| `large_01_feed_selection.png` | Feed menu (3× scale) |
| `large_02_headline_display.png` | Headline reading (3× scale) |
| `large_03_page_indicator.png` | Page indicator (3× scale) |
| `large_04_article_reading.png` | Article reading (3× scale) |
| `large_05_phone_settings.jpg` | Phone config (resized) |

### Composite Banners (ready to use):

| File | Description | Best for |
|------|-------------|----------|
| `banner_reddit.png` | Full banner with title, all 4 screens, captions | **Reddit post** |
| `banner_horizontal.png` | Clean 4-screen flow with labels | GitHub README |
| `appstore_combo.png` | Watch screens + phone config side by side | App store listing |

### Recommended Usage:

**For Reddit:** Use `banner_reddit.png` - Shows the complete user journey with the app name and tagline

**For App Store:** Use `appstore_combo.png` - Demonstrates both watch interface and phone configuration

**For GitHub README:** Use `banner_horizontal.png` - Clean, professional flow diagram

### Design Notes:

The black-and-white UI is intentional for maximum readability on the small e-paper-like Pebble display. The dark backgrounds in banners create contrast that makes the watch screens pop.

---

## 📊 KEY SELLING POINTS (for any platform)

1. **RSVP Speed Reading** - Scientifically-backed technique, not gimmick
2. **Perfect for Small Screens** - The Pebble's limitation becomes irrelevant
3. **Multiple Quality Sources** - 6 respected international news outlets
4. **Customizable** - Add any RSS feed, adjust speed, toggle backlight
5. **Smart Reading Flow** - Punctuation-aware pauses feel natural
6. **Open Source** - No tracking, no ads, community-driven
7. **Works Offline (partially)** - Headlines cached after first load

---

## 🔗 SUGGESTED SUBREDDITS

- r/pebble (primary)
- r/smartwatch
- r/opensource
- r/speedreading (niche but relevant)
- r/rss (for RSS enthusiasts)

---

## HASHTAGS (for social media)

#Pebble #PebbleWatch #SmartWatch #RSVP #SpeedReading #OpenSource #RSS #NewsReader #WearableApps
