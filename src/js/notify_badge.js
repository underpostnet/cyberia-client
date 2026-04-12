/**
 * @file notify_badge.js
 * @brief General-purpose notification badge module for Cyberia Online.
 *
 * Provides a per-entity unread message counter and message store that
 * persists across overlay open/close cycles.  Designed for reuse:
 * currently handles player-to-player chat, but the same API works for
 * future bot messages, system alerts, quest notifications, etc.
 *
 * Architecture:
 *   C message_parser (chat msg)  →  js_notify_badge_push()       → stores message + increments count
 *   JS interact overlay open     →  js_notify_badge_read()        → returns stored messages, clears count
 *   C interaction_bubble draw    →  js_notify_badge_count()       → returns unread count for badge rendering
 *   JS interact overlay close    →  (count stays — only read() clears)
 *
 * The module is entirely client-side.  The server's only responsibility
 * is relaying chat messages — persistence is handled here in JS because
 * the client needs it for the lifetime of the session (not across
 * page reloads — that would require IndexedDB, out of scope).
 */

mergeInto(LibraryManager.library, {
  /* ================================================================
   * State — per-entity message store
   * ================================================================ */

  /**
   * NB.store = {
   *   [entityId]: {
   *     unread: number,
   *     messages: [ { sender, text, ts } ... ]
   *   }
   * }
   *
   * MAX_MESSAGES_PER_ENTITY caps stored messages to prevent memory bloat.
   */
  $NB: {
    store: {},
    MAX_MESSAGES: 100,
  },

  /* ================================================================
   * Internal helpers
   * ================================================================ */

  $nbEnsure__deps: ['$NB'],
  $nbEnsure: function (entityId) {
    if (!NB.store[entityId]) {
      NB.store[entityId] = { unread: 0, messages: [] };
    }
    return NB.store[entityId];
  },

  /* ================================================================
   * Public API — called from C and JS
   * ================================================================ */

  /**
   * Push a new message into the store for a given entity.
   * Increments the unread counter.
   * Called from C when a chat message arrives (message_parser.c).
   */
  js_notify_badge_push__deps: ['$NB', '$nbEnsure'],
  js_notify_badge_push: function (entity_id_ptr, sender_ptr, text_ptr) {
    var entityId = UTF8ToString(entity_id_ptr);
    var sender = UTF8ToString(sender_ptr);
    var text = UTF8ToString(text_ptr);

    var entry = nbEnsure(entityId);
    entry.messages.push({ sender: sender, text: text, ts: Date.now() });
    entry.unread++;

    /* Cap stored messages */
    if (entry.messages.length > NB.MAX_MESSAGES) {
      entry.messages = entry.messages.slice(-NB.MAX_MESSAGES);
    }
  },

  /**
   * Return the number of unread messages for an entity.
   * Called from C to render badge on interaction bubbles.
   */
  js_notify_badge_count__deps: ['$NB'],
  js_notify_badge_count: function (entity_id_ptr) {
    var entityId = UTF8ToString(entity_id_ptr);
    var entry = NB.store[entityId];
    return entry ? entry.unread : 0;
  },

  /**
   * Mark all messages for an entity as read (clear unread counter).
   * Called from JS when the chat tab is opened for that entity.
   */
  js_notify_badge_read__deps: ['$NB'],
  js_notify_badge_read: function (entity_id_ptr) {
    var entityId = UTF8ToString(entity_id_ptr);
    var entry = NB.store[entityId];
    if (entry) entry.unread = 0;
  },

  /**
   * Get stored messages for an entity as a JSON string.
   * Returns a pointer to a UTF8 string (caller must _free).
   * Called from JS to populate chat history when opening an entity's chat.
   */
  js_notify_badge_get_messages__deps: ['$NB'],
  js_notify_badge_get_messages: function (entity_id_ptr) {
    var entityId = UTF8ToString(entity_id_ptr);
    var entry = NB.store[entityId];
    var msgs = entry ? entry.messages : [];
    var json = JSON.stringify(msgs);
    return allocateUTF8(json);
  },
});
