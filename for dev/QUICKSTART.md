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

## ⌨️ Quick Controls (v0.6)

| Key | Action |
|-----|--------|
| **; .** or **Up/Down** | Scroll posts/comments |
| **, /** or **Left/Right** | Switch sections / Scroll comment text |
| **Enter** | Like/Unlike OR confirm selection |
| **Fn+Enter** | New post / New comment |
| **M** | Open Donate QR popup |
| **Enter on MicroCast** | Open Donate QR (when header focused) |
| **C** | View comments on post |
| **R** | Refresh current section |
| **U** | Change username |
| **N** | Add new WiFi network |
| **S** | Settings menu (in YOU section) |
| **Opt+R** | Show Recovery Code (backup) |
| **I** | Show Info window (help) |
| **Fn+Opt+Del** | Clear device (with confirmation) |

**New Features (v0.6):**
- **Recovery Code:** Press **Opt+R** to view backup code (save it!)
- **Account Restore:** Recover username after device reset
- **Secure Design:** Code stored only on device
- **Improved Clear:** Fn+Opt+Del with confirmation
- **Updated Help:** Info popup includes all new controls
- **Donate QR:** Press **M** or focus header **MicroCast** and press **Enter** to open QR (ko-fi.com/kotov)

**From v0.5:**
- **Settings Menu:** Press **S** in YOU section
- **Notifications:** Background checks, sound, LED alerts
- **WiFi List:** Save up to 5 networks, auto-connect
- **Battery:** Real-time level in top bar
- **Badge:** Unread post count on NEW button

**Previous Features:**
- **NEW** section: Latest posts
- **TOP** section: Most liked posts (all-time)
- **YOU** section: Your profile + stats (Total Likes, Post count)
- **Comments:** View and post comments on any post
- **Auto-sleep:** Screen off after 60s inactivity

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

## ✅ Success Check (v0.6)

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

3. **Comments test:**
   - Press C on any post
   - Comments view opens
   - Press Fn+Enter to write comment
   - Type and send comment

4. **Settings test:**
   - Switch to YOU section
   - Press S key
   - Settings menu appears
   - Navigate with ; .
   - Press Enter to open submenu

5. **Notifications test:**
   - In Settings, select Notifications
   - Press 1 to toggle notifications
   - Press +/- to change interval
   - Press 2 for sound, 3 for LED
   - Check if settings save (ESC and reopen)

6. **WiFi List test:**
   - In Settings, select WiFi List
   - See your connected network with [*]
   - Press A to add another network
   - ESC to cancel without connecting

7. **Battery test:**
   - Check top bar for battery indicator
   - Should show real percentage (not 100%)

8. **Badge test:**
   - Wait for notification check
   - NEW button should show badge if unread posts

9. **Recovery Code test:**
   - Press Opt+R anywhere
   - Recovery code popup appears
   - Code displayed (e.g., ABC-123)
   - Press Enter to close

10. **Device Clear test:**
   - Press Fn+Opt+Del
   - Confirmation screen appears
   - Shows what will be deleted
   - ESC to cancel (don't actually clear!)

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

## 🆕 What's New in v0.6

**Recovery Code System:**
- Unique backup code (ABC-123 format) for each account
- Press Opt+R to view code anytime
- Account restore after device reset
- Code stored locally on device (secure!)
- Migration flow: existing users get code on first stats load

**Improved Device Clear:**
- New combo: Fn+Opt+Del (safer than old Fn+C)
- Confirmation screen prevents accidental wipes
- Shows what will be deleted before clearing
- Graceful restart after clear

**UI Improvements:**
- Info popup updated with new controls
- YOU section shows [Opt+R] Code and [S] Settings
- Recovery code popup with centered design
- Registration menu no longer flickers

**Screen Timeout Fix:**
- Timer resets on ALL key presses (arrows, text, etc.)
- Works in text input, WiFi selection, confirmations
- Consistent 60-second timeout everywhere

**Security:**
- Removed insecure GET /v1/recovery_code endpoint
- Code shown only via local Opt+R (can't be stolen remotely)
- Safe account restore via POST /v1/recovery_verify

**Backend Changes:**
- SQL migration adds recovery_code column
- Trigger auto-generates code on registration
- Recovery code returned once in /v1/register and /v1/stats
- New endpoint: POST /v1/recovery_verify (for restore)

**All v0.5 Features Included:**
- Settings menu, Notifications, Multi-WiFi, Battery, Badge, Full comment scrolling

*MicroCast v0.6 - Quick dev setup for M5Cardputer*
