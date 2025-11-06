# 🚀 MicroCast - Pocket social microblogging platform for M5Cardputer / ESP32-S3

<img width="1442" height="326" alt="MC_gh" src="https://github.com/user-attachments/assets/a5fa08b4-71dd-4202-bb70-442ec6c060e4" />

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/kotov)

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
│ MicroCast [15]          beta0.4      │
│ [N]WiFi🟢 [U]User:alice   [I]Info   │
│ ┌────┐┌────┐┌────┐┌──┐              │
│ │NEW ││TOP ││YOU ││# │ ← Sections   │
│ └────┘└────┘└────┘└──┘              │
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

### Four Sections

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
| **M** | Open Donate QR popup |
| **Enter on MicroCast** | Open Donate QR (when header is focused) |
| **C** | View comments on selected post |
| **R** | Refresh current section |
| **U** | Change username |
| **N** | Add new WiFi network |
| **S** | Settings menu (in YOU section) |
| **Opt + R** | Show Recovery Code (save to restore account) |
| **I** | Show Info window (controls help) |
| **Fn + Opt + Del** | Clear device (requires confirmation) |

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
- Comment text (up to 120 chars, scrollable)

**Navigation:**
- **; .** (up/down) - Select comment (highlighted with green frame)
- **, /** (left/right) - Scroll selected comment text horizontally
- **ESC** - Return to main feed
- **Fn+Enter** - Write a new comment

**Reading Long Comments:**
If a comment is longer than the screen width, you can scroll it:
1. Use **; .** to select the comment you want to read
2. Selected comment will have a **green frame**
3. Use **, /** (left/right arrows) to scroll the text
4. **<** and **>** indicators show you can scroll more
5. Scroll by 5 characters at a time for smooth reading

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

## ⚙️ Settings Menu (NEW!)

Access advanced settings from the **YOU** section!

### How to Open
1. Switch to **YOU** section
2. Press **S** key
3. Settings menu opens

```
┌──────────────────────────────────┐
│ Settings                         │
│ ────────────────────────────────│
│  > Notifications                 │
│    WiFi List                     │
│                                  │
│ [;/.] Navigate  [Enter] Select   │
│ [ESC] Back                       │
└──────────────────────────────────┘
```

### Notifications Settings

Configure push notifications for new posts:

```
┌──────────────────────────────────┐
│ Settings > Notifications         │
│ ────────────────────────────────│
│ [X] Enable notifications         │
│ Check interval: 10 min (1-60)    │
│ [X] Sound alert                  │
│ [X] LED indicator                │
│                                  │
│ Last check: 5 min ago            │
│ Unread: 3 posts                  │
│                                  │
│ [1] Toggle  [2] Sound  [3] LED   │
│ [+/-] Interval  [ESC] Back       │
└──────────────────────────────────┘
```

**Features:**
- 🔔 **Enable/disable** notifications
- ⏱️ **Interval:** 1, 5, 10, 15, 30, or 60 minutes
- 🔊 **Sound alert:** Beep once per batch of new posts
- 💡 **LED indicator:** Green blinking every 10 seconds
- 📊 **Status:** See last check time and unread count

**Controls:**
- **[1]** - Toggle notifications on/off
- **[2]** - Toggle sound alert
- **[3]** - Toggle LED indicator
- **[+]** - Increase interval (1→5→10→15→30→60)
- **[-]** - Decrease interval (60→30→15→10→5→1)
- **[ESC]** - Save and return

**Badge Indicator:**
When you have unread posts, a red badge appears on the **NEW** section button showing the count!

### WiFi List Settings

Manage multiple WiFi networks (up to 5):

```
┌──────────────────────────────────┐
│ Settings > WiFi List             │
│ ────────────────────────────────│
│  > Home WiFi             [*]     │
│    Office Network                │
│    Coffee Shop Guest             │
│                                  │
│ [;/.] Select  [A] Add            │
│ [D] Delete  [ESC] Back           │
└──────────────────────────────────┘
```

**Features:**
- 📡 **Save up to 5** Wi-Fi networks
- 🔄 **Auto-connect** to any available saved network
- ⭐ **Current network** marked with **[*]**
- ➕ **Add** new networks without disconnecting
- 🗑️ **Delete** networks you no longer use

**Controls:**
- **[; .]** - Navigate through saved networks
- **[A]** - Add new network (scans available networks)
- **[D]** - Delete selected network
- **[ESC]** - Return to Settings menu

**How it works:**
1. Device stores up to 5 Wi-Fi credentials
2. On startup, auto-connects to any available saved network
3. No need to manually select every time!
4. Add networks from different locations (home, work, cafe)
5. Device always tries to connect to a saved network first

---

## 🌐 WiFi Settings

### Quick Add Network
Press **N** key anywhere to add a new Wi-Fi network:
1. Device scans for available networks
2. Select network with **; .**
3. Press **Enter**
4. Type password (if needed)
5. Press **Enter**
6. **Network is saved** to your list!
7. Press **ESC** at any time to cancel

**Note:** Pressing **N** no longer disconnects you from current WiFi! It just adds a new network to your saved list.

### WiFi Indicator
- 🟢 **Green circle** = Connected
- 🔴 **Red circle** = Disconnected
- **[N]WiFi🟢** shows in top bar

If red, press **N** to add/connect to a network.

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

## 🔐 Recovery Code (NEW!)

**Save your account in case of device reset!**

### What is Recovery Code?

When you register, MicroCast creates a special code (like `ABC-123`) that lets you restore your account if:
- Device breaks or resets
- You lose your settings
- Something goes wrong

### How to See Your Code

1. Press **Opt + R** anywhere in the app
2. Your recovery code appears on screen
3. **WRITE IT DOWN!** Take a photo or save it somewhere safe
4. You'll need it to restore your account

```
┌──────────────────────────┐
│   Recovery Code          │
├──────────────────────────┤
│                          │
│       ABC-123            │
│                          │
│   WRITE IT DOWN!         │
│ You'll need it to restore│
│ account if device resets.│
│                          │
│ [Enter] Close            │
└──────────────────────────┘
```

**Important:**
- ✅ Code is shown ONLY on your device (secure!)
- ✅ You can view it anytime with Opt+R
- ✅ Anyone with the code can access your account
- ⚠️ Keep it secret and safe!

### How to Restore Account

If your device resets:

1. First run will ask: **New Registration** or **Restore Account**
2. Choose **Restore Account**
3. Enter your recovery code (e.g., `ABC-123`)
4. Press Enter
5. Your username and device_id are restored!

**Without Recovery Code:**
- You'll lose access to your old username
- All your posts stay, but you can't post as that user anymore
- You'll have to register a new username

**Save your code now! Press Opt+R** 🔐

---

## 🧹 Reset Everything

**Warning:** This erases all your data!

1. Press **Fn + Opt + Del** (hold Fn and Opt, press Del)
2. Confirmation screen appears
3. Press **Enter** to confirm
4. Device will restart

**What gets deleted:**
- Your username
- WiFi settings
- Device ID
- Recovery code
- All settings

Use this if:
- You want to start fresh
- Selling/giving away device
- Something is broken

**⚠️ Save your recovery code before reset if you want to restore later!**

---

## 💤 Power Saving (NEW!)

MicroCast now automatically saves battery!

**Auto Screen Sleep:**
- Screen turns off after **60 seconds** of no activity
- Press **any key** to wake up
- Returns to the screen you were on
- Extends battery life **2-3x longer**

**What counts as activity:**
- Any key press
- Scrolling posts
- Liking posts
- Writing text

**Note:** Screen won't sleep during space matrix animation!

```

**Features:**
- ASCII stars with twinkling effect
- Color gradient: dark blue → cyan → white
- Smooth animation (no flicker!)
- 30 seconds duration
- Press **ESC** to exit anytime

**Characters used:** `.':*+=#@`

---

## 💡 Tips & Tricks

### Save Battery
- **NEW:** Auto-sleep saves battery automatically!
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

**Q: What's the difference between NEW, TOP, YOU, and # sections?**  
A: NEW shows latest posts, TOP shows most liked posts ever, YOU shows your profile and posts, # shows space matrix animation.

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

**Q: How do I read long comments?**  
A: Select the comment with ; . keys (it will have a green frame), then use , / to scroll the text left/right.

**Q: Why does the screen turn off?**  
A: Auto power-saving! Screen sleeps after 60 seconds of no activity. Press any key to wake up.

**Q: Can I disable auto-sleep?**  
A: No, it's always on to save battery. Just press any key to wake the screen.

**Q: What is a Recovery Code?**  
A: A backup code (like ABC-123) to restore your account if device resets. Press Opt+R to see it.

**Q: What happens if I lose my recovery code?**  
A: You'll lose access to your username forever. Your posts stay, but you can't post as that user anymore. Always save your code!

**Q: Can I use periods and semicolons in posts now?**  
A: Yes! Write natural sentences with punctuation!

**Q: How do I change the device reset button?**  
A: Press Fn+Opt+Del (not Fn+C anymore in v0.6) for safer accidental press protection.

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

**Q: How do I see battery level?**  
A: Battery indicator shows in the top bar with icon and percentage. Updates automatically.

**Q: How do I manage notifications?**  
A: Press **S** in YOU section → Settings → Notifications. Configure interval, sound, LED.

**Q: Can I save multiple Wi-Fi networks?**  
A: Yes! Up to 5 networks. Press **S** → WiFi List to manage them.

**Q: Does pressing N disconnect me from WiFi?**  
A: No! In v0.5, **N** just adds a new network without disconnecting.

**Q: How do I read a comment that's 120 characters long?**  
A: Select it with **; .** (green frame appears), then scroll with **, /** arrows. All text is accessible!

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

*MicroCast v0.6 - Made for M5Cardputer community*

**New in v0.6:**
- 🔐 **Recovery Code System!** Save your account with a backup code
- 🔄 **Account Restore!** Recover username after device reset
- 🛡️ **Secure Design!** Code shown only on your device
- 🔐 **Opt+R** - View your recovery code anytime
- 🗑️ **Fn+Opt+Del** - New device clear combo (safer)
- ℹ️ **Updated Help** - Info popup with all new controls
- 💸 **Donate QR popup** — press **M** or focus header **MicroCast** and press **Enter**
- 🎨 **Clean layout** — large square QR left, text on right; Telegram (cyan), Reddit (orange)
- ⚙️ **Settings Menu** (YOU → S)
- 🔔 **Notifications** (intervals, sound, LED)
- 📡 **Multi‑WiFi** (до 5 сетей, авто‑подключение)
- 📛 **Unread Badge** на кнопке NEW
- 🔋 **Battery Level** в верхней строке
- 📜 **Full Comment Scrolling** до 120 символов

**Previous (v0.4):**
- 📜 **Scrollable comments!** Read full long comments with left/right arrows
- 💤 **Auto screen sleep** after 60s - saves battery!
- 🌌 **# Space Matrix** - beautiful cosmic animation
- ✅ **Period and semicolon** now work in text input
- 🎨 Improved comment layout with date/time
- 🔋 2-3x longer battery life with power saving

**Previous (v0.3):**
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
