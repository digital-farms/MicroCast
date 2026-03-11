# 📋 MicroCast Changelog

## v0.7 - OTA & Roles Update (2025-11-11)

### 🆕 New Features

#### **OTA (Over-The-Air) Updates**
- 📡 **Wireless Firmware Updates** — check and install updates without USB cable
- 🔄 **Manual Check** — Settings → Check Updates menu option
- 🟢 **Auto Background Check** — silent check every 24 hours
- 📍 **Update Indicator** — green dot next to version in header when update available
- ⚠️ **Safety Warnings** — "Do NOT turn off device" during update
- ✅ **Progress Bar** — visual progress with file size and percentage
- 🔄 **Auto Restart** — device reboots automatically after successful update
- ❌ **Safe Failure** — if update fails, device continues with current version

#### **Colored Nicknames (Role System)**
- 🎨 **Color-coded usernames** based on platform roles:
  - **Cyan (0x07FF)** — default for all regular users
  - **Red (0xF800)** — admin/moderator users
  - **Yellow (0xFFE0)** — OG (early adopters, VIP)
- 📊 **Automatic assignment** — colors set server-side by platform admin
- 🔄 **Real-time display** — colors shown in feed, comments, and profile

#### **Fixed Post Timestamps**
- ⏰ **Accurate relative time** — posts no longer always show "now"
- 🔄 **Server time reference** — API returns `server_time` for correct relative calculation
- 📍 **Works everywhere** — feed, best, profile, and comments all use server time

#### **Full Post Text in Comments**
- 📝 **Dynamic layout** — original post shown with full text (not truncated)
- 📏 **Adaptive height** — separator and comments position adjust based on post length
- 👁️ **No more truncation** — up to 6 lines of post text visible
- 🔄 **Variable comment count** — more comments shown if post is short

#### **Truncation Indicator in Feed**
- ✂️ **"..." indicator** — shows when post text doesn't fit in 3 lines
- 📏 **114 chars limit** — posts longer than 114 chars show truncation marker
- 👆 **Hint to open comments** — click [C] to see full text

#### **Improved Battery Accuracy**
- 🔋 **Voltage-based calculation** — uses AXP2101 PMIC voltage reading
- 📊 **Non-linear mapping** — better reflects actual battery percentage
- 🔄 **4-sample smoothing** — moving average for stable readings
- 📍 **UI fix** — OTA indicator no longer overlaps battery display

### 🐛 Fixes

- ✅ **Time display bug** — posts always showed "now" due to wrong reference time
- ✅ **Time display in comments** — same fix applied to comment timestamps
- ✅ **UI overlap** — "UPD" text no longer overlaps battery indicator (replaced with dot)
- ✅ **Battery accuracy** — percentage now matches actual charge level
- ✅ **Battery smoothing** — readings no longer jump around

### 📊 Statistics

- **New features:** 6 major (OTA updates, colored nicknames, timestamp fix, full post text, truncation indicator, battery improvements)
- **Worker changes:** 4 endpoints updated, 2 new helper functions, 1 new field
- **Database changes:** 1 new column
- **Client changes:** ~180 lines added/modified
- **UI screens:** 3 new (OTA check, OTA progress, OTA confirmation)
- **Bug fixes:** 6 (time display, UI overlap, battery accuracy)
- **Development time:** ~4 hours
- **Backward compatible:** Yes (server_time and name_color optional)

### 🔄 Migration from v0.6

**Automatic!** v0.7 seamlessly migrates existing users:
1. ✅ On first launch, server_time enables accurate timestamps
2. ✅ All existing posts get correct relative time
3. ✅ All users default to cyan (no manual action needed)
4. ✅ Admin can manually assign roles in Supabase anytime

**What's new on first launch:**
1. Green dot may appear if update available (requires deployed Worker)
2. Timestamps become accurate (requires deployed Worker)
3. Battery reading more accurate (new code, no server needed)

---

## v0.6 - Recovery Code Update (2025-11-05)

### 🆕 New Features

#### **Recovery Code System**
- 🔐 **Account Backup** - Unique code (e.g., ABC-123) generated for each device
- 🔄 **Account Restore** - Recover username and device_id after reset
- 🛡️ **Secure Design** - Code stored only on device, not accessible remotely
- 📱 **Opt+R** - View recovery code anytime from any screen
- 💾 **Persistent Storage** - Code saved in Preferences automatically

#### **Donate QR Popup**
- 💸 **Donate screen** with large square QR (ko-fi.com/kotov)
- ⌨️ Open via **M** key or focus header **MicroCast** and press **Enter**

