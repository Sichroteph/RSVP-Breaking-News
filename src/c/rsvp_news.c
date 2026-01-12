#include <pebble.h>

// Spritz constants for optimal word display (relative to screen dimensions)
#define SPRITZ_HEADER_Y 5 // Y position for HEADLINE/ARTICLE header
#define SPRITZ_WORD_Y 55  // Y position of word center (moved up for header)
#define SPRITZ_LINE_TOP_Y (SPRITZ_WORD_Y - 22)    // Y of line above word
#define SPRITZ_LINE_BOTTOM_Y (SPRITZ_WORD_Y + 30) // Y of line below word
#define SPRITZ_LINE_LENGTH 20  // Length of vertical guide lines
#define SPRITZ_CIRCLE_RADIUS 5 // Radius of pivot indicator circle

// Message keys
#define KEY_NEWS_TITLE 172
#define KEY_REQUEST_NEWS 173
#define KEY_READING_SPEED_WPM 177
#define KEY_CONFIG_OPENED 178
#define KEY_CONFIG_RECEIVED 179
#define KEY_REQUEST_ARTICLE 180
#define KEY_NEWS_ARTICLE 181
#define KEY_BACKLIGHT_ENABLED 182
#define KEY_FEED_NAME 183
#define KEY_REQUEST_FEEDS 184
#define KEY_SELECT_FEED 185
#define KEY_FEEDS_COUNT 186

// Main window and layers
static Window *s_main_window;
static Layer *s_canvas_layer;

// Menu for journal selection
static MenuLayer *s_menu_layer;
static bool s_showing_menu = true;

// Feed/Journal data
static char feed_names[20][32];         // Store up to 20 feed names
static uint8_t feed_count = 0;          // Number of stored feeds
static int8_t selected_feed_index = -1; // Currently selected feed

// News data
static char news_title[104] = "";
static char news_titles[50][104];      // Store up to 50 news titles
static uint8_t news_titles_count = 0;  // Number of stored titles
static int8_t current_news_index = -1; // Current news index (-1 = none)

// Article data (only store one at a time to save memory)
static char news_article[512] = "";    // Current article content
static bool s_reading_article = false; // True when reading article content
static int8_t s_article_news_index =
    -1; // Index of the news whose article we're reading

// RSVP (Rapid Serial Visual Presentation)
static char rsvp_word[32] = "";
static uint8_t rsvp_word_index = 0;
static uint16_t rsvp_wpm_ms = 150; // 150ms per word (400 WPM)
static AppTimer *rsvp_timer = NULL;
static AppTimer *rsvp_start_timer = NULL;
static AppTimer *page_number_timer = NULL;
static bool s_backlight_enabled = true; // Keep backlight on during reading

// Display states
static bool s_splash_active = false;
static bool s_end_screen = false;
static bool s_paused = false;
static bool s_waiting_for_config = false;
static bool s_showing_page_number =
    false; // True when showing page number as word
static bool s_first_news_after_splash =
    true; // True for first news, requires delay

// News rotation
static uint8_t news_display_count = 0;
static uint8_t news_max_count = 50;
static AppTimer *news_timer = NULL;
static AppTimer *end_timer = NULL;
static bool s_user_navigating = false; // True when user manually navigates

// News retry protection
static uint8_t news_retry_count = 0;
static uint8_t news_max_retries = 3;

// Forward declarations
static void news_timer_callback(void *context);
static void show_journal_menu(void);
static void hide_journal_menu(void);
static void click_config_provider(void *context);

// Calculate the optimal recognition point (ORP) / pivot letter index
// Based on Spritz algorithm from OpenSpritz
static int get_pivot_index(int word_length) {
  if (word_length <= 0)
    return 0;

  switch (word_length) {
  case 1:
    return 0; // first letter (index 0)
  case 2:
  case 3:
  case 4:
  case 5:
    return 1; // second letter (index 1)
  case 6:
  case 7:
  case 8:
  case 9:
    return 2; // third letter (index 2)
  case 10:
  case 11:
  case 12:
  case 13:
    return 3; // fourth letter (index 3)
  default:
    return 4; // fifth letter (index 4)
  }
}

