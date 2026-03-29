/**
 * OpenSWBF Service Worker
 *
 * Caches game assets after the first download so subsequent visits
 * load instantly, even offline.
 *
 * Strategy:
 *   - WASM, .lvl, and large binary assets: cache-first (serve from cache, fall back to network)
 *   - HTML, CSS, JS shell files: network-first (always try fresh, fall back to cache)
 *   - Everything else: network-only
 */

var CACHE_NAME = 'openswbf-assets-v1';

// Shell files to pre-cache on install.
var SHELL_FILES = [
  './',
  'index.html',
  'style.css',
  'loader.js'
];

// File extensions that should use the cache-first strategy.
var CACHE_FIRST_EXTENSIONS = [
  '.wasm',
  '.lvl',
  '.data',
  '.mem',
  '.bin',
  '.pak'
];

// File extensions for shell resources (network-first).
var NETWORK_FIRST_EXTENSIONS = [
  '.html',
  '.css',
  '.js'
];

// ============================================================
// Helpers
// ============================================================

function getExtension(url) {
  try {
    var pathname = new URL(url).pathname;
    var dot = pathname.lastIndexOf('.');
    if (dot !== -1) {
      return pathname.substring(dot).toLowerCase();
    }
  } catch (e) {
    // ignore
  }
  return '';
}

function isCacheFirst(url) {
  var ext = getExtension(url);
  return CACHE_FIRST_EXTENSIONS.indexOf(ext) !== -1;
}

function isNetworkFirst(url) {
  var ext = getExtension(url);
  return NETWORK_FIRST_EXTENSIONS.indexOf(ext) !== -1;
}

// ============================================================
// Install
// ============================================================

self.addEventListener('install', function (event) {
  console.log('[SW] Installing', CACHE_NAME);

  event.waitUntil(
    caches.open(CACHE_NAME).then(function (cache) {
      return cache.addAll(SHELL_FILES);
    }).then(function () {
      // Activate immediately without waiting for existing clients to close.
      return self.skipWaiting();
    })
  );
});

// ============================================================
// Activate
// ============================================================

self.addEventListener('activate', function (event) {
  console.log('[SW] Activating', CACHE_NAME);

  event.waitUntil(
    caches.keys().then(function (names) {
      return Promise.all(
        names
          .filter(function (name) {
            // Delete old caches from previous versions.
            return name.startsWith('openswbf-') && name !== CACHE_NAME;
          })
          .map(function (name) {
            console.log('[SW] Deleting old cache:', name);
            return caches.delete(name);
          })
      );
    }).then(function () {
      // Take control of all open clients immediately.
      return self.clients.claim();
    })
  );
});

// ============================================================
// Fetch
// ============================================================

self.addEventListener('fetch', function (event) {
  var request = event.request;

  // Only handle GET requests.
  if (request.method !== 'GET') {
    return;
  }

  // Only handle same-origin requests.
  if (!request.url.startsWith(self.location.origin)) {
    return;
  }

  if (isCacheFirst(request.url)) {
    // --- Cache-first strategy for large binary assets ---
    event.respondWith(
      caches.open(CACHE_NAME).then(function (cache) {
        return cache.match(request).then(function (cachedResponse) {
          if (cachedResponse) {
            return cachedResponse;
          }

          return fetch(request).then(function (networkResponse) {
            // Cache the response for next time.
            if (networkResponse && networkResponse.status === 200) {
              cache.put(request, networkResponse.clone());
            }
            return networkResponse;
          });
        });
      })
    );

  } else if (isNetworkFirst(request.url)) {
    // --- Network-first strategy for shell files ---
    event.respondWith(
      fetch(request)
        .then(function (networkResponse) {
          // Update cache with the fresh response.
          if (networkResponse && networkResponse.status === 200) {
            var responseClone = networkResponse.clone();
            caches.open(CACHE_NAME).then(function (cache) {
              cache.put(request, responseClone);
            });
          }
          return networkResponse;
        })
        .catch(function () {
          // Network failed — serve from cache.
          return caches.match(request);
        })
    );

  }
  // All other requests pass through to the network without caching.
});
