/**
 * OpenSWBF Asset Loader
 *
 * Handles downloading game assets with progress tracking,
 * persistent caching via IndexedDB, and service worker registration.
 */

// ============================================================
// IndexedDB Helpers
// ============================================================

var AssetDB = (function () {
  'use strict';

  var DB_NAME = 'openswbf-assets';
  var DB_VERSION = 1;
  var STORE_NAME = 'files';

  function open() {
    return new Promise(function (resolve, reject) {
      var request = indexedDB.open(DB_NAME, DB_VERSION);

      request.onupgradeneeded = function (event) {
        var db = event.target.result;
        if (!db.objectStoreNames.contains(STORE_NAME)) {
          db.createObjectStore(STORE_NAME);
        }
      };

      request.onsuccess = function (event) {
        resolve(event.target.result);
      };

      request.onerror = function (event) {
        console.warn('[AssetDB] Failed to open IndexedDB:', event.target.error);
        reject(event.target.error);
      };
    });
  }

  function get(key) {
    return open().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(STORE_NAME, 'readonly');
        var store = tx.objectStore(STORE_NAME);
        var request = store.get(key);

        request.onsuccess = function () {
          resolve(request.result || null);
        };

        request.onerror = function () {
          reject(request.error);
        };
      });
    });
  }

  function put(key, value) {
    return open().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(STORE_NAME, 'readwrite');
        var store = tx.objectStore(STORE_NAME);
        var request = store.put(value, key);

        request.onsuccess = function () {
          resolve();
        };

        request.onerror = function () {
          reject(request.error);
        };
      });
    });
  }

  function remove(key) {
    return open().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(STORE_NAME, 'readwrite');
        var store = tx.objectStore(STORE_NAME);
        var request = store.delete(key);

        request.onsuccess = function () {
          resolve();
        };

        request.onerror = function () {
          reject(request.error);
        };
      });
    });
  }

  function clear() {
    return open().then(function (db) {
      return new Promise(function (resolve, reject) {
        var tx = db.transaction(STORE_NAME, 'readwrite');
        var store = tx.objectStore(STORE_NAME);
        var request = store.clear();

        request.onsuccess = function () {
          resolve();
        };

        request.onerror = function () {
          reject(request.error);
        };
      });
    });
  }

  return {
    get: get,
    put: put,
    remove: remove,
    clear: clear
  };
})();


// ============================================================
// AssetLoader Class
// ============================================================

function AssetLoader() {
  this._progressCallback = null;
  this._totalBytes = 0;
  this._loadedBytes = 0;
}

/**
 * Set a callback for download progress updates.
 * @param {function(loaded: number, total: number): void} fn
 */
AssetLoader.prototype.setProgressCallback = function (fn) {
  this._progressCallback = fn;
};

/**
 * Report progress to the registered callback.
 */
AssetLoader.prototype._reportProgress = function () {
  if (this._progressCallback) {
    this._progressCallback(this._loadedBytes, this._totalBytes);
  }
};

/**
 * Fetch a file from a URL with progress tracking.
 * Returns an ArrayBuffer of the file contents.
 *
 * @param {string} url - URL to fetch
 * @returns {Promise<ArrayBuffer>}
 */
AssetLoader.prototype.loadFile = function (url) {
  var self = this;

  return new Promise(function (resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'arraybuffer';

    xhr.onprogress = function (event) {
      if (event.lengthComputable) {
        // On the first progress event with a known total, register
        // the total bytes for this file.
        if (event.total && !xhr._totalRegistered) {
          self._totalBytes += event.total;
          xhr._totalRegistered = true;
        }
        self._loadedBytes += event.loaded - (xhr._lastLoaded || 0);
        xhr._lastLoaded = event.loaded;
        self._reportProgress();
      }
    };

    xhr.onload = function () {
      if (xhr.status >= 200 && xhr.status < 300) {
        resolve(xhr.response);
      } else {
        reject(new Error('HTTP ' + xhr.status + ' fetching ' + url));
      }
    };

    xhr.onerror = function () {
      reject(new Error('Network error fetching ' + url));
    };

    xhr.ontimeout = function () {
      reject(new Error('Timeout fetching ' + url));
    };

    xhr.send();
  });
};

/**
 * Download a .lvl (or any binary) file with persistent caching.
 *
 * 1. Check IndexedDB for a cached copy.
 * 2. If not cached, download from `url` and store in IndexedDB.
 * 3. Write the data into the Emscripten virtual filesystem so the
 *    engine can access it at the given path.
 *
 * @param {string} url       - Remote URL of the .lvl file
 * @param {string} [fsPath]  - Path inside the Emscripten FS (defaults to filename from URL)
 * @returns {Promise<Uint8Array>}
 */
AssetLoader.prototype.loadLVL = function (url, fsPath) {
  var self = this;

  // Derive a filesystem path from the URL if none provided.
  if (!fsPath) {
    var parts = url.split('/');
    fsPath = '/' + parts[parts.length - 1];
  }

  var cacheKey = 'lvl:' + url;

  return AssetDB.get(cacheKey)
    .catch(function () {
      // IndexedDB unavailable — proceed without cache
      return null;
    })
    .then(function (cached) {
      if (cached) {
        console.log('[AssetLoader] Cache hit for', url);
        return cached;
      }

      console.log('[AssetLoader] Downloading', url);
      return self.loadFile(url).then(function (buffer) {
        // Store in IndexedDB (fire-and-forget; don't block on it)
        AssetDB.put(cacheKey, buffer).catch(function (err) {
          console.warn('[AssetLoader] Failed to cache asset:', err);
        });
        return buffer;
      });
    })
    .then(function (buffer) {
      var data = new Uint8Array(buffer);

      // Write into Emscripten FS if available
      if (typeof Module !== 'undefined' && Module.FS) {
        try {
          // Ensure parent directories exist
          var dir = fsPath.substring(0, fsPath.lastIndexOf('/'));
          if (dir && dir !== '/') {
            Module.FS.mkdirTree(dir);
          }
          Module.FS.writeFile(fsPath, data);
          console.log('[AssetLoader] Wrote', fsPath, '(' + data.length + ' bytes) to Emscripten FS');
        } catch (err) {
          console.warn('[AssetLoader] Could not write to Emscripten FS:', err);
        }
      }

      return data;
    });
};

/**
 * Load multiple .lvl files in parallel.
 *
 * @param {Array<{url: string, fsPath?: string}>} manifest
 * @returns {Promise<Uint8Array[]>}
 */
AssetLoader.prototype.loadManifest = function (manifest) {
  var self = this;
  return Promise.all(
    manifest.map(function (entry) {
      return self.loadLVL(entry.url, entry.fsPath);
    })
  );
};

/**
 * Clear all cached assets from IndexedDB.
 * @returns {Promise<void>}
 */
AssetLoader.prototype.clearCache = function () {
  return AssetDB.clear().then(function () {
    console.log('[AssetLoader] Cache cleared.');
  });
};


// ============================================================
// Service Worker Registration
// ============================================================

(function () {
  'use strict';

  if ('serviceWorker' in navigator) {
    window.addEventListener('load', function () {
      navigator.serviceWorker.register('sw.js')
        .then(function (registration) {
          console.log('[SW] Registered with scope:', registration.scope);
        })
        .catch(function (err) {
          console.warn('[SW] Registration failed:', err);
        });
    });
  }
})();
