if (document.body) { applyDarkMode(IS_DARK); } else { document.addEventListener('DOMContentLoaded', () => applyDarkMode(IS_DARK), {once:true}); }
var LAST_ENTRY = null;
document.addEventListener('DOMContentLoaded', function() {
    if (document.body) {
        document.body.tabIndex = -1;
        document.body.focus();
    }
    if (typeof RESTORE_SCROLL !== 'undefined') {
        window.scrollTo(0, RESTORE_SCROLL);
        setTimeout(() => window.scrollTo(0, RESTORE_SCROLL), 50);
    }
    log('DOMContentLoaded; KEY_VIEW='+KEY_VIEW+' KEY_SHOT='+KEY_SHOT+' KEY_EDIT='+KEY_EDIT);
});
function currentEntry() {
    var active = document.activeElement;
    if (active) {
        var e = active.closest('.entry');
        if (e) {
            LAST_ENTRY = e;
            return e;
        }
    }
    if (LAST_ENTRY && LAST_ENTRY.isConnected) return LAST_ENTRY;
    LAST_ENTRY = null;
    return document.querySelector('.entry');
}
function updateStatus(mode) {
   var ind = document.getElementById('status-indicator');
   if (ind) {
       if (mode === 'edit') ind.innerText = 'MODE: EDIT';
       else if (mode === 'raw') ind.innerText = 'MODE: RAW';
       else ind.innerText = 'MODE: VIEW';
   }
}

