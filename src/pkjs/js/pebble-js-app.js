// Configuration
var CONFIG_URL = 'https://sichroteph.github.io/RSVP-Breaking-News/';

// Default RSS feeds
var RSS_DEFAULT = 'https://feeds.bbci.co.uk/news/world/rss.xml';
var RSS_BREAKING = 'https://feeds.bbci.co.uk/news/world/rss.xml';
var RSS_GAMING = 'http://feeds.ign.com/ign/games-all';
var RSS_FINANCE = 'https://feeds.bbci.co.uk/news/business/rss.xml';

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

// State
var g_items = [];        // Array of {title: string, description: string}
var g_current_index = 0;
var g_channel_title = '';

// Get RSS URL from localStorage or use default
function getRssUrl() {
  var customUrl = localStorage.getItem('news_feed_url');
  if (customUrl && customUrl.trim() !== '') {
    console.log('Using custom RSS URL: ' + customUrl);
    return customUrl.trim();
  }
  console.log('Using default RSS URL: ' + RSS_DEFAULT);
  return RSS_DEFAULT;
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
        description = description.replace(/<[^>]*>/g, '');  // Remove HTML tags
        description = description.trim();
        // Limit description length to 500 chars to fit in Pebble memory
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

  console.log('Parsed ' + g_items.length + ' news items with descriptions');
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
  fetchRssFeed();
});

Pebble.addEventListener('appmessage', function (e) {
  console.log('Received message from Pebble: ' + JSON.stringify(e.payload));

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

    // La page de config envoie input_news_feed_url
    var feedUrl = configData.input_news_feed_url || configData.news_feed_url;
    if (feedUrl !== undefined) {
      var customUrl = feedUrl.trim();
      if (customUrl !== '') {
        console.log('Saving custom feed URL: ' + customUrl);
        localStorage.setItem('news_feed_url', customUrl);

        // Reset items pour forcer le rechargement
        g_items = [];
        g_current_index = 0;

        // Vibration de confirmation
        var dict = {};
        dict[KEY_NEWS_FEED_URL] = 1; // Signal de confirmation
        Pebble.sendAppMessage(dict, function () {
          console.log('Confirmation vibration sent');
        }, function (err) {
          console.log('Failed to send confirmation: ' + JSON.stringify(err));
        });

        fetchRssFeed();
      } else {
        console.log('Clearing custom feed URL');
        localStorage.removeItem('news_feed_url');
        g_items = [];
        g_current_index = 0;
        fetchRssFeed();
      }
    }

    // Gestion de la vitesse de lecture
    var readingSpeed = configData.reading_speed_wpm;
    if (readingSpeed !== undefined) {
      console.log('Saving reading speed: ' + readingSpeed + ' WPM');
      localStorage.setItem('reading_speed_wpm', readingSpeed);
    }
    
    // Envoyer toutes les données de config en une seule fois avec signal de réception
    var configDict = {};
    configDict[KEY_CONFIG_RECEIVED] = 1; // Signal de réception des paramètres (déclenche vibration + reset)
    if (readingSpeed !== undefined) {
      configDict[KEY_READING_SPEED_WPM] = parseInt(readingSpeed);
    }
    
    Pebble.sendAppMessage(configDict, function () {
      console.log('Config received signal and speed sent');
      // Recharger le flux RSS après envoi des paramètres
      fetchRssFeed();
    }, function (err) {
      console.log('Failed to send config: ' + JSON.stringify(err));
    });
  } catch (err) {
    console.log('Error parsing configuration: ' + err.message);
  }
});

console.log('Pebble JS app loaded');
