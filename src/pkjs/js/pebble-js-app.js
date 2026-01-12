// Configuration
var CONFIG_URL = 'https://sichroteph.github.io/RSVP-Breaking-News/';

// Default RSS feeds
var DEFAULT_FEEDS = [
  { name: 'BBC World', url: 'https://feeds.bbci.co.uk/news/world/rss.xml' },
  { name: 'NY Times', url: 'https://rss.nytimes.com/services/xml/rss/nyt/World.xml' },
  { name: 'NPR News', url: 'https://feeds.npr.org/1001/rss.xml' },
  { name: 'Guardian', url: 'https://www.theguardian.com/world/rss' },
  { name: 'Le Monde', url: 'https://www.lemonde.fr/rss/une.xml' },
  { name: 'Reuters', url: 'https://feeds.reuters.com/reuters/topNews' }
];

// Message keys
var KEY_NEWS_TITLE = 172;
var KEY_REQUEST_NEWS = 173;
var KEY_NEWS_FEED_URL = 175;
var KEY_NEWS_CHANNEL_TITLE = 176;
var KEY_READING_SPEED_WPM = 177;
var KEY_CONFIG_OPENED = 178;
var KEY_CONFIG_RECEIVED = 179;
var KEY_REQUEST_ARTICLE = 180;
var KEY_NEWS_ARTICLE = 181;
var KEY_BACKLIGHT_ENABLED = 182;
var KEY_FEED_NAME = 183;
var KEY_REQUEST_FEEDS = 184;
var KEY_SELECT_FEED = 185;
var KEY_FEEDS_COUNT = 186;

// State
var g_items = [];        // Array of {title: string, description: string}
var g_current_index = 0;
var g_channel_title = '';
var g_feeds = [];        // Array of {name: string, url: string}
var g_selected_feed_index = 0;
var g_feeds_sent_index = 0;

// Load feeds from localStorage or use defaults
function loadFeeds() {
  var stored = localStorage.getItem('rss_feeds');
  if (stored) {
    try {
      g_feeds = JSON.parse(stored);
      if (!Array.isArray(g_feeds) || g_feeds.length === 0) {
        g_feeds = DEFAULT_FEEDS.slice();
      }
    } catch (e) {
      g_feeds = DEFAULT_FEEDS.slice();
    }
  } else {
    g_feeds = DEFAULT_FEEDS.slice();
  }
  console.log('Loaded ' + g_feeds.length + ' feeds');
}

// Get RSS URL for selected feed
function getRssUrl() {
  if (g_feeds.length === 0) {
    loadFeeds();
  }
  if (g_selected_feed_index >= 0 && g_selected_feed_index < g_feeds.length) {
    console.log('Using feed: ' + g_feeds[g_selected_feed_index].name + ' - ' + g_feeds[g_selected_feed_index].url);
    return g_feeds[g_selected_feed_index].url;
  }
  console.log('Using default feed');
  return DEFAULT_FEEDS[0].url;
}

