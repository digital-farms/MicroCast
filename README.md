# 🚀 MicroCast - Pocket social microblogging platform for M5Cardputer / ESP32-S3

<img width="1442" height="326" alt="MC_gh" src="https://github.com/user-attachments/assets/a5fa08b4-71dd-4202-bb70-442ec6c060e4" />

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/I2I314IWIP)

# 📱 User Guide

**Simple microblogging for your M5Cardputer**

Post messages, like content, and connect with others - all from your pocket device!

---

## 🚀 Quick Start (5 minutes)

### What You Need
- ✅ M5Cardputer device
- ✅ USB-C cable
- ✅ WiFi network (2.4GHz)

### Installation

#### Option 1: M5Burner (Recommended)
1. Download [M5Burner](https://docs.m5stack.com/en/download)
2. Connect M5Cardputer via USB
3. Open M5Burner
4. Search for **"MicroCast"**
5. Click **"Burn"**
6. Wait for completion
7. Done! Unplug and restart device

#### Option 2: M5Launcher
1. On your M5Cardputer, open **M5Launcher**
2. Go to **"OTA"** section
3. Find **"MicroCast"**
4. Click **"Install"**
5. Wait for download
6. Launch the app

---

## 🎮 First Time Setup

### 1. Register Username
When you first open MicroCast:

```
┌──────────────────────────┐
│ First run - Registration │
├──────────────────────────┤
│ Enter username (min 3    │
│ chars):                  │
│ Press ESC to skip        │
└──────────────────────────┘
```

**Type your username** (3-24 characters, English only)
- Examples: `alice`, `bob123`, `cool_user`
- Must be unique - if taken, try another!

**Or press ESC** to skip and use temporary name
- You can register later by pressing [U]

### 2. Connect WiFi
```
┌──────────────────────────┐
│ Select WiFi Network      │
├──────────────────────────┤
│ > MyHomeWiFi             │
│   CoffeeShop_Free        │
│   Office_Guest           │
└──────────────────────────┘
```

- Use **W/A** or **; .** to scroll
- Press **Enter** to select
- Type password and press **Enter**

### 3. You're Ready!
The main feed will appear with latest posts.

---

## ⌨️ How to Use

### Main Screen

```
┌──────────────────────────────────────┐
│ MicroCast [15]          beta0.2      │
│ [N]WiFi🟢 [U]User:alice   [I]Info   │
│ ┌────┐ ┌────┐ ┌────┐                │
│ │NEW │ │TOP │ │YOU │  ← Sections    │
│ └────┘ └────┘ └────┘                │
│ ┌──────────────────────────────────┐ │
│ │ bob                              │ │
│ │ Hello everyone!                  │ │
│ │ ❤️ 5  💬 3     23.10.25 14:30   │ │
│ └──────────────────────────────────┘ │
│ ┌──────────────────────────────────┐ │
│ │ alice                            │ │
│ │ First post here!                 │ │
│ │ ❤️ 3  💬 0     23.10.25 14:25   │ │
│ └──────────────────────────────────┘ │
└──────────────────────────────────────┘
```

### Three Sections

**NEW** - Latest posts (newest first)
**TOP** - Most liked posts (all-time best)
**YOU** - Your profile and posts

**How to switch sections:**
1. Press **;** (up) until you reach the section buttons
2. Use **,** (left) and **/** (right) to select section
3. Press **Enter** to confirm
4. Press **.** (down) to return to posts

### Controls

| Key | What it does |
|-----|--------------|
| **; or Up** | Scroll up (or go to sections) |
| **. or Down** | Scroll down |
| **, or Left** | Previous section (when in section menu) |
| **/ or Right** | Next section (when in section menu) |
| **Enter** | Like/Unlike post OR confirm section |
| **Fn + Enter** | Create new post |
| **C** | View comments on selected post |
| **R** | Refresh current section |
| **U** | Change username |
| **N** | Change WiFi |
| **I** | Show Info window (controls help) |

---

## 📝 Creating Posts

1. Press **Fn + Enter** (hold Fn, then press Enter)
2. Type your message (3-120 characters)
3. Press **Enter** to post
4. Press **ESC** (`) to cancel

**Tips:**
- ✅ Keep it short and sweet
- ✅ Only English letters work
- ✅ Wait 10 seconds between posts
- ❌ No emojis (they show as ?)
- ❌ No Russian/Chinese text

---

## ❤️ Liking Posts

1. Scroll to any post using **; .**
2. Press **Enter** to like
3. Heart counter increases
4. Press **Enter** again to unlike

**Note:** 
- You can only like each post once!
- Likes update instantly without refreshing
- You stay on the same post after liking

---

## 💬 Comments (NEW!)

### Viewing Comments

Each post now shows a comment counter: **💬 3**

**To view comments:**
1. Scroll to any post using **; .**
2. Press **C** key
3. Comments screen opens!

```
┌──────────────────────────────────┐
│ Comments ========================│
│ bob                              │
│ Hello everyone! This is my...    │
│ ❤️ 5  💬 3 comments              │
│ ────────────────────────────────│
│ alice                      14:30 │
│ Great post!                      │
│                                  │
│ charlie                    14:25 │
│ Thanks for sharing               │
│                                  │
│ [Fn+Enter] Write  [ESC] Back     │
└──────────────────────────────────┘
```

**What you'll see:**
- Original post at top
- Up to 3 comments visible at once
- Author name and time for each comment
- Comment text (max 38 chars per line)

**Navigation:**
- **; .** - Scroll through comments
- **ESC** - Return to main feed
- **Fn+Enter** - Write a new comment

### Writing Comments

**To comment on a post:**
1. Open comments with **C** key
2. Press **Fn + Enter** (hold Fn, then press Enter)
3. Type your comment (3-120 characters)
4. Press **Enter** to see preview
5. Press **Enter** again to post
6. Or press **E** to edit, **ESC** to cancel

**Comment Confirmation:**
```
┌──────────────────────────────────┐
│ Confirm                          │
│ Post comment:                    │
│ ┌──────────────────────────────┐ │
│ │ My first comment! Very cool! │ │
│ └──────────────────────────────┘ │
│ Send this?                       │
│                                  │
│ [Enter] Yes  [E] Edit  [ESC] No  │
└──────────────────────────────────┘
```

**Tips:**
- ✅ Comments are 3-120 characters
- ✅ Wait 10 seconds between comments
- ✅ Comments update the counter instantly
- ✅ You can scroll through all comments
- ❌ Can't delete comments (yet)
- ❌ No emojis (they show as ?)

**Comment Counter:**
- **💬 0** - No comments yet (be first!)
- **💬 3** - 3 comments on this post
- Counter updates after you post

---

## 📊 Your Profile (YOU Section)

View your stats and posts in the **YOU** section!

```
┌──────────────────────────────────┐
│ User:alice                       │
│ ID:a1b2c3d4...                   │
│ ❤️ Total Likes:42                │
│ Posts:15                         │
│ ┌────────────────────────────┐   │
│ │ alice                      │   │
│ │ My latest post!            │   │
│ │ ❤️ 8  💬 2  26.10.25 19:41│   │
│ └────────────────────────────┘   │
└──────────────────────────────────┘
```

**What you'll see:**
- Your username and device ID
- **Total Likes** - All likes you've received
- **Posts** - How many posts you've made
- Your last 20 posts (newest first)

**How to access:**
1. Scroll up to section buttons
2. Select **YOU** with **,** **/** keys
3. Press **Enter**

---

## 👤 Changing Username

1. Press **U** key
2. Type new username (3-24 characters)
3. Press **Enter**

**Important:**
- Your old username becomes available for others
- New username must be unique
- If taken, try a different one

---

## ℹ️ Info Window (Quick Help)

Press **I** key anytime to see controls help!

```
┌─────────────────────────────┐
│         Controls            │
├─────────────────────────────┤
│ [Up/Dn] Scroll [Enter] Like │
│ [Lt/Rt] Sections [R] Refresh│
│ [Fn+Enter] Post [C] Comments│
│ [U] Username [N] WiFi [I]...│
│                             │
│ NEW latest TOP best YOU...  │
│ Press any key to close      │
└─────────────────────────────┘
```

**Quick reference for all controls!**
- Press **I** to open
- Press any key to close

---

## 🌐 WiFi Settings

### Connect to Different Network
1. Press **N** key
2. Select network with **; .**
3. Press **Enter**
4. Type password
5. Press **Enter**

### WiFi Indicator
- 🟢 **Green circle** = Connected
- 🔴 **Red circle** = Disconnected

If red, press **N** to reconnect.

---

## 🔧 Troubleshooting

### "Username may be taken"
**Problem:** Someone else is using that name  
**Solution:** Try a different username

### "Device not registered"
**Problem:** You skipped registration  
**Solution:** Press **U** to register now

### Red WiFi indicator
**Problem:** Not connected to internet  
**Solution:** Press **N** to select WiFi

### Posts not showing
**Problem:** Feed not updated  
**Solution:** Press **R** to refresh

### "slow device" error
**Problem:** Posting too fast (anti-spam)  
**Solution:** Wait 10 seconds between posts

### Device frozen
**Problem:** Stuck on some screen  
**Solution:** 
1. Try pressing **ESC** (`)
2. If still frozen, unplug and replug USB

---

## 🧹 Reset Everything

**Warning:** This erases all your data!

1. Press **Fn + C** (hold Fn, press C)
2. Confirmation screen appears
3. Press **Enter** to confirm
4. Device will reset
5. Unplug and replug to restart

**What gets deleted:**
- Your username
- WiFi settings
- Device ID

Use this if:
- You want to start fresh
- Selling/giving away device
- Something is broken

---

## 💡 Tips & Tricks

### Save Battery
- Turn off device when not using
- Reduce screen brightness (if possible)

### Better Experience
- Use short, clear messages
- Like posts you enjoy
- Press **I** to see all controls anytime
- Check **TOP** section for most popular posts
- Use **YOU** section to track your stats
- Refresh with **R** only when needed (likes update automatically!)
- Check WiFi indicator often
- Press **C** to read comments on interesting posts
- Comment counter 💬 shows how active a discussion is

### Privacy
- Don't use your real name
- Don't post personal info
- Remember: posts are public
- Anyone can see what you write

---

## ❓ FAQ

**Q: What's the difference between NEW, TOP, and YOU sections?**  
A: NEW shows latest posts, TOP shows most liked posts ever, YOU shows your profile and your posts.

**Q: Can I delete my posts?**  
A: No, posts are permanent once posted.

**Q: Can I delete my comments?**  
A: No, comments are also permanent once posted.

**Q: Can I send private messages?**  
A: No, all posts are public.

**Q: How many posts can I see?**  
A: Up to 50 posts per section.

**Q: Why does my position reset after I refresh?**  
A: Press **R** to refresh (resets position). Liking a post updates feed but keeps your position!

**Q: What is Total Likes in YOU section?**  
A: Total Likes = all likes you've received across all your posts (all-time).

**Q: Can I see who liked my posts?**  
A: No, only the total count is shown.

**Q: How many comments can I see?**  
A: All comments are loaded, 3 visible at a time. Scroll with ; . keys.

**Q: Can I reply to a specific comment?**  
A: No, all comments are on the post level (no nested replies).

**Q: Can I use emojis?**  
A: No, only English letters and symbols work.

**Q: What if I lose my device?**  
A: Your username is tied to device. If lost, username is lost too.

**Q: Can I have multiple accounts?**  
A: One username per device only.

**Q: Is it free?**  
A: Yes! Completely free to use.

**Q: Do I need to create an account?**  
A: Just pick a username - no email or password needed!

**Q: What does the device count [N] mean?**  
A: Shows how many devices are registered on the network.

---

## 🆘 Need Help?

**Something not working?**
1. Try pressing **R** to refresh
2. Check WiFi (press **N**)
3. Restart device (unplug/replug)

**Still having issues?**
- Check if others are online (device count at top)
- Make sure WiFi is 2.4GHz (not 5GHz)
- Try different WiFi network

**Report bugs:**
- Open issue on GitHub
- Describe what happened
- Include what you were doing

---

## 🎉 Have Fun!

MicroCast is all about simple, quick communication.

**Remember:**
- ✅ Be kind to others
- ✅ Keep messages short
- ✅ Have fun posting!
- ❌ Don't spam
- ❌ Don't post mean things

**Enjoy your pocket social network! 📱✨**

---

*MicroCast v0.3 - Made for M5Cardputer community*

**New in v0.3:**
- 💬 **Comments system!** View and post comments on any post
- 🔢 Comment counter on all posts (💬 N)
- 📝 Write comments with Fn+Enter in comments view
- 📜 Scroll through comments with ; . keys
- ✅ Comment confirmation before posting
- 🎨 Beautiful comments UI with icons

**Previous (v0.2):**
- 📊 Three sections: NEW, TOP, YOU
- ℹ️ Info window with quick help ([I] key)
- 🏆 TOP section shows most liked posts
- 👤 YOU section with profile stats
- ⚡ Instant like updates (no refresh needed)
- 🎨 Improved UI with section buttons