// Get the width of a string segment using the given font
static int get_text_width(GContext *ctx, const char *text, int length,
                          GFont font) {
  if (length <= 0 || !text)
    return 0;

  // Create a temporary buffer for the substring
  char temp[32];
  int copy_len = (length < 31) ? length : 31;
  memcpy(temp, text, copy_len);
  temp[copy_len] = '\0';

  GSize size = graphics_text_layout_get_content_size(
      temp, font, GRect(0, 0, 500, 50), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  return size.w;
}

// Get the width of a single character
static int get_char_width(GContext *ctx, char c, GFont font) {
  char temp[2] = {c, '\0'};
  GSize size = graphics_text_layout_get_content_size(
      temp, font, GRect(0, 0, 100, 50), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  return size.w;
}

// Calculate Spritz-style delay for a word
// Based on OpenSpritz algorithm: longer pause for punctuation and long words
static uint16_t calculate_spritz_delay(const char *word) {
  if (!word || word[0] == '\0') {
    return rsvp_wpm_ms;
  }

  uint16_t delay = rsvp_wpm_ms;
  int len = strlen(word);

  // Check last character for punctuation
  char last_char = word[len - 1];

  // Strong pause for sentence-ending punctuation (. ! ?)
  if (last_char == '.' || last_char == '!' || last_char == '?') {
    delay = rsvp_wpm_ms * 3; // Triple delay for sentence end
  }
  // Medium pause for clause-ending punctuation (, : ; ))
  else if (last_char == ',' || last_char == ':' || last_char == ';' ||
           last_char == ')') {
    delay = rsvp_wpm_ms * 2; // Double delay for clause break
  }
  // Check for opening parenthesis or dash (mid-word pause)
  else {
    for (int i = 0; i < len; i++) {
      if (word[i] == '(' || word[i] == '-') {
        delay = rsvp_wpm_ms + (rsvp_wpm_ms / 2); // 1.5x delay
        break;
      }
    }
  }

  // Extra time for long words (> 8 characters)
  if (len > 8) {
    delay += rsvp_wpm_ms; // Add extra time for long words
  }

  return delay;
}

// Menu layer callbacks
static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer,
                                           uint16_t section_index, void *data) {
  return feed_count > 0 ? feed_count : 1;
}

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer,
                                   MenuIndex *cell_index, void *data) {
  if (feed_count == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "Loading...", NULL, NULL);
  } else {
    menu_cell_basic_draw(ctx, cell_layer, feed_names[cell_index->row], NULL,
                         NULL);

    // Draw a separator line at the bottom of each cell
    GRect bounds = layer_get_bounds(cell_layer);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1),
                       GPoint(bounds.size.w, bounds.size.h - 1));
  }
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index,
                                 void *data) {
  if (feed_count == 0)
    return;

  selected_feed_index = cell_index->row;
  APP_LOG(APP_LOG_LEVEL_INFO, "Selected feed: %d - %s", selected_feed_index,
          feed_names[selected_feed_index]);

  // Send feed selection to JS
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, KEY_SELECT_FEED, selected_feed_index);
    app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_INFO, "Feed selection sent");
  }

  // Hide menu and show loading state
  hide_journal_menu();

  // Reset news data
  news_titles_count = 0;
  current_news_index = -1;
  news_title[0] = '\0';
  rsvp_word[0] = '\0';
  s_first_news_after_splash = true;
  s_user_navigating = false;

  layer_mark_dirty(s_canvas_layer);
}

static void show_journal_menu(void) {
  if (!s_menu_layer)
    return;

  s_showing_menu = true;
  layer_set_hidden(menu_layer_get_layer(s_menu_layer), false);
  layer_set_hidden(s_canvas_layer, true);
  menu_layer_reload_data(s_menu_layer);

  // Set menu click config
  menu_layer_set_click_config_onto_window(s_menu_layer, s_main_window);
}

static void hide_journal_menu(void) {
  if (!s_menu_layer)
    return;

  s_showing_menu = false;
  layer_set_hidden(menu_layer_get_layer(s_menu_layer), true);
  layer_set_hidden(s_canvas_layer, false);

  // Set canvas click config
  window_set_click_config_provider(s_main_window, click_config_provider);
}