// Decode HTML entities
function decodeHtmlEntities(text) {
  if (!text) return '';

  var decoded = text;
  decoded = decoded.replace(/&amp;/g, '&');
  decoded = decoded.replace(/&lt;/g, '<');
  decoded = decoded.replace(/&gt;/g, '>');
  decoded = decoded.replace(/&quot;/g, '"');
  decoded = decoded.replace(/&#39;/g, "'");
  decoded = decoded.replace(/&apos;/g, "'");
  decoded = decoded.replace(/&nbsp;/g, ' ');
  decoded = decoded.replace(/&#8217;/g, "'");
  decoded = decoded.replace(/&#8220;/g, '"');
  decoded = decoded.replace(/&#8221;/g, '"');
  decoded = decoded.replace(/&#8211;/g, '-');
  decoded = decoded.replace(/&#8212;/g, '-');
  decoded = decoded.replace(/&#160;/g, ' ');
  decoded = decoded.replace(/&rsquo;/g, "'");
  decoded = decoded.replace(/&lsquo;/g, "'");
  decoded = decoded.replace(/&rdquo;/g, '"');
  decoded = decoded.replace(/&ldquo;/g, '"');
  decoded = decoded.replace(/&mdash;/g, '-');
  decoded = decoded.replace(/&ndash;/g, '-');

  // French accented characters
  decoded = decoded.replace(/&eacute;/g, 'é');
  decoded = decoded.replace(/&#233;/g, 'é');
  decoded = decoded.replace(/&egrave;/g, 'è');
  decoded = decoded.replace(/&#232;/g, 'è');
  decoded = decoded.replace(/&ecirc;/g, 'ê');
  decoded = decoded.replace(/&#234;/g, 'ê');
  decoded = decoded.replace(/&euml;/g, 'ë');
  decoded = decoded.replace(/&#235;/g, 'ë');
  decoded = decoded.replace(/&agrave;/g, 'à');
  decoded = decoded.replace(/&#224;/g, 'à');
  decoded = decoded.replace(/&acirc;/g, 'â');
  decoded = decoded.replace(/&#226;/g, 'â');
  decoded = decoded.replace(/&auml;/g, 'ä');
  decoded = decoded.replace(/&#228;/g, 'ä');
  decoded = decoded.replace(/&ugrave;/g, 'ù');
  decoded = decoded.replace(/&#249;/g, 'ù');
  decoded = decoded.replace(/&ucirc;/g, 'û');
  decoded = decoded.replace(/&#251;/g, 'û');
  decoded = decoded.replace(/&uuml;/g, 'ü');
  decoded = decoded.replace(/&#252;/g, 'ü');
  decoded = decoded.replace(/&ocirc;/g, 'ô');
  decoded = decoded.replace(/&#244;/g, 'ô');
  decoded = decoded.replace(/&ouml;/g, 'ö');
  decoded = decoded.replace(/&#246;/g, 'ö');
  decoded = decoded.replace(/&icirc;/g, 'î');
  decoded = decoded.replace(/&#238;/g, 'î');
  decoded = decoded.replace(/&iuml;/g, 'ï');
  decoded = decoded.replace(/&#239;/g, 'ï');
  decoded = decoded.replace(/&ccedil;/g, 'ç');
  decoded = decoded.replace(/&#231;/g, 'ç');
  decoded = decoded.replace(/&aelig;/g, 'æ');
  decoded = decoded.replace(/&#230;/g, 'æ');
  decoded = decoded.replace(/&oelig;/g, 'œ');
  decoded = decoded.replace(/&#339;/g, 'œ');

  // Capital accented characters
  decoded = decoded.replace(/&Eacute;/g, 'É');
  decoded = decoded.replace(/&#201;/g, 'É');
  decoded = decoded.replace(/&Egrave;/g, 'È');
  decoded = decoded.replace(/&#200;/g, 'È');
  decoded = decoded.replace(/&Ecirc;/g, 'Ê');
  decoded = decoded.replace(/&#202;/g, 'Ê');
  decoded = decoded.replace(/&Agrave;/g, 'À');
  decoded = decoded.replace(/&#192;/g, 'À');
  decoded = decoded.replace(/&Acirc;/g, 'Â');
  decoded = decoded.replace(/&#194;/g, 'Â');
  decoded = decoded.replace(/&Ccedil;/g, 'Ç');
  decoded = decoded.replace(/&#199;/g, 'Ç');

  return decoded;
}

// Send channel title to Pebble
function sendNewsChannelTitle() {
  if (!g_channel_title || g_channel_title.trim() === '') {
    return;
  }

  console.log('Sending channel title: ' + g_channel_title);
  var dict = {};
  dict[KEY_NEWS_CHANNEL_TITLE] = g_channel_title;
  Pebble.sendAppMessage(dict, function () {
    console.log('Channel title sent successfully');
  }, function (e) {
    console.log('Failed to send channel title: ' + JSON.stringify(e));
  });
}

// Fetch and parse RSS feed
function fetchRssFeed() {
  var rssUrl = getRssUrl();
  console.log('Fetching RSS feed from: ' + rssUrl);

  var xhr = new XMLHttpRequest();
  xhr.open('GET', rssUrl, true);
  xhr.setRequestHeader('Content-Type', 'text/xml; charset=UTF-8');

  xhr.onload = function () {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        console.log('RSS feed fetched successfully');
        parseRssFeed(xhr.responseText);
      } else {
        console.log('Request failed with status: ' + xhr.status);
      }
    }
  };

  xhr.onerror = function () {
    console.log('Network error while fetching RSS feed');
  };

  xhr.send();
}

// Parse RSS XML
function parseRssFeed(xmlText) {
  console.log('Starting RSS parsing, text length: ' + xmlText.length);

  try {
    // Try DOMParser first
    if (typeof DOMParser !== 'undefined') {
      console.log('Using DOMParser');
      var parser = new DOMParser();
      var xmlDoc = parser.parseFromString(xmlText, 'text/xml');

      var items = xmlDoc.getElementsByTagName('item');
      console.log('Found ' + items.length + ' items in RSS feed');

      // Get channel title
      var channelElements = xmlDoc.getElementsByTagName('channel');
      if (channelElements.length > 0) {
        var titleElements = channelElements[0].getElementsByTagName('title');
        if (titleElements.length > 0) {
          g_channel_title = titleElements[0].textContent || '';
          g_channel_title = decodeHtmlEntities(g_channel_title);
          console.log('Channel title: ' + g_channel_title);
          sendNewsChannelTitle();
        }
      }

      // Parse items (title + description)
      g_items = [];
      for (var i = 0; i < items.length && i < 50; i++) {
        var titleNode = items[i].getElementsByTagName('title')[0];
        var descNode = items[i].getElementsByTagName('description')[0];

        if (titleNode) {
          var title = titleNode.textContent || '';
          title = decodeHtmlEntities(title);
          title = title.replace(/<[^>]*>/g, '');
          title = title.trim();

          var description = '';
          if (descNode) {
            description = descNode.textContent || '';
            description = decodeHtmlEntities(description);
            description = description.replace(/<[^>]*>/g, '');
            description = description.trim();
            if (description.length > 500) {
              description = description.substring(0, 497) + '...';
            }
          }

          if (title.length > 0) {
            g_items.push({
              title: title,
              description: description
            });
          }
        }
      }

      if (g_items.length > 0) {
        console.log('Parsed ' + g_items.length + ' news items with DOMParser');
        g_current_index = 0;
        sendNextNewsItem();
        return;
      }
    }
  } catch (e) {
    console.log('DOMParser failed: ' + e.message);
  }

  // Fallback: use regex parsing
  console.log('Using regex fallback parsing');
  parseRssFeedWithRegex(xmlText);
}

// Fallback regex parser for RSS
function parseRssFeedWithRegex(xmlText) {
  g_items = [];

  // Extract channel title
  var channelTitleMatch = xmlText.match(/<channel[^>]*>[\s\S]*?<title[^>]*>([^<]+)<\/title>/i);
  if (channelTitleMatch) {
    g_channel_title = decodeHtmlEntities(channelTitleMatch[1].trim());
    console.log('Channel title (regex): ' + g_channel_title);
    sendNewsChannelTitle();
  }

  // Extract items using regex
  var itemRegex = /<item[^>]*>([\s\S]*?)<\/item>/gi;
  var titleRegex = /<title[^>]*>(?:<!\[CDATA\[)?([\s\S]*?)(?:\]\]>)?<\/title>/i;
  var descRegex = /<description[^>]*>(?:<!\[CDATA\[)?([\s\S]*?)(?:\]\]>)?<\/description>/i;

  var match;
  var count = 0;
  while ((match = itemRegex.exec(xmlText)) !== null && count < 50) {
    var itemContent = match[1];

    var titleMatch = itemContent.match(titleRegex);
    if (titleMatch) {
      var title = titleMatch[1];
      title = decodeHtmlEntities(title);
      title = title.replace(/<[^>]*>/g, '');
      title = title.trim();

      var description = '';
      var descMatch = itemContent.match(descRegex);
      if (descMatch) {
        description = descMatch[1];
        description = decodeHtmlEntities(description);
        description = description.replace(/<[^>]*>/g, '');
        description = description.trim();
        if (description.length > 500) {
          description = description.substring(0, 497) + '...';
        }
      }

      if (title.length > 0) {
        g_items.push({
          title: title,
          description: description
        });
        count++;
      }
    }
  }

  console.log('Parsed ' + g_items.length + ' news items with regex');
  g_current_index = 0;

  if (g_items.length > 0) {
    sendNextNewsItem();
  } else {
    console.log('No valid items found in RSS feed');
  }
}

// Send next news item to Pebble
function sendNextNewsItem() {
  if (g_current_index >= g_items.length) {
    console.log('All items sent');
    g_current_index = 0;
    return;
  }

  var item = g_items[g_current_index];
  console.log('Sending item ' + (g_current_index + 1) + ': ' + item.title);

  var dict = {};
  dict[KEY_NEWS_TITLE] = item.title;
  Pebble.sendAppMessage(dict, function () {
    console.log('Message sent successfully');
    g_current_index++;
  }, function (e) {
    console.log('Failed to send message: ' + JSON.stringify(e));
  });
}

// Send feed names to Pebble (one at a time)
function sendFeedNames() {
  loadFeeds();
  g_feeds_sent_index = 0;

  // First send the count
  var dict = {};
  dict[KEY_FEEDS_COUNT] = g_feeds.length;
  Pebble.sendAppMessage(dict, function () {
    console.log('Feeds count sent: ' + g_feeds.length);
    // Then send each feed name
    sendNextFeedName();
  }, function (e) {
    console.log('Failed to send feeds count: ' + JSON.stringify(e));
  });
}

function sendNextFeedName() {
  if (g_feeds_sent_index >= g_feeds.length) {
    console.log('All feed names sent');
    return;
  }

  var feed = g_feeds[g_feeds_sent_index];
  console.log('Sending feed name ' + g_feeds_sent_index + ': ' + feed.name);

  var dict = {};
  dict[KEY_FEED_NAME] = feed.name;
  Pebble.sendAppMessage(dict, function () {
    console.log('Feed name sent successfully');
    g_feeds_sent_index++;
    // Send next feed name after a short delay
    setTimeout(sendNextFeedName, 50);
  }, function (e) {
    console.log('Failed to send feed name: ' + JSON.stringify(e));
  });
}

// Send article for a specific index to Pebble
function sendArticle(index) {
  if (index < 0 || index >= g_items.length) {
    console.log('Invalid article index: ' + index);
    return;
  }

  var item = g_items[index];
  var article = item.description || 'No article content available.';
  console.log('Sending article for item ' + index + ' (' + article.length + ' chars)');

  var dict = {};
  dict[KEY_NEWS_ARTICLE] = article;
  Pebble.sendAppMessage(dict, function () {
    console.log('Article sent successfully');
  }, function (e) {
    console.log('Failed to send article: ' + JSON.stringify(e));
  });
}

// Pebble event handlers
Pebble.addEventListener('ready', function (e) {
  console.log('PebbleKit JS ready');
  loadFeeds();
  // Send feed names to the watch for the selection menu
  sendFeedNames();
});

Pebble.addEventListener('appmessage', function (e) {
  console.log('Received message from Pebble: ' + JSON.stringify(e.payload));

  // Handle feed selection
  var feedIndex = e.payload[KEY_SELECT_FEED] || e.payload['KEY_SELECT_FEED'] || e.payload['185'];
  if (feedIndex !== undefined) {
    console.log('Feed selection received: ' + feedIndex);
    g_selected_feed_index = parseInt(feedIndex);
    g_items = [];
    g_current_index = 0;
    fetchRssFeed();
    return;
  }

  // Handle request for feed list
  var requestFeeds = e.payload[KEY_REQUEST_FEEDS] || e.payload['KEY_REQUEST_FEEDS'] || e.payload['184'];
  if (requestFeeds !== undefined) {
    console.log('Feed list request received');
    sendFeedNames();
    return;
  }

  // Handle article request
  var articleIndex = e.payload[KEY_REQUEST_ARTICLE] || e.payload['KEY_REQUEST_ARTICLE'] || e.payload['180'];
  if (articleIndex !== undefined) {
    console.log('Article request received for index: ' + articleIndex);
    sendArticle(parseInt(articleIndex));
    return;
  }

  // Vérifier à la fois la clé numérique et le nom de clé
  if (e.payload[KEY_REQUEST_NEWS] || e.payload['KEY_REQUEST_NEWS'] || e.payload['173']) {
    console.log('News request received');
    if (g_items.length === 0) {
      fetchRssFeed();
    } else {
      sendNextNewsItem();
    }
  }

  if (e.payload[KEY_NEWS_FEED_URL] || e.payload['KEY_NEWS_FEED_URL'] || e.payload['175']) {
    var customUrl = e.payload[KEY_NEWS_FEED_URL] || e.payload['KEY_NEWS_FEED_URL'] || e.payload['175'];
    console.log('Received custom feed URL: ' + customUrl);
    localStorage.setItem('news_feed_url', customUrl);
    fetchRssFeed();
  }
});

Pebble.addEventListener('showConfiguration', function (e) {
  console.log('Opening configuration page');

  // Envoyer un signal à la montre pour afficher l'écran d'attente
  var dict = {};
  dict[KEY_CONFIG_OPENED] = 1;
  Pebble.sendAppMessage(dict, function () {
    console.log('Config opened signal sent');
  }, function (err) {
    console.log('Failed to send config opened signal: ' + JSON.stringify(err));
  });

  Pebble.openURL(CONFIG_URL);
});

Pebble.addEventListener('webviewclosed', function (e) {
  console.log('Configuration closed');

  if (!e.response || e.response === 'CANCELLED') {
    console.log('Configuration cancelled');
    return;
  }

  try {
    var configData = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration data: ' + JSON.stringify(configData));

    // Handle RSS feeds array
    var rssFeeds = configData.rss_feeds;
    if (rssFeeds !== undefined && Array.isArray(rssFeeds)) {
      console.log('Saving ' + rssFeeds.length + ' RSS feeds');
      localStorage.setItem('rss_feeds', JSON.stringify(rssFeeds));
      g_feeds = rssFeeds;

      // Reset items pour forcer le rechargement
      g_items = [];
      g_current_index = 0;
    }

    // Gestion de la vitesse de lecture
    var readingSpeed = configData.reading_speed_wpm;
    if (readingSpeed !== undefined) {
      console.log('Saving reading speed: ' + readingSpeed + ' WPM');
      localStorage.setItem('reading_speed_wpm', readingSpeed);
    }

    // Gestion de l'option de rétroéclairage
    var backlightEnabled = configData.backlight_enabled;
    if (backlightEnabled !== undefined) {
      console.log('Saving backlight enabled: ' + backlightEnabled);
      localStorage.setItem('backlight_enabled', backlightEnabled);
    }

    // Envoyer toutes les données de config en une seule fois avec signal de réception
    var configDict = {};
    configDict[KEY_CONFIG_RECEIVED] = 1; // Signal de réception des paramètres (déclenche vibration + reset)
    if (readingSpeed !== undefined) {
      configDict[KEY_READING_SPEED_WPM] = parseInt(readingSpeed);
    }
    if (backlightEnabled !== undefined) {
      configDict[KEY_BACKLIGHT_ENABLED] = backlightEnabled ? 1 : 0;
    }

    Pebble.sendAppMessage(configDict, function () {
      console.log('Config received signal and speed sent');
      // Send updated feed names to watch
      sendFeedNames();
    }, function (err) {
      console.log('Failed to send config: ' + JSON.stringify(err));
    });
  } catch (err) {
    console.log('Error parsing configuration: ' + err.message);
  }
});

console.log('Pebble JS app loaded');
