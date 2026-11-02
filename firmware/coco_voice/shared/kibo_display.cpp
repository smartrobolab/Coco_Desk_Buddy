#include "kibo_display.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <wifi_manager.h>

#include <cstring>
#include <ctime>

#include "application.h"
#include "kibo_time.h"
#include "device_state.h"
#include "kibo_screens.h"


namespace kibo {

namespace {

constexpr uint32_t kNapAfterMs = 5 * 60 * 1000;  // no interaction -> sleeping face
constexpr uint32_t kErrorShowMs = 8000;
constexpr uint8_t kContrastNormal = 0xCF;
constexpr uint8_t kContrastNap = 0x08;
constexpr uint32_t kBlinkMs = 90;  // blink between two faces

bool IsErrorIcon(const char* e) {
  return strcmp(e, "warning") == 0 || strcmp(e, "cancel") == 0 || strcmp(e, "cloud_off") == 0 ||
         strcmp(e, "triangle_exclamation") == 0 || strcmp(e, "circle_xmark") == 0;
}

// The server's activation message contains the 6-digit code; pull it out.
std::string FindCode(const std::string& s) {
  int run = 0;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] >= '0' && s[i] <= '9') {
      run++;
      if (run == 6 && (i + 1 == s.size() || s[i + 1] < '0' || s[i + 1] > '9')) return s.substr(i - 5, 6);
    } else {
      run = 0;
    }
  }
  return "";
}

struct ActivityInfo {
  const char* name;
  uint32_t ms;
  uint8_t weight;
};
const ActivityInfo kActivities[] = {
    {"none", 0, 0},    {"eat", 6000, 3},   {"play", 9600, 3}, {"music", 6300, 2},       {"sing", 7200, 2},
    {"yawn", 2800, 1}, {"clock", 6000, 3}, {"nap", 0, 0},     {"low_battery", 4000, 0},
};
constexpr int kActivityCount = sizeof(kActivities) / sizeof(kActivities[0]);

}  // namespace

const char* activityName(Activity a) { return kActivities[(int)a].name; }

bool activityFromName(const char* name, Activity& out) {
  for (int i = 0; i < kActivityCount; i++) {
    if (strcmp(name, kActivities[i].name) == 0) {
      out = (Activity)i;
      return true;
    }
  }
  if (strcmp(name, "dance") == 0) {
    out = Activity::Music;
    return true;
  }
  if (strcmp(name, "sleep") == 0) {
    out = Activity::Nap;
    return true;
  }
  return false;
}

KiboDisplay::KiboDisplay(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, int width, int height)
    : io_(io), panel_(panel) {
  width_ = width;
  height_ = height;
  boot_at_ = NowMs();
  last_interaction_ = boot_at_;
  next_activity_at_ = boot_at_ + 30000;
  canvas_.clear();
  memset(last_sent_, 0, sizeof(last_sent_));
  xTaskCreate(TaskEntry, "kibo_face", 4096, this, 2, &task_);
}

KiboDisplay::~KiboDisplay() {
  if (task_) vTaskDelete(task_);
}

bool KiboDisplay::Lock(int timeout_ms) {
  if (timeout_ms <= 0) {
    lock_.lock();
    return true;
  }
  return lock_.try_lock_for(std::chrono::milliseconds(timeout_ms));
}

void KiboDisplay::Unlock() { lock_.unlock(); }

// ------------------------------------------------------------ XiaoZhi hooks

void KiboDisplay::SetStatus(const char* status) {
  std::lock_guard<std::mutex> g(state_mutex_);
  status_ = status ? status : "";
}

void KiboDisplay::ShowNotification(const char* notification, int duration_ms) {
  std::lock_guard<std::mutex> g(state_mutex_);
  notification_ = notification ? notification : "";
  notification_until_ = NowMs() + (uint32_t)duration_ms;
}

void KiboDisplay::SetEmotion(const char* emotion) {
  if (!emotion) return;
  std::lock_guard<std::mutex> g(state_mutex_);
  uint32_t now = NowMs();
  if (IsErrorIcon(emotion)) {
    error_until_ = now + kErrorShowMs;
    return;
  }
  if (emotion_ != emotion) {
    emotion_ = emotion;
    emotion_face_ = faceForEmotion(emotion);
    emotion_since_ = now;
  }
}

void KiboDisplay::SetChatMessage(const char* role, const char* content) {
  if (!role) return;
  std::lock_guard<std::mutex> g(state_mutex_);
  std::string text = content ? content : "";
  if (strcmp(role, "assistant") == 0) {
    assistant_text_ = text;
    assistant_since_ = NowMs();
  } else if (strcmp(role, "user") == 0) {
    user_text_ = text;
  } else {
    system_text_ = text;
    std::string code = FindCode(text);
    if (!code.empty()) activation_code_ = code;
  }
}

