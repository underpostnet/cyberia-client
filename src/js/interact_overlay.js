/**
 * @file interact_overlay.js
 * @brief General-purpose interact overlay — non-intrusive, real-time MMORPG.
 *
 * Bottom-docked panel for entity interactions: Dialog, Chat, Party, etc.
 * Uses the browser DOM for keyboard input and text rendering — areas where
 * the web platform has clear advantages over Raylib.
 *
 * Architecture:
 *   C interaction_bubble click  →  js_interact_overlay_open()    → JS panel
 *   JS "Dialog" button          →  c_open_dialogue_from_js()     → C freeze + modal
 *   JS chat send                →  c_send_ws_message()           → C client_send()
 *   JS panel close              →  c_interact_overlay_did_close()→ C cleanup
 *   C incoming chat             →  js_interact_overlay_receive_chat() → JS update
 *   C dialogue close callback   →  js_interact_overlay_restore()     → JS re-show
 *
 * DOM is built once and reused (hide/show) to avoid expensive rebuilds.
 */

mergeInto(LibraryManager.library, {
  /* ================================================================
   * State — single reusable panel instance
   * ================================================================ */

  $IP: {
    el: null,
    backdrop: null,
    open: false,
    hidden: false,
    entityId: '',
    displayName: '',
    dlgItemId: '',
    interactFlags: 0,
    isPlayer: false,
    activeTab: 'chat',
    chatHistory: [],
    dom: {},
  },

  /* ================================================================
   * Style Constants
   * ================================================================ */

  $IPS: {
    panelBg: 'rgba(8,8,18,0.92)',
    headerBg: 'rgba(12,12,28,0.96)',
    border: 'rgba(50,50,90,0.6)',
    nameCol: 'rgba(180,200,240,1)',
    textCol: 'rgba(200,200,215,0.92)',
    hintCol: 'rgba(100,100,130,0.65)',
    tabBg: 'rgba(16,16,32,0.92)',
    tabActive: 'rgba(8,8,18,0.92)',
    tabInactive: 'rgba(22,24,44,0.88)',
    tabTextActive: 'rgba(220,230,255,1)',
    tabTextInactive: 'rgba(130,140,170,0.85)',
    btnBg: 'rgba(25,30,55,0.88)',
    btnHover: 'rgba(40,50,90,0.95)',
    btnText: 'rgba(180,190,220,1)',
    btnAccent: 'rgba(50,70,140,1)',
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

  $IP_QC: ['Hello!', 'GG', 'Help!', 'Trade?', 'Follow me', 'Thanks!'],

  /* ================================================================
   * DOM helpers
   * ================================================================ */

  $ipEl__deps: [],
  $ipEl: function (tag, css, parent) {
    var e = document.createElement(tag);
    if (css) {
      for (var k in css) {
        if (css.hasOwnProperty(k)) e.style[k] = css[k];
      }
    }
    if (parent) parent.appendChild(e);
    return e;
  },

  $ipBtn__deps: ['$IPS', '$ipEl'],
  $ipBtn: function (label, onClick, parent, extra) {
    var S = IPS;
    var b = ipEl(
      'button',
      {
        background: S.btnBg,
        color: S.btnText,
        border: 'none',
        borderRadius: '5px',
        padding: '8px 14px',
        fontSize: '15px',
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

  $ipBuild__deps: [
    '$IP',
    '$IPS',
    '$ipEl',
    '$ipBtn',
    '$IP_QC',
    '$ipRenderChat',
    '$ipSwitchTab',
    'js_interact_overlay_close',
    'js_interact_overlay_send_chat',
  ],
  $ipBuild: function () {
    var P = IP,
      S = IPS,
      D = P.dom;
    if (P.el) return;

    /* animation keyframe */
    if (!document.getElementById('ip-kf')) {
      var sheet = document.createElement('style');
      sheet.id = 'ip-kf';
      sheet.textContent =
        '@keyframes ipSlide{from{transform:translateY(12px);opacity:0}' + 'to{transform:translateY(0);opacity:1}}';
      document.head.appendChild(sheet);
    }

    /* ── Backdrop (tap outside = close) ── */
    P.backdrop = ipEl(
      'div',
      {
        position: 'fixed',
        top: '0',
        left: '0',
        right: '0',
        bottom: '0',
        background: 'rgba(0,0,0,0.18)',
        zIndex: '9998',
        display: 'none',
      },
      document.body,
    );
    P.backdrop.onclick = function () {
      _js_interact_overlay_close();
    };

    /* ── Root panel (bottom-docked) ── */
    P.el = ipEl(
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
        animation: 'ipSlide 0.14s ease-out',
        maxHeight: '42vh',
        pointerEvents: 'auto',
      },
      document.body,
    );

    /* ── Header with entity name + close ── */
    var hdr = ipEl(
      'div',
      {
        display: 'flex',
        alignItems: 'center',
        padding: '6px 12px',
        background: S.headerBg,
        borderTop: '1px solid ' + S.border,
        minHeight: '36px',
        flexShrink: '0',
      },
      P.el,
    );

    D.nameEl = ipEl(
      'span',
      {
        flex: '1',
        fontSize: '16px',
        fontWeight: 'bold',
        color: S.nameCol,
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap',
      },
      hdr,
    );

    var xBtn = ipEl(
      'button',
      {
        background: 'none',
        border: 'none',
        color: S.closeBg,
        fontSize: '18px',
        cursor: 'pointer',
        padding: '2px 8px',
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
      _js_interact_overlay_close();
    };

    /* ── Tab bar ── */
    D.tabBar = ipEl(
      'div',
      {
        display: 'flex',
        background: S.tabBg,
        flexShrink: '0',
        borderBottom: 'none',
      },
      P.el,
    );

    /* ── Tab body — seamless with active tab, consistent height ── */
    D.tabBody = ipEl(
      'div',
      {
        height: '180px',
        background: S.tabActive,
        padding: '10px 12px',
        overflowY: 'auto',
        overflowX: 'hidden',
        WebkitOverflowScrolling: 'touch',
      },
      P.el,
    );
  },

  /* ================================================================
   * Tab switching — no minimize, always shows content
   * ================================================================ */

  $ipSwitchTab__deps: [
    '$IP',
    '$IPS',
    '$NB',
    '$ipEl',
    '$ipBtn',
    '$ipRenderChat',
    '$ipBuildDialogTab',
    '$ipBuildChatTab',
    '$ipBuildActionsTab',
  ],
  $ipSwitchTab: function (tabId) {
    var P = IP,
      S = IPS,
      D = P.dom;
    P.activeTab = tabId;

    /* Clear badge when user views the chat tab. */
    if (tabId === 'chat' && NB.store[P.entityId]) {
      NB.store[P.entityId].unread = 0;
    }

    /* Update tab button styles */
    var btns = D.tabBar.querySelectorAll('button');
    for (var i = 0; i < btns.length; i++) {
      var b = btns[i];
      var isActive = b.dataset.tabId === tabId;
      b.style.background = isActive ? S.tabActive : S.tabInactive;
      b.style.color = isActive ? S.tabTextActive : S.tabTextInactive;
      b.style.borderBottom = isActive ? 'none' : '1px solid ' + S.border;
      b.style.fontWeight = isActive ? 'bold' : 'normal';
    }

    /* Rebuild body for the selected tab */
    D.tabBody.innerHTML = '';

    switch (tabId) {
      case 'dialog':
        ipBuildDialogTab(D.tabBody);
        break;
      case 'chat':
        ipBuildChatTab(D.tabBody);
        break;
      case 'actions':
        ipBuildActionsTab(D.tabBody);
        break;
    }
  },

  /* ================================================================
   * Dialog tab — delegates to C modal_dialogue
   * ================================================================ */

  $ipBuildDialogTab__deps: ['$IP', '$IPS', '$ipEl', '$ipBtn'],
  $ipBuildDialogTab: function (container) {
    var P = IP,
      S = IPS;

    if (!P.dlgItemId) {
      var hint = ipEl('div', { color: S.hintCol, fontSize: '14px', padding: '16px 0' }, container);
      hint.textContent = 'No dialog available for this entity.';
      return;
    }

    ipBtn(
      '\uD83D\uDCDC Dialog',
      function () {
        if (!P.dlgItemId) return;
        P.hidden = true;
        P.el.style.display = 'none';
        P.backdrop.style.display = 'none';
        var ePtr = allocateUTF8(P.entityId);
        var iPtr = allocateUTF8(P.dlgItemId);
        Module._c_open_dialogue_from_js(ePtr, iPtr);
        _free(ePtr);
        _free(iPtr);
      },
      container,
      { background: S.btnAccent, fontSize: '16px', padding: '10px 20px' },
    );
  },

  /* ================================================================
   * Chat tab — messages + input + quick-chat
   * ================================================================ */

  $ipBuildChatTab__deps: ['$IP', '$IPS', '$IP_QC', '$ipEl', '$ipBtn', '$ipRenderChat', 'js_interact_overlay_send_chat'],
  $ipBuildChatTab: function (container) {
    var P = IP,
      S = IPS,
      D = P.dom;

    /* message list */
    D.chatList = ipEl(
      'div',
      {
        background: S.chatBg,
        borderRadius: '4px',
        padding: '6px',
        height: '90px',
        overflowY: 'auto',
        fontSize: '13px',
        lineHeight: '1.35',
        marginBottom: '6px',
        WebkitOverflowScrolling: 'touch',
      },
      container,
    );

    ipRenderChat();

    /* input row */
    var iRow = ipEl('div', { display: 'flex', gap: '4px', marginBottom: '6px' }, container);

    D.chatInput = ipEl(
      'input',
      {
        flex: '1',
        background: S.inputBg,
        border: '1px solid ' + S.inputBorder,
        borderRadius: '4px',
        color: S.textCol,
        fontSize: '14px',
        fontFamily: 'monospace',
        padding: '6px 8px',
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
        _js_interact_overlay_send_chat(ptr);
        D.chatInput.value = '';
      }
    });
    D.chatInput.addEventListener('focus', function (ev) {
      ev.stopPropagation();
    });

    ipBtn(
      'Send',
      function () {
        if (D.chatInput.value.trim()) {
          var ptr = allocateUTF8(D.chatInput.value.trim());
          _js_interact_overlay_send_chat(ptr);
          D.chatInput.value = '';
          D.chatInput.focus();
        }
      },
      iRow,
      { background: S.sendBg, flexShrink: '0', padding: '6px 10px' },
    );

    /* quick-chat row */
    var qcRow = ipEl('div', { display: 'flex', gap: '4px', flexWrap: 'wrap' }, container);

    IP_QC.forEach(function (preset) {
      ipBtn(
        preset,
        function () {
          var ptr = allocateUTF8(preset);
          _js_interact_overlay_send_chat(ptr);
        },
        qcRow,
        { fontSize: '12px', padding: '4px 8px', background: S.qcBg },
      );
    });
  },

  /* ================================================================
   * Actions tab — party, friend, etc.
   * ================================================================ */

  $ipBuildActionsTab__deps: ['$IP', '$IPS', '$ipEl', '$ipBtn'],
  $ipBuildActionsTab: function (container) {
    var P = IP,
      S = IPS;

    var row = ipEl('div', { display: 'flex', gap: '6px', flexWrap: 'wrap' }, container);

    ipBtn(
      '\uD83D\uDC65 Party',
      function () {
        console.log('[IP] Party invite \u2192 ' + P.entityId);
      },
      row,
    );

    ipBtn(
      '\u2795 Friend',
      function () {
        console.log('[IP] Friend request \u2192 ' + P.entityId);
      },
      row,
    );

    ipBtn(
      '\uD83D\uDCAC Whisper',
      function () {
        console.log('[IP] Whisper \u2192 ' + P.entityId);
      },
      row,
    );
  },

  /* ================================================================
   * Render chat messages
   * ================================================================ */

  $ipRenderChat__deps: ['$IP', '$IPS', '$ipEl'],
  $ipRenderChat: function () {
    var P = IP,
      S = IPS,
      box = P.dom.chatList;
    if (!box) return;

    box.innerHTML = '';

    if (P.chatHistory.length === 0) {
      var hint = ipEl('div', { color: S.hintCol, fontSize: '12px', fontStyle: 'italic' }, box);
      hint.textContent = 'No messages yet\u2026';
      return;
    }

    var history = P.chatHistory;
    var start = history.length > 50 ? history.length - 50 : 0;
    for (var i = start; i < history.length; i++) {
      var m = history[i];
      var row = ipEl('div', { marginBottom: '2px' }, box);
      var who = ipEl('span', { color: m.isMe ? S.chatMe : S.chatThem, fontWeight: 'bold' }, row);
      who.textContent = m.sender + ': ';
      var txt = ipEl('span', { color: S.textCol }, row);
      txt.textContent = m.text;
    }

    box.scrollTop = box.scrollHeight;
  },

  /* ================================================================
   * Populate — rebuild tabs and content for the current entity
   * ================================================================ */

  $ipPopulate__deps: ['$IP', '$IPS', '$ipEl', '$ipSwitchTab'],
  $ipPopulate: function () {
    var P = IP,
      D = P.dom,
      S = IPS;
    var flags = P.interactFlags;

    /* Header: players show their websocket ID, bots show display name. */
    D.nameEl.textContent = P.isPlayer ? P.entityId : P.displayName || P.entityId.substring(0, 12);

    /* Rebuild tab bar */
    D.tabBar.innerHTML = '';

    var tabs = [];
    if (flags & 1) tabs.push({ id: 'dialog', label: 'Dialog' });
    tabs.push({ id: 'chat', label: 'Chat' });
    tabs.push({ id: 'actions', label: 'Actions' });

    tabs.forEach(function (tab) {
      var btn = ipEl(
        'button',
        {
          flex: '1',
          background: S.tabInactive,
          color: S.tabTextInactive,
          border: 'none',
          borderBottom: '1px solid ' + S.border,
          fontSize: '14px',
          fontFamily: 'monospace',
          padding: '8px 0',
          cursor: 'pointer',
          touchAction: 'manipulation',
          transition: 'background 0.1s, color 0.1s',
        },
        D.tabBar,
      );
      btn.textContent = tab.label;
      btn.dataset.tabId = tab.id;
      btn.onclick = function (ev) {
        ev.stopPropagation();
        ipSwitchTab(tab.id);
      };
    });

    /* Default to first tab */
    var defaultTab = tabs[0].id;
    P.activeTab = defaultTab;
    ipSwitchTab(defaultTab);
  },

  /* ================================================================
   * Public API — called from C via extern declarations
   * ================================================================ */

  js_interact_overlay_open__deps: ['$IP', '$NB', '$nbEnsure', '$ipBuild', '$ipPopulate'],
  js_interact_overlay_open: function (entity_id_ptr, display_name_ptr, dlg_item_id_ptr, interact_flags, is_player) {
    var P = IP;

    P.entityId = UTF8ToString(entity_id_ptr);
    P.displayName = UTF8ToString(display_name_ptr);
    P.dlgItemId = UTF8ToString(dlg_item_id_ptr);
    P.interactFlags = interact_flags;
    P.isPlayer = !!is_player;
    P.open = true;
    P.hidden = false;

    /* Load persisted messages from the badge store so chat history
     * survives across overlay open/close cycles. */
    var entry = nbEnsure(P.entityId);
    P.chatHistory = entry.messages.map(function (m) {
      return { sender: m.sender, text: m.text, isMe: false, ts: m.ts };
    });
    /* Mark as read — the user is now viewing this entity's chat. */
    entry.unread = 0;

    ipBuild();
    ipPopulate();

    P.el.style.display = 'flex';
    P.backdrop.style.display = 'block';
  },

  js_interact_overlay_close__deps: ['$IP'],
  js_interact_overlay_close: function () {
    var P = IP;
    if (!P.open) return;

    P.open = false;
    P.hidden = false;
    if (P.el) P.el.style.display = 'none';
    if (P.backdrop) P.backdrop.style.display = 'none';

    Module._c_interact_overlay_did_close();
  },

  js_interact_overlay_is_open__deps: ['$IP'],
  js_interact_overlay_is_open: function () {
    return IP.open && !IP.hidden ? 1 : 0;
  },

  js_interact_overlay_send_chat__deps: ['$IP', '$NB', '$nbEnsure', '$ipRenderChat'],
  js_interact_overlay_send_chat: function (text_ptr) {
    var P = IP;
    var text = UTF8ToString(text_ptr);
    _free(text_ptr);

    if (!text || !text.trim()) return;
    text = text.trim().substring(0, 128);

    var msg = { sender: 'You', text: text, isMe: true, ts: Date.now() };
    P.chatHistory.push(msg);
    if (P.activeTab === 'chat') ipRenderChat();

    /* Persist in badge store so history survives overlay close. */
    var entry = nbEnsure(P.entityId);
    entry.messages.push({ sender: 'You', text: text, ts: Date.now() });
    if (entry.messages.length > NB.MAX_MESSAGES) entry.messages = entry.messages.slice(-NB.MAX_MESSAGES);

    var json = JSON.stringify({ type: 'chat', payload: { to: P.entityId, text: text } });
    var jPtr = allocateUTF8(json);
    Module._c_send_ws_message(jPtr);
    _free(jPtr);
  },

  js_interact_overlay_receive_chat__deps: ['$IP', '$NB', '$ipRenderChat'],
  js_interact_overlay_receive_chat: function (from_id_ptr, from_name_ptr, text_ptr) {
    var P = IP;
    var fromId = UTF8ToString(from_id_ptr);
    var fromName = UTF8ToString(from_name_ptr);
    var text = UTF8ToString(text_ptr);

    /* Only update the live overlay if it's open for THIS entity. */
    if (P.open && P.entityId === fromId) {
      P.chatHistory.push({
        sender: fromName || fromId.substring(0, 8),
        text: text,
        isMe: false,
        ts: Date.now(),
      });

      /* Clear unread since the user is viewing this entity's chat. */
      if (NB.store[fromId]) NB.store[fromId].unread = 0;

      if (P.activeTab === 'chat') ipRenderChat();
    }
  },

  js_interact_overlay_restore__deps: ['$IP'],
  js_interact_overlay_restore: function () {
    var P = IP;
    if (!P.el || !P.open) return;

    P.hidden = false;
    P.el.style.display = 'flex';
    if (P.backdrop) P.backdrop.style.display = 'block';
  },
});