### ✅ Included (merged from planned v0.5)
- ⚙️ Settings Menu (YOU → S)
- 🔔 Notifications (intervals, sound, LED, unread badge on NEW)
- 📡 Multiple Wi‑Fi networks (up to 5), auto‑connect, manage list
- 🔋 Battery indicator in top bar
- 📜 Full comment scrolling (120 chars), better arrows and layout
- 🧭 UI polishing and stability improvements

**How it works:**
1. Register username → server generates recovery code
2. Code sent to device once and saved locally
3. Press Opt+R to view your code
4. On device reset, choose "Restore Account" and enter code
5. Username and account restored!

**Recovery Flow:**
```
First Run:
 > New Registration
   Restore Account

[Enter recovery code]
→ ABC-123

✅ Account restored!
```

#### **Improved Device Clear**
- 🔒 **Fn+Opt+Del** - New safe combo (replaces Fn+C)
- ⚠️ **Confirmation Screen** - Prevents accidental wipes
- 📋 **Clear Preview** - Shows what will be deleted
- 🔄 **Graceful Restart** - Auto-restart after clear

#### **Updated Info Popup**
- ℹ️ **New Controls Added:**
  - [Opt+R] Recovery Code
  - [S] Settings  
  - [Fn+Opt+Del] Clear Device
- ❌ **Removed:** SOON section (no longer in UI)
- 🎨 **Larger Window** - Fits all controls comfortably

### ✨ UI/UX Improvements

#### **YOU Section**
- 🔐 **[Opt+R] Code** button displayed next to Total Likes
- ⚙️ **[S] Settings** button displayed next to Posts
- 📐 **Better Layout** - Buttons don't overlap post list
- 🎨 **Visual Balance** - Green code button, yellow settings button

#### **Recovery Code Popup**
- 💚 **Centered Design** - Code displayed prominently
- ⚠️ **Bold Warning** - "WRITE IT DOWN!" in red
- 📝 **Instructions** - Clear text about importance
- 🔲 **Clean Close** - [Enter] to dismiss

#### **Screen Timeout Fix**
- ⏱️ **Universal Timer** - Works in ALL screens and menus
- ⌨️ **All Keys Count** - Arrow keys, text input, everything resets timer
- 💤 **60 Second Rule** - Consistent across entire app
- 🎯 **Bug Fixes** - Timer now updates in WiFi selection, text input, confirmations

### 🐛 Fixes

- ✅ **Screen timeout** in text input (was ignoring keyboard)
- ✅ **Screen timeout** in WiFi selection (timer now updates)
- ✅ **Screen timeout** in confirmation dialogs (all keys reset timer)
- ✅ **Recovery code popup** UI centering and text alignment
- ✅ **Info popup** size increased to fit all controls
- ✅ **Registration menu** no flicker (draws only when selection changes)

### 🔒 Security Improvements

- 🛡️ **No Remote Code Access** - Removed GET /v1/recovery_code endpoint
- 🔐 **Local-Only Display** - Code shown only via Opt+R on physical device
- 🚫 **Attack Prevention** - Impossible to steal code remotely by device_id
- ✅ **Safe Restore** - POST /v1/recovery_verify validates code securely

### 📊 Statistics

- **New features:** 3 major (Recovery Code, Account Restore, Improved Clear)
- **Security fixes:** Removed insecure endpoint
- **UI improvements:** 4 (YOU section, Info popup, timer fixes, registration menu)
- **Bug fixes:** 5 (screen timeout in all contexts, UI centering)
- **Code changes:** ~150 lines added/modified
- **Development time:** ~2 hours
- **Backward compatible:** Yes (works with existing accounts)

### 🔄 Migration from v0.4

**Automatic!** v0.6 seamlessly migrates existing users:
1. ✅ On first launch after update, recovery code generated
2. ✅ Code shown via popup on first stats load (if new)
3. ✅ All posts, likes, comments, settings preserved
4. ✅ WiFi networks and notifications unchanged
5. ✅ Device clear combo updated (Fn+Opt+Del instead of Fn+C)

**What's new on first launch:**
1. Recovery code generated automatically
2. Popup shows your new code (save it!)
3. YOU section displays [Opt+R] Code button
4. Info popup includes new controls
5. Device clear requires Fn+Opt+Del combo

---

## v0.4 - UX & Power Saving Update (2025-10-31)

### 🆕 New Features

#### **Scrollable Comments**
- 📜 Horizontal text scrolling for long comments
- 🎯 Select comment with up/down arrows (`;` `.`)
- ⬅️➡️ Scroll comment text with left/right arrows (`,` `/`)
- 🖼️ Visual indicators: `<` and `>` show scroll direction
- 🎨 Selected comment highlighted with green frame
- 👁️ 3 comments visible at once, full text readable

#### **Power Saving Mode**
- 💤 Auto screen sleep after 60 seconds of inactivity
- 🔆 Any key press wakes up the screen
- 🔋 Significantly extends battery life
- 🎨 Screen state preserved (returns to current view)
- ⏱️ Activity timer resets on any interaction

