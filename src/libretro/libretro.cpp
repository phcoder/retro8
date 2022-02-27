#include "libretro.h"

#include "common.h"
#include "vm/gfx.h"

#include "io/loader.h"
#include "io/stegano.h"
#include "vm/machine.h"
#include "vm/input.h"

#include <stdio.h>
#include <cstdarg>
#include <cstring>

#define LIBRETRO_LOG(x, ...) env.logger(retro_log_level::RETRO_LOG_INFO, x # __VA_ARGS__)

namespace r8 = retro8;

constexpr int SAMPLE_RATE = 44100;
constexpr int SAMPLES_PER_FRAME = SAMPLE_RATE / 60;
constexpr int SOUND_CHANNELS = 2;

r8::Machine machine;
r8::io::Loader loader;

r8::input::InputManager input;
template <typename pixel_t>
class Screen {
public:
  ~Screen() {
    delete[] buffer;
  }

  void draw(const r8::gfx::color_byte_t *data, r8::gfx::palette_t *palette) {
    auto pointer = buffer;

    for (size_t i = 0; i < r8::gfx::BYTES_PER_SCREEN; ++i) {
      const r8::gfx::color_byte_t* pixels = data + i;
      const auto rc1 = colorTable.get(palette->get((pixels)->low()));
      const auto rc2 = colorTable.get(palette->get((pixels)->high()));

      *(pointer) = rc1;
      *((pointer)+1) = rc2;
      (pointer) += 2;
    }
  }

  const pixel_t *getBuffer() {
    return buffer;
  }
protected:
  Screen(): buffer(new pixel_t[r8::gfx::SCREEN_WIDTH * r8::gfx::SCREEN_HEIGHT]) {
  }

protected:
  r8::gfx::ColorTable colorTable;
private:
  pixel_t *buffer;
};

struct Screen32 : Screen<uint32_t> {
public:
  Screen32() {
    colorTable.init(ColorMapper32());
  }
private:
  struct ColorMapper32
  {
    r8::gfx::ColorTable::pixel_t operator()(uint8_t r, uint8_t g, uint8_t b) const { return 0xff000000 | (r << 16) | (g << 8) | b; }
  };
};

struct Screen16 : Screen<uint16_t> {
public:
  Screen16() {
    colorTable.init(ColorMapper16());
  }
private:
  struct ColorMapper16
  {
    r8::gfx::ColorTable::pixel_t operator()(uint8_t r, uint8_t g, uint8_t b) const { return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xf8) >> 3); }
  };
};

Screen16 *screen16;
Screen32 *screen32;
int16_t* audioBuffer;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
  (void)level;
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
}

struct RetroArchEnv
{
  retro_video_refresh_t video;
  retro_audio_sample_t audio;
  retro_audio_sample_batch_t audioBatch;
  retro_input_poll_t inputPoll;
  retro_input_state_t inputState;
  retro_log_printf_t logger = fallback_log;
  retro_environment_t retro_cb;

  uint32_t frameCounter;
  uint16_t buttonState;
  bool isRGB32;
};

RetroArchEnv env;

//TODO
uint32_t Platform::getTicks() { return 0; }

static bool tryScreen32() {
	retro_pixel_format pixelFormat = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!env.retro_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixelFormat))
	  return false;
	env.isRGB32 = true;
	screen32 = new Screen32();
	env.logger(retro_log_level::RETRO_LOG_INFO, "Initializing XRGB8888 screen buffer of %d bytes\n", 4*r8::gfx::SCREEN_WIDTH*r8::gfx::SCREEN_HEIGHT);
	return true;
      }
      bool tryScreen16() {
	retro_pixel_format pixelFormat = RETRO_PIXEL_FORMAT_RGB565;
	if (!env.retro_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixelFormat))
	  return false;
	env.isRGB32 = false;
	screen16 = new Screen16();
	env.logger(retro_log_level::RETRO_LOG_INFO, "Initializing RGB565 screen buffer of %d bytes\n", 2*r8::gfx::SCREEN_WIDTH*r8::gfx::SCREEN_HEIGHT);
	return true;
      }

