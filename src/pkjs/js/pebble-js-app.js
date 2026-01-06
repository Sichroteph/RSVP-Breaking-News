// RSVP Breaking News - JavaScript component
// Fetches news from RSS feed and sends titles to Pebble

console.log("===== RSVP Breaking News JS Loading =====");

// RSS News cache
var newsCache = [];
var newsCacheTime = 0;
var newsIndex = 0;
var NEWS_CACHE_DURATION = 30 * 60 * 1000; // 30 minutes in ms
var RSS_URL = "https://rss.app/feeds/SdI37Q5uDrVQuAOr.xml";

// Throttle protection
var lastNewsRequestTime = 0;
var NEWS_REQUEST_THROTTLE = 1000;
var newsXhrPending = false;

// Message keys
var KEY_NEWS_TITLE = 172;
var KEY_REQUEST_NEWS = 173;

console.log("Message keys defined: NEWS_TITLE=" + KEY_NEWS_TITLE + ", REQUEST_NEWS=" + KEY_REQUEST_NEWS);

function sendNewsTitle(title) {
  // Truncate to 100 chars max for Pebble memory
  if (title.length > 100) {
    title = title.substring(0, 97) + "...";
  }
  var dict = {};
  dict[KEY_NEWS_TITLE] = title;
  Pebble.sendAppMessage(dict, function () {
    console.log("News title sent successfully");
  }, function (err) {
    console.log("Error sending news title: " + err);
  });
}

function fetchAndSendNews() {
  var now = Date.now();

  // Throttle: ignore requests that come too fast
  if ((now - lastNewsRequestTime) < NEWS_REQUEST_THROTTLE) {
    console.log("News request throttled");
    return;
  }
  lastNewsRequestTime = now;

  // Check if cache is still valid
  if (newsCache.length > 0 && (now - newsCacheTime) < NEWS_CACHE_DURATION) {
    // Use cached data, increment index
    newsIndex = (newsIndex + 1) % newsCache.length;
    console.log("Sending cached title #" + newsIndex + ": " + newsCache[newsIndex]);
    sendNewsTitle(newsCache[newsIndex]);
    return;
  }

  // Prevent concurrent XHR requests
  if (newsXhrPending) {
    console.log("News XHR already pending");
    if (newsCache.length > 0) {
      newsIndex = (newsIndex + 1) % newsCache.length;
      sendNewsTitle(newsCache[newsIndex]);
    } else {
      sendNewsTitle("Loading...");
    }
    return;
  }

  // Cache expired or empty, fetch new data
  newsXhrPending = true;
  var xhr = new XMLHttpRequest();

  xhr.timeout = 10000;
  xhr.ontimeout = function () {
    newsXhrPending = false;
    console.log("News XHR timeout");
    sendNewsTitle("Timeout");
  };

  xhr.onload = function () {
    newsXhrPending = false;
    if (xhr.status === 200) {
      var titles = [];
      var text = xhr.responseText;

      // Parse titles from XML using regex
      var regex = /<title>\s*<!\[CDATA\[\s*([^\]]+?)\s*\]\]>\s*<\/title>/g;
      var match;
      while ((match = regex.exec(text)) !== null) {
        var title = match[1].trim();
        if (title && title.length > 0) {
          titles.push(title);
        }
      }

      // Also try without CDATA
      var regex2 = /<title>([^<]+)<\/title>/g;
      while ((match = regex2.exec(text)) !== null) {
        var title = match[1].trim();
        if (title && title.length > 0 && titles.indexOf(title) === -1) {
          titles.push(title);
        }
      }

      if (titles.length > 0) {
        // Remove first title if it's the RSS feed title
        if (titles[0].indexOf("Reuters") !== -1 && titles[0].indexOf("Breaking") !== -1) {
          titles.shift();
        }
        newsCache = titles;
        newsCacheTime = now;
        newsIndex = 0;
        sendNewsTitle(newsCache[newsIndex]);
      } else {
        sendNewsTitle("No news available");
      }
    } else {
      sendNewsTitle("News fetch failed");
    }
  };

  xhr.onerror = function () {
    newsXhrPending = false;
    sendNewsTitle("Network error");
  };

  xhr.open("GET", RSS_URL, true);
  xhr.send();
}

// Listen for when the app is opened
Pebble.addEventListener('ready', function () {
  console.log("===== RSVP Breaking News JS ready event =====");

  // Send ready signal and pre-fetch news on startup
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    if (xhr.status === 200) {
      var titles = [];
      var text = xhr.responseText;
      var regex = /<title>\s*<!\[CDATA\[\s*([^\]]+?)\s*\]\]>\s*<\/title>/g;
      var match;
      while ((match = regex.exec(text)) !== null) {
        var title = match[1].trim();
        if (title && title.length > 0) {
          titles.push(title);
        }
      }
      var regex2 = /<title>([^<]+)<\/title>/g;
      while ((match = regex2.exec(text)) !== null) {
        var title = match[1].trim();
        if (title && title.length > 0 && titles.indexOf(title) === -1) {
          titles.push(title);
        }
      }
      if (titles.length > 0) {
        if (titles[0].indexOf("Reuters") !== -1) {
          titles.shift();
        }
        newsCache = titles;
        newsCacheTime = Date.now();
        newsIndex = 0;
        console.log("News cache prefetched: " + titles.length + " titles");
        // Don't send automatically - wait for watch to request
      }
    }
  };
  xhr.onerror = function () {
    console.log("Error pre-fetching news");
  };
  xhr.open("GET", RSS_URL, true);
  xhr.send();
});

// Listen for messages from the watch
Pebble.addEventListener('appmessage', function (e) {
  console.log("===== Received message from watch =====");
  console.log("Payload: " + JSON.stringify(e.payload));

  // Check if this is a news request (using string key name)
  if (e.payload && (e.payload["KEY_REQUEST_NEWS"] !== undefined || e.payload[KEY_REQUEST_NEWS] !== undefined)) {
    console.log("News request received, fetching news...");
    fetchAndSendNews();
    return;
  } else {
    console.log("No news request found in payload");
  }
});

console.log("===== Event listeners registered =====");
