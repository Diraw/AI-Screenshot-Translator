if (typeof marked !== 'undefined') {
  marked.setOptions({ breaks: true, gfm: true });
}
var INIT_DATA = {}; var CUR_MD = '';
try { INIT_DATA = JSON.parse(document.getElementById('init-data').textContent || '{}'); CUR_MD = INIT_DATA.raw_md || ''; } catch(e) {}
const DARK_MODE = !!INIT_DATA.is_dark;
var SHOW_ADV_DEBUG = (INIT_DATA.show_adv_debug !== false);
document.documentElement.classList.toggle('dark-mode', DARK_MODE);
document.body.classList.toggle('dark-mode', DARK_MODE);
try {
  if (INIT_DATA.mark_bg) document.documentElement.style.setProperty('--mark-bg', INIT_DATA.mark_bg);
  if (INIT_DATA.mark_bg_dark) document.documentElement.style.setProperty('--mark-bg-dark', INIT_DATA.mark_bg_dark);
} catch(e) {}
function log(m) { if (window.chrome&&window.chrome.webview) window.chrome.webview.postMessage(JSON.stringify(["log", m])); }
window.onerror = function(m,u,l,c,e) { log("JS ERR: "+m+" @ "+l+":"+c); };
var EDIT = false;
var CURRENT_DEBUG_INFO = '';

function isSafeUrl(url, allowDataImage) {
  var v = (url || '').trim().toLowerCase();
  if (!v) return true;
  if (v.indexOf('javascript:') === 0 || v.indexOf('vbscript:') === 0) return false;
  if (v.indexOf('data:') === 0) {
    return !!allowDataImage && v.indexOf('data:image/') === 0;
  }
  return (
    v.indexOf('http://') === 0 ||
    v.indexOf('https://') === 0 ||
    v.indexOf('mailto:') === 0 ||
    v.indexOf('tel:') === 0 ||
    v.indexOf('#') === 0 ||
    v.indexOf('/') === 0 ||
    v.indexOf('./') === 0 ||
    v.indexOf('../') === 0
  );
}

function sanitizeRenderedHtml(html) {
  var template = document.createElement('template');
  template.innerHTML = html || '';

  var allowedTags = {
    A:1, B:1, BLOCKQUOTE:1, BR:1, CODE:1, DEL:1, DIV:1, EM:1,
    H1:1, H2:1, H3:1, H4:1, H5:1, H6:1, HR:1, I:1, IMG:1, LI:1,
    MARK:1, OL:1, P:1, PRE:1, S:1, SPAN:1, STRONG:1, SUB:1, SUP:1,
    TABLE:1, TBODY:1, TD:1, TH:1, THEAD:1, TR:1, U:1, UL:1
  };
  var blockedTags = { SCRIPT:1, STYLE:1, IFRAME:1, OBJECT:1, EMBED:1, LINK:1, META:1 };
  var globalAttrs = { 'class':1, 'title':1, 'aria-label':1, 'aria-hidden':1 };

  function clean(parent) {
    var node = parent.firstChild;
    while (node) {
      var next = node.nextSibling;

      if (node.nodeType === 1) {
        var tag = node.tagName.toUpperCase();
        if (blockedTags[tag]) {
          node.remove();
          node = next;
          continue;
        }
        if (!allowedTags[tag]) {
          while (node.firstChild) parent.insertBefore(node.firstChild, node);
          node.remove();
          node = next;
          continue;
        }

        var attrs = Array.prototype.slice.call(node.attributes || []);
        for (var i = 0; i < attrs.length; i++) {
          var name = attrs[i].name;
          var lower = name.toLowerCase();
          var value = attrs[i].value || '';
          var keep = !!globalAttrs[lower];

          if (tag === 'A' && (lower === 'href' || lower === 'target' || lower === 'rel')) keep = true;
          if (tag === 'IMG' && (lower === 'src' || lower === 'alt')) keep = true;
          if ((tag === 'CODE' || tag === 'PRE' || tag === 'SPAN' || tag === 'DIV' || tag === 'P') && lower === 'class') keep = true;

          if (lower.indexOf('on') === 0 || lower === 'style' || lower === 'srcdoc') keep = false;
          if ((lower === 'href' || lower === 'src') && keep) {
            var allowDataImage = tag === 'IMG' && lower === 'src';
            if (!isSafeUrl(value, allowDataImage)) keep = false;
          }

          if (!keep) node.removeAttribute(name);
        }

        if (tag === 'A') {
          var href = (node.getAttribute('href') || '').trim();
          if (href) node.setAttribute('rel', 'noopener noreferrer');
        }

        clean(node);
      } else if (node.nodeType === 8) {
        node.remove();
      }

      node = next;
    }
  }

  clean(template.content || template);
  return template.innerHTML;
}

