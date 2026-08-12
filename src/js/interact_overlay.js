/**
 * @file interact_overlay.js
 * @brief General-purpose interact overlay — non-intrusive, real-time MMORPG.
 *
 * Panel for entity interactions: Chat, Integration. Uses the browser DOM for
 * keyboard input and text rendering — areas where the web platform has clear
 * advantages over Raylib.
 *
 * The panel is translucent in every layout — the world keeps reading through
 * it, so a chat window never hides what the grid is doing. Two layouts, chosen
 * by C and pushed through js_interact_overlay_set_anchor:
 *   full-bleed — covers the viewport (mobile, small windows). No "outside"
 *                area, so only the Back button closes it.
 *   anchored   — above the desktop breakpoint, a compact card on the screen
 *                rect C resolved over the target entity, the rest of the world
 *                left clear. An outside tap dismisses, matching the C panel
 *                modals.
 *
 * Tabs:
 *   Chat        — 1:1 chat input + history + quick-chat presets.
 *   Integration — OL stack composite preview + active item list; surface for
 *                 external integrations.
 *
 * The C interaction modal opens this overlay directly on the requested tab.
 *
 * Architecture:
 *   C interaction_bubble click  →  js_interact_overlay_open()         → JS panel
 *   JS chat send                →  c_send_chat_binary()               → C binary uplink
 *   C incoming chat             →  js_interact_overlay_receive_chat() → JS update
 *
 * DOM is built once and reused (hide/show) to avoid expensive rebuilds.
 */

