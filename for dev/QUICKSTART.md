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
4. Copy SQL from readme.md (SUPABASE section)
5. Paste and click "Run"
6. Save these:
   - Project URL (Settings → API → URL)
   - Service Role Key (Settings → API → service_role key)
```

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

## ⌨️ Quick Controls

| Key | Action |
|-----|--------|
| **W/A** | Scroll up/down |
| **Enter** | Like/Unlike |
| **Fn+Enter** | New post |
| **R** | Refresh |
| **U** | Change username |
| **N** | Change WiFi |
| **Fn+C** | Clear device |

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

## ✅ Success Check

Test your setup:

1. **Post test:**
   - Press Fn+Enter
   - Type "Hello MicroCast!"
   - Press Enter
   - Should appear in feed

2. **Like test:**
   - Scroll to any post
   - Press Enter
   - Heart should turn red ❤️

3. **Refresh test:**
   - Press R
   - Feed updates

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
