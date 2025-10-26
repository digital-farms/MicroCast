# MicroCast Quick Start Guide ⚡

**For Developers: Deploy Your Own MicroCast Server**

> **Note:** This guide is for developers who want to host their own MicroCast server.
> 
> **Regular users:** See [USER_GUIDE.md](USER_GUIDE.md) for simple installation via M5Burner.

**Get your own MicroCast server running in 15 minutes!**

---

## 📦 What You Need

- M5Cardputer device
- USB-C cable
- Arduino IDE 2.0+
- Internet connection
- Free accounts on:
  - [Supabase](https://supabase.com) (database)
  - [Cloudflare](https://cloudflare.com) (API)

---

## 🚀 3-Step Setup

### Step 1: Backend (10 min)

#### A. Supabase Database
```bash
1. Go to supabase.com → Sign up
2. Create new project (choose any name)
3. Go to SQL Editor
4. Copy SQL from BACKEND_UPDATE_INSTRUCTIONS.md (v0.2)
   OR readme.md (includes like_count field + triggers)
5. Paste and click "Run"
6. Save these:
   - Project URL (Settings → API → URL)
   - Service Role Key (Settings → API → service_role key)
```

**⚠️ Important for v0.2:**
If upgrading from v0.1, run the SQL migration to add:
- `like_count` column to posts table
- Automatic like count triggers
- Index for TOP section performance

#### B. Cloudflare Worker
```bash
1. Go to cloudflare.com → Sign up
2. Workers & Pages → Create Worker
3. Copy JS code from readme.md (CLOUDFLARE section)
4. Paste in editor
5. Settings → Variables → Add:
   - SUPABASE_URL = <your project URL>
   - SERVICE_ROLE_KEY = <your service key>
6. Deploy
7. Copy worker URL (e.g., https://xxx.workers.dev)
```

### Step 2: Flash Device (3 min)

```bash
1. Install Arduino IDE 2.0+
2. Add M5Stack board support:
   File → Preferences → Additional URLs:
   https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json

3. Install boards & libraries:
   Tools → Board Manager → "M5Stack" → Install
   Tools → Manage Libraries → "M5Cardputer" → Install
   Tools → Manage Libraries → "ArduinoJson" → Install

4. Open DEV_MicroCast_v0.1.ino
5. Edit line 28:
   const char* PROXY_BASE = "https://YOUR-WORKER.workers.dev";

6. Select:
   Tools → Board → M5Stack → M5Cardputer
   Tools → Port → <your COM port>

7. Click Upload button ⬆️
8. Wait for "Done uploading"
```

### Step 3: First Run (2 min)

```bash
1. Unplug and replug USB
2. Device shows: "Enter username (min 3 chars)"
3. Type username and press Enter
   OR press ESC to skip
4. Select WiFi network
5. Enter WiFi password
6. Done! You're in the feed
```

---

## ⌨️ Quick Controls (v0.2)

| Key | Action |
|-----|--------|
| **; .** or **Up/Down** | Scroll posts |
| **, /** or **Left/Right** | Switch sections (NEW/TOP/YOU) |
| **Enter** | Like/Unlike OR confirm section |
| **Fn+Enter** | New post |
| **R** | Refresh current section |
| **U** | Change username |
| **N** | Change WiFi |
| **I** | Show Info window (help) |
| **Fn+C** | Clear device |

**New Features:**
- **NEW** section: Latest posts
- **TOP** section: Most liked posts (all-time)
- **YOU** section: Your profile + stats (Total Likes, Post count)

---

## 🆘 Common Issues

**"Username may be taken"**
→ Try different name

**"Device not registered"**
→ Press [U] to register first

**"No WiFi"**
→ Press [N] to connect

**Device freezes**
→ Press ESC to skip, then connect WiFi

---

## ✅ Success Check (v0.2)

Test your setup:

1. **Post test:**
   - Press Fn+Enter
   - Type "Hello MicroCast!"
   - Press Enter
   - Should appear in NEW section

2. **Like test:**
   - Scroll to any post
   - Press Enter
   - Like counter increases (instant update!)

3. **Section test:**
   - Press ; (up) to reach section buttons
   - Press , / to switch between NEW/TOP/YOU
   - Press Enter to confirm

4. **Profile test (YOU section):**
   - Switch to YOU section
   - See your Total Likes, Posts count
   - Your posts appear below

5. **Info test:**
   - Press I
   - Help window appears
   - Press any key to close

All working? **Congratulations! 🎉**

---

## 📚 Learn More

- Full documentation: [README_USER.md](README_USER.md)
- Russian version: [README_RU.md](README_RU.md)
- Contribute: [CONTRIBUTING.md](CONTRIBUTING.md)
- Changelog: [CHANGELOG.md](CHANGELOG.md)

---

**Need help?** Open an issue on GitHub!

**Happy microblogging! 📱✨**

---

## 🆕 What's New in v0.2

**Three Sections:**
- NEW: Latest posts (created_at DESC)
- TOP: Most liked posts (like_count DESC)
- YOU: Personal profile with stats

**New Endpoints:**
- `/v1/best` - Get top posts by likes
- `/v1/profile?device_id=xxx` - Get user profile + posts

**Database Changes:**
- Added `like_count` column (denormalized for performance)
- Automatic triggers update like_count on like/unlike
- Index on `(like_count DESC, created_at DESC)` for TOP section

**UI Improvements:**
- Info window ([I] key) with controls help
- Section navigation with arrow keys
- Instant like updates (no full refresh)
- Beautiful section buttons with selection highlight

*MicroCast v0.2 - Quick dev setup for M5Cardputer*