mergeInto(LibraryManager.library, {
  /* ================================================================
   * Public Content Authority (engine-cyberia) origin — the client-visible
   * address every client→engine content request resolves against. Seeded from
   * C (js_init_engine_api) with the compile-time public origin, then overridden
   * at runtime by the server-forwarded public URL. It is NEVER the internal
   * cluster/service address (that stays server-side only). Read here for:
   *   - asset previews:      GET {api_base_url}/assets/{type}/{itemId}/08/0.png
   *   - integration links:   {api_base_url}/object-layer-engine-viewer?...
   *
   * The <img src> asset fetch is the ONLY sanctioned JS-side fetch surface, and
   * only because the browser <img> element fetches/decodes/renders natively. For
   * anything that pulls bytes into wasm (REST JSON, blobs), use the native engine
   * fetch API (network/engine_client.h: fetch_request_start) — no JS bridge.
   * ================================================================ */

  $FetchState: {
    api_base_url: 'https://www.cyberiaonline.com',
  },

  js_init_engine_api__deps: ['$FetchState'],
  js_init_engine_api: function (api_base_url_ptr) {
    var api_base_url = UTF8ToString(api_base_url_ptr);

    if (api_base_url) {
      FetchState.api_base_url = api_base_url;
    }

    console.log('[API] Engine API base URL set to: ' + FetchState.api_base_url);
  },

  /* ================================================================
   * State — single reusable panel instance
   * ================================================================ */

  $IP: {
    el: null,
    open: false,
    hidden: false,
    entityId: '',
    displayName: '',
    dlgItemId: '',
    interactFlags: 0,
    isPlayer: false,
    isSelf: false,
    olStack: [],
    borderColor: 'rgba(70,70,120,0.78)',
    activeTab: 'chat',
    initialTab: 'chat',
    chatHistory: [],
    /* Screen rect (CSS px) pushed from C for the anchored desktop layout, or
     * null for the full-bleed panel. See js_interact_overlay_set_anchor. */
    anchor: null,
    dom: {},
  },

  /* ================================================================
   * Style Constants
   * ================================================================ */

  $IPS: {
    /* The panel's single tint, in every layout. Light enough that the grid
     * keeps reading through it — a chat window should never stop you seeing
     * what the world is doing. It only reads as translucent while it is the
     * ONLY full-surface fill: the header and message list below are lifts
     * layered over it, not second sheets, because stacked opaque fills
     * compound (0.92 over 0.92 over 0.82 is 0.999 — solid black). */
    panelBg: 'rgba(8,8,18,0.62)',
    headerBg: 'rgba(12,12,28,0.45)',
    border: 'rgba(50,50,90,0.6)',
    nameCol: 'rgba(180,200,240,1)',
    textCol: 'rgba(200,200,215,0.92)',
    hintCol: 'rgba(100,100,130,0.65)',
    tabBg: 'rgba(16,16,32,0.92)',
    tabInactive: 'rgba(22,24,44,0.88)',
    tabTextActive: 'rgba(220,230,255,1)',
    tabTextInactive: 'rgba(130,140,170,0.85)',
    btnBg: 'rgba(25,30,55,0.88)',
    btnHover: 'rgba(40,50,90,0.95)',
    btnText: 'rgba(180,190,220,1)',
    btnAccent: 'rgba(50,70,140,1)',
    chatBg: 'rgba(14,14,28,0.35)',
    chatMe: 'rgba(120,180,240,1)',
    chatThem: 'rgba(200,200,220,0.88)',
    inputBg: 'rgba(18,18,36,0.88)',
    inputBorder: 'rgba(50,50,88,0.6)',
    sendBg: 'rgba(35,80,160,0.9)',
    closeBg: 'rgba(180,80,80,0.8)',
    closeHover: 'rgba(220,100,100,1)',
    qcBg: 'rgba(22,26,48,0.85)',
    /* Anchored card only: it floats over the live world, so it carries its
     * own edge instead of running to the viewport bounds. */
    cardRadius: '8px',
    cardShadow: '0 10px 28px rgba(0,0,0,0.55)',
    /* Margin the anchored card keeps from the viewport edges (CSS px). */
    anchorPad: 8,
  },

  $IP_QC: ['Hello!', 'GG', 'Help!', 'Trade?', 'Follow me', 'Thanks!'],

  /* ================================================================
   * Per-entity chat history for the overlay's Chat tab (display only;
   * the unread badge is owned by the C interaction modal).
   * ================================================================ */

  $IPStore: {
    store: {},
    MAX_MESSAGES: 100,
  },

  $ipStoreEnsure__deps: ['$IPStore'],
  $ipStoreEnsure: function (entityId) {
    if (!IPStore.store[entityId]) {
      IPStore.store[entityId] = { messages: [] };
    }
    return IPStore.store[entityId];
  },

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
   * Layout — full-bleed panel, or a card anchored over the entity
   *
   * Mirrors ui/modal_anchor.c: above the desktop breakpoint C resolves a
   * screen rect over the target entity and pushes it here, and the panel
   * shrinks to a card on that rect instead of covering the viewport. C owns
   * the breakpoint and the rect so both surfaces place panels the same way;
   * this only applies what it is handed, re-clamping to the viewport because a
   * resize can outdate a rect C is no longer recomputing.
   * ================================================================ */

  $ipApplyLayout__deps: ['$IP', '$IPS'],
  $ipApplyLayout: function () {
    var P = IP,
      S = IPS,
      D = P.dom;
    if (!P.el || !D.card) return;

    /* The layer never paints — it only positions the card and catches taps.
     * The card owns the one translucent tint in both layouts, so only its
     * geometry and edge treatment change below. */
    if (!P.anchor) {
      /* Full-bleed: the card covers the viewport. */
      D.card.style.left = '0';
      D.card.style.top = '0';
      D.card.style.width = '100%';
      D.card.style.height = '100%';
      D.card.style.border = 'none';
      D.card.style.borderRadius = '0';
      D.card.style.boxShadow = 'none';
      return;
    }

    /* Anchored: card fitted into the viewport. Shrink before clamping so an
     * oversized rect lands inside rather than hanging off an edge — the same
     * order as fit_into() in modal_anchor.c. */
    var pad = S.anchorPad;
    var vw = window.innerWidth,
      vh = window.innerHeight;
    var w = Math.min(P.anchor.w, vw - 2 * pad);
    var h = Math.min(P.anchor.h, vh - 2 * pad);
    var x = Math.max(pad, Math.min(P.anchor.x, vw - pad - w));
    var y = Math.max(pad, Math.min(P.anchor.y, vh - pad - h));

    D.card.style.left = x + 'px';
    D.card.style.top = y + 'px';
    D.card.style.width = w + 'px';
    D.card.style.height = h + 'px';
    D.card.style.border = '1px solid ' + P.borderColor;
    D.card.style.borderRadius = S.cardRadius;
    D.card.style.boxShadow = S.cardShadow;
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
    '$ipApplyLayout',
    '$FetchState',
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

    /* ── Root layer ──
     * Never painted: it positions the card and catches pointer events, so a
     * tap beside an anchored card dismisses instead of reaching the canvas —
     * the same gesture the C panel modals answer to. */
    P.el = ipEl(
      'div',
      {
        position: 'fixed',
        top: '0',
        left: '0',
        right: '0',
        bottom: '0',
        zIndex: '9999',
        fontFamily: 'monospace',
        display: 'none',
        pointerEvents: 'auto',
        animation: 'ipSlide 0.14s ease-out',
      },
      document.body,
    );
    P.el.onclick = function (ev) {
      /* Only a hit on the layer itself — the card covers it entirely in the
       * full-bleed layout, so that layout never sees this. */
      if (ev.target !== P.el) return;
      _js_interact_overlay_close();
    };

    /* ── Card: header + body, and the panel's one translucent fill ── */
    D.card = ipEl(
      'div',
      {
        position: 'absolute',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        background: S.panelBg,
      },
      P.el,
    );

    /* A resize invalidates a rect resolved against the old viewport. */
    window.addEventListener('resize', function () {
      if (P.open) ipApplyLayout();
    });

    /* ── Header with entity name + close ── */
    var hdr = ipEl(
      'div',
      {
        display: 'flex',
        alignItems: 'center',
        padding: '10px 14px',
        background: S.headerBg,
        borderBottom: '1px solid ' + S.border,
        minHeight: '40px',
        flexShrink: '0',
      },
      D.card,
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

    var backBtn = ipEl(
      'button',
      {
        background: 'rgba(40,50,90,0.85)',
        border: '1px solid ' + S.border,
        borderRadius: '6px',
        cursor: 'pointer',
        padding: '6px',
        width: '32px',
        height: '32px',
        boxSizing: 'border-box',
        lineHeight: '0',
        touchAction: 'manipulation',
        transition: 'background 0.1s',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      },
      hdr,
    );
    backBtn.title = 'Back';
    backBtn.setAttribute('aria-label', 'Back');
    var backIcon = ipEl(
      'img',
      {
        width: '18px',
        height: '18px',
        display: 'block',
        imageRendering: 'pixelated',
        pointerEvents: 'none',
      },
      backBtn,
    );
    backIcon.src = FetchState.api_base_url + '/assets/ui-icons/close-yellow.png';
    backIcon.alt = '';
    backBtn.onpointerenter = function () {
      backBtn.style.background = 'rgba(60,75,140,0.95)';
    };
    backBtn.onpointerleave = function () {
      backBtn.style.background = 'rgba(40,50,90,0.85)';
    };
    backBtn.onclick = function (ev) {
      ev.stopPropagation();
      _js_interact_overlay_close();
    };

    /* Tab bar is not needed — only the requested tab is shown. */
    D.tabBar = null;

    /* ── Tab body — flex-fill, seamless with active tab ── */
    D.tabBody = ipEl(
      'div',
      {
        flex: '1',
        padding: '12px 14px',
        overflowY: 'auto',
        overflowX: 'hidden',
        WebkitOverflowScrolling: 'touch',
        display: 'flex',
        flexDirection: 'column',
      },
      D.card,
    );
  },

  /* ================================================================
   * Tab switching — single-tab mode, no tab bar shown
   * Only the tab requested by the C modal is rendered.
   * ================================================================ */

  $ipSwitchTab__deps: [
    '$IP',
    '$IPS',
    '$IPStore',
    '$ipEl',
    '$ipBtn',
    '$ipRenderChat',
    '$ipBuildIntegrationTab',
    '$ipBuildChatTab',
  ],
  $ipSwitchTab: function (tabId) {
    var P = IP,
      D = P.dom;

    P.activeTab = tabId;

    /* Rebuild body for the selected tab */
    D.tabBody.innerHTML = '';

    switch (tabId) {
      case 'integration':
        ipBuildIntegrationTab(D.tabBody);
        break;
      case 'chat':
        ipBuildChatTab(D.tabBody);
        break;
    }
  },

  /* ================================================================
   * OL Stack Preview — renders entity's active OL stack as composited
   * images using the public static asset directory convention:
   *   {api_base_url}/assets/{type}/{itemId}/08/0.png
   * Direction code 08 = down_idle (standard icon direction).
   * ================================================================ */

  $ipBuildOlStackPreview__deps: ['$IP', '$IPS', '$ipEl', '$FetchState'],
  $ipBuildOlStackPreview: function (parent, size) {
    var P = IP,
      S = IPS;
    var stack = P.olStack;
    if (!stack || stack.length === 0) return null;

    var wrap = ipEl(
      'div',
      {
        position: 'relative',
        width: size + 'px',
        height: size + 'px',
        flexShrink: '0',
        borderRadius: '6px',
        overflow: 'hidden',
        background: 'rgba(20,20,40,0.6)',
        border: P.isSelf ? '2px solid ' + P.borderColor : '1px solid ' + S.border,
      },
      parent,
    );

    var base = FetchState.api_base_url;
    for (var i = 0; i < stack.length; i++) {
      var ol = stack[i];
      if (!ol.type) continue;
      var img = ipEl(
        'img',
        {
          position: 'absolute',
          top: '0',
          left: '0',
          width: '100%',
          height: '100%',
          imageRendering: 'pixelated',
          pointerEvents: 'none',
        },
        wrap,
      );
      img.src = base + '/assets/' + ol.type + '/' + ol.itemId + '/08/0.png';
      img.alt = '';
      img.onerror = function () {
        this.style.display = 'none';
      };
    }

    return wrap;
  },

  /* ================================================================
   * Integration tab — external integrations surface. Two columns:
   *   Left:  composited OL stack preview (all active layers stacked)
   *   Right: active item list (read-only)
   * ================================================================ */

  $ipBuildIntegrationTab__deps: ['$IP', '$IPS', '$ipEl', '$ipBtn', '$ipBuildOlStackPreview', '$FetchState'],
  $ipBuildIntegrationTab: function (container) {
    var P = IP,
      S = IPS;
    var stack = P.olStack;

    /* 2-column wrapper */
    var row = ipEl('div', { display: 'flex', gap: '10px', height: '100%' }, container);

    /* Left column: composite OL stack preview */
    ipBuildOlStackPreview(row, 120);

    /* Right column: active item list */
    var right = ipEl(
      'div',
      {
        flex: '1',
        display: 'flex',
        flexDirection: 'column',
        gap: '4px',
        overflowY: 'auto',
        WebkitOverflowScrolling: 'touch',
      },
      row,
    );

    var base = typeof FetchState !== 'undefined' ? FetchState.api_base_url : '';

    if (stack.length === 0) {
      var hint = ipEl('div', { color: S.hintCol, fontSize: '13px', padding: '4px 0', flex: '1' }, right);
      hint.textContent = 'No active layers.';
    } else {
      /* Item list — read-only: icon + label per active OL */
      var list = ipEl(
        'div',
        { flex: '1', display: 'flex', flexDirection: 'column', gap: '3px', overflowY: 'auto' },
        right,
      );
      for (var i = 0; i < stack.length; i++) {
        var ol = stack[i];
        var itemRow = ipEl(
          'div',
          {
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            padding: '3px 4px',
            background: 'rgba(20,22,40,0.5)',
            borderRadius: '4px',
            cursor: 'pointer',
          },
          list,
        );
        /* Open the item in the Object Layer Engine Viewer (external page). */
        itemRow.dataset.itemId = ol.itemId;
        itemRow.onclick = function (ev) {
          ev.stopPropagation();
          var filter = JSON.stringify({
            'data.item.id': { filterType: 'text', type: 'contains', filter: this.dataset.itemId },
          });
          window.open(
            base + '/object-layer-engine-viewer?page=1&limit=10&id=&filterModel=' + encodeURIComponent(filter),
            '_blank',
          );
        };
        if (ol.type) {
          var ico = ipEl(
            'img',
            {
              width: '24px',
              height: '24px',
              imageRendering: 'pixelated',
              flexShrink: '0',
              borderRadius: '3px',
              background: 'rgba(14,14,28,0.6)',
            },
            itemRow,
          );
          ico.src = base + '/assets/' + ol.type + '/' + ol.itemId + '/08/0.png';
          ico.alt = '';
          ico.onerror = function () {
            this.style.display = 'none';
          };
        }
        var lbl = ipEl(
          'span',
          {
            flex: '1',
            fontSize: '12px',
            color: S.textCol,
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          },
          itemRow,
        );
        lbl.textContent = ol.itemId;
      }
    }
  },

  /* ================================================================
   * Chat tab — messages + input + quick-chat
   * ================================================================ */

  $ipBuildChatTab__deps: [
    '$IP',
    '$IPS',
    '$IP_QC',
    '$ipEl',
    '$ipBtn',
    '$ipRenderChat',
    'js_interact_overlay_close',
    'js_interact_overlay_send_chat',
  ],
  $ipBuildChatTab: function (container) {
    var P = IP,
      S = IPS,
      D = P.dom;

    /* message list — flex-fill to use available height */
    D.chatList = ipEl(
      'div',
      {
        flex: '1',
        background: S.chatBg,
        borderRadius: '4px',
        padding: '6px',
        minHeight: '60px',
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
      if (ev.key === 'Escape') {
        ev.preventDefault();
        _js_interact_overlay_close();
        return;
      }
      if (ev.key === 'Backspace') {
        /* C engine consumes keyboard events — manually remove last char. */
        if (D.chatInput.value.length > 0) {
          D.chatInput.value = D.chatInput.value.slice(0, -1);
        }
        return;
      }
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

  $ipPopulate__deps: ['$IP', '$IPS', '$ipEl', '$ipSwitchTab', '$IPStore'],
  $ipPopulate: function () {
    var P = IP,
      D = P.dom,
      S = IPS;
    var flags = P.interactFlags;

    /* Border colour resolved by C from server-driven config. */
    var statusCol = P.borderColor;

    /* Header: use the display name resolved by the C nameplate module
     * for both players and bots — consistent with overhead UI and bubbles. */
    var headerText = P.displayName || P.entityId.substring(0, 12);
    if (P.isSelf) headerText = '\u2726 ' + headerText + ' (You)';
    D.nameEl.textContent = headerText;
    D.nameEl.style.color = statusCol;

    /* No tab bar to build — single tab only. Open on the tab requested
     * by the C interaction modal. */
    var defaultTab = P.initialTab === 'integration' ? 'integration' : 'chat';
    P.activeTab = defaultTab;
    ipSwitchTab(defaultTab);
  },

  /* ================================================================
   * Public API — called from C via extern declarations
   * ================================================================ */

  js_interact_overlay_set_anchor__deps: ['$IP', '$ipApplyLayout'],
  js_interact_overlay_set_anchor: function (anchored, x, y, width, height) {
    var P = IP;
    P.anchor = anchored ? { x: x, y: y, w: width, h: height } : null;
    if (P.open) ipApplyLayout();
  },

  js_interact_overlay_open__deps: ['$IP', '$IPStore', '$ipStoreEnsure', '$ipBuild', '$ipPopulate', '$ipApplyLayout'],
  js_interact_overlay_open: function (
    entity_id_ptr,
    display_name_ptr,
    dlg_item_id_ptr,
    interact_flags,
    is_player,
    is_self,
    border_r,
    border_g,
    border_b,
    border_a,
    initial_tab,
  ) {
    var P = IP;

    P.entityId = UTF8ToString(entity_id_ptr);
    P.displayName = UTF8ToString(display_name_ptr);
    P.dlgItemId = UTF8ToString(dlg_item_id_ptr);
    P.interactFlags = interact_flags;
    P.isPlayer = !!is_player;
    P.isSelf = !!is_self;
    P.initialTab = initial_tab === 1 ? 'integration' : 'chat';
    /* Border colour resolved by C from server-driven StatusIconConfig.
     * Convert to CSS rgba string for DOM styling. */
    var a = ((border_a & 0xff) / 255).toFixed(2);
    P.borderColor = 'rgba(' + (border_r & 0xff) + ',' + (border_g & 0xff) + ',' + (border_b & 0xff) + ',' + a + ')';
    P.olStack = [];
    P.open = true;
    P.hidden = false;

    /* Load persisted messages from the store so chat history
     * survives across overlay open/close cycles. */
    var entry = ipStoreEnsure(P.entityId);
    P.chatHistory = entry.messages.map(function (m) {
      return { sender: m.sender, text: m.text, isMe: m.isMe || false, ts: m.ts };
    });

    ipBuild();
    ipApplyLayout();
    ipPopulate();

    P.el.style.display = 'block';
  },

  js_interact_overlay_set_ol_stack__deps: ['$IP', '$ipPopulate'],
  js_interact_overlay_set_ol_stack: function (json_ptr) {
    var P = IP;
    var json = UTF8ToString(json_ptr);
    try {
      P.olStack = JSON.parse(json);
    } catch (e) {
      P.olStack = [];
    }
    /* Re-populate so the dialog tab picks up the OL data. */
    if (P.open && P.el) ipPopulate();
  },

  js_interact_overlay_close__deps: ['$IP'],
  js_interact_overlay_close: function () {
    var P = IP;
    if (!P.open) return;

    P.open = false;
    P.hidden = false;
    if (P.el) P.el.style.display = 'none';

    /* Notify C to reopen the interaction modal. */
    _c_interact_overlay_closed();
  },

  js_interact_overlay_is_open__deps: ['$IP'],
  js_interact_overlay_is_open: function () {
    return IP.open && !IP.hidden ? 1 : 0;
  },

  js_interact_overlay_send_chat__deps: ['$IP', '$IPStore', '$ipStoreEnsure', '$ipRenderChat'],
  js_interact_overlay_send_chat: function (text_ptr) {
    var P = IP;
    var text = UTF8ToString(text_ptr);
    _free(text_ptr);

    if (!text || !text.trim()) return;
    text = text.trim().substring(0, 128);

    var msg = { sender: 'You', text: text, isMe: true, ts: Date.now() };
    P.chatHistory.push(msg);
    if (P.activeTab === 'chat') ipRenderChat();

    /* Persist in store so history survives overlay close. */
    var entry = ipStoreEnsure(P.entityId);
    entry.messages.push({ sender: 'You', text: text, ts: Date.now() });
    if (entry.messages.length > IPStore.MAX_MESSAGES) entry.messages = entry.messages.slice(-IPStore.MAX_MESSAGES);

    var toPtr = allocateUTF8(P.entityId);
    var textPtr = allocateUTF8(text);
    Module._c_send_chat_binary(toPtr, textPtr);
    _free(toPtr);
    _free(textPtr);
  },

  js_interact_overlay_receive_chat__deps: ['$IP', '$IPStore', '$ipStoreEnsure', '$ipRenderChat'],
  js_interact_overlay_receive_chat: function (from_id_ptr, from_name_ptr, text_ptr) {
    var P = IP;
    var fromId = UTF8ToString(from_id_ptr);
    var fromName = UTF8ToString(from_name_ptr);
    var text = UTF8ToString(text_ptr);

    /* Persist history so the chat tab survives overlay open/close. The
     * unread badge is owned by the C interaction modal's Chat button. */
    var entry = ipStoreEnsure(fromId);
    entry.messages.push({ sender: fromName || fromId.substring(0, 8), text: text, isMe: false, ts: Date.now() });
    if (entry.messages.length > IPStore.MAX_MESSAGES) entry.messages = entry.messages.slice(-IPStore.MAX_MESSAGES);

    if (P.open && P.entityId === fromId) {
      P.chatHistory.push({ sender: fromName || fromId.substring(0, 8), text: text, isMe: false, ts: Date.now() });
      if (P.activeTab === 'chat') ipRenderChat();
    }
  },
});
