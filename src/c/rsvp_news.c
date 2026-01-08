#include <pebble.h>

#define WIDTH 144
#define HEIGHT 168

// Spritz constants
#define SPRITZ_PIVOT_X (WIDTH / 2)
#define SPRITZ_WORD_Y (HEIGHT / 2 + 10)
#define SPRITZ_LINE_TOP_Y (SPRITZ_WORD_Y - 22)
#define SPRITZ_LINE_BOTTOM_Y (SPRITZ_WORD_Y + 30)
#define SPRITZ_CIRCLE_RADIUS 5

// Message keys
#define KEY_NEWS_TITLE 172
#define KEY_REQUEST_NEWS 173
#define KEY_NEWS_FEED_URL 175
#define KEY_NEWS_CHANNEL_TITLE 176

// Main window and layer
static Window *s_main_window;
static Layer *s_canvas_layer;

// News data
static char news_title[104] = "";
static char channel_title[52] = "News Feed";

// RSVP (Rapid Serial Visual Presentation)
static char rsvp_word[32] = "";
static uint8_t rsvp_word_index = 0;
static uint16_t rsvp_wpm_ms = 160; // 160ms base (~375 WPM)
static AppTimer *rsvp_timer = NULL;

// Display states
static bool s_splash_active = true;
static bool s_end_screen = false;
static bool s_paused = false;

// News rotation
static uint8_t news_display_count = 0;
static uint8_t news_max_count = 5;
static uint16_t news_interval_ms = 1000;
static AppTimer *news_timer = NULL;
static AppTimer *end_timer = NULL;

// News retry protection
static uint8_t news_retry_count = 0;
static uint8_t news_max_retries = 3;

// Calculate pivot index (Spritz ORP algorithm)
static int get_pivot_index(int word_length) {
  if (word_length <= 0)
    return 0;
  switch (word_length) {
  case 1:
    return 0;
  case 2:
  case 3:
  case 4:
  case 5:
    return 1;
  case 6:
  case 7:
  case 8:
  case 9:
    return 2;
  case 10:
  case 11:
  case 12:
  case 13:
    return 3;
  default:
    return 4;
  }
}

