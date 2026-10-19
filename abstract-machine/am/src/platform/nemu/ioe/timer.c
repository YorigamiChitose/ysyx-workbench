#include <am.h>
#include <nemu.h>

static uint64_t boot_time = 0;

static uint64_t read_rtc_us() {
  uint32_t high, low, high2;
  do {
    high  = inl(RTC_ADDR + 4);
    low   = inl(RTC_ADDR);
    high2 = inl(RTC_ADDR + 4);
  } while (high != high2);
  return ((uint64_t)high << 32) | low;
}

void __am_timer_init() {
  boot_time = read_rtc_us();
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uptime->us = read_rtc_us() - boot_time;
}

// Convert days since 1970-01-01 to a civil date.
static void days_to_civil(uint32_t days, AM_TIMER_RTC_T *rtc) {
  int64_t z = (int64_t)days + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t year = (int64_t)yoe + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  uint32_t day = doy - (153 * mp + 2) / 5 + 1;
  uint32_t month = mp + (mp < 10 ? 3 : -9);

  rtc->year  = (int)(year + (month <= 2));
  rtc->month = (int)month;
  rtc->day   = (int)day;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  uint64_t total_second = read_rtc_us() / 1000000;
  uint32_t second_in_day = (uint32_t)(total_second % 86400);

  days_to_civil((uint32_t)(total_second / 86400), rtc);
  rtc->hour   = second_in_day / 3600;
  second_in_day %= 3600;
  rtc->minute = second_in_day / 60;
  rtc->second = second_in_day % 60;
}
