/**
 * @file social_overlay.js
 * @brief Compact social interaction panel — non-intrusive, real-time MMORPG.
 *
 * A lightweight bottom-docked panel (max 35vh) that does NOT freeze the
 * player and keeps the game world visible. Designed for frenetic hack-and-
 * slash PVP/PVE where interrupting gameplay is unacceptable.
 *
 * Industry pattern: action-MMORPG context panel (Diablo / Lost Ark style)
 *   - Compact action bar with direct buttons, no tabs
 *   - Inline chat section (togglable)
 *   - Semi-transparent so game world stays visible
 *   - Only NPC dialogue (modal_dialogue.c) triggers freeze
 *
 * Architecture:
 *   C interaction_bubble click  →  js_social_overlay_open()    → JS panel
 *   JS "Talk" button            →  c_open_dialogue_from_js()   → C freeze + modal
 *   JS chat send                →  c_send_ws_message()         → C client_send()
 *   JS panel close              →  c_social_overlay_did_close()→ C cleanup
 *   C incoming chat             →  js_social_overlay_receive_chat() → JS update
 *   C dialogue close callback   →  js_social_overlay_restore()     → JS re-show
 *
 * DOM is built once and reused (hide/show) to avoid expensive rebuilds.
 */

mergeInto(LibraryManager.library, {
  /* ================================================================
   * State — single reusable panel instance
   * ================================================================ */

  $SP: {
    el: null,
    backdrop: null,
    open: false,
    hidden: false /* true while dialogue modal is active (panel hidden but logically open) */,
    entityId: '',
    displayName: '',
    dlgItemId: '',
    interactFlags: 0,
    chatVisible: false,
    chatHistory: [],
    dom: {},
  },

  /* ================================================================
   * Style Constants — dark, semi-transparent, MMORPG-themed
   * ================================================================ */

  $SPS: {
    panelBg: 'rgba(8,8,18,0.88)',
    headerBg: 'rgba(12,12,28,0.94)',
    border: 'rgba(50,50,90,0.6)',
    nameCol: 'rgba(180,200,240,1)',
    textCol: 'rgba(200,200,215,0.92)',
    hintCol: 'rgba(100,100,130,0.65)',
    btnBg: 'rgba(25,30,55,0.88)',
    btnHover: 'rgba(40,50,90,0.95)',
    btnText: 'rgba(180,190,220,1)',
    btnActive: 'rgba(50,70,140,1)',
    chatBg: 'rgba(14,14,28,0.82)',
    chatMe: 'rgba(120,180,240,1)',
    chatThem: 'rgba(200,200,220,0.88)',
    inputBg: 'rgba(18,18,36,0.88)',
    inputBorder: 'rgba(50,50,88,0.6)',
    sendBg: 'rgba(35,80,160,0.9)',
    closeBg: 'rgba(180,80,80,0.8)',
    closeHover: 'rgba(220,100,100,1)',
    qcBg: 'rgba(22,26,48,0.85)',
  },

  $SP_QC: ['Hello!', 'GG', 'Help!', 'Trade?', 'Follow me', 'Thanks!'],

  /* ================================================================
   * DOM helpers
   * ================================================================ */

  $spEl__deps: [],
  $spEl: function (tag, css, parent) {
    var e = document.createElement(tag);
    if (css) {
      for (var k in css) {
        if (css.hasOwnProperty(k)) e.style[k] = css[k];
      }
    }
    if (parent) parent.appendChild(e);
    return e;
  },

  $spBtn__deps: ['$SPS', '$spEl'],
  $spBtn: function (label, onClick, parent, extra) {
    var S = SPS;
    var b = spEl(
      'button',
      {
        background: S.btnBg,
        color: S.btnText,
        border: 'none',
        borderRadius: '5px',
        padding: '7px 11px',
        fontSize: '13px',
        fontFamily: 'monospace',
        cursor: 'pointer',
        touchAction: 'manipulation',
        userSelect: 'none',
        WebkitUserSelect: 'none',
        transition: 'background 0.1s',
      },
      parent,
    );
    b.textContent = label;
    var baseBg = extra && extra.background ? extra.background : S.btnBg;
    b.onpointerenter = function () {
      b.style.background = S.btnHover;
    };
    b.onpointerleave = function () {
      b.style.background = baseBg;
    };
    b.onclick = function (ev) {
      ev.stopPropagation();
      onClick();
    };
    if (extra) {
      for (var k in extra) {
        if (extra.hasOwnProperty(k)) b.style[k] = extra[k];
      }
    }
    return b;
  },

  /* ================================================================
   * Build panel DOM — called once on first open, then reused
   * ================================================================ */

  $spBuild__deps: [
    '$SP',
    '$SPS',
    '$spEl',
    '$spBtn',
    '$SP_QC',
    '$spRenderChat',
    'js_social_overlay_close',
    'js_social_overlay_send_chat',
  ],
  $spBuild: function () {
    var P = SP,
      S = SPS,
      D = P.dom;
    if (P.el) return;

    /* animation keyframe */
    if (!document.getElementById('sp-kf')) {
      var sheet = document.createElement('style');
      sheet.id = 'sp-kf';
      sheet.textContent =
        '@keyframes spSlide{from{transform:translateY(12px);opacity:0}' + 'to{transform:translateY(0);opacity:1}}';
      document.head.appendChild(sheet);
    }

    /* ── Backdrop (tap outside = close) ── */
    P.backdrop = spEl(
      'div',
      {
        position: 'fixed',
        top: '0',
        left: '0',
        right: '0',
        bottom: '0',
        background: 'rgba(0,0,0,0.15)',
        zIndex: '9998',
        display: 'none',
      },
      document.body,
    );
    P.backdrop.onclick = function () {
      _js_social_overlay_close();
    };

    /* ── Root panel (bottom-docked, compact) ── */
    P.el = spEl(
      'div',
      {
        position: 'fixed',
        left: '0',
        right: '0',
        bottom: '0',
        zIndex: '9999',
        fontFamily: 'monospace',
        display: 'none',
        flexDirection: 'column',
        animation: 'spSlide 0.14s ease-out',
        maxHeight: '35vh',
        pointerEvents: 'auto',
      },
      document.body,
    );

    /* ── Header ── */
    var hdr = spEl(
      'div',
      {
        display: 'flex',
        alignItems: 'center',
        padding: '5px 10px',
        background: S.headerBg,
        borderTop: '1px solid ' + S.border,
        minHeight: '32px',
        flexShrink: '0',
      },
      P.el,
    );

    D.nameEl = spEl(
      'span',
      {
        flex: '1',
        fontSize: '14px',
        fontWeight: 'bold',
        color: S.nameCol,
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap',
      },
      hdr,
    );

    var xBtn = spEl(
      'button',
      {
        background: 'none',
        border: 'none',
        color: S.closeBg,
        fontSize: '16px',
        cursor: 'pointer',
        padding: '2px 6px',
        lineHeight: '1',
        fontFamily: 'monospace',
        touchAction: 'manipulation',
      },
      hdr,
    );
    xBtn.textContent = '\u2715';
    xBtn.onpointerenter = function () {
      xBtn.style.color = S.closeHover;
    };
    xBtn.onpointerleave = function () {
      xBtn.style.color = S.closeBg;
    };
    xBtn.onclick = function (ev) {
      ev.stopPropagation();
      _js_social_overlay_close();
    };

    /* ── Action row ── */
    D.actionRow = spEl(
      'div',
      {
        display: 'flex',
        gap: '4px',
        padding: '4px 10px',
        background: S.panelBg,
        borderTop: '1px solid ' + S.border,
        flexShrink: '0',
        flexWrap: 'wrap',
      },
      P.el,
    );

    /* ── Chat section (hidden until toggled) ── */
    D.chatSection = spEl(
      'div',
      {
        display: 'none',
        flexDirection: 'column',
        background: S.panelBg,
        padding: '0 10px 6px',
        overflow: 'hidden',
      },
      P.el,
    );

    /* message list */
    D.chatList = spEl(
      'div',
      {
        background: S.chatBg,
        borderRadius: '4px',
        padding: '5px',
        height: '80px',
        overflowY: 'auto',
        fontSize: '12px',
        lineHeight: '1.3',
        marginBottom: '5px',
        WebkitOverflowScrolling: 'touch',
      },
      D.chatSection,
    );

    /* input row */
    var iRow = spEl(
      'div',
      {
        display: 'flex',
        gap: '4px',
        marginBottom: '5px',
      },
      D.chatSection,
    );

    D.chatInput = spEl(
      'input',
      {
        flex: '1',
        background: S.inputBg,
        border: '1px solid ' + S.inputBorder,
        borderRadius: '4px',
        color: S.textCol,
        fontSize: '13px',
        fontFamily: 'monospace',
        padding: '5px 7px',
        outline: 'none',
      },
      iRow,
    );
    D.chatInput.type = 'text';
    D.chatInput.maxLength = 128;
    D.chatInput.placeholder = 'Say something\u2026';
    D.chatInput.autocomplete = 'off';

    D.chatInput.addEventListener('keydown', function (ev) {
      ev.stopPropagation();
      if (ev.key === 'Enter' && D.chatInput.value.trim()) {
        var ptr = allocateUTF8(D.chatInput.value.trim());
        _js_social_overlay_send_chat(ptr);
        D.chatInput.value = '';
      }
    });
    D.chatInput.addEventListener('focus', function (ev) {
      ev.stopPropagation();
    });

    spBtn(
      'Send',
      function () {
        if (D.chatInput.value.trim()) {
          var ptr = allocateUTF8(D.chatInput.value.trim());
          _js_social_overlay_send_chat(ptr);
          D.chatInput.value = '';
          D.chatInput.focus();
        }
      },
      iRow,
      { background: S.sendBg, flexShrink: '0', padding: '5px 9px' },
    );

    /* quick-chat row */
    var qcRow = spEl(
      'div',
      {
        display: 'flex',
        gap: '3px',
        flexWrap: 'wrap',
      },
      D.chatSection,
    );

    SP_QC.forEach(function (preset) {
      spBtn(
        preset,
        function () {
          var ptr = allocateUTF8(preset);
          _js_social_overlay_send_chat(ptr);
        },
        qcRow,
        { fontSize: '11px', padding: '3px 7px', background: S.qcBg },
      );
    });
  },

  /* ================================================================
   * Update content for the current entity
   * ================================================================ */

  $spPopulate__deps: ['$SP', '$SPS', '$spBtn', '$spRenderChat', '$spToggleChat'],
  $spPopulate: function () {
    var P = SP,
      D = P.dom,
      S = SPS;
    var flags = P.interactFlags;

    D.nameEl.textContent = P.displayName || P.entityId.substring(0, 10);

    /* Rebuild action buttons */
    D.actionRow.innerHTML = '';

    if (flags & 1) {
      spBtn(
        '\uD83D\uDCDC Talk',
        function () {
          if (!P.dlgItemId) return;
          /* Hide panel but stay logically open so clicks reach
           * the Raylib dialogue modal underneath. */
          P.hidden = true;
          P.el.style.display = 'none';
          P.backdrop.style.display = 'none';
          var ePtr = allocateUTF8(P.entityId);
          var iPtr = allocateUTF8(P.dlgItemId);
          Module._c_open_dialogue_from_js(ePtr, iPtr);
          _free(ePtr);
          _free(iPtr);
        },
        D.actionRow,
        { background: S.btnActive },
      );
    }

    if (flags & 2) {
      spBtn(
        '\uD83D\uDCAC Chat',
        function () {
          spToggleChat();
        },
        D.actionRow,
      );

      spBtn(
        '\uD83D\uDC65 Party',
        function () {
          console.log('[SP] Party invite \u2192 ' + P.entityId);
        },
        D.actionRow,
      );

      spBtn(
        '\u2795 Friend',
        function () {
          console.log('[SP] Friend request \u2192 ' + P.entityId);
        },
        D.actionRow,
      );
    }

    /* Auto-show chat for social entities (players), hide for NPCs */
    P.chatVisible = !!(flags & 2);
    D.chatSection.style.display = P.chatVisible ? 'flex' : 'none';

    spRenderChat();
  },

  /* ================================================================
   * Toggle chat section
   * ================================================================ */

  $spToggleChat__deps: ['$SP', '$spRenderChat'],
  $spToggleChat: function () {
    SP.chatVisible = !SP.chatVisible;
    SP.dom.chatSection.style.display = SP.chatVisible ? 'flex' : 'none';
    if (SP.chatVisible) {
      spRenderChat();
      SP.dom.chatInput.focus();
    }
  },

  /* ================================================================
   * Render chat messages
   * ================================================================ */

  $spRenderChat__deps: ['$SP', '$SPS', '$spEl'],
  $spRenderChat: function () {
    var P = SP,
      S = SPS,
      box = P.dom.chatList;
    if (!box) return;

    box.innerHTML = '';

    if (P.chatHistory.length === 0) {
      var hint = spEl(
        'div',
        {
          color: S.hintCol,
          fontSize: '11px',
          fontStyle: 'italic',
        },
        box,
      );
      hint.textContent = 'No messages yet\u2026';
      return;
    }

    /* Show last 50 messages max to keep DOM light */
    var history = P.chatHistory;
    var start = history.length > 50 ? history.length - 50 : 0;
    for (var i = start; i < history.length; i++) {
      var m = history[i];
      var row = spEl('div', { marginBottom: '2px' }, box);
      var who = spEl(
        'span',
        {
          color: m.isMe ? S.chatMe : S.chatThem,
          fontWeight: 'bold',
        },
        row,
      );
      who.textContent = m.sender + ': ';
      var txt = spEl('span', { color: S.textCol }, row);
      txt.textContent = m.text;
    }

    box.scrollTop = box.scrollHeight;
  },

  /* ================================================================
   * Public API — called from C via extern declarations
   * ================================================================ */

  js_social_overlay_open__deps: ['$SP', '$spBuild', '$spPopulate'],
  js_social_overlay_open: function (entity_id_ptr, display_name_ptr, dlg_item_id_ptr, interact_flags) {
    var P = SP;

    P.entityId = UTF8ToString(entity_id_ptr);
    P.displayName = UTF8ToString(display_name_ptr);
    P.dlgItemId = UTF8ToString(dlg_item_id_ptr);
    P.interactFlags = interact_flags;
    P.open = true;
    P.hidden = false;
    P.chatHistory = [];

    spBuild();
    spPopulate();

    P.el.style.display = 'flex';
    P.backdrop.style.display = 'block';
  },

  js_social_overlay_close__deps: ['$SP'],
  js_social_overlay_close: function () {
    var P = SP;
    if (!P.open) return;

    P.open = false;
    P.hidden = false;
    if (P.el) P.el.style.display = 'none';
    if (P.backdrop) P.backdrop.style.display = 'none';

    Module._c_social_overlay_did_close();
  },

  js_social_overlay_is_open__deps: ['$SP'],
  js_social_overlay_is_open: function () {
    /* Return 0 (not open) when the panel is hidden behind the dialogue
     * modal so that clicks pass through to modal_dialogue_handle_click. */
    return SP.open && !SP.hidden ? 1 : 0;
  },

  js_social_overlay_send_chat__deps: ['$SP', '$spRenderChat'],
  js_social_overlay_send_chat: function (text_ptr) {
    var P = SP;
    var text = UTF8ToString(text_ptr);
    _free(text_ptr);

    if (!text || !text.trim()) return;
    text = text.trim().substring(0, 128);

    P.chatHistory.push({ sender: 'You', text: text, isMe: true, ts: Date.now() });
    spRenderChat();

    var json = JSON.stringify({ type: 'chat', payload: { to: P.entityId, text: text } });
    var jPtr = allocateUTF8(json);
    Module._c_send_ws_message(jPtr);
    _free(jPtr);
  },

  js_social_overlay_receive_chat__deps: ['$SP', '$spRenderChat'],
  js_social_overlay_receive_chat: function (from_id_ptr, from_name_ptr, text_ptr) {
    var P = SP;
    var fromId = UTF8ToString(from_id_ptr);
    var fromName = UTF8ToString(from_name_ptr);
    var text = UTF8ToString(text_ptr);

    P.chatHistory.push({
      sender: fromName || fromId.substring(0, 8),
      text: text,
      isMe: false,
      ts: Date.now(),
    });

    if (P.open && P.chatVisible) spRenderChat();
  },

  js_social_overlay_restore__deps: ['$SP'],
  js_social_overlay_restore: function () {
    var P = SP;
    if (!P.el || !P.open) return;

    P.hidden = false;
    P.el.style.display = 'flex';
    if (P.backdrop) P.backdrop.style.display = 'block';
  },
});
