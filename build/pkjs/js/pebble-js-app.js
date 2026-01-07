// Configuration
var CONFIG_URL = 'https://sichroteph.github.io/RSVP-Breaking-News/';

// Default RSS feeds
var RSS_DEFAULT = 'https://www.huffingtonpost.com/feeds/verticals/world/index.xml';
var RSS_BREAKING = 'https://www.huffingtonpost.com/feeds/verticals/world/index.xml';
var RSS_GAMING = 'http://feeds.ign.com/ign/games-all';
var RSS_FINANCE = 'http://feeds.reuters.com/news/wealth';

// Message keys
var KEY_NEWS_TITLE = 172;
var KEY_REQUEST_NEWS = 173;
var KEY_NEWS_FEED_URL = 175;
var KEY_NEWS_CHANNEL_TITLE = 176;

// State
var g_items = [];
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
  var elem = document.createElement('textarea');
  elem.innerHTML = text;
  var decoded = elem.value;
  
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
  
  return decoded;
}

// Send channel title to Pebble
function sendNewsChannelTitle() {
  if (!g_channel_title || g_channel_title.trim() === '') {
    return;
  }
  
  console.log('Sending channel title: ' + g_channel_title);
  Pebble.sendAppMessage({
    'NEWS_CHANNEL_TITLE': g_channel_title
  }, function() {
    console.log('Channel title sent successfully');
  }, function(e) {
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
  
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        console.log('RSS feed fetched successfully');
        parseRssFeed(xhr.responseText);
      } else {
        console.log('Request failed with status: ' + xhr.status);
      }
    }
  };
  
  xhr.onerror = function() {
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
  
  // Parse items
  g_items = [];
  for (var i = 0; i < items.length && i < 50; i++) {
    var titleNode = items[i].getElementsByTagName('title')[0];
    if (titleNode) {
      var title = titleNode.textContent || '';
      title = decodeHtmlEntities(title);
      title = title.replace(/<[^>]*>/g, '');
      title = title.trim();
      
      if (title.length > 0) {
        g_items.push(title);
      }
    }
  }
  
  console.log('Parsed ' + g_items.length + ' news items');
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
  
  var title = g_items[g_current_index];
  console.log('Sending item ' + (g_current_index + 1) + ': ' + title);
  
  Pebble.sendAppMessage({
    'NEWS_TITLE': title
  }, function() {
    console.log('Message sent successfully');
    g_current_index++;
  }, function(e) {
    console.log('Failed to send message: ' + JSON.stringify(e));
  });
}

// Pebble event handlers
Pebble.addEventListener('ready', function(e) {
  console.log('PebbleKit JS ready');
  fetchRssFeed();
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('Received message from Pebble');
  
  if (e.payload.REQUEST_NEWS) {
    console.log('News request received');
    if (g_items.length === 0) {
      fetchRssFeed();
    } else {
      sendNextNewsItem();
    }
  }
  
  if (e.payload.NEWS_FEED_URL) {
    var customUrl = e.payload.NEWS_FEED_URL;
    console.log('Received custom feed URL: ' + customUrl);
    localStorage.setItem('news_feed_url', customUrl);
    fetchRssFeed();
  }
});

Pebble.addEventListener('showConfiguration', function(e) {
  console.log('Opening configuration page');
  Pebble.openURL(CONFIG_URL);
});

Pebble.addEventListener('webviewclosed', function(e) {
  console.log('Configuration closed');
  
  if (!e.response || e.response === 'CANCELLED') {
    console.log('Configuration cancelled');
    return;
  }
  
  try {
    var configData = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration data: ' + JSON.stringify(configData));
    
    if (configData.news_feed_url) {
      var customUrl = configData.news_feed_url.trim();
      if (customUrl !== '') {
        console.log('Saving custom feed URL: ' + customUrl);
        localStorage.setItem('news_feed_url', customUrl);
        fetchRssFeed();
      } else {
        console.log('Clearing custom feed URL');
        localStorage.removeItem('news_feed_url');
        fetchRssFeed();
      }
    }
  } catch (err) {
    console.log('Error parsing configuration: ' + err.message);
  }
});

console.log('Pebble JS app loaded');