static int get_text_width(GContext *ctx, const char *text, int length,
                          GFont font) {
  if (length <= 0 || !text)
    return 0;
  char temp[32];
  int copy_len = (length < 31) ? length : 31;
  memcpy(temp, text, copy_len);
  temp[copy_len] = '\0';
  GSize size = graphics_text_layout_get_content_size(
      temp, font, GRect(0, 0, 500, 50), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  return size.w;
}

static int get_char_width(GContext *ctx, char c, GFont font) {
  char temp[2] = {c, '\0'};
  GSize size = graphics_text_layout_get_content_size(
      temp, font, GRect(0, 0, 100, 50), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  return size.w;
}

// Calculate Spritz delay based on punctuation
static uint16_t calculate_spritz_delay(const char *word) {
  if (!word || word[0] == '\0')
    return rsvp_wpm_ms;
  uint16_t delay = rsvp_wpm_ms;
  int len = strlen(word);
  char last_char = word[len - 1];

  if (last_char == '.' || last_char == '!' || last_char == '?') {
    delay = rsvp_wpm_ms * 3;
  } else if (last_char == ',' || last_char == ':' || last_char == ';' ||
             last_char == ')') {
    delay = rsvp_wpm_ms * 2;
  } else if (last_char == '(' || last_char == '-') {
    delay = (rsvp_wpm_ms * 3) / 2;
  }

  if (len > 8) {
    delay += (len - 8) * 20;
  }
  return delay;
}

// Draw splash screen - shows guide lines and pivot circle for eye positioning
static void draw_splash_screen(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, WIDTH, HEIGHT), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  // Draw the horizontal guide lines above and below the word position
  int line_half_width = 60; // Half width of the horizontal line
  graphics_draw_line(
      ctx, GPoint(SPRITZ_PIVOT_X - line_half_width, SPRITZ_LINE_TOP_Y),
      GPoint(SPRITZ_PIVOT_X + line_half_width, SPRITZ_LINE_TOP_Y));

  graphics_draw_line(
      ctx, GPoint(SPRITZ_PIVOT_X - line_half_width, SPRITZ_LINE_BOTTOM_Y),
      GPoint(SPRITZ_PIVOT_X + line_half_width, SPRITZ_LINE_BOTTOM_Y));

  // Draw pivot indicator circle on the top line
  graphics_draw_circle(ctx, GPoint(SPRITZ_PIVOT_X, SPRITZ_LINE_TOP_Y),
                       SPRITZ_CIRCLE_RADIUS);
}

// Draw Spritz-style RSVP word
static void draw_rsvp_word(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, WIDTH, HEIGHT), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  int line_half_width = 60;
  graphics_draw_line(
      ctx, GPoint(SPRITZ_PIVOT_X - line_half_width, SPRITZ_LINE_TOP_Y),
      GPoint(SPRITZ_PIVOT_X + line_half_width, SPRITZ_LINE_TOP_Y));
  graphics_draw_line(
      ctx, GPoint(SPRITZ_PIVOT_X - line_half_width, SPRITZ_LINE_BOTTOM_Y),
      GPoint(SPRITZ_PIVOT_X + line_half_width, SPRITZ_LINE_BOTTOM_Y));
  graphics_draw_circle(ctx, GPoint(SPRITZ_PIVOT_X, SPRITZ_LINE_TOP_Y),
                       SPRITZ_CIRCLE_RADIUS);

  if (rsvp_word[0] == '\0')
    return;

  int word_length = strlen(rsvp_word);
  if (word_length == 0)
    return;

  int pivot_idx = get_pivot_index(word_length);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28);

  int pre_pivot_width = get_text_width(ctx, rsvp_word, pivot_idx, font);
  int pivot_char_width = get_char_width(ctx, rsvp_word[pivot_idx], font);
  int word_x = SPRITZ_PIVOT_X - pre_pivot_width - (pivot_char_width / 2) + 2;
  int text_y = SPRITZ_WORD_Y - 16;

  char pre_pivot[32] = "";
  char pivot_char[2] = "";
  char post_pivot[32] = "";

  if (pivot_idx > 0) {
    int copy_len = (pivot_idx < 31) ? pivot_idx : 31;
    memcpy(pre_pivot, rsvp_word, copy_len);
    pre_pivot[copy_len] = '\0';
  }

  pivot_char[0] = rsvp_word[pivot_idx];
  pivot_char[1] = '\0';

  if (pivot_idx + 1 < word_length) {
    int remaining = word_length - pivot_idx - 1;
    int copy_len = (remaining < 31) ? remaining : 31;
    memcpy(post_pivot, &rsvp_word[pivot_idx + 1], copy_len);
    post_pivot[copy_len] = '\0';
  }

  int current_x = word_x;
  graphics_context_set_text_color(ctx, GColorWhite);

  if (pre_pivot[0] != '\0') {
    graphics_draw_text(ctx, pre_pivot, font, GRect(current_x, text_y, 200, 40),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                       NULL);
    current_x += pre_pivot_width;
  }

  // Bold pivot letter (multi-pass rendering)
  graphics_draw_text(ctx, pivot_char, font, GRect(current_x, text_y, 50, 40),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);
  graphics_draw_text(
      ctx, pivot_char, font, GRect(current_x + 1, text_y, 50, 40),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(
      ctx, pivot_char, font, GRect(current_x, text_y + 1, 50, 40),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(
      ctx, pivot_char, font, GRect(current_x + 1, text_y + 1, 50, 40),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  current_x += pivot_char_width;

  if (post_pivot[0] != '\0') {
    graphics_draw_text(ctx, post_pivot, font, GRect(current_x, text_y, 200, 40),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                       NULL);
  }
}

// Draw END screen
static void draw_end_screen(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, WIDTH, HEIGHT), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  graphics_draw_text(ctx, "END", font, GRect(0, HEIGHT / 2 - 10, WIDTH, 40),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

static void update_proc(Layer *layer, GContext *ctx) {
  if (s_splash_active) {
    draw_splash_screen(ctx);
  } else if (s_end_screen) {
    draw_end_screen(ctx);
  } else {
    draw_rsvp_word(ctx);
  }
}

static void request_news_from_js(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Requesting news from JS");
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, KEY_REQUEST_NEWS, 1);
    app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_INFO, "News request sent");
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to begin outbox: %d", (int)result);
  }
}

static bool extract_next_word(void) {
  const char *p = news_title;
  uint8_t word_count = 0;
  uint8_t word_start = 0;
  uint8_t word_len = 0;
  uint8_t i = 0;
  bool in_word = false;

  while (p[i] != '\0') {
    if (p[i] == ' ' || p[i] == '\t' || p[i] == '\n') {
      if (in_word) {
        if (word_count == rsvp_word_index) {
          if (word_len > sizeof(rsvp_word) - 1) {
            word_len = sizeof(rsvp_word) - 1;
          }
          memcpy(rsvp_word, &p[word_start], word_len);
          rsvp_word[word_len] = '\0';
          return true;
        }
        word_count++;
        in_word = false;
      }
    } else {
      if (!in_word) {
        word_start = i;
        word_len = 0;
        in_word = true;
      }
      word_len++;
    }
    i++;
  }

  if (in_word && word_count == rsvp_word_index) {
    if (word_len > sizeof(rsvp_word) - 1) {
      word_len = sizeof(rsvp_word) - 1;
    }
    memcpy(rsvp_word, &p[word_start], word_len);
    rsvp_word[word_len] = '\0';
    return true;
  }

  return false;
}