#### **New Section: # **
- Coming soon...

#### **Text Input Improvements**
- ✅ Period (`.`) and semicolon (`;`) now work in text input!
- 📝 All printable characters allowed (ASCII 32-126)
- 🚫 Only ESC and backtick filtered out
- 💬 Write natural sentences with punctuation

### ✨ UI/UX Improvements

#### **Section Buttons**
- 📏 Spacing reduced to 1 pixel between buttons
- 🎨 # button: yellow color (0xFFE0), compact size (20px)
- 🎯 Better visual balance across screen width
- 📐 All buttons properly aligned

#### **Comments View**
- 📅 Date + time in right corner: `DD.MM HH:MM`
- 🎨 Black background under author and date
- 📍 Text positioned 1px up (breaks frame line visually)
- 🔄 3px shift towards center for better symmetry
- 🎯 Selected comment has green frame (0x07E0)

### 🔧 Technical Improvements

#### **Animation Optimization**
- 🚀 No more screen flicker in space matrix
- 💾 Incremental rendering (only changed pixels)
- 📊 Tracking array for previous brightness states
- 🎬 Smooth 12 FPS (80ms delay)
- 🎨 Static elements drawn once (header, footer)

#### **Memory Management**
- 📝 Comment text scroll position tracking
- 🔄 Vertical and horizontal scroll offsets
- 💾 Previous brightness array for animations
- ⚡ Efficient state management

### 🐛 Fixes

- ✅ Fixed `.` and `;` not working in text input mode
- ✅ Fixed long comments being truncated and unreadable
- ✅ Fixed screen flicker in space matrix animation
- ✅ Fixed # button frame not visible on right edge
- ✅ Fixed screen staying on during inactivity (battery drain)

### 📊 Statistics

- **New features:** 4 major (scrollable comments, power saving, space matrix, text input fix)
- **UI improvements:** 6 (section spacing, button design, comment layout, date format)
- **Performance:** 3x faster animation rendering (incremental updates)
- **Battery life:** ~2-3x longer with auto-sleep
- **Code changes:** ~180 lines added/modified
- **New functions:** 3 (screenSleep, screenWake, drawSpaceMatrix optimized)
- **Development time:** ~2 hours

---

## v0.3 - Comments Update (2025-10-28)

### 🆕 New Features

#### **Comments System**
- 💬 Comment on any post
- View all comments for a post
- Real-time comment counter on posts
- Beautiful comments UI with scroll support

#### **UI/UX**
- 💬 Comment bubble icon on posts (cyan)
- Comment counter next to likes: `❤️5  💬3`
- Full comments view screen
- Post preview at top of comments
- Comment list with scroll (3 visible at a time)
- Author name in cyan + timestamp
- Keyboard: **[C]** to open comments
- Keyboard: **[ESC]** to close comments
- Keyboard: **[Fn+Enter]** to write comment

### ✨ Improvements

#### **Text Input**
- **Fn + arrows** for cursor navigation (`,` `/` now available for text!)
- Initial text visible when editing (no need to press key)
- Better UX: no more "frightening" empty field

#### **Post Confirmation**
- Preview before posting
- **[Enter]** = send
- **[E]** = edit (keeps text!)
- **[ESC]** = cancel
- Same for username change

#### **Visual**
- Author name in **cyan** color (0x07FF)
- Dark background under author name (like heart icon)
- Better readability on green borders
- Consistent color scheme
- **Comment counter always visible** (shows 💬0 like ❤️0)
- Icons with dark background for better contrast
- Icons lowered by 2px for better alignment
- Comments preview header in green (0x07E0)
- Centered "Connected!" and "Loading..." messages
- Author name positioned at top of post box (boxY + 0)

#### **Info Window**
- Added **[C] Comments** hint
- Reorganized controls layout
- All features documented

### 🔧 Fixes

- Fixed `,` and `/` not working in text input
- Fixed text not visible when editing
- Fixed author name overlapping with border
- Fixed position reset after refresh (likes still update silently)
- **Fixed comment icon not showing when comments = 0** (now always visible)
- **Fixed missing icons in comments preview** (added heart + bubble icons)
- **Fixed icon positioning** (lowered by 2px in feed and preview)
- Fixed comments preview layout spacing
- Fixed hints position in comments view (moved to y=125)

### 📈 Statistics

- **Backend files:** 2 (SQL migration + Worker v0.3)
- **Client changes:** ~220 lines added/modified
- **New functions:** 5
- **New screens:** 1 (Comments View)
- **New APIs:** 2
- **New database tables:** 1
- **Visual fixes:** 8
- **Development time:** ~3 hours

---

*MicroCast - Pocket social network for M5Cardputer*
*Version 0.3 - October 28, 2025*
