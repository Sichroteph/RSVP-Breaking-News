#include <pebble.h>

#define WIDTH 144
#define HEIGHT 168

// Message keys
#define KEY_NEWS_TITLE 172
#define KEY_REQUEST_NEWS 173
#define KEY_READING_SPEED_WPM 177

// Main window and layer
static Window *s_main_window;
static Layer *s_canvas_layer;

// News data
static char news_title[104] = "";

// RSVP (Rapid Serial Visual Presentation)
static char rsvp_word[32] = "";
static uint8_t rsvp_word_index = 0;
static uint16_t rsvp_wpm_ms = 220; // 220ms per word (~273 WPM)
static AppTimer *rsvp_timer = NULL;
static AppTimer *rsvp_start_timer = NULL;

// Display states
static bool s_splash_active = true;
static bool s_end_screen = false;
static bool s_paused = false;
static bool s_show_focal_lines_only = false;

// News rotation
static uint8_t news_display_count = 0;
static uint8_t news_max_count = 5;
static uint16_t news_interval_ms = 1000;
static AppTimer *news_timer = NULL;
static AppTimer *end_timer = NULL;

// News retry protection
static uint8_t news_retry_count = 0;
static uint8_t news_max_retries = 3;

// Draw splash screen with Reuters branding
static void draw_splash_screen(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, WIDTH, HEIGHT), 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  // "REUTERS" - large centered title
  graphics_draw_text(ctx, "REUTERS",
                     fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(0, 45, WIDTH, 35), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Horizontal line separator
  int line_y = 85;
  graphics_draw_line(ctx, GPoint(20, line_y), GPoint(WIDTH - 20, line_y));
  graphics_draw_line(ctx, GPoint(20, line_y + 1),
                     GPoint(WIDTH - 20, line_y + 1));

  // "Breaking International News" - subtitle
  GFont font_sub = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  graphics_draw_text(ctx, "Breaking", font_sub, GRect(0, 95, WIDTH, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
  graphics_draw_text(
      ctx, "International News", font_sub, GRect(0, 115, WIDTH, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Small decorative dots
  for (int i = 0; i < 3; i++) {
    graphics_fill_circle(ctx, GPoint(WIDTH / 2 - 10 + i * 10, 148), 2);
  }
}

// Draw RSVP word display
static void draw_rsvp_word(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, WIDTH, HEIGHT), 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  // Focal point indicator (small lines to guide the eye)
  int center_y = HEIGHT / 2 - 5;
  graphics_draw_line(ctx, GPoint(5, center_y + 6), GPoint(15, center_y + 6));
  graphics_draw_line(ctx, GPoint(WIDTH - 15, center_y + 6),
                     GPoint(WIDTH - 5, center_y + 6));

  // Only display word if not in "focal lines only" mode
  if (!s_show_focal_lines_only) {
    // Word display - large and centered
    const char *display_text = (rsvp_word[0] != '\0') ? rsvp_word : "";

    GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    int y_offset = HEIGHT / 2 - 20;

    // Wide rect to prevent wrapping
    int wide_width = 500;
    int x_offset = (WIDTH - wide_width) / 2;

    graphics_draw_text(
        ctx, display_text, font, GRect(x_offset, y_offset, wide_width, 40),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// Draw END screen
static void draw_end_screen(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, WIDTH, HEIGHT), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);

  GFont font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  graphics_draw_text(ctx, "END", font, GRect(0, HEIGHT / 2 - 25, WIDTH, 50),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// Main update proc
static void update_proc(Layer *layer, GContext *ctx) {
  if (s_splash_active) {
    draw_splash_screen(ctx);
  } else if (s_end_screen) {
    draw_end_screen(ctx);
  } else {
    draw_rsvp_word(ctx);
  }
}

// Request news from JS
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

// Extract next word from news_title
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

  // Handle last word
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

// Forward declarations
static void news_timer_callback(void *context);
static void rsvp_timer_callback(void *context);
static void rsvp_start_timer_callback(void *context);

// RSVP start timer callback - called after 2 seconds to start showing words
static void rsvp_start_timer_callback(void *context) {
  rsvp_start_timer = NULL;

  // Disable focal lines only mode and show the first word
  s_show_focal_lines_only = false;
  layer_mark_dirty(s_canvas_layer);

  // Start the RSVP word timer
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
  }
  rsvp_timer = app_timer_register(rsvp_wpm_ms, rsvp_timer_callback, NULL);
}

// End timer callback - closes the app after 2 seconds
static void end_timer_callback(void *context) {
  end_timer = NULL;
  window_stack_pop(true);
}

// Start RSVP display for current news_title
static void start_rsvp_for_title(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Starting RSVP for title");
  rsvp_word_index = 0;
  if (extract_next_word()) {
    APP_LOG(APP_LOG_LEVEL_INFO, "First word: %s", rsvp_word);

    // Show only focal lines for 2 seconds before displaying words
    s_show_focal_lines_only = true;
    layer_mark_dirty(s_canvas_layer);

    // Cancel any existing timers
    if (rsvp_timer) {
      app_timer_cancel(rsvp_timer);
      rsvp_timer = NULL;
    }
    if (rsvp_start_timer) {
      app_timer_cancel(rsvp_start_timer);
    }

    // Start a 2-second timer before showing the first word
    rsvp_start_timer =
        app_timer_register(2000, rsvp_start_timer_callback, NULL);
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to extract first word");
  }
}

// RSVP timer callback
static void rsvp_timer_callback(void *context) {
  rsvp_timer = NULL;

  if (s_paused || s_end_screen) {
    return;
  }

  rsvp_word_index++;
  if (extract_next_word()) {
    layer_mark_dirty(s_canvas_layer);
    rsvp_timer = app_timer_register(rsvp_wpm_ms, rsvp_timer_callback, NULL);
  } else {
    // End of title
    rsvp_word[0] = '\0';
    layer_mark_dirty(s_canvas_layer);
    news_display_count++;

    if (news_display_count >= news_max_count) {
      // Done - show END screen and pause
      news_timer = app_timer_register(500, news_timer_callback, NULL);
    } else {
      // Request next news after pause
      news_timer =
          app_timer_register(news_interval_ms, news_timer_callback, NULL);
    }
  }
}

// News timer callback
static void news_timer_callback(void *context) {
  news_timer = NULL;

  if (s_paused) {
    return;
  }

  // If splash was active, now request first news
  if (s_splash_active) {
    s_splash_active = false;
    layer_mark_dirty(s_canvas_layer);
  }

  // Check if we reached the limit
  if (news_display_count >= news_max_count) {
    // Show END screen and pause
    s_end_screen = true;
    s_paused = true;
    layer_mark_dirty(s_canvas_layer);
    // Start 2 second timer to close the app
    end_timer = app_timer_register(1000, end_timer_callback, NULL);
    return;
  }

  // Check retry limit
  if (news_retry_count >= news_max_retries) {
    s_end_screen = true;
    s_paused = true;
    layer_mark_dirty(s_canvas_layer);
    news_retry_count = 0;
    // Start 2 second timer to close the app
    end_timer = app_timer_register(1000, end_timer_callback, NULL);
    return;
  }

  // Request next news
  news_retry_count++;
  request_news_from_js();
  // Safety timeout
  news_timer = app_timer_register(8000, news_timer_callback, NULL);
}

// Message received callback
static void inbox_received_callback(DictionaryIterator *iterator,
                                    void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Received message from JS");
  
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

    start_rsvp_for_title();
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "No title found in message");
  }

  // Gérer la vitesse de lecture
  Tuple *speed_tuple = dict_find(iterator, KEY_READING_SPEED_WPM);
  if (speed_tuple) {
    uint16_t wpm = speed_tuple->value->uint16;
    // Convertir WPM en millisecondes: ms = 60000 / WPM
    rsvp_wpm_ms = 60000 / wpm;
    APP_LOG(APP_LOG_LEVEL_INFO, "Reading speed set to %d WPM (%d ms)", wpm, rsvp_wpm_ms);
    
    // Sauvegarder dans persist storage
    persist_write_int(KEY_READING_SPEED_WPM, wpm);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator,
                                   AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! Reason: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  // Message sent successfully
}

// Button handlers - any button exits the app when paused
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
  // Always allow back to exit
  window_stack_pop(true);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

// Window load/unload
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

// Init
static void init(void) {
  s_main_window = window_create();

  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});

  window_stack_push(s_main_window, true);

  // Charger la vitesse de lecture sauvegardée
  if (persist_exists(KEY_READING_SPEED_WPM)) {
    uint16_t wpm = persist_read_int(KEY_READING_SPEED_WPM);
    rsvp_wpm_ms = 60000 / wpm;
    APP_LOG(APP_LOG_LEVEL_INFO, "Loaded reading speed: %d WPM (%d ms)", wpm, rsvp_wpm_ms);
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Using default reading speed: 273 WPM");
  }

  // Register AppMessage handlers
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage with larger buffers
  const uint32_t inbox_size = 512;
  const uint32_t outbox_size = 128;
  app_message_open(inbox_size, outbox_size);
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage opened with inbox=%lu, outbox=%lu",
          inbox_size, outbox_size);

  // Show splash for 1.5 seconds then request first news
  news_timer = app_timer_register(1500, news_timer_callback, NULL);
}

// Deinit
static void deinit(void) {
  if (news_timer) {
    app_timer_cancel(news_timer);
    news_timer = NULL;
  }
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
    rsvp_timer = NULL;
  }
  if (rsvp_start_timer) {
    app_timer_cancel(rsvp_start_timer);
    rsvp_start_timer = NULL;
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