extern "C"
{
  unsigned retro_api_version()
  {
    return RETRO_API_VERSION;
  }

  void retro_init()
  {
    audioBuffer = new int16_t[SAMPLE_RATE * 2];
    env.logger(retro_log_level::RETRO_LOG_INFO, "Initializing audio buffer of %d bytes\n", sizeof(int16_t) * SAMPLE_RATE * 2);

    machine.font().load();
    machine.code().loadAPI();
    input.setMachine(&machine);
  }

  void retro_deinit()
  {
    delete[] audioBuffer;
    //TODO: release all structures bound to Lua etc
  }

  void retro_get_system_info(retro_system_info* info)
  {
    std::memset(info, 0, sizeof(*info));

    info->library_name = "retro-8 (alpha)";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
    info->library_version = "0.1b" GIT_VERSION;
    info->need_fullpath = false;
    info->valid_extensions = "p8|png";
  }

  void retro_get_system_av_info(retro_system_av_info* info)
  {
    info->timing.fps = 60.0f;
    info->timing.sample_rate = SAMPLE_RATE;
    info->geometry.base_width = retro8::gfx::SCREEN_WIDTH;
    info->geometry.base_height = retro8::gfx::SCREEN_HEIGHT;
    info->geometry.max_width = retro8::gfx::SCREEN_WIDTH;
    info->geometry.max_height = retro8::gfx::SCREEN_HEIGHT;
    info->geometry.aspect_ratio = retro8::gfx::SCREEN_WIDTH / float(retro8::gfx::SCREEN_HEIGHT);
  }

  void retro_set_environment(retro_environment_t e)
  {
    env.retro_cb = e;

    retro_log_callback logger;
    if (e(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger))
      env.logger = logger.log;
  }

  void retro_set_video_refresh(retro_video_refresh_t callback) { env.video = callback; }
  void retro_set_audio_sample(retro_audio_sample_t callback) { env.audio = callback; }
  void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback) { env.audioBatch = callback; }
  void retro_set_input_poll(retro_input_poll_t callback) { env.inputPoll = callback; }
  void retro_set_input_state(retro_input_state_t callback) { env.inputState = callback; }
  void retro_set_controller_port_device(unsigned port, unsigned device) { /* TODO */ }
  

  size_t retro_serialize_size(void) { return 0; }
  bool retro_serialize(void *data, size_t size) { return true; }
  bool retro_unserialize(const void *data, size_t size) { return true; }
  void retro_cheat_reset(void) { }
  void retro_cheat_set(unsigned index, bool enabled, const char *code) { }
  unsigned retro_get_region(void) { return 0; }
  void *retro_get_memory_data(unsigned id) { return nullptr; }
  size_t retro_get_memory_size(unsigned id) { return 0; }

  bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) { return false; }
  bool retro_load_game(const retro_game_info* info)
  {
    if (info && info->data)
    {
      input.reset();

      const char* bdata = static_cast<const char*>(info->data);

      env.logger(RETRO_LOG_INFO, "[Retro8] Loading %s\n", info->path);


      if (std::memcmp(bdata, "\x89PNG", 4) == 0)
      {
        env.logger(RETRO_LOG_INFO, "[Retro8] Game is in PNG format, decoding it.\n");

        std::vector<uint8_t> out;
        unsigned long width, height;
        auto result = Platform::loadPNG(out, width, height, (uint8_t*)bdata, info->size, true);
        assert(result == 0);
        r8::io::Stegano stegano;
        stegano.load({ reinterpret_cast<const uint32_t*>(out.data()), nullptr, out.size() / 4 }, machine);
      }
      else
      {
        //TODO: not efficient since it's copied and it's not checking for '\0'
        std::string raw(bdata);
        loader.loadRaw(raw, machine);
      }

      machine.memory().backupCartridge();

      if (machine.code().hasInit())
      {
        //_initFuture = std::async(std::launch::async, []() {
        LIBRETRO_LOG("[Retro8] Cartridge has _init() function, calling it.");
          machine.code().init();
          LIBRETRO_LOG("[Retro8] _init() function completed execution.");
        //});
      }

      machine.sound().init();

      env.frameCounter = 0;

      screen16 = NULL;
      screen32 = NULL;
      if (!tryScreen32() && !tryScreen16()) {
	env.logger(retro_log_level::RETRO_LOG_ERROR, "Couldn't find compatible pixel format\n");
	return false;
      }

      return true;
    }

    return false;
  }

  void retro_unload_game(void)
  {
    /* TODO */
    if (screen16)
      delete screen16;
    if (screen32)
      delete screen32;
    screen16 = NULL;
    screen32 = NULL;
  }

  void retro_run()
  {
    /* if code is at 60fps or every 2 frames (30fps) */
    if (machine.code().require60fps() || env.frameCounter % 2 == 0)
    {
      /* call _update and _draw of PICO-8 code */
      machine.code().update();
      machine.code().draw();

      /* rasterize screen memory to ARGB framebuffer */
      auto* data = machine.memory().screenData();
      auto* screenPalette = machine.memory().paletteAt(retro8::gfx::SCREEN_PALETTE_INDEX);

      if (env.isRGB32)
	screen32->draw(data, screenPalette);
      else
	screen16->draw(data, screenPalette);

      input.manageKeyRepeat();
    }

    if (env.isRGB32)
      env.video(screen32->getBuffer(), r8::gfx::SCREEN_WIDTH, r8::gfx::SCREEN_HEIGHT, r8::gfx::SCREEN_WIDTH * sizeof(uint32_t));
    else
      env.video(screen16->getBuffer(), r8::gfx::SCREEN_WIDTH, r8::gfx::SCREEN_HEIGHT, r8::gfx::SCREEN_WIDTH * sizeof(uint16_t));
    ++env.frameCounter;

    machine.sound().renderSounds(audioBuffer, SAMPLES_PER_FRAME);
    
    /* duplicate channels */
    auto* audioBuffer2 = audioBuffer + SAMPLE_RATE;
    for (size_t i = 0; i < SAMPLES_PER_FRAME; ++i)
    {
      audioBuffer2[2*i] = audioBuffer[i];
      audioBuffer2[2*i + 1] = audioBuffer[i];
    }
    
    env.audioBatch(audioBuffer2, SAMPLES_PER_FRAME);

    /* manage input */
    {
      struct BtPair {
        unsigned player;
        int16_t rabt;
        size_t r8bt;
        bool isSet;
      };

      static std::array<BtPair, retro8::BUTTON_COUNT + 2> mapping = { {
        { 0, RETRO_DEVICE_ID_JOYPAD_LEFT, 0, false },
        { 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, 1, false },
        { 0, RETRO_DEVICE_ID_JOYPAD_UP, 2, false },
        { 0, RETRO_DEVICE_ID_JOYPAD_DOWN, 3, false },
        { 0, RETRO_DEVICE_ID_JOYPAD_A, 4, false },
        { 0, RETRO_DEVICE_ID_JOYPAD_B, 5, false },
        { 1, RETRO_DEVICE_ID_JOYPAD_X, 4, false },
        { 1, RETRO_DEVICE_ID_JOYPAD_Y, 5, false },

      } };


      //TODO: add mapping for action1/2 of player 2 because it's used by some games

      env.inputPoll();
      for (auto& entry : mapping)
      {
        const bool isSet = env.inputState(entry.player, RETRO_DEVICE_JOYPAD, 0, entry.rabt);
        const bool wasSet = entry.isSet;

        if (wasSet != isSet)
          input.manageKey(entry.player, entry.r8bt, isSet);

        entry.isSet = isSet;
      }

      input.tick();
    }

  }

  void retro_reset()
  {

  }
}