function isSafeUrl(url, allowDataImage) {
   var v = (url || '').trim().toLowerCase();
   if (!v) return true;
   if (v.indexOf('javascript:') === 0 || v.indexOf('vbscript:') === 0) return false;
   if (v.indexOf('data:') === 0) return !!allowDataImage && v.indexOf('data:image/') === 0;
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

function protectMathJs(text) {
   var blocks = [];
   var counter = 0;
   var regex = /(\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$)/g;
   var protectedText = text.replace(regex, function(match) {
       var ph = 'MATHBLOCKPH' + counter++;
       blocks.push(match);
       return ph;
   });
   return {text: protectedText, blocks: blocks};
}

function extractMarkdown(rawEl) {
    if (!rawEl) return '';
    var html = rawEl.innerHTML || '';
    html = html.replace(/<div><br\s*\/?>\s*<\/div>/gi, '\n');
    html = html.replace(/<div>/gi, '\n').replace(/<\/div>/gi, '');
    html = html.replace(/<p>/gi, '').replace(/<\/p>/gi, '\n');
    html = html.replace(/<br\s*\/?>/gi, '\n');
    html = html.replace(/&nbsp;/gi, ' ');
    var tmp = document.createElement('div');
    tmp.style.whiteSpace = 'pre-wrap';
    tmp.innerHTML = html;
    return (tmp.textContent || '').replace(/\r\n?/g, '\n');
}

function ensureMarkedOptions() {
    if (typeof marked !== 'undefined') {
         marked.setOptions({ breaks: true, gfm: true });
    }
}

function renderContent(id, markdownOverride) {
    var raw = document.getElementById('raw_' + id);
    var rendered = document.getElementById('rendered_' + id);
    if (!rendered) return;
    renderContentForElements(raw, rendered, markdownOverride);
}

function renderContentForElements(raw, rendered, markdownOverride) {
    if (!rendered) return;

    ensureMarkedOptions();
    var markdown = (typeof markdownOverride === 'string') ? markdownOverride : (raw ? raw.textContent : '');
    
    // Step 1: Extract LaTeX from backticks (e.g., `$formula$` -> $formula$)
    // Only unwrap if the backticked content is a valid LaTeX expression (properly paired delimiters)
    markdown = markdown.replace(/`([^`]+)`/g, function(match, inner) {
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
    markdown = markdown.replace(
      /\\begin\{(equation\*?|align\*?|gather\*?|multline\*?)\}([\s\S]*?)\\end\{\1\}/g,
      function(_match, _envName, body) {
        var inner = (body || '').trim();
        return '\n$$\n' + inner + '\n$$\n';
      }
    );
    
    // Step 2: Protect math expressions before markdown parsing
    var p = protectMathJs(markdown);
    
    // Step 3: Parse markdown
    var html = marked.parse(p.text);
    html = sanitizeRenderedHtml(html);
    
    // Step 4: Restore protected math blocks (reverse order to avoid prefix collisions, e.g., PH1 vs PH10)
    for (var r = p.blocks.length - 1; r >= 0; r--) {
         var block = p.blocks[r];
         try {
           var placeholder = 'MATHBLOCKPH' + r;
           var escapedPlaceholder = placeholder.replace(/[.*+?^${}()|[\\]\\]/g, '\\$&');
           var regex = new RegExp(escapedPlaceholder, 'g');
           html = html.replace(regex, function() { return block; });
         } catch (e) {
           console.error('Error restoring math block ' + r + ':', e);
         }
    }
    
    rendered.innerHTML = html;
    
    // Step 5: Render LaTeX with KaTeX
    requestAnimationFrame(function() {
      if (!rendered) return;
      renderMathInElement(rendered, {
        delimiters: [
          {left: '$$', right: '$$', display: true}, 
          {left: '$', right: '$', display: false}, 
          {left: '\\(', right: '\\)', display: false}, 
          {left: '\\[', right: '\\]', display: true}
        ], 
        throwOnError: false
        // Use KaTeX defaults for ignoredTags/ignoredClasses for safety
      });
    });
}

function insertMarkdown(startTag, endTag) {
   var sel = window.getSelection();
   if (!sel.rangeCount) return;
   var range = sel.getRangeAt(0);
   var text = range.toString();
   document.execCommand('insertText', false, startTag + text + endTag);
}

function matchHotkey(e, hotkeyStr) {
   if (!hotkeyStr) return false;
   var parts = hotkeyStr.toLowerCase().split('+').map(function(p){ return p.trim(); }).filter(Boolean);
   if (!parts.length) return false;
   var key = parts.pop();
   var ctrl = parts.includes('ctrl') || parts.includes('control');
   var alt = parts.includes('alt');
   var shift = parts.includes('shift');
   return (e.key.toLowerCase() === key && e.ctrlKey === ctrl && e.altKey === alt && e.shiftKey === shift);
}

function toggleView(id) {
  var raw = document.getElementById('raw_' + id);
  var rendered = document.getElementById('rendered_' + id);
  if (!raw || !rendered) return;
  if (raw.style.display === 'none') {
    raw.style.display = 'block';
    rendered.style.display = 'none';
    updateStatus('raw');
  } else {
    raw.style.display = 'none';
    rendered.style.display = 'block';
    updateStatus('view');
  }
  requestMeasureRenderedHeights();
}

function toggleEdit(entry) {
   var id = entry.getAttribute('data-id');
   var rendered = document.getElementById('rendered_' + id);
   var raw = document.getElementById('raw_' + id);
   
   if (entry.classList.contains('mode-edit')) {
       entry.classList.remove('mode-edit');
       entry.classList.add('mode-view');
       updateStatus('view');
       raw.contentEditable = 'false';

       var markdown = extractMarkdown(raw);
       raw.innerText = markdown;
       renderContent(id, markdown);

       raw.style.display = 'none';
       rendered.style.display = 'block';
       
       // Notify C++ of edit
       window.cmd_updateEntry(id, markdown);

       entry.focus();
       requestMeasureRenderedHeights();
   } else {
       entry.classList.remove('mode-view');
       entry.classList.add('mode-edit');
       updateStatus('edit');
       rendered.style.display = 'none';
       raw.style.display = 'block';
       raw.contentEditable = 'true';
       raw.focus();
       requestMeasureRenderedHeights();
   }
}

document.addEventListener('focusin', function(e) {
   var entry = e.target.closest('.entry');
   if (entry) {
       var id = entry.getAttribute('data-id');
       var raw = document.getElementById('raw_' + id);
       var isRaw = raw && raw.style.display !== 'none';
       if (entry.classList.contains('mode-edit')) updateStatus('edit');
       else if (isRaw) updateStatus('raw');
       else updateStatus('view');
       LAST_ENTRY = entry;
   } else { updateStatus('view'); }
});

function handleKey(e) {
   log(`handleKey enter key=${e.key} ctrl=${e.ctrlKey} alt=${e.altKey} shift=${e.shiftKey}`);
   var keyLower = e.key ? e.key.toLowerCase() : '';
   if (keyLower === 'f12' || (e.ctrlKey && e.shiftKey && keyLower === 'i')) {
       if (window.cmd_openDevTools) {
           log('Trigger openDevTools from JS');
           window.cmd_openDevTools();
       } else {
           log('cmd_openDevTools not bound');
       }
       e.preventDefault();
       return;
   }
   if (SELECTION_MODE && e.key === 'Escape') {
       if (window.cmd_exitSelectionMode) window.cmd_exitSelectionMode();
       e.preventDefault();
       return;
   }
   var entry = currentEntry();
   if (!entry) { log('handleKey: no entry'); return; }
   log(`handleKey currentEntry id=${entry.getAttribute('data-id')}`);
   
   var isEditing = entry.classList.contains('mode-edit');
   var k = keyLower;
   log(`KEY_VIEW=${KEY_VIEW} KEY_SHOT=${KEY_SHOT} k=${k} isEditing=${isEditing}`);

   // Batch selection mode toggle (only when not editing).
   // Default is Ctrl+S, but it's configurable via KEY_SELECT.
   if (!isEditing && matchHotkey(e, (typeof KEY_SELECT !== 'undefined') ? KEY_SELECT : 'ctrl+s')) {
       if (window.cmd_toggleSelectionMode) window.cmd_toggleSelectionMode();
       e.preventDefault();
       return;
   }

   if (matchHotkey(e, KEY_EDIT)) {
       e.preventDefault(); toggleEdit(entry); return; 
   }
   
   if (isEditing) {
      if (e.key === 'Escape') {
          e.preventDefault(); toggleEdit(entry); return;
      }
      if (matchHotkey(e, KEY_BOLD)) {
          e.preventDefault(); insertMarkdown('**', '**'); return;
      }
      if (matchHotkey(e, KEY_UNDERLINE)) {
          e.preventDefault(); insertMarkdown('<u>', '</u>'); return;
      }
      if (matchHotkey(e, KEY_HIGHLIGHT)) {
          e.preventDefault(); insertMarkdown('<mark>', '</mark>'); return;
      }
      return; 
    }
    if (!isEditing) {
        // Esc exits RAW mode back to VIEW (when currently in RAW)
        if (e.key === 'Escape') {
            var id0 = entry.getAttribute('data-id');
            var raw0 = document.getElementById('raw_' + id0);
            if (raw0 && raw0.style.display !== 'none') {
                e.preventDefault();
                toggleView(id0);
                return;
            }
        }
        if (matchHotkey(e, KEY_VIEW)) { toggleView(entry.getAttribute('data-id')); e.preventDefault(); return; }
        if (matchHotkey(e, KEY_SHOT)) { 
            // Call native webview binding
            window.cmd_restore(entry.getAttribute('data-id'));
            e.preventDefault(); return; 
        }
        
        // Handle 'dd' for deletion
        if (k === 'd') {
            e.preventDefault();
            var now = Date.now();
            if (entry.lastDTime && (now - entry.lastDTime < 500)) {
                 // Double press detected
                 var id = entry.getAttribute('data-id');
                 // No confirm dialog as requested
                 window.cmd_delete(id);
                 removeEntryById(id);
                 entry.lastDTime = 0;
            } else {
                 entry.lastDTime = now;
            }
            return;
        }
    }
}

if (!window.__INIT_ONCE__) {
    window.__INIT_ONCE__ = true;
    window.__FOCUS_GUARD_ID__ = (Math.random().toString(16).slice(2) + Date.now().toString(16));
    log('init_guard gid=' + window.__FOCUS_GUARD_ID__);
    window.addEventListener('keydown', handleKey, true);
    window.addEventListener('keydown', function(e){
        log(`keydown key=${e.key} ctrl=${e.ctrlKey} alt=${e.altKey} shift=${e.shiftKey} target=${(e.target && e.target.tagName)}`);
    }, true);
    // 非 capture：避免把元素级 focus/blur 误记为 window focus/blur。
    window.addEventListener('focus', ()=>log('window focus gid=' + window.__FOCUS_GUARD_ID__));
    window.addEventListener('blur', ()=>log('window blur gid=' + window.__FOCUS_GUARD_ID__));
    document.addEventListener('visibilitychange', ()=>log('visibility='+document.visibilityState), true);
}

function updateEntryInDom(id, newMarkdown) {
    var idx = ENTRY_INDEX[id];
    if (typeof idx === 'undefined') return;

    ENTRY_DATA[idx].originalRaw = String(newMarkdown || '');

    var raw = document.getElementById('raw_' + id);
    if (raw) {
        raw.innerText = ENTRY_DATA[idx].originalRaw;
        var entry = raw.closest('.entry');
        if (entry && !entry.classList.contains('mode-edit')) {
            renderContent(id, ENTRY_DATA[idx].originalRaw);
            requestMeasureRenderedHeights();
        }
    }
}

var VIRTUAL_OVERSCAN_ITEMS = 3;
var VIRTUAL_ESTIMATED_HEIGHT = 260;
var VIRTUAL_ENTRY_GAP = 12;

var ENTRY_DATA = [];
var ENTRY_INDEX = Object.create(null);
var ENTRY_HEIGHTS = Object.create(null); // id -> measured height (without margin)
var ENTRY_OFFSETS = []; // index -> top offset
var ENTRY_TOTAL_HEIGHT = 0;

var RENDERED_ENTRY_NODES = Object.create(null); // id -> DOM node (only currently rendered range)
var SELECTED_IDS = Object.create(null); // id -> true

var VIRTUAL_ROOT = null;
var VIRTUAL_TOP_SPACER = null;
var VIRTUAL_ITEMS_HOST = null;
var VIRTUAL_BOTTOM_SPACER = null;
var VIRTUAL_RANGE_START = 0;
var VIRTUAL_RANGE_END = -1;
var VIRTUAL_SCROLL_SCHEDULED = false;
var VIRTUAL_MEASURE_SCHEDULED = false;
var VIRTUAL_EVENTS_BOUND = false;
var VIRTUAL_SCROLL_IDLE_MS = 160;
var VIRTUAL_IS_SCROLLING = false;
var VIRTUAL_SCROLL_IDLE_TIMER = null;
var VIRTUAL_PENDING_HEIGHTS = Object.create(null);
var VIRTUAL_SUPPRESS_AUTO_MEASURE = false;

function ensureVirtualRoot() {
    if (VIRTUAL_ROOT) return;

    var root = document.createElement('div');
    root.id = 'virtual-entry-root';
    root.style.width = '100%';

    var topSpacer = document.createElement('div');
    topSpacer.id = 'virtual-top-spacer';

    var itemsHost = document.createElement('div');
    itemsHost.id = 'virtual-items-host';

    var bottomSpacer = document.createElement('div');
    bottomSpacer.id = 'virtual-bottom-spacer';

    root.appendChild(topSpacer);
    root.appendChild(itemsHost);
    root.appendChild(bottomSpacer);
    document.body.appendChild(root);

    VIRTUAL_ROOT = root;
    VIRTUAL_TOP_SPACER = topSpacer;
    VIRTUAL_ITEMS_HOST = itemsHost;
    VIRTUAL_BOTTOM_SPACER = bottomSpacer;
}

function bindVirtualEventsOnce() {
    if (VIRTUAL_EVENTS_BOUND) return;
    VIRTUAL_EVENTS_BOUND = true;

    window.addEventListener('scroll', function() {
        markVirtualScrollActivity();
        scheduleVirtualRender();
    }, { passive: true });

    window.addEventListener('resize', function() {
        scheduleVirtualRender();
    }, { passive: true });
}

function markVirtualScrollActivity() {
    VIRTUAL_IS_SCROLLING = true;
    if (VIRTUAL_SCROLL_IDLE_TIMER) {
        clearTimeout(VIRTUAL_SCROLL_IDLE_TIMER);
    }
    VIRTUAL_SCROLL_IDLE_TIMER = setTimeout(function() {
        VIRTUAL_SCROLL_IDLE_TIMER = null;
        VIRTUAL_IS_SCROLLING = false;
        commitPendingMeasuredHeights(true);
    }, VIRTUAL_SCROLL_IDLE_MS);
}

function normalizeEntryPayload(entry) {
    if (!entry || typeof entry !== 'object') return null;
    var id = String(entry.id || '');
    if (!id) return null;
    return {
        id: id,
        time: String(entry.time || ''),
        originalRaw: String(entry.originalRaw || ''),
        tags: Array.isArray(entry.tags) ? entry.tags.slice() : []
    };
}

function getItemBlockHeightByIndex(index) {
    var e = ENTRY_DATA[index];
    if (!e) return VIRTUAL_ESTIMATED_HEIGHT + VIRTUAL_ENTRY_GAP;
    var h = ENTRY_HEIGHTS[e.id];
    if (!(h > 0)) h = VIRTUAL_ESTIMATED_HEIGHT;
    return h + VIRTUAL_ENTRY_GAP;
}

function rebuildVirtualIndex() {
    ENTRY_INDEX = Object.create(null);
    ENTRY_OFFSETS = new Array(ENTRY_DATA.length);
    var offset = 0;

    for (var i = 0; i < ENTRY_DATA.length; i++) {
        var e = ENTRY_DATA[i];
        ENTRY_INDEX[e.id] = i;
        ENTRY_OFFSETS[i] = offset;
        offset += getItemBlockHeightByIndex(i);
    }

    ENTRY_TOTAL_HEIGHT = offset;
}

function lowerBoundByItemBottom(viewTop) {
    var n = ENTRY_DATA.length;
    if (!n) return 0;
    var lo = 0;
    var hi = n - 1;
    var ans = n - 1;
    while (lo <= hi) {
        var mid = (lo + hi) >> 1;
        var itemBottom = ENTRY_OFFSETS[mid] + getItemBlockHeightByIndex(mid);
        if (itemBottom >= viewTop) {
            ans = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return ans;
}

function upperBoundByItemTop(viewBottom) {
    var n = ENTRY_DATA.length;
    if (!n) return -1;
    var lo = 0;
    var hi = n - 1;
    var ans = -1;
    while (lo <= hi) {
        var mid = (lo + hi) >> 1;
        if (ENTRY_OFFSETS[mid] <= viewBottom) {
            ans = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return ans;
}

function lowerBoundByItemBottomFromArrays(offsets, blockHeights, viewTop) {
    var n = offsets.length;
    if (!n) return 0;
    var lo = 0;
    var hi = n - 1;
    var ans = n - 1;
    while (lo <= hi) {
        var mid = (lo + hi) >> 1;
        var itemBottom = offsets[mid] + blockHeights[mid];
        if (itemBottom >= viewTop) {
            ans = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return ans;
}

function preserveViewportAnchorDuringRebuild(rebuildFn) {
    if (!VIRTUAL_ROOT || !ENTRY_DATA.length) {
        rebuildFn();
        return;
    }

    var oldOffsets = ENTRY_OFFSETS.slice();
    var oldBlockHeights = new Array(ENTRY_DATA.length);
    for (var i = 0; i < ENTRY_DATA.length; i++) {
        oldBlockHeights[i] = getItemBlockHeightByIndex(i);
    }

    var rootRect = VIRTUAL_ROOT.getBoundingClientRect();
    var oldViewTop = Math.max(0, -rootRect.top);
    var anchorIdx = lowerBoundByItemBottomFromArrays(oldOffsets, oldBlockHeights, oldViewTop);
    if (!(anchorIdx >= 0 && anchorIdx < ENTRY_DATA.length)) {
        rebuildFn();
        return;
    }

    var anchorOffsetInItem = oldViewTop - oldOffsets[anchorIdx];
    rebuildFn();

    if (!(anchorIdx < ENTRY_OFFSETS.length)) return;
    var newViewTop = ENTRY_OFFSETS[anchorIdx] + anchorOffsetInItem;
    var delta = newViewTop - oldViewTop;
    if (Math.abs(delta) > 0.5) {
        window.scrollTo(0, window.scrollY + delta);
    }
}

function getPinnedEntryId() {
    var active = document.activeElement;
    if (active) {
        var focusedEntry = active.closest('.entry');
        if (focusedEntry) {
            return focusedEntry.getAttribute('data-id');
        }
    }
    if (LAST_ENTRY && LAST_ENTRY.isConnected) {
        return LAST_ENTRY.getAttribute('data-id');
    }
    return '';
}

function computeVirtualRange() {
    var n = ENTRY_DATA.length;
    if (!n) return { start: 0, end: -1 };

    ensureVirtualRoot();

    var rootRect = VIRTUAL_ROOT.getBoundingClientRect();
    var viewTop = Math.max(0, -rootRect.top);
    var viewBottom = viewTop + window.innerHeight;

    var firstVisible = lowerBoundByItemBottom(viewTop);
    var lastVisible = upperBoundByItemTop(viewBottom);
    if (lastVisible < firstVisible) lastVisible = firstVisible;

    var start = Math.max(0, firstVisible - VIRTUAL_OVERSCAN_ITEMS);
    var end = Math.min(n - 1, lastVisible + VIRTUAL_OVERSCAN_ITEMS);

    var pinnedId = getPinnedEntryId();
    if (pinnedId && typeof ENTRY_INDEX[pinnedId] !== 'undefined') {
        var pinnedIdx = ENTRY_INDEX[pinnedId];
        if (pinnedIdx < start) start = pinnedIdx;
        if (pinnedIdx > end) end = pinnedIdx;
    }

    return { start: start, end: end };
}

function applySelectionStateToNode(entryNode, id) {
    if (!entryNode) return;
    var cb = entryNode.querySelector('.selection-checkbox');
    if (!cb) return;
    cb.style.display = SELECTION_MODE ? 'block' : 'none';
    cb.checked = !!SELECTED_IDS[id];
    entryNode.classList.toggle('selected', SELECTION_MODE && !!SELECTED_IDS[id]);
}

function syncNodeMeta(entryNode, entry) {
    if (!entryNode || !entry) return;
    var timeLine = entryNode.querySelector('.entry-time');
    if (timeLine) timeLine.textContent = entry.time;

    var tagLine = entryNode.querySelector('.entry-tags');
    if (tagLine) {
        if (entry.tags && entry.tags.length) {
            tagLine.textContent = 'Tags: ' + entry.tags.join(', ');
            tagLine.style.display = '';
        } else {
            tagLine.textContent = '';
            tagLine.style.display = 'none';
        }
    }
}

function buildEntryNode(entry) {
    var id = entry.id;

    var div = document.createElement('div');
    div.className = 'entry mode-view';
    div.id = 'entry_' + id;
    div.setAttribute('data-id', id);
    div.tabIndex = 0;

    var header = document.createElement('div');
    header.className = 'entry-header';

    var checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.className = 'selection-checkbox';
    checkbox.setAttribute('data-id', id);
    header.appendChild(checkbox);

    var info = document.createElement('div');
    info.className = 'entry-info';

    var timeLine = document.createElement('div');
    timeLine.className = 'entry-time';
    timeLine.textContent = entry.time;
    info.appendChild(timeLine);

    var tagLine = document.createElement('div');
    tagLine.className = 'entry-tags';
    if (entry.tags && entry.tags.length) {
        tagLine.textContent = 'Tags: ' + entry.tags.join(', ');
    } else {
        tagLine.style.display = 'none';
    }
    info.appendChild(tagLine);

    header.appendChild(info);
    div.appendChild(header);

    var contentArea = document.createElement('div');
    contentArea.className = 'content-area';

    var rendered = document.createElement('div');
    rendered.id = 'rendered_' + id;
    rendered.className = 'rendered-html';
    contentArea.appendChild(rendered);

    var raw = document.createElement('div');
    raw.id = 'raw_' + id;
    raw.className = 'raw-text';
    raw.style.display = 'none';
    raw.spellcheck = false;
    raw.textContent = entry.originalRaw;
    contentArea.appendChild(raw);

    div.appendChild(contentArea);
    applySelectionStateToNode(div, id);
    renderContentForElements(raw, rendered, entry.originalRaw);
    return div;
}

function updateSpacers(start, end) {
    if (!VIRTUAL_TOP_SPACER || !VIRTUAL_BOTTOM_SPACER) return;
    if (end < start || !ENTRY_DATA.length) {
        VIRTUAL_TOP_SPACER.style.height = '0px';
        VIRTUAL_BOTTOM_SPACER.style.height = '0px';
        return;
    }

    var top = ENTRY_OFFSETS[start] || 0;
    var endBottom = (ENTRY_OFFSETS[end] || 0) + getItemBlockHeightByIndex(end);
    var bottom = Math.max(0, ENTRY_TOTAL_HEIGHT - endBottom);

    VIRTUAL_TOP_SPACER.style.height = top + 'px';
    VIRTUAL_BOTTOM_SPACER.style.height = bottom + 'px';
}

function requestMeasureRenderedHeights() {
    if (VIRTUAL_MEASURE_SCHEDULED) return;
    VIRTUAL_MEASURE_SCHEDULED = true;

    requestAnimationFrame(function() {
        VIRTUAL_MEASURE_SCHEDULED = false;
        var hasPending = false;

        for (var id in RENDERED_ENTRY_NODES) {
            if (!Object.prototype.hasOwnProperty.call(RENDERED_ENTRY_NODES, id)) continue;
            var node = RENDERED_ENTRY_NODES[id];
            if (!node || !node.isConnected) continue;
            var h = Math.ceil(node.getBoundingClientRect().height);
            if (!(h > 0)) continue;
            var current = ENTRY_HEIGHTS[id] || 0;
            var pending = VIRTUAL_PENDING_HEIGHTS[id] || 0;
            if (Math.abs(current - h) > 1 || Math.abs(pending - h) > 1) {
                VIRTUAL_PENDING_HEIGHTS[id] = h;
                hasPending = true;
            }
        }

        if (!hasPending) return;

        // Freeze total height during scrolling; align once scrolling stops.
        if (VIRTUAL_IS_SCROLLING) {
            return;
        }
        commitPendingMeasuredHeights(true);
    });
}

function commitPendingMeasuredHeights(lockTopAnchor) {
    if (VIRTUAL_IS_SCROLLING) return false;

    var changed = false;
    for (var id in VIRTUAL_PENDING_HEIGHTS) {
        if (!Object.prototype.hasOwnProperty.call(VIRTUAL_PENDING_HEIGHTS, id)) continue;
        var h = VIRTUAL_PENDING_HEIGHTS[id];
        delete VIRTUAL_PENDING_HEIGHTS[id];
        if (!(h > 0)) continue;
        if (Math.abs((ENTRY_HEIGHTS[id] || 0) - h) > 1) {
            ENTRY_HEIGHTS[id] = h;
            changed = true;
        }
    }

    if (!changed) return false;

    if (lockTopAnchor !== false) {
        // Lock the upper edge of current viewport while rebuilding offsets.
        preserveViewportAnchorDuringRebuild(function() {
            rebuildVirtualIndex();
        });
    } else {
        rebuildVirtualIndex();
    }

    VIRTUAL_SUPPRESS_AUTO_MEASURE = true;
    renderVirtualWindow(true);
    requestAnimationFrame(function() {
        VIRTUAL_SUPPRESS_AUTO_MEASURE = false;
        requestMeasureRenderedHeights();
    });
    return true;
}

function scheduleVirtualRender() {
    if (VIRTUAL_SCROLL_SCHEDULED) return;
    VIRTUAL_SCROLL_SCHEDULED = true;
    requestAnimationFrame(function() {
        VIRTUAL_SCROLL_SCHEDULED = false;
        renderVirtualWindow(false);
    });
}

function renderVirtualWindow(force) {
    ensureVirtualRoot();
    bindVirtualEventsOnce();

    if (!ENTRY_DATA.length) {
        for (var rid in RENDERED_ENTRY_NODES) {
            if (!Object.prototype.hasOwnProperty.call(RENDERED_ENTRY_NODES, rid)) continue;
            var staleNode = RENDERED_ENTRY_NODES[rid];
            if (staleNode) staleNode.remove();
        }
        RENDERED_ENTRY_NODES = Object.create(null);
        VIRTUAL_RANGE_START = 0;
        VIRTUAL_RANGE_END = -1;
        updateSpacers(0, -1);
        if (LAST_ENTRY && !LAST_ENTRY.isConnected) LAST_ENTRY = null;
        return;
    }

    var range = computeVirtualRange();
    if (!force && range.start === VIRTUAL_RANGE_START && range.end === VIRTUAL_RANGE_END) {
        return;
    }

    var wanted = Object.create(null);
    for (var i = range.start; i <= range.end; i++) {
        var entry = ENTRY_DATA[i];
        if (!entry) continue;
        wanted[entry.id] = true;
        if (!RENDERED_ENTRY_NODES[entry.id]) {
            RENDERED_ENTRY_NODES[entry.id] = buildEntryNode(entry);
        } else {
            syncNodeMeta(RENDERED_ENTRY_NODES[entry.id], entry);
            applySelectionStateToNode(RENDERED_ENTRY_NODES[entry.id], entry.id);
        }
    }

    for (var id in RENDERED_ENTRY_NODES) {
        if (!Object.prototype.hasOwnProperty.call(RENDERED_ENTRY_NODES, id)) continue;
        if (wanted[id]) continue;
        if (LAST_ENTRY === RENDERED_ENTRY_NODES[id]) LAST_ENTRY = null;
        RENDERED_ENTRY_NODES[id].remove();
        delete RENDERED_ENTRY_NODES[id];
    }

    for (var j = range.start; j <= range.end; j++) {
        var e2 = ENTRY_DATA[j];
        if (!e2) continue;
        VIRTUAL_ITEMS_HOST.appendChild(RENDERED_ENTRY_NODES[e2.id]);
    }

    updateSpacers(range.start, range.end);
    VIRTUAL_RANGE_START = range.start;
    VIRTUAL_RANGE_END = range.end;

    if (LAST_ENTRY && !LAST_ENTRY.isConnected) LAST_ENTRY = null;
    if (!VIRTUAL_SUPPRESS_AUTO_MEASURE) {
        requestMeasureRenderedHeights();
    }
}

function setEntriesFromNative(entries, isSelectionMode) {
    ensureVirtualRoot();
    bindVirtualEventsOnce();

    SELECTION_MODE = !!isSelectionMode;
    SELECTED_IDS = Object.create(null);
    LAST_ENTRY = null;
    VIRTUAL_IS_SCROLLING = false;
    if (VIRTUAL_SCROLL_IDLE_TIMER) {
        clearTimeout(VIRTUAL_SCROLL_IDLE_TIMER);
        VIRTUAL_SCROLL_IDLE_TIMER = null;
    }
    VIRTUAL_PENDING_HEIGHTS = Object.create(null);
    VIRTUAL_SUPPRESS_AUTO_MEASURE = false;

    for (var id in RENDERED_ENTRY_NODES) {
        if (!Object.prototype.hasOwnProperty.call(RENDERED_ENTRY_NODES, id)) continue;
        if (RENDERED_ENTRY_NODES[id]) RENDERED_ENTRY_NODES[id].remove();
    }
    RENDERED_ENTRY_NODES = Object.create(null);

    ENTRY_DATA = [];
    if (Array.isArray(entries)) {
        for (var i = 0; i < entries.length; i++) {
            var normalized = normalizeEntryPayload(entries[i]);
            if (normalized) ENTRY_DATA.push(normalized);
        }
    }

    rebuildVirtualIndex();
    VIRTUAL_RANGE_START = 0;
    VIRTUAL_RANGE_END = -1;
    renderVirtualWindow(true);
}

function resetEntriesFromNative() {
    setEntriesFromNative([], SELECTION_MODE);
}

function replaceEntriesFromNative(entries, isSelectionMode) {
    setEntriesFromNative(Array.isArray(entries) ? entries : [], isSelectionMode);
}

function addEntryFromNative(entry, isSelectionMode) {
    ensureVirtualRoot();
    bindVirtualEventsOnce();

    if (typeof isSelectionMode !== 'undefined') {
        SELECTION_MODE = !!isSelectionMode;
    }

    var normalized = normalizeEntryPayload(entry);
    if (!normalized) return;

    var idx = ENTRY_INDEX[normalized.id];
    if (typeof idx === 'undefined') {
        ENTRY_DATA.push(normalized);
    } else {
        ENTRY_DATA[idx] = normalized;
    }

    rebuildVirtualIndex();
    renderVirtualWindow(true);
}

function bootstrapEntriesFromNative(scriptId, isSelectionMode) {
    var dataNode = document.getElementById(scriptId);
    if (!dataNode) return;
    var raw = dataNode.textContent || '[]';
    var entries = [];
    try {
        entries = JSON.parse(raw);
    } catch (e) {
        console.error('Failed to parse initial entry JSON:', e);
        return;
    }
    if (!Array.isArray(entries)) return;
    setEntriesFromNative(entries, isSelectionMode);
}

function removeEntryById(id) {
    var idx = ENTRY_INDEX[id];
    if (typeof idx === 'undefined') return;

    delete SELECTED_IDS[id];

    if (RENDERED_ENTRY_NODES[id]) {
        if (LAST_ENTRY === RENDERED_ENTRY_NODES[id]) LAST_ENTRY = null;
        RENDERED_ENTRY_NODES[id].remove();
        delete RENDERED_ENTRY_NODES[id];
    }

    ENTRY_DATA.splice(idx, 1);
    rebuildVirtualIndex();
    renderVirtualWindow(true);
}

function toggleSelectionMode(show) {
    SELECTION_MODE = !!show;
    if (!SELECTION_MODE) {
        SELECTED_IDS = Object.create(null);
    }

    for (var id in RENDERED_ENTRY_NODES) {
        if (!Object.prototype.hasOwnProperty.call(RENDERED_ENTRY_NODES, id)) continue;
        applySelectionStateToNode(RENDERED_ENTRY_NODES[id], id);
    }
}

function getSelectedIds() {
    if (!SELECTION_MODE) return [];
    var ids = [];
    for (var i = 0; i < ENTRY_DATA.length; i++) {
        var id = ENTRY_DATA[i].id;
        if (SELECTED_IDS[id]) ids.push(id);
    }
    return ids;
}

function selectAllEntries(select) {
    if (!SELECTION_MODE) return;
    if (select) {
        for (var i = 0; i < ENTRY_DATA.length; i++) {
            SELECTED_IDS[ENTRY_DATA[i].id] = true;
        }
    } else {
        SELECTED_IDS = Object.create(null);
    }

    for (var id in RENDERED_ENTRY_NODES) {
        if (!Object.prototype.hasOwnProperty.call(RENDERED_ENTRY_NODES, id)) continue;
        applySelectionStateToNode(RENDERED_ENTRY_NODES[id], id);
    }
}

// Click-to-select in selection mode
document.addEventListener('click', function(e) {
    if (!SELECTION_MODE) return;
    var entry = e.target.closest('.entry');
    if (!entry) return;
    var id = entry.getAttribute('data-id');
    if (!id) return;

    if (SELECTED_IDS[id]) delete SELECTED_IDS[id];
    else SELECTED_IDS[id] = true;

    applySelectionStateToNode(entry, id);
    e.preventDefault();
});

// Drag rectangle selection
var dragSelect = false;
var dragStart = {x: 0, y: 0};
var selectionRectEl = null;
document.addEventListener('mousedown', function(e) {
    if (!SELECTION_MODE || e.button !== 0) return;
    dragSelect = true;
    dragStart = {x: e.clientX, y: e.clientY};
    selectionRectEl = document.createElement('div');
    selectionRectEl.className = 'selection-rect';
    document.body.appendChild(selectionRectEl);
});
document.addEventListener('mousemove', function(e) {
    if (!dragSelect || !selectionRectEl) return;
    var x1 = Math.min(dragStart.x, e.clientX);
    var y1 = Math.min(dragStart.y, e.clientY);
    var x2 = Math.max(dragStart.x, e.clientX);
    var y2 = Math.max(dragStart.y, e.clientY);
    selectionRectEl.style.left = x1 + 'px';
    selectionRectEl.style.top = y1 + 'px';
    selectionRectEl.style.width = (x2 - x1) + 'px';
    selectionRectEl.style.height = (y2 - y1) + 'px';

    for (var id in RENDERED_ENTRY_NODES) {
        if (!Object.prototype.hasOwnProperty.call(RENDERED_ENTRY_NODES, id)) continue;
        var entry = RENDERED_ENTRY_NODES[id];
        if (!entry) continue;
        var rect = entry.getBoundingClientRect();
        var overlap = rect.left < x2 && rect.right > x1 && rect.top < y2 && rect.bottom > y1;
        if (overlap) SELECTED_IDS[id] = true;
        else delete SELECTED_IDS[id];
        applySelectionStateToNode(entry, id);
    }
});
document.addEventListener('mouseup', function() {
    if (!dragSelect) return;
    dragSelect = false;
    if (selectionRectEl) {
        selectionRectEl.remove();
        selectionRectEl = null;
    }
});
