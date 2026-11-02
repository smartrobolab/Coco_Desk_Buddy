// KiboDisplay - XiaoZhi Display implementation that draws Kibo's animated
// faces on a 128x64 SH1106/SSD1306 OLED, instead of XiaoZhi's LVGL text UI.
//
// It follows the XiaoZhi device state (idle / listening / speaking / Wi-Fi
// setup / activation...) and the LLM emotion, and runs Kibo's own idle life
// (eating, playing, music, naps, clock) when nobody is talking to it.
#pragma once

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "display/display.h"
#include "kibo_canvas.h"
#include "kibo_faces.h"

namespace kibo {

enum class Activity : uint8_t { None, Eating, Playing, Music, Sing, Yawn, Clock, Nap, LowBattery };

const char* activityName(Activity a);
bool activityFromName(const char* name, Activity& out);

class KiboDisplay : public Display {
 public:
  KiboDisplay(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, int width, int height);
  ~KiboDisplay() override;

  // ---- XiaoZhi Display interface ----
  void SetStatus(const char* status) override;
  void ShowNotification(const char* notification, int duration_ms = 3000) override;
  void SetEmotion(const char* emotion) override;
  void SetChatMessage(const char* role, const char* content) override;
  void ClearChatMessages() override;
  void SetPowerSaveMode(bool on) override;
  void UpdateStatusBar(bool update_all = false) override {}
  bool IsMonochrome() const override { return true; }

  // ---- Kibo behaviour API (thread-safe) ----
  // Shows `face` for `duration_ms`. Only while idle unless `force` (e.g. AI tool call).
  void React(Face face, uint32_t duration_ms, bool force = false);
  void StartActivity(Activity a, uint32_t duration_ms = 0);  // 0 = default length
  void SetGaze(int x, int y);           // from the IMU; pixels, small range
  void NoteInteraction();               // touch/motion: wakes Kibo, resets idle timers
  void SetHoldMs(uint32_t held_ms);     // long-press progress overlay, 0 = hidden
  void SetBattery(int percent, bool present);
  void SetContrast(uint8_t value);
  bool IsNapping() const;
  Face CurrentFace() const { return shown_face_.load(); }
  void PowerOff();                      // panel off before deep sleep
  // Called when an idle activity starts (used to play its sound). Set once at startup.
  void SetActivityCallback(std::function<void(Activity)> cb) { on_activity_ = std::move(cb); }

 protected:
  bool Lock(int timeout_ms = 0) override;
  void Unlock() override;

 private:
  static void TaskEntry(void* arg);
  void Loop();
  void Render(uint32_t now);
  void RenderIdle(uint32_t now, FaceCtx& ctx);
  void PickNextActivity(uint32_t now);
  void Flush();

  esp_lcd_panel_io_handle_t io_;
  esp_lcd_panel_handle_t panel_;
  TaskHandle_t task_ = nullptr;
  std::recursive_timed_mutex lock_;  // Display::Lock for DisplayLockGuard users
  std::mutex state_mutex_;           // protects the fields below

  Canvas canvas_;
  uint8_t last_sent_[sizeof(Canvas::buf)];
  bool have_sent_ = false;

  // what the XiaoZhi app told us
  std::string status_;
  std::string notification_;
  uint32_t notification_until_ = 0;
  std::string emotion_ = "neutral";
  Face emotion_face_ = Face::Neutral;
  uint32_t emotion_since_ = 0;
  std::string assistant_text_;
  uint32_t assistant_since_ = 0;
  std::string user_text_;
  std::string system_text_;
  std::string activation_code_;
  uint32_t error_until_ = 0;
  bool power_save_ = false;

  // Kibo's own state
  Face reaction_face_ = Face::Neutral;
  uint32_t reaction_since_ = 0;
  uint32_t reaction_until_ = 0;
  bool reaction_forced_ = false;
  Activity activity_ = Activity::None;
  Activity pending_activity_ = Activity::None;
  uint32_t pending_activity_ms_ = 0;
  uint32_t activity_since_ = 0;
  uint32_t activity_until_ = 0;
  uint32_t next_activity_at_ = 0;
  uint32_t last_interaction_ = 0;
  uint32_t hold_ms_ = 0;
  int battery_percent_ = 100;
  bool battery_present_ = false;

  // gaze
  int imu_gaze_x_ = 0, imu_gaze_y_ = 0;
  int wander_x_ = 0, wander_y_ = 0;
  uint32_t next_wander_at_ = 0;
  float gaze_x_ = 0, gaze_y_ = 0;

  // state tracking
  int last_state_ = -1;
  uint32_t state_since_ = 0;
  Face last_face_ = Face::Count;
  uint32_t face_since_ = 0;
  std::atomic<Face> shown_face_{Face::Neutral};
  std::atomic<bool> stopped_{false};
  uint32_t boot_at_ = 0;
  bool phone_joined_ = false;
  uint32_t next_ap_check_ = 0;
  uint32_t rng_ = 0x4b49424f;  // "KIBO"
  std::function<void(Activity)> on_activity_;
};

}  // namespace kibo