static void news_timer_callback(void *context);
static void rsvp_timer_callback(void *context);

static void end_timer_callback(void *context) {
  end_timer = NULL;
  window_stack_pop(true);
}

static void start_rsvp_for_title(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Starting RSVP for title");
  rsvp_word_index = 0;
  if (extract_next_word()) {
    APP_LOG(APP_LOG_LEVEL_INFO, "First word: %s", rsvp_word);
    layer_mark_dirty(s_canvas_layer);
    if (rsvp_timer) {
      app_timer_cancel(rsvp_timer);
    }
    uint16_t delay = calculate_spritz_delay(rsvp_word);
    rsvp_timer = app_timer_register(delay, rsvp_timer_callback, NULL);
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to extract first word");
  }
}

static void rsvp_timer_callback(void *context) {
  rsvp_timer = NULL;
  if (s_paused || s_end_screen) {
    return;
  }

  rsvp_word_index++;
  if (extract_next_word()) {
    layer_mark_dirty(s_canvas_layer);
    uint16_t delay = calculate_spritz_delay(rsvp_word);
    rsvp_timer = app_timer_register(delay, rsvp_timer_callback, NULL);
  } else {
    rsvp_word[0] = '\0';
    layer_mark_dirty(s_canvas_layer);
    news_display_count++;

    // Cycle infini - demande toujours la prochaine news
    news_timer = app_timer_register(news_interval_ms, news_timer_callback, NULL);
  }
}

static void news_timer_callback(void *context) {
  news_timer = NULL;
  if (s_paused) {
    return;
  }

  if (s_splash_active) {
    s_splash_active = false;
    layer_mark_dirty(s_canvas_layer);
  }

  // En cas d'échec répété, on continue quand même
  if (news_retry_count >= news_max_retries) {
    news_retry_count = 0;
  }

  news_retry_count++;
  request_news_from_js();
  news_timer = app_timer_register(8000, news_timer_callback, NULL);
}

static void inbox_received_callback(DictionaryIterator *iterator,
                                    void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Received message from JS");

  Tuple *channel_title_tuple = dict_find(iterator, KEY_NEWS_CHANNEL_TITLE);
  if (channel_title_tuple) {
    snprintf(channel_title, sizeof(channel_title), "%s",
             channel_title_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_INFO, "Received channel title: %s", channel_title);
    if (s_splash_active) {
      layer_mark_dirty(s_canvas_layer);
    }
  }

  // Vibration de confirmation quand un custom feed est configuré
  Tuple *feed_url_tuple = dict_find(iterator, KEY_NEWS_FEED_URL);
  if (feed_url_tuple) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Custom feed configured - vibrating");
    vibes_short_pulse();
  }

  Tuple *news_title_tuple = dict_find(iterator, KEY_NEWS_TITLE);
  if (news_title_tuple) {
    snprintf(news_title, sizeof(news_title), "%s",
             news_title_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_INFO, "Received title: %s", news_title);
    news_retry_count = 0;

    if (news_timer) {
      app_timer_cancel(news_timer);
      news_timer = NULL;
    }

    // Désactiver le splash screen si encore actif
    if (s_splash_active) {
      s_splash_active = false;
    }

    start_rsvp_for_title();
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "No title found in message");
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator,
                                   AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! Reason: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_paused && s_end_screen) {
    window_stack_pop(true);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_paused && s_end_screen) {
    window_stack_pop(true);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_paused && s_end_screen) {
    window_stack_pop(true);
  }
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop(true);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  window_set_click_config_provider(window, click_config_provider);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  s_main_window = window_create();

  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});

  window_stack_push(s_main_window, true);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  const uint32_t inbox_size = 512;
  const uint32_t outbox_size = 128;
  app_message_open(inbox_size, outbox_size);
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage opened with inbox=%lu, outbox=%lu",
          inbox_size, outbox_size);

  news_timer = app_timer_register(1500, news_timer_callback, NULL);
}

static void deinit(void) {
  if (news_timer) {
    app_timer_cancel(news_timer);
    news_timer = NULL;
  }
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
    rsvp_timer = NULL;
  }
  if (end_timer) {
    app_timer_cancel(end_timer);
    end_timer = NULL;
  }

  app_message_deregister_callbacks();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