void KiboDisplay::ClearChatMessages() {
  std::lock_guard<std::mutex> g(state_mutex_);
  assistant_text_.clear();
  user_text_.clear();
}

void KiboDisplay::SetPowerSaveMode(bool on) {
  {
    std::lock_guard<std::mutex> g(state_mutex_);
    power_save_ = on;
  }
  SetContrast(on ? kContrastNap : kContrastNormal);
}

// ------------------------------------------------------------ behaviour API

void KiboDisplay::React(Face face, uint32_t duration_ms, bool force) {
  std::lock_guard<std::mutex> g(state_mutex_);
  uint32_t now = NowMs();
  reaction_face_ = face;
  reaction_since_ = now;
  reaction_until_ = now + duration_ms;
  reaction_forced_ = force;
  activity_ = Activity::None;
  last_interaction_ = now;
}

void KiboDisplay::StartActivity(Activity a, uint32_t duration_ms) {
  std::lock_guard<std::mutex> g(state_mutex_);
  if (a == Activity::Nap) {
    last_interaction_ = NowMs() - kNapAfterMs;  // fall asleep right away
    reaction_until_ = 0;
    activity_ = Activity::None;
    return;
  }
  pending_activity_ = a;  // starts on the next idle frame
  pending_activity_ms_ = duration_ms;
}

void KiboDisplay::SetGaze(int x, int y) {
  std::lock_guard<std::mutex> g(state_mutex_);
  imu_gaze_x_ = clampi(x, -10, 10);
  imu_gaze_y_ = clampi(y, -6, 6);
}

void KiboDisplay::NoteInteraction() {
  bool woke = false;
  {
    std::lock_guard<std::mutex> g(state_mutex_);
    uint32_t now = NowMs();
    if (now - last_interaction_ > kNapAfterMs) {
      woke = true;
      reaction_face_ = Face::Yawn;
      reaction_since_ = now;
      reaction_until_ = now + 2800;
      reaction_forced_ = false;
    }
    last_interaction_ = now;
  }
  if (woke) SetContrast(kContrastNormal);
}

void KiboDisplay::SetHoldMs(uint32_t held_ms) {
  std::lock_guard<std::mutex> g(state_mutex_);
  hold_ms_ = held_ms;
}

void KiboDisplay::SetBattery(int percent, bool present) {
  std::lock_guard<std::mutex> g(state_mutex_);
  battery_percent_ = percent;
  battery_present_ = present;
}

void KiboDisplay::SetContrast(uint8_t value) {
  // 0x81 = set contrast, same command on SSD1306 and SH1106
  if (io_) esp_lcd_panel_io_tx_param(io_, 0x81, &value, 1);
}

bool KiboDisplay::IsNapping() const {
  auto* self = const_cast<KiboDisplay*>(this);
  std::lock_guard<std::mutex> g(self->state_mutex_);
  return NowMs() - last_interaction_ > kNapAfterMs;
}

void KiboDisplay::PowerOff() {
  // Let the render task finish its current frame (it may be mid-I2C transfer),
  // then switch the panel off. Never suspend it: it could hold the I2C bus lock.
  stopped_ = true;
  vTaskDelay(pdMS_TO_TICKS(150));
  if (panel_) esp_lcd_panel_disp_on_off(panel_, false);
}

// ------------------------------------------------------------ rendering

void KiboDisplay::TaskEntry(void* arg) { static_cast<KiboDisplay*>(arg)->Loop(); }

void KiboDisplay::Loop() {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    uint32_t now = NowMs();
    if (stopped_) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (!IsSetupUICalled()) {
      // XiaoZhi is still starting: don't touch Application yet, just animate.
      canvas_.clear();
      canvas_.setColor(1);
      drawBoot(canvas_, now - boot_at_, nullptr);
      Flush();
      vTaskDelayUntil(&last, pdMS_TO_TICKS(40));
      continue;
    }
    Render(now);
    Flush();
    // Lower frame rate while the audio pipeline is busy (single-core C3).
    auto state = Application::GetInstance().GetDeviceState();
    bool busy = state == kDeviceStateListening || state == kDeviceStateSpeaking || state == kDeviceStateConnecting;
    vTaskDelayUntil(&last, pdMS_TO_TICKS(busy ? 70 : 40));
  }
}

void KiboDisplay::Flush() {
  if (!panel_) return;  // panel failed to initialise: run headless
  if (have_sent_ && memcmp(last_sent_, canvas_.buf, sizeof(last_sent_)) == 0) return;  // unchanged
  if (esp_lcd_panel_draw_bitmap(panel_, 0, 0, kWidth, kHeight, canvas_.buf) == ESP_OK) {
    memcpy(last_sent_, canvas_.buf, sizeof(last_sent_));
    have_sent_ = true;
  }
}