// Draw Spritz-style RSVP word display with pivot letter highlighting
static void draw_rsvp_word(GContext *ctx, GRect bounds) {
  int width = bounds.size.w;
  int height = bounds.size.h;
  int pivot_x = width / 2;
  int help_y = height - 60;

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, width, height), 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_text_color(ctx, GColorWhite);

  // Draw header: "HEADLINE" or "ARTICLE" at top, centered, bold
  const char *header_text = s_reading_article ? "ARTICLE" : "HEADLINE";
  GFont font_header = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  graphics_draw_text(
      ctx, header_text, font_header, GRect(0, SPRITZ_HEADER_Y, width, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Draw the horizontal guide lines above and below the word (always visible)
  int line_half_width = 60; // Half width of the horizontal line
  graphics_draw_line(ctx, GPoint(pivot_x - line_half_width, SPRITZ_LINE_TOP_Y),
                     GPoint(pivot_x + line_half_width, SPRITZ_LINE_TOP_Y));

  graphics_draw_line(ctx,
                     GPoint(pivot_x - line_half_width, SPRITZ_LINE_BOTTOM_Y),
                     GPoint(pivot_x + line_half_width, SPRITZ_LINE_BOTTOM_Y));

  // Draw a small circle on the top line at the pivot position (pivot indicator)
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_circle(ctx, GPoint(pivot_x, SPRITZ_LINE_TOP_Y),
                       SPRITZ_CIRCLE_RADIUS);

  // Handle empty or null word
  const char *word = (rsvp_word[0] != '\0') ? rsvp_word : "";
  if (word[0] == '\0') {
    // Even when no word is displayed, show navigation help and counter
    // Draw navigation help at bottom in small font, left-aligned, 3 lines
    GFont font_help = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    graphics_context_set_text_color(ctx, GColorWhite);

    // Build help text based on current mode (3 separate lines)
    const char *help_line1 = "Arrows: navigation";
    const char *help_line2;
    const char *help_line3;

    if (s_reading_article) {
      help_line2 = "Select: stop";
      help_line3 = "Back: title";
    } else {
      help_line2 = "Select: read";
      help_line3 = "Back: menu";
    }

    graphics_draw_text(
        ctx, help_line1, font_help, GRect(5, help_y, width - 10, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(
        ctx, help_line2, font_help, GRect(5, help_y + 15, width - 10, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(
        ctx, help_line3, font_help, GRect(5, help_y + 30, width - 10, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    return;
  }

  // Calculate word length
  int word_length = strlen(word);
  if (word_length == 0)
    return;

  // Get the pivot index based on Spritz algorithm
  int pivot_idx = get_pivot_index(word_length);

  // Safety check: ensure pivot_idx is within bounds
  if (pivot_idx >= word_length) {
    pivot_idx = word_length - 1;
  }
  if (pivot_idx < 0) {
    pivot_idx = 0;
  }

  // Font for word display
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28);

  // Calculate widths for positioning
  // Width of text before pivot letter
  int pre_pivot_width = get_text_width(ctx, word, pivot_idx, font);
  // Width of the pivot letter itself
  int pivot_char_width = get_char_width(ctx, word[pivot_idx], font);

  // Calculate X position so pivot letter is centered at pivot_x
  // The pivot letter's center should be at pivot_x
  // Shift 3 pixels to the left
  int word_x = pivot_x - pre_pivot_width - (pivot_char_width / 2) + 2 - 3;

  // Y position for text
  int text_y = SPRITZ_WORD_Y - 16; // Adjust for font baseline

  // Create buffers for the three parts of the word
  char pre_pivot[32] = "";
  char pivot_char[2] = "";
  char post_pivot[32] = "";

  // Split the word into parts
  if (pivot_idx > 0) {
    int copy_len = (pivot_idx < 31) ? pivot_idx : 31;
    memcpy(pre_pivot, word, copy_len);
    pre_pivot[copy_len] = '\0';
  }

  pivot_char[0] = word[pivot_idx];
  pivot_char[1] = '\0';

  if (pivot_idx + 1 < word_length) {
    int remaining = word_length - pivot_idx - 1;
    int copy_len = (remaining < 31) ? remaining : 31;
    memcpy(post_pivot, &word[pivot_idx + 1], copy_len);
    post_pivot[copy_len] = '\0';
  }

  // Draw the three parts of the word
  int current_x = word_x;

  // Part 1: Text before pivot (white)
  if (pre_pivot[0] != '\0') {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, pre_pivot, font, GRect(current_x, text_y, 200, 40),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                       NULL);
    current_x += pre_pivot_width;
  }

  // Part 2: Pivot letter (with bold effect for emphasis)
  graphics_context_set_text_color(ctx, GColorWhite);

  // Draw pivot letter multiple times with offsets to create strong bold effect
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

  // Part 3: Text after pivot (white)
  if (post_pivot[0] != '\0') {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, post_pivot, font, GRect(current_x, text_y, 200, 40),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                       NULL);
  }

  // Draw navigation help at bottom in small font, left-aligned, 3 lines
  GFont font_help = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, GColorWhite);

  // Build help text based on current mode (3 separate lines)
  const char *help_line1 = "Arrows: navigation";
  const char *help_line2;
  const char *help_line3;

  if (s_reading_article) {
    help_line2 = "Select: stop";
    help_line3 = "Back: title";
  } else {
    help_line2 = "Select: read";
    help_line3 = "Back: menu";
  }

  graphics_draw_text(
      ctx, help_line1, font_help, GRect(5, help_y, width - 10, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(
      ctx, help_line2, font_help, GRect(5, help_y + 15, width - 10, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(
      ctx, help_line3, font_help, GRect(5, help_y + 30, width - 10, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

// Draw END screen
static void draw_end_screen(GContext *ctx, GRect bounds) {
  int width = bounds.size.w;
  int height = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, width, height), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);

  GFont font_end = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  graphics_draw_text(ctx, "END", font_end, GRect(0, height / 2 - 25, width, 50),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// Draw waiting for config screen
static void draw_waiting_screen(GContext *ctx, GRect bounds) {
  int width = bounds.size.w;
  int height = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, width, height), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);

  GFont font_title = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font_sub = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  graphics_draw_text(
      ctx, "Settings", font_title, GRect(0, height / 2 - 35, width, 30),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(
      ctx, "Use your phone", font_sub, GRect(0, height / 2, width, 25),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(
      ctx, "to configure...", font_sub, GRect(0, height / 2 + 20, width, 25),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Draw loading screen (waiting for news)
static void draw_loading_screen(GContext *ctx, GRect bounds) {
  int width = bounds.size.w;
  int height = bounds.size.h;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, width, height), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);

  GFont font_title = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font_sub = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  // Show selected feed name
  if (selected_feed_index >= 0 && selected_feed_index < (int8_t)feed_count) {
    graphics_draw_text(ctx, feed_names[selected_feed_index], font_title,
                       GRect(0, height / 2 - 35, width, 30),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }

  graphics_draw_text(
      ctx, "Loading...", font_sub, GRect(0, height / 2 + 5, width, 25),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Main update proc
static void update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  if (s_waiting_for_config) {
    draw_waiting_screen(ctx, bounds);
  } else if (s_showing_menu) {
    // Menu is shown separately
    return;
  } else if (news_titles_count == 0 && selected_feed_index >= 0) {
    // Waiting for news to load
    draw_loading_screen(ctx, bounds);
  } else if (s_end_screen) {
    draw_end_screen(ctx, bounds);
  } else {
    draw_rsvp_word(ctx, bounds);
  }
}

// Reset app state to restart from beginning
static void reset_app_state(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Resetting app state");

  // Cancel all timers
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
    rsvp_timer = NULL;
  }
  if (rsvp_start_timer) {
    app_timer_cancel(rsvp_start_timer);
    rsvp_start_timer = NULL;
  }
  if (news_timer) {
    app_timer_cancel(news_timer);
    news_timer = NULL;
  }
  if (end_timer) {
    app_timer_cancel(end_timer);
    end_timer = NULL;
  }
  if (page_number_timer) {
    app_timer_cancel(page_number_timer);
    page_number_timer = NULL;
  }

  // Reset state variables
  news_title[0] = '\0';
  rsvp_word[0] = '\0';
  rsvp_word_index = 0;
  news_display_count = 0;
  news_retry_count = 0;
  news_titles_count = 0;
  current_news_index = -1;
  selected_feed_index = -1;
  s_splash_active = false;
  s_end_screen = false;
  s_paused = false;
  s_waiting_for_config = false;
  s_first_news_after_splash = true;
  s_reading_article = false;
  s_article_news_index = -1;
  s_showing_page_number = false;
  s_user_navigating = false;
  news_article[0] = '\0';

  // Show the journal menu
  show_journal_menu();
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

// Request article for current news index from JS
static void request_article_from_js(uint8_t index) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Requesting article %d from JS", index);
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, KEY_REQUEST_ARTICLE, index);
    app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_INFO, "Article request sent for index %d", index);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to begin outbox: %d", (int)result);
  }
}

// Extract next word from the current text (title or article)
static bool extract_next_word(void) {
  // Use article if reading article, otherwise use title
  const char *p = s_reading_article ? news_article : news_title;

  // Safety check: ensure we have valid text
  if (!p || p[0] == '\0') {
    rsvp_word[0] = '\0';
    return false;
  }

  uint16_t word_count = 0;
  uint16_t word_start = 0;
  uint16_t word_len = 0;
  uint16_t i = 0;
  bool in_word = false;

  while (p[i] != '\0' && i < 1000) { // Safety limit to prevent infinite loop
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
static void page_number_timer_callback(void *context);

// Page number timer callback - shows page number as word after 500ms pause
static void page_number_timer_callback(void *context) {
  page_number_timer = NULL;

  // Display page number as a word (only for titles, not articles)
  if (!s_reading_article && news_titles_count > 0 && current_news_index >= 0) {
    snprintf(rsvp_word, sizeof(rsvp_word), "%d/%d", current_news_index + 1,
             news_titles_count);
    s_showing_page_number = true;
    layer_mark_dirty(s_canvas_layer);
  }
}

// RSVP start timer callback - called after 2 seconds to start showing words
static void rsvp_start_timer_callback(void *context) {
  rsvp_start_timer = NULL;

  // Show the first word
  layer_mark_dirty(s_canvas_layer);

  // Start the RSVP word timer with Spritz-style delay
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
  }
  uint16_t delay = calculate_spritz_delay(rsvp_word);
  rsvp_timer = app_timer_register(delay, rsvp_timer_callback, NULL);
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
  s_showing_page_number = false;
  if (extract_next_word()) {
    APP_LOG(APP_LOG_LEVEL_INFO, "First word: %s", rsvp_word);

    // Cancel any existing timers
    if (rsvp_timer) {
      app_timer_cancel(rsvp_timer);
      rsvp_timer = NULL;
    }
    if (rsvp_start_timer) {
      app_timer_cancel(rsvp_start_timer);
      rsvp_start_timer = NULL;
    }

    // Enable backlight for reading if option is enabled
    if (s_backlight_enabled) {
      light_enable_interaction();
    }

    // On first news after splash, start immediately without help screen
    // On subsequent news (after navigation), also start immediately
    layer_mark_dirty(s_canvas_layer);

    if (s_first_news_after_splash) {
      // First news: small delay before showing words
      rsvp_start_timer =
          app_timer_register(500, rsvp_start_timer_callback, NULL);
      s_first_news_after_splash = false; // Clear flag after first use
    } else {
      // Instant display for button navigation
      uint16_t delay = calculate_spritz_delay(rsvp_word);
      rsvp_timer = app_timer_register(delay, rsvp_timer_callback, NULL);
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to extract first word");
  }
}

// Show splash and then go to next title after article reading
static void show_splash_then_next_title(void) {
  // Cancel any existing timers
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
    rsvp_timer = NULL;
  }
  if (rsvp_start_timer) {
    app_timer_cancel(rsvp_start_timer);
    rsvp_start_timer = NULL;
  }
  if (page_number_timer) {
    app_timer_cancel(page_number_timer);
    page_number_timer = NULL;
  }

  // Clear article mode
  s_reading_article = false;
  s_showing_page_number = false;
  news_article[0] = '\0';
  rsvp_word[0] = '\0';

  // Stay on the same title (don't increment)
  current_news_index = s_article_news_index;
  s_article_news_index = -1;
  snprintf(news_title, sizeof(news_title), "%s",
           news_titles[current_news_index]);

  // Don't start reading - just show the page number after a pause
  rsvp_word[0] = '\0';
  layer_mark_dirty(s_canvas_layer);

  // Show page number after 500ms pause
  if (page_number_timer) {
    app_timer_cancel(page_number_timer);
  }
  page_number_timer = app_timer_register(500, page_number_timer_callback, NULL);
}

// Start reading the article content
static void start_article_reading(void) {
  if (news_article[0] == '\0') {
    APP_LOG(APP_LOG_LEVEL_WARNING, "No article content to read");
    return;
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "Starting article reading");
  s_reading_article = true;
  rsvp_word_index = 0;

  // Enable backlight for reading if option is enabled
  if (s_backlight_enabled) {
    light_enable_interaction();
  }

  if (extract_next_word()) {
    layer_mark_dirty(s_canvas_layer);

    // Start the timer
    uint16_t delay = calculate_spritz_delay(rsvp_word);
    rsvp_timer = app_timer_register(delay, rsvp_timer_callback, NULL);
  }
}

// RSVP timer callback
static void rsvp_timer_callback(void *context) {
  rsvp_timer = NULL;

  if (s_paused || s_end_screen) {
    return;
  }

  // Keep backlight on during reading if option is enabled
  if (s_backlight_enabled) {
    light_enable_interaction();
  }

  rsvp_word_index++;
  if (extract_next_word()) {
    layer_mark_dirty(s_canvas_layer);
    // Calculate Spritz-style variable delay based on word characteristics
    uint16_t delay = calculate_spritz_delay(rsvp_word);
    rsvp_timer = app_timer_register(delay, rsvp_timer_callback, NULL);
  } else {
    // End of text
    rsvp_word[0] = '\0';
    layer_mark_dirty(s_canvas_layer);

    if (s_reading_article) {
      // End of article - show splash then go to next title
      show_splash_then_next_title();
    } else {
      // End of title - show page number after 500ms pause
      if (page_number_timer) {
        app_timer_cancel(page_number_timer);
      }
      page_number_timer =
          app_timer_register(500, page_number_timer_callback, NULL);
    }
  }
}

// News timer callback
static void news_timer_callback(void *context) {
  news_timer = NULL;

  if (s_paused) {
    return;
  }

  // Check if we already have all the news we need
  if (news_titles_count >= news_max_count) {
    return;
  }

  // Check retry limit
  if (news_retry_count >= news_max_retries) {
    // If we have at least some news, just stop requesting more
    if (news_titles_count > 0) {
      news_retry_count = 0;
      return;
    }
    // No news at all - show error
    s_end_screen = true;
    s_paused = true;
    layer_mark_dirty(s_canvas_layer);
    news_retry_count = 0;
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

  // Handle feeds count
  Tuple *feeds_count_tuple = dict_find(iterator, KEY_FEEDS_COUNT);
  if (feeds_count_tuple) {
    feed_count = feeds_count_tuple->value->uint8;
    APP_LOG(APP_LOG_LEVEL_INFO, "Received feeds count: %d", feed_count);
    if (feed_count > 20)
      feed_count = 20;
    // Reset feed names array
    for (int i = 0; i < 20; i++) {
      feed_names[i][0] = '\0';
    }
    // Reload menu if visible
    if (s_showing_menu && s_menu_layer) {
      menu_layer_reload_data(s_menu_layer);
    }
    return;
  }

  // Handle feed name
  Tuple *feed_name_tuple = dict_find(iterator, KEY_FEED_NAME);
  if (feed_name_tuple && feed_name_tuple->value &&
      feed_name_tuple->value->cstring) {
    // Find first empty slot
    for (int i = 0; i < feed_count && i < 20; i++) {
      if (feed_names[i][0] == '\0') {
        snprintf(feed_names[i], sizeof(feed_names[0]), "%s",
                 feed_name_tuple->value->cstring);
        APP_LOG(APP_LOG_LEVEL_INFO, "Received feed name %d: %s", i,
                feed_names[i]);
        break;
      }
    }
    // Reload menu if visible
    if (s_showing_menu && s_menu_layer) {
      menu_layer_reload_data(s_menu_layer);
    }
    return;
  }

  // Handle article content
  Tuple *article_tuple = dict_find(iterator, KEY_NEWS_ARTICLE);
  if (article_tuple && article_tuple->value && article_tuple->value->cstring) {
    snprintf(news_article, sizeof(news_article), "%s",
             article_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_INFO, "Received article (%d chars)",
            (int)strlen(news_article));

    // Start reading the article
    start_article_reading();
    return; // Don't process other messages
  }

  Tuple *news_title_tuple = dict_find(iterator, KEY_NEWS_TITLE);
  if (news_title_tuple && news_title_tuple->value &&
      news_title_tuple->value->cstring) {
    snprintf(news_title, sizeof(news_title), "%s",
             news_title_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_INFO, "Received title: %s", news_title);
    news_retry_count = 0;

    if (news_timer) {
      app_timer_cancel(news_timer);
      news_timer = NULL;
    }

    // Store the title in our array
    if (news_titles_count < 50) {
      snprintf(news_titles[news_titles_count], sizeof(news_titles[0]), "%s",
               news_title);
      news_titles_count++;
      APP_LOG(APP_LOG_LEVEL_INFO, "Stored news %d, total: %d",
              news_titles_count - 1, news_titles_count);

      // If this is the first news, start displaying it
      if (news_titles_count == 1) {
        current_news_index = 0;
        start_rsvp_for_title();
      }

      // Request more news if we haven't reached the limit and user is not navigating
      if (news_titles_count < news_max_count && !s_user_navigating) {
        news_timer = app_timer_register(100, news_timer_callback, NULL);
      }
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "No title found in message");
  }

  // Gérer l'ouverture de la page de configuration
  Tuple *config_opened_tuple = dict_find(iterator, KEY_CONFIG_OPENED);
  if (config_opened_tuple) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Config page opened - showing waiting screen");

    // Arrêter tous les timers
    if (rsvp_timer) {
      app_timer_cancel(rsvp_timer);
      rsvp_timer = NULL;
    }
    if (rsvp_start_timer) {
      app_timer_cancel(rsvp_start_timer);
      rsvp_start_timer = NULL;
    }
    if (news_timer) {
      app_timer_cancel(news_timer);
      news_timer = NULL;
    }
    if (end_timer) {
      app_timer_cancel(end_timer);
      end_timer = NULL;
    }

    // Afficher l'écran d'attente
    s_waiting_for_config = true;
    s_paused = true;
    layer_mark_dirty(s_canvas_layer);
  }

  // Gérer la réception des paramètres de configuration
  Tuple *config_received_tuple = dict_find(iterator, KEY_CONFIG_RECEIVED);
  if (config_received_tuple) {
    APP_LOG(APP_LOG_LEVEL_INFO,
            "Config received - vibrating and resetting app");

    // Double vibration de confirmation
    static const uint32_t segments[] = {100, 100, 100};
    VibePattern pattern = {.durations = segments,
                           .num_segments = ARRAY_LENGTH(segments)};
    vibes_enqueue_custom_pattern(pattern);

    // Gérer la vitesse de lecture si présente
    Tuple *speed_tuple = dict_find(iterator, KEY_READING_SPEED_WPM);
    if (speed_tuple) {
      uint16_t wpm = speed_tuple->value->uint16;
      rsvp_wpm_ms = 60000 / wpm;
      APP_LOG(APP_LOG_LEVEL_INFO, "Reading speed set to %d WPM (%d ms)", wpm,
              rsvp_wpm_ms);
      persist_write_int(KEY_READING_SPEED_WPM, wpm);
    }

    // Gérer l'option de rétroéclairage si présente
    Tuple *backlight_tuple = dict_find(iterator, KEY_BACKLIGHT_ENABLED);
    if (backlight_tuple) {
      s_backlight_enabled = backlight_tuple->value->uint8 != 0;
      APP_LOG(APP_LOG_LEVEL_INFO, "Backlight enabled: %d", s_backlight_enabled);
      persist_write_bool(KEY_BACKLIGHT_ENABLED, s_backlight_enabled);
    }

    // Réinitialiser l'état de l'application
    reset_app_state();
    return;
  }

  // Gérer la vitesse de lecture (si envoyée seule, sans config_received)
  Tuple *speed_tuple = dict_find(iterator, KEY_READING_SPEED_WPM);
  if (speed_tuple && !config_received_tuple) {
    uint16_t wpm = speed_tuple->value->uint16;
    // Convertir WPM en millisecondes: ms = 60000 / WPM
    rsvp_wpm_ms = 60000 / wpm;
    APP_LOG(APP_LOG_LEVEL_INFO, "Reading speed set to %d WPM (%d ms)", wpm,
            rsvp_wpm_ms);

    // Sauvegarder dans persist storage
    persist_write_int(KEY_READING_SPEED_WPM, wpm);
  }

  // Gérer l'option de rétroéclairage (si envoyée seule)
  Tuple *backlight_tuple = dict_find(iterator, KEY_BACKLIGHT_ENABLED);
  if (backlight_tuple && !config_received_tuple) {
    s_backlight_enabled = backlight_tuple->value->uint8 != 0;
    APP_LOG(APP_LOG_LEVEL_INFO, "Backlight enabled: %d", s_backlight_enabled);
    persist_write_bool(KEY_BACKLIGHT_ENABLED, s_backlight_enabled);
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

// Start displaying news at given index
static void display_news_at_index(int8_t index) {
  if (news_titles_count == 0 || index < 0 || index >= news_titles_count) {
    return;
  }

  // Cancel any existing timers
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
    rsvp_timer = NULL;
  }
  if (rsvp_start_timer) {
    app_timer_cancel(rsvp_start_timer);
    rsvp_start_timer = NULL;
  }
  if (news_timer) {
    app_timer_cancel(news_timer);
    news_timer = NULL;
  }
  if (end_timer) {
    app_timer_cancel(end_timer);
    end_timer = NULL;
  }
  if (page_number_timer) {
    app_timer_cancel(page_number_timer);
    page_number_timer = NULL;
  }

  // Clear end screen state
  s_end_screen = false;
  s_paused = false;
  s_showing_page_number = false;

  // Copy the selected title to news_title
  current_news_index = index;
  snprintf(news_title, sizeof(news_title), "%s", news_titles[index]);
  APP_LOG(APP_LOG_LEVEL_INFO, "Displaying news %d: %s", index, news_title);

  // Clear article mode when switching titles
  s_reading_article = false;
  news_article[0] = '\0';

  // Start RSVP for this title
  start_rsvp_for_title();
}

// Button handlers
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // If reading article, stop and go to splash then next title
  if (s_reading_article) {
    show_splash_then_next_title();
    return;
  }

  // If at end screen, exit app
  if (s_paused && s_end_screen) {
    window_stack_pop(true);
    return;
  }

  // If we have a valid news index, request the article
  if (current_news_index >= 0 && current_news_index < news_titles_count) {
    // Stop any current title reading
    if (rsvp_timer) {
      app_timer_cancel(rsvp_timer);
      rsvp_timer = NULL;
    }
    if (rsvp_start_timer) {
      app_timer_cancel(rsvp_start_timer);
      rsvp_start_timer = NULL;
    }
    if (page_number_timer) {
      app_timer_cancel(page_number_timer);
      page_number_timer = NULL;
    }

    // Reset page number display state
    s_showing_page_number = false;

    // Remember which news we're reading the article for
    s_article_news_index = current_news_index;

    // Request the article from JS
    request_article_from_js(current_news_index);

    // Show waiting state
    rsvp_word[0] = '\0';
    layer_mark_dirty(s_canvas_layer);
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // If reading article, stop and go to splash then next title
  if (s_reading_article) {
    show_splash_then_next_title();
    return;
  }

  // Previous news
  if (news_titles_count == 0) {
    return;
  }

  // Stop automatic news fetching when user starts navigating
  s_user_navigating = true;
  if (news_timer) {
    app_timer_cancel(news_timer);
    news_timer = NULL;
  }

  int8_t new_index;
  if (current_news_index <= 0) {
    // Wrap to end
    new_index = news_titles_count - 1;
  } else {
    new_index = current_news_index - 1;
  }

  display_news_at_index(new_index);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // If reading article, stop and go to splash then next title
  if (s_reading_article) {
    show_splash_then_next_title();
    return;
  }

  // Next news
  if (news_titles_count == 0) {
    return;
  }

  // Stop automatic news fetching when user starts navigating
  s_user_navigating = true;
  if (news_timer) {
    app_timer_cancel(news_timer);
    news_timer = NULL;
  }

  int8_t new_index;
  if (current_news_index >= news_titles_count - 1) {
    // Wrap to beginning
    new_index = 0;
  } else {
    new_index = current_news_index + 1;
  }

  display_news_at_index(new_index);
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  // If reading article, stop and go back to title list
  if (s_reading_article) {
    // Cancel timers
    if (rsvp_timer) {
      app_timer_cancel(rsvp_timer);
      rsvp_timer = NULL;
    }
    if (rsvp_start_timer) {
      app_timer_cancel(rsvp_start_timer);
      rsvp_start_timer = NULL;
    }

    s_reading_article = false;
    news_article[0] = '\0';
    s_article_news_index = -1;

    // Go back to showing the title
    start_rsvp_for_title();
    return;
  }

  // If showing menu, exit the app
  if (s_showing_menu) {
    window_stack_pop(true);
    return;
  }

  // Otherwise, go back to journal menu
  // Cancel all timers
  if (rsvp_timer) {
    app_timer_cancel(rsvp_timer);
    rsvp_timer = NULL;
  }
  if (rsvp_start_timer) {
    app_timer_cancel(rsvp_start_timer);
    rsvp_start_timer = NULL;
  }
  if (news_timer) {
    app_timer_cancel(news_timer);
    news_timer = NULL;
  }
  if (end_timer) {
    app_timer_cancel(end_timer);
    end_timer = NULL;
  }
  if (page_number_timer) {
    app_timer_cancel(page_number_timer);
    page_number_timer = NULL;
  }

  // Reset news state
  news_titles_count = 0;
  current_news_index = -1;
  news_title[0] = '\0';
  rsvp_word[0] = '\0';
  s_end_screen = false;
  s_paused = false;
  s_first_news_after_splash = true;
  s_showing_page_number = false;
  s_user_navigating = false;

  // Show the journal menu
  show_journal_menu();
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

  // Create canvas layer for RSVP display
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);
  layer_set_hidden(s_canvas_layer, true); // Start hidden

  // Create menu layer for journal selection
  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL,
                           (MenuLayerCallbacks){
                               .get_num_rows = menu_get_num_rows_callback,
                               .draw_row = menu_draw_row_callback,
                               .select_click = menu_select_callback,
                           });
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  // Start with menu visible
  s_showing_menu = true;
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  menu_layer_destroy(s_menu_layer);
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
    APP_LOG(APP_LOG_LEVEL_INFO, "Loaded reading speed: %d WPM (%d ms)", wpm,
            rsvp_wpm_ms);
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Using default reading speed: 400 WPM");
  }

  // Charger l'option de rétroéclairage sauvegardée
  if (persist_exists(KEY_BACKLIGHT_ENABLED)) {
    s_backlight_enabled = persist_read_bool(KEY_BACKLIGHT_ENABLED);
    APP_LOG(APP_LOG_LEVEL_INFO, "Loaded backlight enabled: %d",
            s_backlight_enabled);
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Using default backlight enabled: true");
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

  // App starts with journal menu - feed names will be sent by JS on ready
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
  if (page_number_timer) {
    app_timer_cancel(page_number_timer);
    page_number_timer = NULL;
  }

  app_message_deregister_callbacks();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