function splitDebugFromMarkdown(md) {
  var src = (typeof md === 'string') ? md : '';
  var re = /^```(?:text)?\r?\n\[Advanced API Debug\]\r?\n([\s\S]*?)\r?\n```\s*\r?\n?/;
  var m = src.match(re);
  if (!m) return { md: src, debug: '' };
  return { md: src.slice(m[0].length), debug: (m[1] || '').trim() };
}

function applyDebugBarVisibility() {
  var bar = document.getElementById('debug_bar');
  if (!bar) return;
  bar.style.display = SHOW_ADV_DEBUG ? '' : 'none';
}

function setDebugInfo(text) {
  var el = document.getElementById('debug_lines');
  if (!el) return;
  applyDebugBarVisibility();
  if (!SHOW_ADV_DEBUG) return;
  var v = (text || '').trim();
  if (!v) { el.textContent = 'debug: -'; return; }
  var lines = v.split(/\r?\n/).filter(function(line){ return line.trim().length > 0; });
  el.textContent = lines.length ? lines.join('\n') : 'debug: -';
}

function focusSink() {
  try {
    var kt = document.getElementById('keytrap');
    if (kt && kt.focus) {
      kt.focus({ preventScroll: true });
      return;
    }
    document.body.tabIndex = 0;
    document.body.focus({ preventScroll: true });
  } catch(e) {}
}
function render(md, opts) {
  var parsed = splitDebugFromMarkdown(md || '');
  if (parsed.debug) CURRENT_DEBUG_INFO = parsed.debug;
  else if (!(opts && opts.keepDebug)) CURRENT_DEBUG_INFO = '';
  setDebugInfo(CURRENT_DEBUG_INFO);
  if (typeof marked==='undefined') return;
  // Step 1: Extract LaTeX from backticks (e.g., `$formula$` -> $formula$)
  // Only unwrap if the backticked content is a valid LaTeX expression (properly paired delimiters)
  var text = parsed.md || '';
  text = text.replace(/`([^`]+)`/g, function(match, inner) {
    // Check if content is a complete LaTeX expression with properly paired delimiters
    // Match: $...$, $$...$$, \(...\), or \[...\]
    // Using [\s\S]* to allow empty or any content including newlines
    if (/^(?:\$\$[\s\S]*?\$\$|\$[^\$\n]+\$|\\\([\s\S]*?\\\)|\\\[[\s\S]*?\\\])$/.test(inner.trim())) {
      return inner; // Unwrap backticks
    }
    return match; // Keep original backticks if not valid LaTeX
  });

  // Normalize common LaTeX display environments so KaTeX auto-render can process them
  // through the configured $$...$$ delimiters.
  text = text.replace(
    /\\begin\{(equation\*?|align\*?|gather\*?|multline\*?)\}([\s\S]*?)\\end\{\1\}/g,
    function(_match, _envName, body) {
      var inner = (body || '').trim();
      return '\n$$\n' + inner + '\n$$\n';
    }
  );
  
  // Step 2: Protect all LaTeX expressions before markdown parsing
  var prot = (function(t){
    var b=[], c=0, r=/(\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$)/g;
    return {text: t.replace(r, function(m){ b.push(m); return 'MATHPH'+(c++); }), blocks: b};
  })(text);
  
  // Step 3: Parse markdown
  var h = marked.parse(prot.text);
  h = sanitizeRenderedHtml(h);
  
  // Step 4: Restore protected LaTeX expressions
  // Use regex replace for better performance and reliability with long strings
  // This prevents issues when content is very long or has many formulas
  // Restore in reverse order to avoid prefix collisions (e.g., MATHPH1 vs MATHPH10)
  for (var ri = prot.blocks.length - 1; ri >= 0; ri--) {
    var b = prot.blocks[ri];
    try {
      var placeholder = 'MATHPH' + ri;
      var escapedPlaceholder = placeholder.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
      var regex = new RegExp(escapedPlaceholder, 'g');
      h = h.replace(regex, function() { return b; });
    } catch (e) {
      console.error('Error restoring LaTeX block ' + ri + ':', e);
    }
  }
  
  // Step 5: Render to DOM
  var d = document.getElementById('content');
  if (d) {
    d.innerHTML = h;
    if (window.hljs) hljs.highlightAll();
    
    // Step 6: Render LaTeX with KaTeX
    if (window.renderMathInElement) {
      renderMathInElement(d, {
        delimiters: [
          {left:'$$', right:'$$', display:true},
          {left:'$', right:'$', display:false},
          {left:'\\(', right:'\\)', display:false},
          {left:'\\[', right:'\\]', display:true}
        ],
        throwOnError: false
        // Use KaTeX defaults for ignoredTags/ignoredClasses for safety
      });
    }
  }
}
window.__setAdvancedDebugVisible = function(show) {
  SHOW_ADV_DEBUG = !!show;
  applyDebugBarVisibility();
  render(CUR_MD, { keepDebug: true });
};
window.updateContentFromNative = function(payload) {
  var next = payload;
  if (payload && typeof payload === 'object' && !Array.isArray(payload)) {
    next = payload.raw_md;
  }
  if (typeof next !== 'string') next = '';
  CUR_MD = next;
  if (!EDIT) render(next);
};
window.toggleSource = function() {
  if (EDIT) return;
  var c = document.getElementById('content'), r = document.getElementById('raw_view');
  var parsed = splitDebugFromMarkdown(CUR_MD);
  if (r.style.display==='none') { r.innerText = parsed.md; r.style.display='block'; c.style.display='none'; if(window.cmd_updateStatus) window.cmd_updateStatus('raw'); }
  else { r.style.display='none'; c.style.display='block'; if(window.cmd_updateStatus) window.cmd_updateStatus('view'); focusSink(); }
};
window.toggleEdit = function() {
  var c = document.getElementById('content'), e = document.getElementById('edit_view'), r = document.getElementById('raw_view');
  if (!EDIT) {
    EDIT=true;
    var parsed = splitDebugFromMarkdown(CUR_MD);
    if (parsed.debug) CURRENT_DEBUG_INFO = parsed.debug;
    setDebugInfo(CURRENT_DEBUG_INFO);
    e.value=parsed.md;
    e.style.display='block'; c.style.display='none'; r.style.display='none'; fitEditHeight(); e.focus(); if(window.cmd_updateStatus) window.cmd_updateStatus('edit');
  }
  else {
    EDIT=false;
    CUR_MD=e.value;
    if(window.cmd_updateContent) window.cmd_updateContent(CUR_MD);
    render(CUR_MD, { keepDebug: true });
    c.style.display='block';
    setTimeout(function(){
      e.style.display='none';
      focusSink();
      if(window.cmd_updateStatus) window.cmd_updateStatus('view');
    }, 0);
  }
};
function applyFormat(t) {
  var e = document.getElementById('edit_view'); if (!EDIT || !e) return;
  var start = e.selectionStart, end = e.selectionEnd, val = e.value;
  var tag = (t==='bold'?'b':(t==='underline'?'u':'mark')), txt = val.substring(start, end);
  var ins = '<'+tag+'>'+txt+'</'+tag+'>';
  e.value = val.substring(0, start) + ins + val.substring(end);
  e.selectionStart = start; e.selectionEnd = start + ins.length; e.focus();
  CUR_MD = e.value; if(window.cmd_updateContent) window.cmd_updateContent(CUR_MD);
}
window.toggleScreenshot = function() { if(window.cmd_showScreenshot) window.cmd_showScreenshot(); };
function parseHK(hk) {
  if(!hk) return null; var p = hk.toLowerCase().split('+').map(x=>x.trim()).filter(x=>x.length>0), k = p.pop();
  return {key:k, ctrl:p.includes('ctrl')||p.includes('control'), alt:p.includes('alt'), shift:p.includes('shift')};
}
function matchHK(e, h) { return h && !!e.ctrlKey==!!h.ctrl && !!e.altKey==!!h.alt && !!e.shiftKey==!!h.shift && e.key.toLowerCase()==h.key; }
const HK_V=parseHK(INIT_DATA.key_view), HK_E=parseHK(INIT_DATA.key_edit), HK_S=parseHK(INIT_DATA.key_screenshot), HK_P=parseHK(INIT_DATA.key_prev), HK_N=parseHK(INIT_DATA.key_next), HK_T=parseHK(INIT_DATA.key_tag), HK_B=parseHK(INIT_DATA.key_bold), HK_U=parseHK(INIT_DATA.key_underline), HK_H=parseHK(INIT_DATA.key_highlight);
window.addEventListener('keydown', function(e) {
  log("Keydown: " + e.key + " (code: " + e.code + ")");
  var k = (e.key || '').toLowerCase();
  if (k === 'f12' || (e.ctrlKey && e.shiftKey && k === 'i')) {
    if (window.cmd_openDevTools) {
      log('Trigger openDevTools from JS');
      window.cmd_openDevTools();
    } else {
      log('cmd_openDevTools not bound');
    }
    e.preventDefault();
    return;
  }
  var raw = document.getElementById('raw_view');
  var isRawVisible = raw && raw.style.display !== 'none';
  if (!EDIT && isRawVisible && e.key === 'Escape') {
    e.preventDefault();
    toggleSource();
    setTimeout(function() { focusSink(); }, 0);
    return;
  }
  if (EDIT) {
    if (matchHK(e, HK_E) || e.key === 'Escape') {
      e.preventDefault(); toggleEdit(); return;
    }
    if (matchHK(e, HK_B)) { e.preventDefault(); applyFormat('bold'); return; }
    if (matchHK(e, HK_U)) { e.preventDefault(); applyFormat('underline'); return; }
    if (matchHK(e, HK_H)) { e.preventDefault(); applyFormat('highlight'); return; }
    return;
  }
  if (matchHK(e, HK_V)){ log("Matched View"); e.preventDefault(); toggleSource(); return; }
  if (matchHK(e, HK_E)){ log("Matched Edit"); e.preventDefault(); toggleEdit(); return; }
  if (matchHK(e, HK_S)){ log("Matched Screenshot"); e.preventDefault(); toggleScreenshot(); return; }
  if (matchHK(e, HK_P)){ e.preventDefault(); if(window.cmd_showPrevious) window.cmd_showPrevious(); return; }
  if (matchHK(e, HK_N)){ e.preventDefault(); if(window.cmd_showNext) window.cmd_showNext(); return; }
  if (matchHK(e, HK_T)){ e.preventDefault(); if(window.cmd_openTagDialog) window.cmd_openTagDialog(); return; }
  if (EDIT) {
    if (matchHK(e, HK_B)){ e.preventDefault(); applyFormat('bold'); return; }
    if (matchHK(e, HK_U)){ e.preventDefault(); applyFormat('underline'); return; }
    if (matchHK(e, HK_H)){ e.preventDefault(); applyFormat('highlight'); return; }
  }
}, true);
document.addEventListener('DOMContentLoaded', () => { render(CUR_MD); focusSink(); log("WIN=" + (INIT_DATA.win_id||"?") + " DOMContentLoaded; KEY_VIEW=" + (INIT_DATA.key_view||"") + " KEY_SHOT=" + (INIT_DATA.key_screenshot||"") + " KEY_EDIT=" + (INIT_DATA.key_edit||"")); });

document.addEventListener('mouseup', () => {
  if (!EDIT) focusSink();
}, true);

function fitEditHeight() {
  var e = document.getElementById('edit_view');
  if (!e) return;
  e.style.height = 'auto';
  var target = Math.max(200, e.scrollHeight);
  e.style.height = target + 'px';
}
window.addEventListener('resize', fitEditHeight);
window.addEventListener('input', function(e){ if(e && e.target && e.target.id==='edit_view') fitEditHeight(); }, true);