void KiboDisplay::PickNextActivity(uint32_t now) {
  bool timeValid = time(nullptr) > 1700000000;
  bool lowBattery = battery_present_ && battery_percent_ <= 15;
  int total = 0;
  int weights[kActivityCount] = {0};
  for (int i = 1; i <= (int)Activity::LowBattery; i++) {
    Activity a = (Activity)i;
    int w = kActivities[i].weight;
    if (a == Activity::Clock && !timeValid) w = 0;
    if (a == Activity::LowBattery && lowBattery) w = 6;
    weights[i] = w;
    total += w;
  }
  rng_ = hash32(rng_ + now);
  int pick = total ? (int)(rng_ % (uint32_t)total) : 0;
  Activity chosen = Activity::None;
  for (int i = 1; i <= (int)Activity::LowBattery && total; i++) {
    if (pick < weights[i]) {
      chosen = (Activity)i;
      break;
    }
    pick -= weights[i];
  }
  activity_ = chosen;
  activity_since_ = now;
  activity_until_ = now + kActivities[(int)chosen].ms;
  next_activity_at_ = activity_until_ + 25000 + hash32(rng_) % 35000;  // 25-60 s later
  if (chosen != Activity::None && on_activity_) on_activity_(chosen);
}

void KiboDisplay::RenderIdle(uint32_t now, FaceCtx& ctx) {
  Face face = Face::Neutral;
  if (pending_activity_ != Activity::None) {
    Activity a = pending_activity_;
    uint32_t len = pending_activity_ms_ ? pending_activity_ms_ : kActivities[(int)a].ms;
    pending_activity_ = Activity::None;
    activity_ = a;
    activity_since_ = now;
    activity_until_ = now + len;
    reaction_until_ = 0;
    last_interaction_ = now;
    if (on_activity_) on_activity_(a);
  }

  bool napping = now - last_interaction_ > kNapAfterMs;
  if (now < reaction_until_) {
    face = reaction_face_;
  } else if (activity_ != Activity::None && now < activity_until_) {
    switch (activity_) {
      case Activity::Eating: face = Face::Eating; break;
      case Activity::Playing: face = Face::Playing; break;
      case Activity::Music: face = Face::Music; break;
      case Activity::Sing: face = Face::Singing; break;
      case Activity::Yawn: face = Face::Yawn; break;
      case Activity::LowBattery: face = Face::LowBattery; break;
      case Activity::Clock: {
        time_t t = time(nullptr);
        struct tm tm;
        localtime_r(&t, &tm);
        char date[24];
        strftime(date, sizeof(date), "%a %d %b", &tm);
        drawClock(canvas_, tm.tm_hour, tm.tm_min, date, now - activity_since_);
        face = Face::Count;  // clock drawn instead of a face
        break;
      }
      default: break;
    }
  } else {
    activity_ = Activity::None;
    if (napping) {
      face = Face::Sleeping;
    } else if (now >= next_activity_at_) {
      PickNextActivity(now);
    }
    // idle gaze wandering
    if (now >= next_wander_at_) {
      rng_ = hash32(rng_ ^ now);
      bool centre = (rng_ & 3) == 0;
      wander_x_ = centre ? 0 : (int)(rng_ % 13) - 6;
      wander_y_ = centre ? 0 : (int)((rng_ >> 8) % 7) - 3;
      next_wander_at_ = now + 1500 + (rng_ >> 16) % 2500;
    }
  }
  if (face != Face::Count) {
    ctx.lookX += wander_x_;
    ctx.lookY += wander_y_;
  }
  ctx.level = -1;
  shown_face_ = face == Face::Count ? Face::Neutral : face;
  if (face != last_face_) {
    last_face_ = face;
    face_since_ = now;
  }
  if (face != Face::Count) {
    ctx.t = now - face_since_;
    if (ctx.t < kBlinkMs) drawBlink(canvas_, ctx);
    else drawFace(canvas_, face, ctx);
  }
}

