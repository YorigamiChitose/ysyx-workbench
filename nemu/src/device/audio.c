/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>

enum {
  reg_freq,
  reg_channels,
  reg_samples,
  reg_sbuf_size,
  reg_init,
  reg_count,
  nr_reg
};

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;
static bool audio_init_finish = false;
static uint32_t audio_sbuf_flag = 0;

static void audio_callback(void *userdata, uint8_t *stream, int len) {
  uint32_t sbuf_size = audio_base[reg_sbuf_size];
  uint32_t count = audio_base[reg_count];
  uint32_t len_ctrl = (count < (uint32_t)len) ? count : (uint32_t)len;

  if (len_ctrl < (uint32_t)len) {
    memset(stream + len_ctrl, 0, len - len_ctrl);
  }

  uint32_t first = sbuf_size - audio_sbuf_flag;
  if (first > len_ctrl) {
    first = len_ctrl;
  }
  memcpy(stream, sbuf + audio_sbuf_flag, first);
  if (len_ctrl > first) {
    memcpy(stream + first, sbuf, len_ctrl - first);
  }

  audio_sbuf_flag += len_ctrl;
  if (audio_sbuf_flag >= sbuf_size) {
    audio_sbuf_flag -= sbuf_size;
  }
  audio_base[reg_count] -= len_ctrl;
}

static void audio_init(void) {
  SDL_AudioSpec s = {};
  s.freq = audio_base[reg_freq];
  s.channels = audio_base[reg_channels];
  s.samples = audio_base[reg_samples];
  s.callback = audio_callback;
  s.userdata = NULL;

  if (audio_init_finish) {
    SDL_CloseAudio();
  }
  audio_sbuf_flag = 0;
  audio_base[reg_count] = 0;
  audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
  SDL_InitSubSystem(SDL_INIT_AUDIO);
  SDL_OpenAudio(&s, NULL);
  SDL_PauseAudio(0);
  audio_init_finish = true;
}

static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  if (is_write && offset == reg_init * sizeof(uint32_t) && audio_base[reg_init]) {
    audio_init();
  }
}

void init_audio() {
  uint32_t space_size = sizeof(uint32_t) * nr_reg;
  audio_base = (uint32_t *)new_space(space_size);
  memset(audio_base, 0, space_size);
  audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("audio", CONFIG_AUDIO_CTL_PORT, audio_base, space_size, audio_io_handler);
#else
  add_mmio_map("audio", CONFIG_AUDIO_CTL_MMIO, audio_base, space_size, audio_io_handler);
#endif

  sbuf = (uint8_t *)new_space(CONFIG_SB_SIZE);
  add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, NULL);
}