void KiboDisplay::Render(uint32_t now) {
  auto& app = Application::GetInstance();
  int state = (int)app.GetDeviceState();
  bool voice = state == kDeviceStateListening && app.IsVoiceDetected();

  std::lock_guard<std::mutex> g(state_mutex_);
  if (state != last_state_) {
    if (state == kDeviceStateIdle) {
      // conversation over: wait a bit before the next idle activity
      next_activity_at_ = now + 20000;
    }
    // New turn: forget the last subtitles. (Not on Speaking - the first sentence
    // can arrive just before this task notices the state change.)
    if (state == kDeviceStateIdle || state == kDeviceStateListening) {
      user_text_.clear();
      assistant_text_.clear();
    }
    last_state_ = state;
    state_since_ = now;
  }
  if (state == kDeviceStateListening || state == kDeviceStateSpeaking || state == kDeviceStateConnecting) {
    last_interaction_ = now;  // talking counts as interaction
  }

  canvas_.clear();
  canvas_.setColor(1);

  // gaze: IMU tilt + (idle) wander, smoothed
  gaze_x_ += ((float)imu_gaze_x_ - gaze_x_) * 0.3f;
  gaze_y_ += ((float)imu_gaze_y_ - gaze_y_) * 0.3f;
  FaceCtx ctx;
  ctx.lookX = (int)gaze_x_;
  ctx.lookY = (int)gaze_y_;

  Face face = Face::Count;  // Count = a screen was drawn instead of a face
  const char* subtitle = nullptr;
  uint32_t subtitle_since = 0;
  bool forced = reaction_forced_ && now < reaction_until_;

  switch (state) {
    case kDeviceStateUnknown:
    case kDeviceStateStarting:
      drawBoot(canvas_, now - boot_at_, nullptr);
      break;
    case kDeviceStateWifiConfiguring: {
      if (now >= next_ap_check_) {
        wifi_sta_list_t list;
        phone_joined_ = esp_wifi_ap_get_sta_list(&list) == ESP_OK && list.num > 0;
        next_ap_check_ = now + 1000;
      }
      auto& wifi = WifiManager::GetInstance();
      std::string ssid = wifi.GetApSsid();
      std::string url = wifi.GetApWebUrl();
      drawWifiSetup(canvas_, ssid.c_str(), url.c_str(), phone_joined_, now - state_since_);
      break;
    }
    case kDeviceStateActivating:
      if (!activation_code_.empty()) drawActivation(canvas_, activation_code_.c_str(), now);
      else drawNotice(canvas_, status_.c_str(), system_text_.c_str(), false);
      break;
    case kDeviceStateUpgrading:
      drawNotice(canvas_, status_.c_str(), system_text_.c_str(), false);
      break;
    case kDeviceStateFatalError:
      drawNotice(canvas_, status_.c_str(), system_text_.c_str(), true);
      break;
    case kDeviceStateConnecting:
      face = forced ? reaction_face_ : Face::Surprised;  // "aaaa?" - just woke up
      break;
    case kDeviceStateListening:
    case kDeviceStateAudioTesting:
      face = forced ? reaction_face_ : Face::Listening;
      ctx.level = voice ? 85 : 12;
      if (!user_text_.empty()) {
        subtitle = user_text_.c_str();
        subtitle_since = state_since_;
      }
      break;
    case kDeviceStateSpeaking:
    case kDeviceStateNotifying:
      face = forced ? reaction_face_ : emotion_face_;
      if (face == Face::Neutral) face = Face::Talking;
      // the AI is singing a song (its text contains a music note) -> sing with the mic
      if (!forced && (assistant_text_.find("\xE2\x99\xAA") != std::string::npos ||   // U+266A
                      assistant_text_.find("\xE2\x99\xAB") != std::string::npos ||   // U+266B
                      assistant_text_.find("\xF0\x9F\x8E\xB5") != std::string::npos)) {  // U+1F3B5
        face = Face::Singing;
      }
      ctx.talking = true;
      if (!assistant_text_.empty()) {
        subtitle = assistant_text_.c_str();
        subtitle_since = assistant_since_;
      }
      break;
    case kDeviceStateIdle:
    default:
      if (now < error_until_) {
        drawNotice(canvas_, status_.c_str(), system_text_.c_str(), true);
      } else {
        RenderIdle(now, ctx);
        face = Face::Count;  // RenderIdle already drew
      }
      break;
  }

  if (face != Face::Count) {
    if (face != last_face_) {
      last_face_ = face;
      face_since_ = now;
    }
    ctx.t = now - face_since_;
    shown_face_ = face;
    if (ctx.t < kBlinkMs && face != Face::Listening) drawBlink(canvas_, ctx);
    else drawFace(canvas_, face, ctx);
  }

  // ---- overlays ----
  if (subtitle) drawSubtitle(canvas_, subtitle, now - subtitle_since);
  if (hold_ms_ >= 2000) drawHoldMenu(canvas_, hold_ms_);
  if (state == kDeviceStateIdle && battery_present_ && battery_percent_ <= 20) {
    canvas_.setColor(0);
    canvas_.rect(112, 0, 16, 9);
    canvas_.setColor(1);
    drawBatteryIcon(canvas_, 114, 1, battery_percent_, false);
  }
  if (now < notification_until_ && !notification_.empty() && state != kDeviceStateWifiConfiguring) {
    canvas_.setColor(0);
    canvas_.rect(0, 0, kWidth, 9);
    canvas_.setColor(1);
    canvas_.textCentered(64, 1, notification_.c_str(), kFontSmall);
    canvas_.hline(0, kWidth - 1, 9);
  }
}

}  // namespace kibo
